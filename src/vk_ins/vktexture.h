#pragma once

#include <filesystem>

#include <vulkan/vulkan.h>

namespace fs = std::filesystem;

namespace vkkk
{

class VkWrappedInstance;

class Texture {
public:
    VkWrappedInstance*      instance;
    std::string             name;
    VkShaderStageFlagBits   stage;
    size_t                  vecsize = 1;
    VkImage                 image;
    VkImageLayout           image_layout;
    VkDeviceMemory          memory;
    VkImageView             view;
    uint32_t                width, height;
    uint32_t                mipmap_lv;
    uint32_t                layout_cnt;
    VkDescriptorImageInfo   descriptor;
    VkSampler               sampler;
    bool                    loaded = false;

    Texture(VkWrappedInstance* ins, const std::string& n,
        VkShaderStageFlagBits t=VK_SHADER_STAGE_VERTEX_BIT);
    ~Texture();

    void update_descriptor();
    void destroy();
    bool load_image(const fs::path&);
};

}