#pragma once

#include <cstdint>
#include <filesystem>
#include <string_view>

#include <glm/vec4.hpp>

#include "vk_ins/context.hpp"

namespace vkkk::font
{

struct TextRenderOptions {
    uint32_t pixel_height = 32;
    glm::vec4 color{1.0f};
    uint32_t padding = 4;
    float line_spacing = 1.0f;
};

struct TextTexture {
    uint32_t target_index = kInvalidTargetIndex;
    vk::Extent2D extent{};

    bool valid() const { return target_index != kInvalidTargetIndex; }
};

class TextRenderer {
public:
    explicit TextRenderer(std::filesystem::path font_path);

    TextTexture render(Context& context, std::string_view text,
        const TextRenderOptions& options = {}) const;

private:
    std::filesystem::path font_path;
};

} // namespace vkkk::font
