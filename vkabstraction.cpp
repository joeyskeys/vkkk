#include <algorithm>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <set>
#include <stdexcept>

#include "vkabstraction.h"

namespace fs = std::filesystem;

namespace vkkk
{

inline constexpr int MAX_FRAMES_IN_FLIGHT = 2;

VkWrappedInstance::VkWrappedInstance()
    : window(nullptr)
{
    // Gui bounded to Vk for now
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    window = glfwCreateWindow(width, height, app_name.c_str(), nullptr, nullptr);

    // Init vulkan
    VkApplicationInfo app_info{};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = app_name.c_str();
    app_info.applicationVersion = app_version;
    app_info.pEngineName = engine_name.c_str();
    app_info.apiVersion = api_version;

    VkInstanceCreateInfo instance_info{};
    instance_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instance_info.pApplicationInfo = &app_info;

    auto extensions = get_default_instance_extensions();
    instance_info.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    instance_info.ppEnabledExtensionNames = extensions.data();

    VkDebugUtilsMessengerCreateInfoEXT debug_create_info{};
    if (enable_validation_layers) {
        instance_info.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
        instance_info.ppEnabledExtensionNames = extensions.data();

        debug_create_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        debug_create_info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT
            | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        debug_create_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
            | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        debug_create_info.pfnUserCallback = default_debug_callback;
        instance_info.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debug_create_info;

    }
    else {
        instance_info.enabledExtensionCount = 0;
        instance_info.pNext = nullptr;
    }

    if (vkCreateInstance(&instance_info, nullptr, &instance) != VK_SUCCESS)
        throw std::runtime_error("failed to create instance..");

    if (enable_validation_layers) {
        auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
            instance, "vkCreateDebugUtilsMessengerEXT");
        VkResult ret = VK_ERROR_EXTENSION_NOT_PRESENT;
        if (func != nullptr) {
            auto ret = func(instance, &debug_create_info, nullptr, &debug_messenger);
            if (ret != VK_SUCCESS)
                throw std::runtime_error("failed to set up debug messenger..");
        }
    }

    uint32_t device_cnt = 0;
    vkEnumeratePhysicalDevices(instance, &device_cnt, nullptr);

    if (device_cnt > 0) {
        physical_devices.resize(device_cnt);
        vkEnumeratePhysicalDevices(instance, &device_cnt, physical_devices.data());
        physical_device = physical_devices[0];
    }
}

VkWrappedInstance::VkWrappedInstance(uint32_t w, uint32_t h, const std::string& appname, const std::string& enginename)
    : width(w)
    , height(h)
    , app_name(appname)
    , engine_name(enginename)
{}

VkWrappedInstance::~VkWrappedInstance() {
    if (syncobj_created) {
        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
            vkDestroySemaphore(device, render_finished_semaphores[i], nullptr);
            vkDestroySemaphore(device, image_available_semaphores[i], nullptr);
            vkDestroyFence(device, in_flight_fences[i], nullptr);
        }
    }

    if (commandpool_created)
        vkDestroyCommandPool(device, command_pool, nullptr);

    if (framebuffer_created)
        for (auto framebuffer : swapchain_framebuffers)
            vkDestroyFramebuffer(device, framebuffer, nullptr);

    if (pipeline_created)
        vkDestroyPipelineLayout(device, pipeline_layout, nullptr);

    if (render_pass_created)
        vkDestroyRenderPass(device, render_pass, nullptr);

    if (imageviews_created) {
        for (auto imageview : swapchain_imageviews)
            vkDestroyImageView(device, imageview, nullptr);
    }

    if (swapchain_created)
        vkDestroySwapchainKHR(device, swapchain, nullptr);

    vkDestroyDevice(device, nullptr);

    if (enable_validation_layers) {
        auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
        if (func != nullptr)
            func(instance, debug_messenger, nullptr);
    }

    vkDestroySurfaceKHR(instance, surface, nullptr);
    vkDestroyInstance(instance, nullptr);

    glfwDestroyWindow(window);
    glfwTerminate();
}

void VkWrappedInstance::create_surface() {
    assert(window != nullptr);
    if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS)
        throw std::runtime_error("failed to create window surface!");
}

bool VkWrappedInstance::validate_current_device(QueueFamilyIndex* idx) {
    if (idx == nullptr)
        return false;

    auto default_device_extensions = get_default_device_extensions();

    bool extensions_supported = check_device_extension_support(default_device_extensions);
    if (!extensions_supported)
        return false;

    bool swapchain_adequate = false;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device, surface, &swapchain_details.capabilities);

    uint32_t format_cnt;
    vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &format_cnt, nullptr);
    if (format_cnt != 0) {
        swapchain_details.formats.resize(format_cnt);
        vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &format_cnt, swapchain_details.formats.data());
    }

    uint32_t present_mode_cnt;
    vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &present_mode_cnt, nullptr);
    if (present_mode_cnt != 0) {
        swapchain_details.present_modes.resize(present_mode_cnt);
        vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &present_mode_cnt, swapchain_details.present_modes.data());
    }

    if (swapchain_details.formats.empty() || swapchain_details.present_modes.empty())
        return false;

    uint32_t queue_family_cnt = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_cnt, nullptr);

    std::vector<VkQueueFamilyProperties> queue_families(queue_family_cnt);
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_cnt, queue_families.data());

    int i = 0;
    VkBool32 present_support = false;
    for (const auto& queue_family : queue_families) {
        if (queue_family.queueFlags & VK_QUEUE_GRAPHICS_BIT)
            idx->graphic_family = i;

        vkGetPhysicalDeviceSurfaceSupportKHR(physical_device, i, surface, &present_support);
        if (present_support)
            idx->present_family = i;

        if (idx->is_valid())
            return true;

        ++i;
    }

    return false;
}

void VkWrappedInstance::create_logical_device() {
    if (!validate_current_device(&queue_family_idx))
        throw std::runtime_error("Queue specified not available");

    // Queue create info
    std::vector<VkDeviceQueueCreateInfo> queue_create_infos;
    std::set<uint32_t> queue_families = {
        queue_family_idx.graphic_family.value(),
        queue_family_idx.present_family.value()
    };
    
    float queue_priority = 1.f;
    for (auto& queue_family : queue_families) {
        VkDeviceQueueCreateInfo queue_create_info{};
        queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue_create_info.queueFamilyIndex = queue_family;
        queue_create_info.queueCount = 1;
        queue_create_info.pQueuePriorities = &queue_priority;
        queue_create_infos.push_back(queue_create_info);
        queue_idx_vec.push_back(queue_family);
    }

    // Device feature
    VkPhysicalDeviceFeatures device_features{};

    // Device create info
    VkDeviceCreateInfo device_create_info{};
    device_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device_create_info.queueCreateInfoCount = static_cast<uint32_t>(queue_create_infos.size());
    device_create_info.pQueueCreateInfos = queue_create_infos.data();
    device_create_info.pEnabledFeatures = &device_features;

    auto default_device_extensions = get_default_device_extensions();
    device_create_info.enabledExtensionCount = static_cast<uint32_t>(default_device_extensions.size());
    device_create_info.ppEnabledExtensionNames = default_device_extensions.data();

    // Modern impls will have this info from instance, ignore for now..
    // device_create_info.enabledExtensionCount = 0;
    // if (enable_validation_layers) {
    //  device_create_info.enabledLayerCount = validationLayers.size();
    //  device_create_info.ppEnabledLayerNames = validationLayers.data();
    // }
    // else
    //  device_create_info.enabledLayerCount = 0;

    // Create logical device
    if (vkCreateDevice(physical_device, &device_create_info, nullptr, &device) != VK_SUCCESS)
        throw std::runtime_error("failed to create logical device!");

    // Retrieve queue
    vkGetDeviceQueue(device, queue_family_idx.graphic_family.value(), 0, &graphic_queue);
    vkGetDeviceQueue(device, queue_family_idx.present_family.value(), 0, &present_queue);

    queue_created = true;
}

void VkWrappedInstance::create_swapchain() {
    if (!queue_created)
        throw std::runtime_error("Queue not created yet, cannot create swapchain");

    swapchain_surface_format = choose_swap_surface_format(swapchain_details.formats);
    VkPresentModeKHR present_mode = choose_swap_present_mode(swapchain_details.present_modes);
    swapchain_extent = choose_swap_extent(swapchain_details.capabilities);

    uint32_t image_cnt = swapchain_details.capabilities.minImageCount + 1;
    if (swapchain_details.capabilities.maxImageCount > 0 && image_cnt > swapchain_details.capabilities.maxImageCount)
        image_cnt = swapchain_details.capabilities.maxImageCount;

    VkSwapchainCreateInfoKHR create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    create_info.surface = surface;
    
    create_info.minImageCount = image_cnt;
    create_info.imageFormat = swapchain_surface_format.format;
    create_info.imageColorSpace = swapchain_surface_format.colorSpace;
    create_info.imageExtent = swapchain_extent;
    create_info.imageArrayLayers = 1;
    create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    if (queue_idx_vec.size() == 1)
        create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    else {
        create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        create_info.queueFamilyIndexCount = 2;
        create_info.pQueueFamilyIndices = queue_idx_vec.data();
    }

    create_info.preTransform = swapchain_details.capabilities.currentTransform;
    create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    create_info.presentMode = present_mode;
    create_info.clipped = VK_TRUE;

    create_info.oldSwapchain = VK_NULL_HANDLE;

    if (vkCreateSwapchainKHR(device, &create_info, nullptr, &swapchain) != VK_SUCCESS)
        throw std::runtime_error("failed to create swap chain");

    vkGetSwapchainImagesKHR(device, swapchain, &image_cnt, nullptr);
    swapchain_images.resize(image_cnt);
    vkGetSwapchainImagesKHR(device, swapchain, &image_cnt, swapchain_images.data());

    swapchain_created = true;
}

void VkWrappedInstance::create_imageviews() {
    swapchain_imageviews.resize(swapchain_images.size());

    for (int i = 0; i < swapchain_images.size(); ++i) {
        VkImageViewCreateInfo create_info{};
        create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        create_info.image = swapchain_images[i];
        create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        create_info.format = swapchain_surface_format.format;
        create_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        create_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        create_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        create_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        
        create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        create_info.subresourceRange.baseMipLevel = 0;
        create_info.subresourceRange.levelCount = 1;
        create_info.subresourceRange.baseArrayLayer = 0;
        create_info.subresourceRange.layerCount = 1;

        if (vkCreateImageView(device, &create_info, nullptr, &swapchain_imageviews[i]) != VK_SUCCESS)
            throw std::runtime_error("failed to create image views!");

        imageviews_created = true;
    }
}

static std::vector<char> load_file(const fs::path& filepath) {
    std::ifstream file(filepath, std::ios::ate | std::ios::binary);

    if (!file.good()) {
        //throw std::runtime_error(std::format("failed to open file : {}..", filepath));
        throw std::runtime_error("failed to open file..");
    }

    size_t file_size = fs::file_size(filepath);
    std::vector<char> buffer(file_size);
    file.seekg(0);
    file.read(buffer.data(), file_size);
    file.close();

    return buffer;
}

void VkWrappedInstance::create_renderpass() {
    VkAttachmentDescription attachment{};
    attachment.format = swapchain_surface_format.format;
    attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference attachment_ref{};
    attachment_ref.attachment = 0;
    attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &attachment_ref;

    VkRenderPassCreateInfo pass_info{};
    pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    pass_info.attachmentCount = 1;
    pass_info.pAttachments = &attachment;
    pass_info.subpassCount = 1;
    pass_info.pSubpasses = &subpass;

    if (vkCreateRenderPass(device, &pass_info, nullptr, &render_pass) != VK_SUCCESS)
        throw std::runtime_error("failed to create render pass!");

    render_pass_created = true;
}

VkShaderModule VkWrappedInstance::create_shader_module(std::vector<char>& buf) {
    VkShaderModuleCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    create_info.codeSize = buf.size();
    create_info.pCode = reinterpret_cast<const uint32_t*>(buf.data());

    VkShaderModule shader_module;
    if (vkCreateShaderModule(device, &create_info, nullptr, &shader_module) != VK_SUCCESS)
        throw std::runtime_error("failed to create shader module!");

    return shader_module;
}

void VkWrappedInstance::create_graphics_pipeline() {
    auto vert_code = load_file("../resource/shaders/default_vert.spv");
    auto frag_code = load_file("../resource/shaders/default_frag.spv");

    auto vert_shader_module = create_shader_module(vert_code);
    auto frag_shader_module = create_shader_module(frag_code);

    VkPipelineShaderStageCreateInfo vert_stage_create_info{};
    vert_stage_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vert_stage_create_info.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vert_stage_create_info.module = vert_shader_module;
    vert_stage_create_info.pName = "main";

    VkPipelineShaderStageCreateInfo frag_stage_create_info{};
    frag_stage_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    frag_stage_create_info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    frag_stage_create_info.module = frag_shader_module;
    frag_stage_create_info.pName = "main";

    VkPipelineShaderStageCreateInfo shader_stages[] = { vert_stage_create_info, frag_stage_create_info };
    
    VkPipelineVertexInputStateCreateInfo vert_input_info{};
    vert_input_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vert_input_info.vertexBindingDescriptionCount = 0;
    vert_input_info.vertexAttributeDescriptionCount = 0;

    VkPipelineInputAssemblyStateCreateInfo input_assembly{};
    input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    input_assembly.primitiveRestartEnable = VK_FALSE; // Restart with index 0xffff

    VkViewport viewport{};
    viewport.x = 0.f;
    viewport.y = 0.f;
    viewport.width = static_cast<float>(swapchain_extent.width);
    viewport.height = static_cast<float>(swapchain_extent.height);
    viewport.minDepth = 0.f;
    viewport.maxDepth = 1.f; // Vk depth range is [0, 1]

    VkRect2D scissor{};
    scissor.offset = { 0, 0 };
    scissor.extent = swapchain_extent;

    VkPipelineViewportStateCreateInfo viewport_state{};
    viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_state.viewportCount = 1;
    viewport_state.pViewports = &viewport;
    viewport_state.scissorCount = 1;
    viewport_state.pScissors = &scissor;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.f;
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState colorblend_attachment{};
    colorblend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
        VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorblend_attachment.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo colorblending{};
    colorblending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorblending.logicOpEnable = VK_FALSE;
    colorblending.logicOp = VK_LOGIC_OP_COPY;
    colorblending.attachmentCount = 1;
    colorblending.pAttachments = &colorblend_attachment;
    colorblending.blendConstants[0] = 0.f;
    colorblending.blendConstants[1] = 0.f;
    colorblending.blendConstants[2] = 0.f;
    colorblending.blendConstants[3] = 0.f;

    VkPipelineLayoutCreateInfo pipeline_layout_info{};
    pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipeline_layout_info.setLayoutCount = 0;
    pipeline_layout_info.pushConstantRangeCount = 0;

    if (vkCreatePipelineLayout(device, &pipeline_layout_info, nullptr, &pipeline_layout) != VK_SUCCESS)
        throw std::runtime_error("failed to create pipeline layout!");

    VkGraphicsPipelineCreateInfo pipeline_info{};
    pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline_info.stageCount = 2;
    pipeline_info.pStages = shader_stages;
    pipeline_info.pVertexInputState = &vert_input_info;
    pipeline_info.pInputAssemblyState = &input_assembly;
    pipeline_info.pViewportState = &viewport_state;
    pipeline_info.pRasterizationState = &rasterizer;
    pipeline_info.pMultisampleState = &multisampling;
    pipeline_info.pColorBlendState = &colorblending;
    pipeline_info.layout = pipeline_layout;
    pipeline_info.renderPass = render_pass;
    pipeline_info.subpass = 0;
    pipeline_info.basePipelineHandle = VK_NULL_HANDLE;

    if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &pipeline) != VK_SUCCESS)
        throw std::runtime_error("failed to create graphics pipeline!");

    vkDestroyShaderModule(device, vert_shader_module, nullptr);
    vkDestroyShaderModule(device, frag_shader_module, nullptr);

    pipeline_created = true;
}

void VkWrappedInstance::create_framebuffers() {
    swapchain_framebuffers.resize(swapchain_imageviews.size());

    for (int i = 0; i < swapchain_imageviews.size(); ++i) {
        VkImageView attachments[] = {
            swapchain_imageviews[i]
        };

        VkFramebufferCreateInfo framebuffer_info{};
        framebuffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebuffer_info.renderPass = render_pass;
        framebuffer_info.attachmentCount = 1;
        framebuffer_info.pAttachments = attachments;
        framebuffer_info.width = swapchain_extent.width;
        framebuffer_info.height = swapchain_extent.height;
        framebuffer_info.layers = 1;

        if (vkCreateFramebuffer(device, &framebuffer_info, nullptr, &swapchain_framebuffers[i]) != VK_SUCCESS)
            throw std::runtime_error("failed to create framebuffer!");
    }

    framebuffer_created = true;
}

void VkWrappedInstance::create_command_pool() {
    VkCommandPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_info.queueFamilyIndex = queue_family_idx.graphic_family.value();

    if (vkCreateCommandPool(device, &pool_info, nullptr, &command_pool) != VK_SUCCESS)
        throw std::runtime_error("failed to create command pool!");

    commandpool_created = true;
}

void VkWrappedInstance::create_commandbuffers() {
    if (!commandpool_created)
        throw std::runtime_error("command pool not created!");

    commandbuffers.resize(swapchain_framebuffers.size());

    VkCommandBufferAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.commandPool = command_pool;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandBufferCount = static_cast<uint32_t>(commandbuffers.size());

    if (vkAllocateCommandBuffers(device, &alloc_info, commandbuffers.data()) != VK_SUCCESS)
        throw std::runtime_error("failed to allocate command buffers!");

    for (int i = 0; i < commandbuffers.size(); ++i) {
        VkCommandBufferBeginInfo begin_info{};
        begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        
        if (vkBeginCommandBuffer(commandbuffers[i], &begin_info) != VK_SUCCESS)
            throw std::runtime_error("failed to begin recording command buffer!");

        VkRenderPassBeginInfo renderpass_info{};
        renderpass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderpass_info.renderPass = render_pass;
        renderpass_info.framebuffer = swapchain_framebuffers[i];
        renderpass_info.renderArea.offset = { 0, 0 };
        renderpass_info.renderArea.extent = swapchain_extent;

        VkClearValue clear_color = { {{0.f, 0.f, 0.f, 1.f}} };
        renderpass_info.clearValueCount = 1;
        renderpass_info.pClearValues = &clear_color;

        vkCmdBeginRenderPass(commandbuffers[i], &renderpass_info, VK_SUBPASS_CONTENTS_INLINE);
            vkCmdBindPipeline(commandbuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
            vkCmdDraw(commandbuffers[i], 3, 1, 0, 0);
        vkCmdEndRenderPass(commandbuffers[i]);

        if (vkEndCommandBuffer(commandbuffers[i]) != VK_SUCCESS)
            throw std::runtime_error("failed to record command buffer!");

        commandbuffer_created = true;
    }
}

void VkWrappedInstance::create_sync_objects() {
    image_available_semaphores.resize(MAX_FRAMES_IN_FLIGHT);
    render_finished_semaphores.resize(MAX_FRAMES_IN_FLIGHT);
    in_flight_fences.resize(MAX_FRAMES_IN_FLIGHT);
    images_in_flight.resize(swapchain_images.size(), VK_NULL_HANDLE);

    VkSemaphoreCreateInfo semaphor_info{};
    semaphor_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fence_info{};
    fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        if (vkCreateSemaphore(device, &semaphor_info, nullptr, &image_available_semaphores[i]) != VK_SUCCESS ||
            vkCreateSemaphore(device, &semaphor_info, nullptr, &render_finished_semaphores[i]) != VK_SUCCESS ||
            vkCreateFence(device, &fence_info, nullptr, &in_flight_fences[i]) != VK_SUCCESS)
            throw std::runtime_error("failed to create synchronization objects for a frame");
    }
}

void VkWrappedInstance::draw_frame() {
    vkWaitForFences(device, 1, &in_flight_fences[current_frame], VK_TRUE, UINT64_MAX);

    uint32_t image_idx;
    vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, image_available_semaphores[current_frame],
        VK_NULL_HANDLE, &image_idx);

    if (images_in_flight[image_idx] != VK_NULL_HANDLE)
        vkWaitForFences(device, 1, &images_in_flight[image_idx], VK_TRUE, UINT64_MAX);
    images_in_flight[image_idx] = in_flight_fences[current_frame];

    VkSubmitInfo submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore wait_semaphores[] = { image_available_semaphores[current_frame] };
    VkPipelineStageFlags wait_stages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    submit_info.waitSemaphoreCount = 1;
    submit_info.pWaitSemaphores = wait_semaphores;
    submit_info.pWaitDstStageMask = wait_stages;

    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &commandbuffers[image_idx];

    VkSemaphore signal_semaphores[] = { render_finished_semaphores[current_frame] };
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = signal_semaphores;

    vkResetFences(device, 1, &in_flight_fences[current_frame]);

    if (vkQueueSubmit(graphic_queue, 1, &submit_info, in_flight_fences[current_frame]) != VK_SUCCESS)
        throw std::runtime_error("failed to submit draw command buffer!");

    VkPresentInfoKHR present_info{};
    present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores = signal_semaphores;

    VkSwapchainKHR swapchains_for_present[] = { swapchain };
    present_info.swapchainCount = 1;
    present_info.pSwapchains = swapchains_for_present;

    present_info.pImageIndices = &image_idx;

    vkQueuePresentKHR(present_queue, &present_info);
    current_frame = (current_frame + 1) % MAX_FRAMES_IN_FLIGHT;
}

void VkWrappedInstance::mainloop() {
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        draw_frame();
    }

    vkDeviceWaitIdle(device);
}

std::vector<const char*> VkWrappedInstance::get_default_instance_extensions() {
    uint32_t glfw_extension_cnt = 0;
    const char** glfw_extensions;
    glfw_extensions = glfwGetRequiredInstanceExtensions(&glfw_extension_cnt);
    std::vector<const char*> extensions(glfw_extensions, glfw_extensions + glfw_extension_cnt);
    if (enable_validation_layers)
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

    return extensions;
}

bool VkWrappedInstance::check_device_extension_support(const std::span<const char*> extensions) const {
    uint32_t extension_cnt;
    vkEnumerateDeviceExtensionProperties(physical_device, nullptr, &extension_cnt, nullptr);
    std::vector<VkExtensionProperties> available_extensions(extension_cnt);
    vkEnumerateDeviceExtensionProperties(physical_device, nullptr, &extension_cnt, available_extensions.data());

    std::set<std::string> required_extensions(extensions.begin(), extensions.end());
    for (const auto& extension : available_extensions) {
        required_extensions.erase(extension.extensionName);
    }
    return required_extensions.empty();
}

VkSurfaceFormatKHR VkWrappedInstance::choose_swap_surface_format(const std::vector<VkSurfaceFormatKHR>& available_formats) const {
    for (const auto& available_format : available_formats) {
        if (available_format.format == VK_FORMAT_R8G8B8A8_SRGB && available_format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            return available_format;
    }

    return available_formats[0];
}

VkPresentModeKHR VkWrappedInstance::choose_swap_present_mode(const std::vector<VkPresentModeKHR>& available_present_modes) const {
    for (const auto& available_present_mode : available_present_modes) {
        if (available_present_mode == VK_PRESENT_MODE_MAILBOX_KHR)
            return available_present_mode;
    }

    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D VkWrappedInstance::choose_swap_extent(const VkSurfaceCapabilitiesKHR& capabilities) const {
    if (capabilities.currentExtent.width != UINT32_MAX)
        return capabilities.currentExtent;
    else {
        int w, h;
        glfwGetFramebufferSize(window, &w, &h);
        VkExtent2D actual_extent = {
            static_cast<uint32_t>(w),
            static_cast<uint32_t>(h)
        };

        actual_extent.width = std::clamp(actual_extent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
        actual_extent.height = std::clamp(actual_extent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
        return actual_extent;
    }
}

}