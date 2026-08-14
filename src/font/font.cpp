#include "font/font.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <utility>
#include <vector>

#include <ft2build.h>
#include FT_FREETYPE_H

namespace vkkk::font
{

namespace
{

constexpr uint32_t kMaxTextureExtent = 8192;

struct GlyphBitmap {
    int32_t x = 0;
    int32_t y = 0;
    uint32_t width = 0;
    uint32_t height = 0;
    std::vector<uint8_t> coverage;
};

bool decode_utf8(std::string_view text, std::vector<uint32_t>& codepoints) {
    for (size_t index = 0; index < text.size();) {
        const uint8_t first = static_cast<uint8_t>(text[index++]);
        if (first < 0x80) {
            codepoints.push_back(first);
            continue;
        }

        uint32_t codepoint = 0;
        uint32_t continuation_count = 0;
        uint32_t minimum_value = 0;
        if ((first & 0xe0) == 0xc0) {
            codepoint = first & 0x1f;
            continuation_count = 1;
            minimum_value = 0x80;
        }
        else if ((first & 0xf0) == 0xe0) {
            codepoint = first & 0x0f;
            continuation_count = 2;
            minimum_value = 0x800;
        }
        else if ((first & 0xf8) == 0xf0) {
            codepoint = first & 0x07;
            continuation_count = 3;
            minimum_value = 0x10000;
        }
        else {
            return false;
        }

        if (index + continuation_count > text.size()) {
            return false;
        }
        for (uint32_t continuation = 0; continuation < continuation_count; ++continuation) {
            const uint8_t byte = static_cast<uint8_t>(text[index++]);
            if ((byte & 0xc0) != 0x80) {
                return false;
            }
            codepoint = (codepoint << 6) | (byte & 0x3f);
        }
        if (codepoint < minimum_value || codepoint > 0x10ffff
            || (codepoint >= 0xd800 && codepoint <= 0xdfff))
        {
            return false;
        }
        codepoints.push_back(codepoint);
    }
    return true;
}

bool valid_options(const TextRenderOptions& options) {
    return options.pixel_height > 0 && options.pixel_height <= kMaxTextureExtent
        && options.padding <= kMaxTextureExtent
        && std::isfinite(options.line_spacing) && options.line_spacing > 0.0f
        && std::isfinite(options.color.r) && std::isfinite(options.color.g)
        && std::isfinite(options.color.b) && std::isfinite(options.color.a)
        && options.color.r >= 0.0f && options.color.r <= 1.0f
        && options.color.g >= 0.0f && options.color.g <= 1.0f
        && options.color.b >= 0.0f && options.color.b <= 1.0f
        && options.color.a >= 0.0f && options.color.a <= 1.0f;
}

uint8_t to_unorm(float value) {
    return static_cast<uint8_t>(std::round(std::clamp(value, 0.0f, 1.0f) * 255.0f));
}

} // namespace

TextRenderer::TextRenderer(std::filesystem::path path)
    : font_path(std::move(path))
{
}

TextTexture TextRenderer::render(Context& context, std::string_view text,
    const TextRenderOptions& options) const
{
    if (text.empty() || !valid_options(options) || !std::filesystem::is_regular_file(font_path)) {
        return {};
    }

    std::vector<uint32_t> codepoints;
    if (!decode_utf8(text, codepoints)) {
        return {};
    }

    FT_Library library = nullptr;
    FT_Face face = nullptr;
    if (FT_Init_FreeType(&library) != 0
        || FT_New_Face(library, font_path.string().c_str(), 0, &face) != 0
        || FT_Set_Pixel_Sizes(face, 0, options.pixel_height) != 0)
    {
        if (face != nullptr) {
            FT_Done_Face(face);
        }
        if (library != nullptr) {
            FT_Done_FreeType(library);
        }
        return {};
    }

    const int32_t ascender = static_cast<int32_t>(face->size->metrics.ascender >> 6);
    const int32_t base_line_height = std::max<int32_t>(1, face->size->metrics.height >> 6);
    const int32_t line_height = std::max<int32_t>(1,
        static_cast<int32_t>(std::round(base_line_height * options.line_spacing)));
    int32_t baseline = ascender;
    int32_t pen_x = 0;
    int32_t min_x = 0;
    int32_t min_y = 0;
    int32_t max_x = 0;
    int32_t max_y = line_height;
    FT_UInt previous_glyph = 0;
    std::vector<GlyphBitmap> glyphs;

    for (const uint32_t codepoint : codepoints) {
        if (codepoint == '\n') {
            max_x = std::max(max_x, static_cast<int32_t>((pen_x + 63) >> 6));
            baseline += line_height;
            max_y = std::max(max_y, baseline - ascender + line_height);
            pen_x = 0;
            previous_glyph = 0;
            continue;
        }

        const FT_UInt glyph_index = FT_Get_Char_Index(face, codepoint);
        if (glyph_index == 0) {
            FT_Done_Face(face);
            FT_Done_FreeType(library);
            return {};
        }
        if (previous_glyph != 0 && FT_HAS_KERNING(face)) {
            FT_Vector kerning{};
            if (FT_Get_Kerning(face, previous_glyph, glyph_index, FT_KERNING_DEFAULT, &kerning) == 0) {
                pen_x += static_cast<int32_t>(kerning.x);
            }
        }
        if (FT_Load_Glyph(face, glyph_index, FT_LOAD_DEFAULT) != 0
            || FT_Render_Glyph(face->glyph, FT_RENDER_MODE_NORMAL) != 0)
        {
            FT_Done_Face(face);
            FT_Done_FreeType(library);
            return {};
        }

        const FT_GlyphSlot slot = face->glyph;
        const FT_Bitmap& bitmap = slot->bitmap;
        GlyphBitmap glyph{};
        glyph.x = static_cast<int32_t>(pen_x >> 6) + slot->bitmap_left;
        glyph.y = baseline - slot->bitmap_top;
        glyph.width = bitmap.width;
        glyph.height = bitmap.rows;
        if (glyph.width != 0 && glyph.height != 0) {
            glyph.coverage.resize(static_cast<size_t>(glyph.width) * glyph.height);
            const int32_t pitch = bitmap.pitch;
            for (uint32_t row = 0; row < glyph.height; ++row) {
                const uint8_t* source = pitch >= 0
                    ? bitmap.buffer + row * pitch
                    : bitmap.buffer + (glyph.height - 1 - row) * -pitch;
                std::copy_n(source, glyph.width, glyph.coverage.data() + row * glyph.width);
            }
            min_x = std::min(min_x, glyph.x);
            min_y = std::min(min_y, glyph.y);
            max_x = std::max(max_x, glyph.x + static_cast<int32_t>(glyph.width));
            max_y = std::max(max_y, glyph.y + static_cast<int32_t>(glyph.height));
            glyphs.push_back(std::move(glyph));
        }

        pen_x += static_cast<int32_t>(slot->advance.x);
        max_x = std::max(max_x, static_cast<int32_t>((pen_x + 63) >> 6));
        previous_glyph = glyph_index;
    }

    FT_Done_Face(face);
    FT_Done_FreeType(library);

    const uint64_t width64 = static_cast<uint64_t>(max_x - min_x) + options.padding * 2ull;
    const uint64_t height64 = static_cast<uint64_t>(max_y - min_y) + options.padding * 2ull;
    if (width64 == 0 || height64 == 0 || width64 > kMaxTextureExtent
        || height64 > kMaxTextureExtent
        || width64 * height64 > std::numeric_limits<uint32_t>::max() / 4)
    {
        return {};
    }

    const uint32_t width = static_cast<uint32_t>(width64);
    const uint32_t height = static_cast<uint32_t>(height64);
    std::vector<uint8_t> pixels(static_cast<size_t>(width) * height * 4, 0);
    const uint8_t red = to_unorm(options.color.r);
    const uint8_t green = to_unorm(options.color.g);
    const uint8_t blue = to_unorm(options.color.b);
    const uint8_t alpha = to_unorm(options.color.a);
    for (const auto& glyph : glyphs) {
        const uint32_t dst_x = static_cast<uint32_t>(glyph.x - min_x) + options.padding;
        const uint32_t dst_y = static_cast<uint32_t>(glyph.y - min_y) + options.padding;
        for (uint32_t row = 0; row < glyph.height; ++row) {
            for (uint32_t col = 0; col < glyph.width; ++col) {
                const uint8_t coverage = glyph.coverage[row * glyph.width + col];
                const uint8_t source_alpha = static_cast<uint8_t>(
                    (static_cast<uint32_t>(coverage) * alpha + 127) / 255);
                // FreeType rows begin at the glyph top; Vulkan UV origin for this billboard
                // maps the first uploaded image row to the quad bottom.
                const uint32_t texture_row = height - 1 - (dst_y + row);
                uint8_t* pixel = pixels.data() + (texture_row * width + dst_x + col) * 4;
                pixel[0] = red;
                pixel[1] = green;
                pixel[2] = blue;
                pixel[3] = std::max(pixel[3], source_alpha);
            }
        }
    }

    const uint32_t target_index = context.create_rgba8_render_target(
        pixels.data(), width, height, pixels.size());
    if (target_index == kInvalidTargetIndex) {
        return {};
    }
    return TextTexture{target_index, vk::Extent2D{width, height}};
}

} // namespace vkkk::font
