#include <algorithm>
#include <cassert>
#include <iterator>
#include <set>
#include <stdexcept>

#include "vkabstraction.h"

namespace vkkk
{

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

    auto extensions = get_default_extensions();
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
    QueueFamilyIndex idx;
    if (!validate_current_device(&idx))
        throw std::runtime_error("Queue specified not available");

    // Queue create info
    std::vector<VkDeviceQueueCreateInfo> queue_create_infos;
    std::set<uint32_t> queue_families = {
        idx.graphic_family.value(),
        idx.present_family.value()
    };
    
    float queue_priority = 1.f;
    for (auto& queue_family : queue_families) {
        VkDeviceQueueCreateInfo queue_create_info{};
        queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue_create_info.queueFamilyIndex = queue_family;
        queue_create_info.queueCount = 1;
        queue_create_info.pQueuePriorities = &queue_priority;
        queue_create_infos.push_back(queue_create_info);
    }

    // Device feature
    VkPhysicalDeviceFeatures device_features{};

    // Device create info
    VkDeviceCreateInfo device_create_info{};
    device_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device_create_info.queueCreateInfoCount = static_cast<uint32_t>(queue_create_infos.size());
    device_create_info.pQueueCreateInfos = queue_create_infos.data();
    device_create_info.pEnabledFeatures = &device_features;

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
    vkGetDeviceQueue(device, idx.graphic_family.value(), 0, &graphic_queue);
    vkGetDeviceQueue(device, idx.present_family.value(), 0, &present_queue);
}

std::vector<const char*> VkWrappedInstance::get_default_extensions() {
    uint32_t glfw_extension_cnt = 0;
    const char** glfw_extensions;
    glfw_extensions = glfwGetRequiredInstanceExtensions(&glfw_extension_cnt);
    std::vector<const char*> extensions(glfw_extensions, glfw_extensions + glfw_extension_cnt);
    if (enable_validation_layers)
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

    return extensions;
}

}