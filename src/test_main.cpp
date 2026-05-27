#include <iostream>
#include <vulkan/vulkan.hpp>

constexpr const char* app_name = "test";
constexpr const char* engine_name = "vkkk";

int main() {
    try {
        constexpr vk::ApplicationInfo app_info(
            app_name,
            1,
            engine_name,
            1,
            vk::ApiVersion14);
        vk::InstanceCreateInfo instance_create_info({}, &app_info);
        vk::Instance ins = vk::createInstance(instance_create_info);
        ins.destroy();
    }
    catch (...) {
        std::cout << "error" << std::endl;
        return -1;
    }

    return 0;
}