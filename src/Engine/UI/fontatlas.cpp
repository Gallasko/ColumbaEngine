#include "stdafx.h"

#include "fontatlas.h"

namespace pg
{
    namespace
    {
        constexpr const char * const DOM = "FontAtlas";

        // Gap between packed glyphs, matching the historic TTF atlas packer.
        constexpr int PADDING = 2;
    }

    FontAtlas::~FontAtlas()
    {
        if (face)
            FT_Done_Face(face);
    }

    FontAtlas::FontAtlas(FontAtlas&& other) noexcept :
        pixelSize(other.pixelSize),
        fontMetrics(other.fontMetrics),
        atlasBuffer(std::move(other.atlasBuffer)),
        glyphs(std::move(other.glyphs)),
        notdefGlyph(other.notdefGlyph),
        kernCache(std::move(other.kernCache)),
        face(other.face)
    {
        other.face = nullptr;
    }

    FontAtlas& FontAtlas::operator=(FontAtlas&& other) noexcept
    {
        if (this != &other)
        {
            if (face)
                FT_Done_Face(face);

            pixelSize = other.pixelSize;
            fontMetrics = other.fontMetrics;
            atlasBuffer = std::move(other.atlasBuffer);
            glyphs = std::move(other.glyphs);
            notdefGlyph = other.notdefGlyph;
            kernCache = std::move(other.kernCache);
            face = other.face;

            other.face = nullptr;
        }

        return *this;
    }

    bool FontAtlas::build(FT_Library ft, const std::string& fontPath, int sizePx, const std::vector<uint32_t>& codepoints)
    {
        if (face)
        {
            FT_Done_Face(face);
            face = nullptr;
        }

        if (FT_New_Face(ft, fontPath.c_str(), 0, &face))
        {
            LOG_ERROR(DOM, "Could not open font: " << fontPath);
            face = nullptr;
            return false;
        }

        FT_Set_Pixel_Sizes(face, 0, sizePx);

        pixelSize = sizePx;

        fontMetrics.ascender = face->size->metrics.ascender / 64.0f;
        fontMetrics.descender = -face->size->metrics.descender / 64.0f;
        fontMetrics.lineHeight = face->size->metrics.height / 64.0f;
        fontMetrics.hasKerning = FT_HAS_KERNING(face);

        atlasBuffer.assign(static_cast<size_t>(Width) * Height, 0);
        glyphs.clear();
        kernCache.clear();

        int currentX = 0;
        int currentY = 0;
        int rowHeight = 0;

        // Rasterises the currently loaded glyph slot into the atlas. Returns false on overflow.
        auto packLoadedGlyph = [&](GlyphInfo& out) -> bool
        {
            FT_GlyphSlot slot = face->glyph;
            const int w = static_cast<int>(slot->bitmap.width);
            const int h = static_cast<int>(slot->bitmap.rows);

            if (currentX + w + PADDING > Width)
            {
                currentX = 0;
                currentY += rowHeight + PADDING;
                rowHeight = 0;
            }

            if (currentY + h > Height)
            {
                LOG_ERROR(DOM, "Font atlas overflow at " << sizePx << " px for: " << fontPath);
                return false;
            }

            for (int y = 0; y < h; ++y)
            {
                for (int x = 0; x < w; ++x)
                {
                    atlasBuffer[(currentY + y) * Width + (currentX + x)] = slot->bitmap.buffer[y * w + x];
                }
            }

            out.size = glm::ivec2(w, h);
            out.bearing = glm::ivec2(slot->bitmap_left, slot->bitmap_top);
            out.advance = slot->advance.x / 64.0f;
            out.uvTopLeft = glm::vec2(static_cast<float>(currentX) / Width, static_cast<float>(currentY) / Height);
            out.uvBottomRight = glm::vec2(static_cast<float>(currentX + w) / Width, static_cast<float>(currentY + h) / Height);

            currentX += w + PADDING;
            if (h > rowHeight)
                rowHeight = h;

            return true;
        };

        // .notdef (glyph index 0) is the fallback returned by glyphOrNotdef.
        if (FT_Load_Glyph(face, 0, FT_LOAD_RENDER) == 0)
        {
            if (not packLoadedGlyph(notdefGlyph))
                return false;
        }

        for (uint32_t cp : codepoints)
        {
            FT_UInt glyphIndex = FT_Get_Char_Index(face, cp);
            if (glyphIndex == 0)
                continue;   // this face has no glyph for the code point

            if (FT_Load_Glyph(face, glyphIndex, FT_LOAD_RENDER))
            {
                LOG_ERROR(DOM, "Failed to load glyph for code point: " << cp);
                continue;
            }

            GlyphInfo info;
            if (not packLoadedGlyph(info))
                return false;

            glyphs[cp] = info;
        }

        return true;
    }

    const GlyphInfo* FontAtlas::glyph(uint32_t codepoint) const
    {
        auto it = glyphs.find(codepoint);

        if (it == glyphs.end())
            return nullptr;

        return &it->second;
    }

    const GlyphInfo& FontAtlas::glyphOrNotdef(uint32_t codepoint) const
    {
        auto it = glyphs.find(codepoint);

        if (it != glyphs.end())
            return it->second;

        return notdefGlyph;
    }

    float FontAtlas::kerning(uint32_t left, uint32_t right) const
    {
        if (not fontMetrics.hasKerning or not face)
            return 0.0f;

        const uint64_t key = (static_cast<uint64_t>(left) << 32) | static_cast<uint64_t>(right);

        auto it = kernCache.find(key);
        if (it != kernCache.end())
            return it->second;

        const FT_UInt leftIndex = FT_Get_Char_Index(face, left);
        const FT_UInt rightIndex = FT_Get_Char_Index(face, right);

        FT_Vector delta;
        float result = 0.0f;
        if (FT_Get_Kerning(face, leftIndex, rightIndex, FT_KERNING_DEFAULT, &delta) == 0)
            result = delta.x / 64.0f;

        kernCache[key] = result;

        return result;
    }

    const std::vector<uint32_t>& defaultCharset()
    {
        static const std::vector<uint32_t> charset = []
        {
            std::vector<uint32_t> codepoints;

            auto addRange = [&codepoints](uint32_t lo, uint32_t hi)
            {
                for (uint32_t c = lo; c <= hi; ++c)
                    codepoints.push_back(c);
            };

            addRange(32, 126);      // ASCII
            addRange(160, 255);     // Latin-1 Supplement (includes U+00D7 x and U+00B7 middle dot)
            addRange(8211, 8212);   // en dash, em dash
            addRange(8216, 8217);   // left/right single quote
            addRange(8220, 8221);   // left/right double quote
            codepoints.push_back(8226);   // bullet
            codepoints.push_back(8230);   // horizontal ellipsis
            addRange(8592, 8595);   // left, up, right, down arrows
            codepoints.push_back(8722);   // minus sign
            codepoints.push_back(65533);  // replacement character

            return codepoints;
        }();

        return charset;
    }
}
