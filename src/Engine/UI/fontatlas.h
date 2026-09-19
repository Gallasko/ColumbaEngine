#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include <glm/glm.hpp>

#include <ft2build.h>
#include FT_FREETYPE_H

namespace pg
{
    struct GlyphInfo
    {
        glm::ivec2 size{0, 0};
        glm::ivec2 bearing{0, 0};
        float advance = 0.0f;          // pixels, fractional (26.6 / 64)
        glm::vec2 uvTopLeft{0.0f, 0.0f};
        glm::vec2 uvBottomRight{0.0f, 0.0f};
    };

    struct FontMetrics
    {
        float ascender = 0.0f;         // pixels above baseline
        float descender = 0.0f;        // pixels below baseline, positive
        float lineHeight = 0.0f;       // face->size->metrics.height / 64
        bool hasKerning = false;
    };

    /// A rasterised font at one pixel size: glyph table, metrics and the single-channel atlas bytes. No GL.
    class FontAtlas
    {
    public:
        static constexpr int Width = 1024;
        static constexpr int Height = 1024;

        FontAtlas() = default;
        ~FontAtlas();

        FontAtlas(FontAtlas&& other) noexcept;
        FontAtlas& operator=(FontAtlas&& other) noexcept;

        /// Returns false (and logs) if the face cannot be opened or the atlas overflows.
        bool build(FT_Library ft, const std::string& fontPath, int sizePx, const std::vector<uint32_t>& codepoints);

        const GlyphInfo* glyph(uint32_t codepoint) const;   // nullptr if not in the atlas
        const GlyphInfo& glyphOrNotdef(uint32_t codepoint) const;
        float kerning(uint32_t left, uint32_t right) const;  // pixels; 0 when the face has no kern table
        const FontMetrics& metrics() const { return fontMetrics; }
        const std::vector<unsigned char>& buffer() const { return atlasBuffer; }
        int sizePx() const { return pixelSize; }

    private:
        FontAtlas(const FontAtlas&) = delete;
        FontAtlas& operator=(const FontAtlas&) = delete;

        int pixelSize = 0;
        FontMetrics fontMetrics;
        std::vector<unsigned char> atlasBuffer;
        std::unordered_map<uint32_t, GlyphInfo> glyphs;
        GlyphInfo notdefGlyph;                                 // fallback for missing code points
        mutable std::unordered_map<uint64_t, float> kernCache; // (left << 32 | right) -> px
        FT_Face face = nullptr;                                // kept open for kerning; freed in the destructor
    };

    /// The code points every registered font rasterises: ASCII, Latin-1 Supplement, the punctuation and
    /// arrows the UI uses. One list, shared by all fonts.
    const std::vector<uint32_t>& defaultCharset();
}
