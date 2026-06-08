#include <GLFW/glfw3.h>
#include <vector>

#include "concepts/context.hpp"

namespace vkkk
{

static VKAPI_ATTR vk::Bool32 VKAPI_CALL debug_callback(
    vk::DebugUtilsMessageSeverityFlagBitsEXT severity,
    vk::DebugUtilsMessageTypeFlagsEXT type,
    const vk::DebugUtilsMessengerCallbackDataEXT* callback_data,
    void* user_data)
{
    if (severity >= vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning) {
        std::cerr << "[vkkk] " << callback_data->pMessage << std::endl;
    }
    return vk::False;
}

WrappedContext::WrappedContext(
    const char* app_name,
    uint32_t app_version,
    const char* engine_name,
    uint32_t api_version,
    bool enable_validation_layers
    const std::vector<const char*>& extra_validation_layers,
    const std::vector<const char*>& extra_extensions)
{
    // 1. create the instance
    constexpr vk::ApplicationInfo app_info(app_name, app_version, engine_name, api_version);

    std::vector<const char*> validation_layers;
    if (enable_validation_layers) {
        validation_layers.assign(default_validation_layers.begin(), default_validation_layers.end());
        if (!extra_validation_layers.empty()) {
            validation_layers.insert(validation_layers.end(), extra_validation_layers.begin(), extra_validation_layers.end());
        }
    }

    auto layer_props = context.enumerateInstanceLayerProperties();
    auto unsupported_layers = std::ranges::find_if(validation_layers, [&layer_props](const auto& layer) {
        return std::ranges:none_of(layer_props, [layer](auto const& layer_prop) { return strcmp(layer_prop.layerName, layer) == 0; });
    });
    if (unsupported_layers != validation_layers.end()) {
        throw std::runtime_error("Unsupported validation layer: " + std::string(unsupported_layers->layerName));
    }

    auto extension_props = context.enumerateInstanceExtensionProperties();
    auto unsupported_extensions = std::ranges::find_if(extra_extensions, [&extension_props](const auto& extension) {
        return std::ranges:none_of(extension_props, [extension](auto const& extension_prop) { return strcmp(extension_prop.extensionName, extension) == 0; });
    });
    if (unsupported_extensions != extra_extensions.end()) {
        throw std::runtime_error("Unsupported extension: " + std::string(unsupported_extensions->extensionName));
    }

    vk::InstanceCreateInfo instance_create_info{
        .pApplicationInfo = &app_info,
        .enabledLayerCount = static_cast<uint32_t>(validation_layers.size()),
        .ppEnabledLayerNames = validation_layers.data(),
        .enabledExtensionCount = static_cast<uint32_t>(extra_extensions.size()),
        .ppEnabledExtensionNames = extra_extensions.data()
    };
    instance = vk::raii::Instance(context, instance_create_info);

    if (enable_validation_layers && enable_debug_messenger) {
        setup_debug_messenger();
    }
}

WrappedContext::setup_debug_messenger() {
    vk::DebugUtilsMessageServerityFlagsEXT severity_flags(vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
                                                          vk::DebugUtilsMessageSeverityFlagBitsEXT::eError);
    vk::DebugUtilsMessageTypeFlagsEXT message_types(vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
                                                    vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation |
                                                    vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance);
    vk::DebugUtilsMessengerCreateInfoEXT debug_utils_messenger_create_info{
        .messageSeverity = severity_flags,
        .messageType = message_types,
        .pfnUserCallback = &debug_callback
    };
    debug_messenger = vk::raii::DebugUtilsMessenger(instance, debug_utils_messenger_create_info);
}

void WrappedContext::transit_image_layout(
    uint32_t image_index,
    vk::ImageLayout old_layout,
    vk::ImageLayout new_layout,
    vk::AccessFlags2 src_access_mask,
    vk::AccessFlags2 dst_access_mask,
    vk::PipelineStageFlags2 src_stage_mask,
    vk::PipelineStageFlags2 dst_stage_mask,
) {
    vk::ImageMemoryBarrier2 barrier{
        .srcStageMask = src_stage_mask,
        .dstStageMask = dst_stage_mask,
        .srcAccessMask = src_access_mask,
        .dstAccessMask = dst_access_mask,
        .oldLayout = old_layout,
        .newLayout = new_layout,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = swapchain_images[image_index],
        .subresourceRange = {
            .aspectMask = vk::ImageAspectFlagBits::eColor,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1
        }
    };
    vk::DependencyInfo dependency_info{
        .dependencyFlags = {},
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &barrier
    };
    command_buffers[image_index].pipelineBarrier2(dependency_info);
}

void WrappedContext::create_swapchain() {
    // create the swapchain
    vk::SurfaceCapabilitiesKHR surface_capabilities = physical_device.getSurfaceCapabilitiesKHR(*surface);
    swapchain_extent = choose_swap_extent(surface_capabilities);
    uint32_t min_image_count = choose_min_image_count(surface_capabilities);

    std::vector<vk::SurfaceFormatKHR> surface_formats = physical_device.getSurfaceFormatsKHR(*surface);
    swapchain_surface_format = choose_swap_surface_format(surface_formats);

    std::vector<vk::PresentModeKHR> present_modes = physical_device.getSurfacePresentModesKHR(*surface);
    vk::PresentModeKHR present_mode = choose_present_mode(present_modes);

    vk::SwapchainCreateInfoKHR swapchain_create_info{
        .surface = *surface,
        .minImageCount = min_image_count,
        .imageFormat = swapchain_surface_format.format,
        .imageColorSpace = swapchain_surface_format.colorSpace,
        .imageExtent = swapchain_extent,
        .imageArrayLayers = 1,
        .imageUsage = vk::ImageUsageFlagBits::eColorAttachment,
        .imageSharingMode = vk::SharingMode::eExclusive,
        .preTransform = surface_capabilities.currentTransform,
        .compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque,
        .presentMode = present_mode,
        .clipped = true
    };
    swapchain = vk::raii::SwapchainKHR(device, swapchain_create_info);
    swapchain_images = swapchain.getImages();
}

void WrappedContext::create_imageviews() {
    // create the image views
    assert(swapchain_image_views.empty());
    vk::ImageViewCreateInfo image_view_create_info{
        .viewType = vk::ImageViewType::e2D,
        .format = swapchain_surface_format.format,
        .subresourceRange = {
            .aspectMask = vk::ImageAspectFlagBits::eColor,
            .levelCount = 1,
            .layerCount = 1
        }
    };
    for (auto& image : swapchain_images) {
        image_view_create_info.image = image;
        swapchain_image_views.emplace_back(device, image_view_create_info);
    }
}

static bool is_device_suitable(const vk::raii::PhysicalDevice& device, const std::vector<const char*>& required_extensions) {
    bool support_vk_1_3 = device.getProperties().apiVersion >= VK_API_VERSION_1_3;

    auto queue_families = device.getQueueFamilyProperties();
    bool support_graphics = std::ranges::any_of(queue_families, [](const auto& queue_family) {
        return queue_family.queueFlags & vk::QueueFlagBits::eGraphics;
    });

    auto available_extensions = device.enumerateDeviceExtensionProperties();
    bool supports_all_required_extensions = std::ranges::all_of(required_extensions, [&available_extensions](const auto& extension) {
        return std::ranges::any_of(available_extensions, [extension](const auto& available_extension) {
            return strcmp(available_extension.extensionName, extension) == 0;
        });
    });
}

static uint32_t choose_min_image_count(const vk::SurfaceCapabilitiesKHR& surface_capabilities) {
    uint32_t min_image_count = surface_capabilities.minImageCount + 1;
    if (surface_capabilities.maxImageCount > 0 && min_image_count > surface_capabilities.maxImageCount) {
        min_image_count = surface_capabilities.maxImageCount;
    }
    return min_image_count;
}

static vk::SurfaceFormatKHR choose_swap_surface_format(const std::vector<vk::SurfaceFormatKHR>& surface_formats) {
    assert(!surface_formats.empty());
    const auto format_iter = std::ranges::find_if(surface_formats, [](const auto& surface_format) {
        return surface_format.format == vk::Format::eB8G8R8A8Srgb && surface_format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear;
    });
    return format_iter != surface_formats.end() ? *format_iter : surface_formats.front();
}

static vk::PresentModeKHR choose_present_mode(const std::vector<vk::PresentModeKHR>& present_modes) {
    assert(std::ranges::any_of(present_modes, [](const auto& present_mode) {
        return present_mode == vk::PresentModeKHR::eFifo;
    }));
    return std::ranges::any_of(present_modes, [](const auto& present_mode) {
        return present_mode == vk::PresentModeKHR::eMailbox;
    }) ? vk::PresentModeKHR::eMailbox : vk::PresentModeKHR::eFifo;
}

static vk::Extent2D choose_swap_extent(const vk::SurfaceCapabilitiesKHR& surface_capabilities) {
    if (surface_capabilities.currentExtent.width != UINT32_MAX) {
        return surface_capabilities.currentExtent;
    }
    int framebuffer_width = 0;
    int framebuffer_height = 0;
    glfwGetFramebufferSize(window, &framebuffer_width, &framebuffer_height);
    return {
        std::clamp<uint32_t>(framebuffer_width, surface_capabilities.minImageExtent.width, surface_capabilities.maxImageExtent.width),
        std::clamp<uint32_t>(framebuffer_height, surface_capabilities.minImageExtent.height, surface_capabilities.maxImageExtent.height)
    }
}

WrappedContext::init(GLFWwindow* window) {
    // create the surface
    VkSurfaceKHR surface = nullptr;
    if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS) {
        throw std::runtime_error("failed to create window surface!");
    }
    surface = vk::raii::SurfaceKHR(instance, surface);

    // choose the physical device
    std::vector<vk::raii::PhysicalDevice> devices = instance.enumeratePhysicalDevices();
    auto const dev_iter = std::ranges::find_if(devices, [&](const auto& device) {
        return is_device_suitable(device, required_extensions);
    });
    if (dev_iter == devices.end()) {
        throw std::runtime_error("no suitable Vulkan 1.3 device with dynamic rendering support");
    }
    physical_device = *dev_iter;

    // create the logical device
    std::vector<vk:QueueFamilyProperties> queue_family_properties = physical_device.getQueueFamilyProperties();
    for (uint32_t i = 0; i < queue_family_properties.size(); ++i) {
        if ((queue_family_properties[i].queueFlags & vk::QueueFlagBits::eGraphics) && (physical_device.getSurfaceSupportKHR(i, *surface))) {
            queue_idx = i;
            break;
        }
    }
    if (queue_idx == ~0) {
        throw std::runtime_error("no suitable queue family found");
    }

    vk::StructureChain<vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceVulkan13Features, vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT> device_features{
        {},
        {.dynamicRendering = true},
        {.extendedDynamicState = true}
    };

    float queue_priority = 0.5f;
    vk::DeviceQueueCreateInfo queue_create_info{.queueFamilyIndex = queue_idx, .queueCount = 1, .pQueuePriorities = &queue_priority};
    vk::DeviceCreateInfo device_create_info{
        .pNext = &device_features.get<vk::PhysicalDeviceFeatures2>(),
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos = &queue_create_info,
        .enabledExtensionCount = static_cast<uint32_t>(required_extensions.size()),
        .ppEnabledExtensionNames = required_extensions.data()
    };
    device = vk::raii::Device(physical_device, device_create_info);
    queue = vk::raii::Queue(device, queue_idx, 0);

    create_swapchain();
    create_imageviews();

    // create the command pool
    // TODO: eResetCommandBuffer is a mode the command pool kinda persists
    // there's also another flag eTransient that is more like a one-time use pool
    // maybe needed in the future. Might need more methods and fields to 
    // manage the command pool.
    vk::CommandPoolCreateInfo command_pool_create_info{
        .queueFamilyIndex = queue_idx,
        .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer
    };
    command_pool = vk::raii::CommandPool(device, command_pool_create_info);

    // create the command buffers
    vk::CommandBufferAllocateInfo alloc_info{.commandPool = command_pool, .level = vk::CommandBufferLevel::ePrimary, .commandBufferCount = max_frames_in_flight};
    command_buffers = vk::raii::CommandBuffers(device, alloc_info);

    // create sync objects
    assert(present_complete_semaphores.empty() && render_complete_semaphores.empty() && in_flight_fences.empty());
    for (size_t i = 0; i < swapchain_images.size(); ++i) {
        render_finished_semaphores.emplace_back(device, vk::SemaphoreCreateInfo{});
    }
    for (size_t i = 0; i < max_frames_in_flight; ++i) {
        present_complete_semaphores.emplace_back(device, vk::SemaphoreCreateInfo{});
        in_flight_fences.emplace_back(device, vk::FenceCreateInfo{
            .flags = vk::FenceCreateFlagBits::eSignaled
        });
    }
}

static std::vector<vk::VertexInputBindingDescription> gen_binding_desc(const std::vector<VERT_COMP>& comps, bool interleaved) {
    // a vertex binding itself being interleaved or not is irrelavent to others
    // the mesh format can be quite flexible, for example, vertices being a single buf,
    // uv and color interleaved being another buf. Each binding desc is related to its
    // attr desc.
    // Here we assume the mesh being either all interleaved or all separated.
    std::vector<vk::VertexInputBindingDescription> binding_descriptions;
    if (interleaved) {
        binding_descriptions.emplace_back(0, get_mesh_component_size(comps) * sizeof(float), vk::VertexInputRate::eVertex);
    }
    else {
        for (const auto& comp : comps) {
            binding_descriptions.emplace_back(0, comp_sizes[comp] * sizeof(float), vk::VertexInputRate::eVertex);
        }
    }
    return binding_descriptions;
}

bool WrappedContext::create_pipeline(const std::string& name,
    const ShaderModulePack& shader_module_pack,
    const PipelineOption& option,
    const std::vector<VERT_COMP>& comps,
    bool interleaved)
{
    if (pipelines.find(name) != pipelines.end()) {
        std::cout << "Pipeline " << name << " already exists" << std::endl;
        return false;
    }

    // resources
    std::vector<vk::VertexInputBindingDescription> input_binding_descs;
    std::vector<vk::VertexInputAttributeDescription> input_attr_descs;
    std::vector<vk::DescriptorSetLayoutBinding> descriptor_layouts;
    std::map<uint32_t, std::string> ubo_binding_to_name;
    std::map<uint32_t, std::string> tex_binding_to_name;

    // shader modules
    std::vector<vk::ShaderModule> shader_modules;
    std::vector<vk::PipelineShaderStageCreateInfo> shader_stage_infos;
    for (auto& [stage, module] : shader_module_pack.modules) {
        // handle shader itself
        vk::ShaderModuleCreateInfo shader_module_create_info{
            .codeSize = module.spirv_code.size() * sizeof(uint32_t),
            .pCode = module.spirv_code.data()
        };
        vk::ShaderModule shader_module = vk::raii::ShaderModule(device, shader_module_create_info);
        shader_modules.emplace_back(std::move(shader_module));

        vk::PipelineShaderStageCreateInfo shader_stage_info{
            .stage = stage,
            .module = shader_modules.back(),
            .pName = "main"
        };
        shader_stage_infos.emplace_back(std::move(shader_stage_info));

        // shader input infos
        // input attrs
        if (stage == vk::ShaderStageFlagBits::eVertex) {
            input_binding_descs = gen_binding_desc(option.comps, interleaved);

            uint32_t offset = 0;
            for (int i = 0; const auto& [attr_loc, glsl_type, attr_name] : module.attr_infos) {
                if (interleaved) {
                    vk::VertexInputAttributeDescription attr_desc{
                        .location = attr_loc,
                        .binding = 0,
                        .format = glsl_type_macro[glsl_type],
                        .offset = offset
                    };
                    input_attr_descs.emplace_back(std::move(attr_desc));
                    offset += glsl_type_sizes[glsl_type];
                }
                else {
                    vk::VertexInputAttributeDescription attr_desc{
                        .location = attr_loc,
                        .binding = i,
                        .format = glsl_type_macro[glsl_type],
                        .offset = 0
                    };
                    input_attr_descs.emplace_back(std::move(attr_desc));
                }
                ++i;
            }
        }

        // ubos
        for (auto& [ubo_name, ubo_info] : module.buf_infos) {
            auto& [struct_size, array_size, binding] = ubo_info;
            auto ppl_ubo_name = name + ":" + ubo_name;
            ubo_binding_to_name[binding] = ppl_ubo_name;

            vk::DescriptorSetLayoutBinding descriptor_layout{
                .binding = binding,
                .descriptorType = vk::DescriptorType::eUniformBuffer,
                .descriptorCount = array_size,
            };
            descriptor_layouts.emplace_back(std::move(descriptor_layout));
        }

        // texes
        for (auto& [tex_name, tex_info] : module.img_infos) {
            const auto ppl_tex_name = name + ":" + tex_name;
            uint32_t descriptor_count = 1;
            if (textures.find(ppl_tex_name) == textures.end()) {
                auto tex_path_info = module.tex_img_pairs.find(tex_name);
                if (tex_path_info == module.tex_img_pairs.end()) {
                    std::cout << "No texture assigned for sampler " << tex_name
                        << std::endl;
                    continue;
                }

                auto& [path, is_cubemap] = tex_path_info->second;
                descriptor_count = is_cubemap ? 6u : 1u;
                if (!is_cubemap) {
                    if (!add_texture(ppl_tex_name, tex_binding, path)) {
                        continue;
                    }
                }
                else if (!add_cubemap(ppl_tex_name, tex_binding, path)) {
                    continue;
                }
            }
            else {
                descriptor_count = static_cast<uint32_t>(textures.at(ppl_tex_name).vecsize);
            }
            tex_binding_to_name[tex_binding] = ppl_tex_name;

            vk::DescriptorSetLayoutBinding binding {
                .binding = tex_binding,
                .descriptorType = vk::DescriptorType::eCombinedImageSampler,
                .descriptorCount = descriptor_count,
                .stageFlags = stage,
                .pImmutableSamplers = nullptr
            };
            descriptor_layouts.emplace_back(std::move(binding));
        }
    }

    vk::PipelineLayoutCreateInfo pipeline_layout_info{.setLayoutCount = 0, .pushConstantRangeCount = 0};
    pipeline_layout = vk::raii::PipelineLayout(device, pipeline_layout_info);

    vk::StructureChain<vk::GraphicsPipelineCreateInfo, vk::PipelineRenderingCreateInfo> pipeline_create_info{
        {
            .stageCount = static_cast<uint32_t>(shader_stage_infos.size()),
            .pStages = shader_stage_infos.data(),
            .pVertexInputState = &option.vert_info,
            .pInputAssemblyState = &option.assembly_info,
            .pViewportState = &option.viewport_info,
            .pRasterizationState = &option.raster_info,
            .pMultisampleState = &option.multisample_info,
            .pDepthStencilState = &option.depth_info,
            .pColorBlendState = &option.blend_info,
            .pDynamicState = &option.dynamic_info,
            .layout = pipeline_layout,
            .renderPass = nullptr
        },
        {
            .colorAttachmentCount = 1,
            .pColorAttachmentFormats = &swapchain_surface_format.format
        }
    };

    Pipeline pipeline;
    pipeline.vk_pipeline = vk::raii::Pipeline(device, shader_module_pack.modules);
    pipeline.vk_pipeline_layout = vk::raii::PipelineLayout(device, shader_module_pack.modules);
    pipelines[name] = std::move(pipeline);
    return true;
}

void WrappedContext::record_cmds(uint32_t image_index, const std::function<void(uint32_t)>& emit_func) {
    auto& cmd_buf = command_buffers[image_index];
    cmd_buf.begin({});
    transit_image_layout(
        image_index,
        vk::ImageLayout::eUndefined,
        vk::ImageLayout::eColorAttachmentOptimal,
        {},
        vk::AccessFlags2::eColorAttachmentWrite,
        vk::PipelineStageFlags2::eColorAttachmentOutput,
        vk::PipelineStageFlags2::eColorAttachmentOutput);

    // TODO: clear color needs to be customizable
    vk::ClearValue clear_value = vk::ClearColorValue(0.f, 0.f, 0.f, 1.f);
    vk::RenderingAttachmentInfo attachment_info = {
        .imageView = swapchain_image_views[image_index],
        .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
        .loadOp = vk::AttachmentLoadOp::eClear,
        .storeOp = vk::AttachmentStoreOp::eStore,
        .clearValue = clear_value
    };
    vk::RenderingInfo rendering_info{
        .renderArea = {
            .offset = {0, 0},
            .extent = swapchain_extent
        },
        .layerCount = 1,
        .colorAttachmentCount = 1,
        .pColorAttachments = &attachment_info
    };
    cmd_buf.beginRendering(rendering_info);
    cmd_buf.setViewport(0, {
        .x = 0,
        .y = 0,
        .width = swapchain_extent.width,
        .height = swapchain_extent.height,
        .minDepth = 0,
        .maxDepth = 1
    });
    cmd_buf.setScissor(0, {
        .offset = {0, 0},
        .extent = swapchain_extent
    });
    emit_func(image_index);
    cmd_buf.endRendering();
}

void WrappedContext::draw_frame() {
    auto fence_ret = device.waitForFences(*in_flight_fences[current_frame], vk::True, UINT64_MAX);
    if (fence_ret != vk::Result::eSuccess) {
        throw std::runtime_error("failed to wait for fence");
    }
    device.resetFences(*in_flight_fences[current_frame]);

    auto [result, image_index] = swapchain.acquireNextImageKHR(UINT64_MAX, *present_complete_semaphores[current_frame], nullptr);

    if (result == vk::Result::eErrorOutOfDateKHR || result == vk::Result::eSuboptimalKHR || frame_buffer_resized) {
        frame_buffer_resized = false;
        recreate_swapchain();
        return;
    }

    //command_buffers[current_frame].reset();

    vk::PipelineStageFlags wait_stage_mask(vk::PipelineStageFlagBits::eColorAttachmentOutput);
    vk::SubmitInfo submit_info{
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = present_complete_semaphores[current_frame],
        .pWaitDstStageMask = &wait_stage_mask,
        .commandBufferCount = 1,
        .pCommandBuffers = command_buffers[current_frame],
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = render_finished_semaphores[image_index]
    };
    queue.submit(submit_info, *in_flight_fences[current_frame]);

    vk::PresentInfoKHR present_info{
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = render_finished_semaphores[image_index],
        .swapchainCount = 1,
        .pSwapchains = swapchain,
        .pImageIndices = &image_index
    };
    result = queue.presentKHR(present_info);
    if (result == vk::Result::eErrorOutOfDateKHR || result == vk::Result::eSuboptimalKHR) {
        recreate_swapchain();
    }

    current_frame = (current_frame + 1) % max_frames_in_flight;
}

void WrappedContext::recreate_swapchain() {
    int width = 0, height = 0;
    glfwGetFramebufferSize(window, &width, &height);
    while (width == 0 || height == 0) {
        glfwGetFramebufferSize(window, &width, &height);
        glfwWaitEvents();
    }
    device.waitIdle();

    swapchain_image_views.clear();
    swapchain = nullptr;
    create_swapchain();
    create_imageviews();
}

bool WrappedContext::add_ubo(const std::string& name, const uint32_t binding,
    uint32_t size, uint32_t vecsize) {
    if (ubos.find(name) != ubos.end()) {
        std::cout << "UBO " << name << " already exists" << std::endl;
        return false;
    }

    UBO ubo{.size = size, .vecsize = vecsize, .binding = binding};
    ubo.cpu_buf = std::make_shared<char[]>(size * vecsize);
    ubo.gpu_bufs.resize(swapchain_images.size());
    ubo.memos.resize(swapchain_images.size());
    ubo.descriptors.resize(swapchain_images.size());
}