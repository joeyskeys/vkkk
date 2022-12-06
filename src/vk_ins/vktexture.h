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
    VkImage                 image;
    VkImageLayout           image_layout;
    VkDeviceMemory          memory;
    VkImageView             view;
    uint32_t                width, height;
    uint32_t                mipmap_lv;
    uint32_t                layout_cnt;
    VkDescriptorImageInfo   descriptor;
    VkSampler               sampler;

    Texture(VkWrappedInstance* ins)
        : instance(ins)
    {}

    void update_descriptor();
    void destroy();
    bool load_image(const fs::path&);
};

}