#include <algorithm>

#include "vkabstraction.h"

void create_vk_instance(const std::string& app_name, const std::string& engine_name, std::function<vector<char*>(VkDebugUtilsMessagerCreateInfoEXT&)> validation_layer_functor) {
    if (!validation_layer_functor)
        throw std::runtime_error("validation layers requested, but not available..");

    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = app_name.c_str();
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = engine_name.c_str();
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_0;

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;

    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions;
    glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
    std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

    VkDebugUtilsMessengerCreateInfoEXT debug_create_info{};
    auto extra_extensions = validation_layer_functor(debug_create_info);
    if (extra_extensions.size() == 0) {
        createInfo.enabledLayerCount = 0;
        createInfo.pNext = nullptr;
    }
    else {
        createInfo.eabledLayerCount = extra_extensions.size();
        createInfo.ppEnabledLayerNames = extra_extensions.data();
    }
    std::move(extra_extensions.begin(), extra_extensions.end(), std::back_inserter(extensions));

    //createInfo.enabledExtensionCount = glfwExtensionCount;
    //createInfo.ppEnabledExtensionNames = glfwExtensions;
    createInfo.enabledExtensionCount = extensions.size();
    createInfo.ppEnabledExtensionNames = extensions.data();

    if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) {
        throw std::runtime_error("failed to create instance!");
    }
}

void init_vulkan() {
    create_vk_instance();
    // setup_debug_messenger();
}