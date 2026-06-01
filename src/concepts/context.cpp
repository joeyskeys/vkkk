#include <GLFW/glfw3.h>

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

WrappedContext::init(GLFWwindow* window) {
    VkSurfaceKHR surface = nullptr;
    if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS) {
        throw std::runtime_error("failed to create window surface!");
    }
    surface = vk::raii::SurfaceKHR(instance, surface);
}

}