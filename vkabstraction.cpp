#include <algorithm>
#include <iterator>
#include <stdexcept>

#include <GLFW/glfw3.h>

#include "vkabstraction.h"

namespace vkkk
{

std::vector<const char*> default_validation_layer_func() {
    std::vector<const char*> validation_layers = {
        "VK_LAYER_KHRONOS_validation"
    };
    uint32_t layer_cnt;
    vkEnumerateInstanceLayerProperties(&layer_cnt, nullptr);

    std::vector<VkLayerProperties> available_layers(layer_cnt);
    vkEnumerateInstanceLayerProperties(&layer_cnt, available_layers.data());

    for (const char* layer_name : validation_layers) {
        bool layer_found = false;
        for (const auto& layer_properties : available_layers) {
            if (strcmp(layer_name, layer_properties.layerName) == 0) {
                layer_found = true;
                break;
            }
        }

        if (!layer_found)
            return std::vector<const char*>();
    }

    return validation_layers;
}

void create_instance(
    VkInstance& instance,
    const std::string& app_name,
    const std::string& engine_name,
    std::vector<const char*> extensions,
    std::function<std::vector<const char*>()> validation_layer_functor,
    VkDebugUtilsMessengerEXT* debug_messenger,
    PFN_vkDebugUtilsMessengerCallbackEXT debug_cbk)
{
    if (!validation_layer_functor)
        throw std::runtime_error("validation layers requested, but not available..");

    // Fill in app info
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = app_name.c_str();
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = engine_name.c_str();
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_0;

    // Fill in instance create info
    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;

    if (validation_layer_functor)
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();

    VkDebugUtilsMessengerCreateInfoEXT debug_create_info{};
    auto validation_layers = std::vector<const char*>();
    if (!validation_layer_functor)
        validation_layers = validation_layer_functor();

    if (validation_layers.size() == 0) {
        createInfo.enabledLayerCount = 0;
        createInfo.pNext = nullptr;
    }
    else {
        createInfo.enabledLayerCount = static_cast<uint32_t>(validation_layers.size());
        createInfo.ppEnabledLayerNames = validation_layers.data();

        debug_create_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        debug_create_info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT
            | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT
            | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        debug_create_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
            | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
            | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        debug_create_info.pfnUserCallback = debug_cbk;
        createInfo.pNext = reinterpret_cast<VkDebugUtilsMessengerCreateInfoEXT*>(&debug_create_info);
    }

    if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) {
        throw std::runtime_error("failed to create instance!");
    }

    if (validation_layers.size() > 0) {
        auto debug_func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
        VkResult ret = VK_ERROR_EXTENSION_NOT_PRESENT;
        if (debug_func != nullptr)
            ret = debug_func(instance, &debug_create_info, nullptr, debug_messenger);
        
        if (ret != VK_SUCCESS)
            throw std::runtime_error("failed to set up debug messenger!");
    }
}

std::vector<VkPhysicalDevice> get_physical_devices(const VkInstance& instance) {
    uint32_t device_cnt = 0;
    vkEnumeratePhysicalDevices(instance, &device_cnt, nullptr);

    auto devices = std::vector<VkPhysicalDevice>();
    if (device_cnt > 0) {
        devices.reserve(device_cnt);
        vkEnumeratePhysicalDevices(instance, &device_cnt, devices.data());
    }

    return devices;
}

bool validate_device(const VkPhysicalDevice& device, const VkQueueFlagBits& flags, QueueFamilyIndex* idx) {
    uint32_t queue_family_cnt = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_cnt, nullptr);

    std::vector<VkQueueFamilyProperties> queue_families(queue_family_cnt);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_cnt, queue_families.data());

    int i = 0;
    for (const auto& queue_family : queue_families) {
        if (queue_family.queueFlags & flags) {
            if (idx)
                *idx = i;
            return true;
        }
    }

    return false;
}

void create_logical_device(const VkPhysicalDevice& physical_device, const VkQueueFlagBits& flags, VkDevice& device) {
    QueueFamilyIndex idx;
    if (!validate_device(physical_device, flags, &idx))
        throw std::runtime_error("Queue specified not available");

    // Queue create info
    VkDeviceQueueCreateInfo queue_create_info{};
    queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue_create_info.queueFamilyIndex = idx.value();
    queue_create_info.queueCount = 1;
    float queue_priority = 1.f;
    queue_create_info.pQueuePriorities = &queue_priority;

    // Device feature
    VkPhysicalDeviceFeatures device_features{};

    // Device create info
    VkDeviceCreateInfo device_create_info{};
    device_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device_create_info.pQueueCreateInfos = &queue_create_info;
    device_create_info.queueCreateInfoCount = 1;
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
    //vkGetDeviceQueue(device, idx.value(), 0, );
}

VkWrappedInstance::VkWrappedInstance() {
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
}

VkWrappedInstance::VkWrappedInstance(uint32_t w, uint32_t h, const std::string& appname, const std::string& enginename)
    : width(w)
    , height(h)
    , app_name(appname)
    , engine_name(enginename)
{}

VkWrappedInstance::~VkWrappedInstance() {
    if (enable_validation_layers) {
        auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
        if (func != nullptr)
            func(instance, debug_messenger, nullptr);
    }

    vkDestroyInstance(instance, nullptr);
}

bool VkWrappedInstance::init() {
    return false;
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