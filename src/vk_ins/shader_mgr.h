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

    bool add_module(fs::path path, VkShaderStageFlagBits t);
    std::vector<VkPipelineShaderStageCreateInfo> get_create_info_array() const;
    
private:
    VkDevice                            device;
    std::vector<VkShaderModule>         shader_modules;
    std::vector<VkShaderStageFlagBits>  shader_types;
};

}