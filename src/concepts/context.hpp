#pragma once

#include <cstdint>
#include <vector>

#include <vulkan/vulkan_raii.hpp>

namespace vkkk
{

namespace
{

const std::vector<const char*> default_validation_layers = {
    "VK_LAYER_KHRONOS_validation"
};

constexpr char* default_app_name = "vkkk";
constexpr char* default_engine_name = "vulkan";
constexpr uint32_t default_app_version = VK_MAKE_VERSION(1, 0, 0);
constexpr uint32_t default_api_version = vk::ApiVersion14;

}

// a class manages vulkan instance, physical device, logical device, surface
// these parts are not frequently changed or used.
class WrappedContext {
  public:
    WrappedContext(
        const char* app_name = default_app_name,
        uint32_t app_version = default_app_version,
        const char* engine_name = default_engine_name,
        uint32_t api_version = default_api_version,
        bool enable_validation_layers = true,
        const std::vector<const char*>& extra_validation_layers = {},
        const std::vector<const char*>& extra_extensions = {},
        bool enable_debug_messenger = true
      ) noexcept;

  private:
    void setup_debug_messenger();

  private:
    vk::raii::Context context;
    vk::raii::Instance instance = nullptr;
    vk::raii::DebugUtilMessenger debug_messenger = nullptr;
}

}