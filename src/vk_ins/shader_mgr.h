#pragma onces

#include <filesystem>
#include <vector>

#include <vulkan/vulkan.h>

namespace fs = std::filesystem;

namespace vkkk
{

class ShaderModules {
public:
    ShaderModules(const VkDevice d) : device(d) {}
    virtual ~ShaderModules();

    bool add_module(fs::path path, VkDevice &device, VkShaderStageFlagBits t);
    auto get_create_info_array() const;
    
private:
    VkDevice                            device;
    std::vector<VkShaderModule>         shader_modules;
    std::vector<VkShaderStageFlagBits>  shader_types;
};

}