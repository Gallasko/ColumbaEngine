#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

struct NSVGimage;

namespace pg
{
    /// A parsed SVG document. Owns the nanosvg image; move-only.
    class SvgDocument
    {
    public:
        SvgDocument() = default;
        SvgDocument(SvgDocument&&) noexcept = default;
        SvgDocument& operator=(SvgDocument&&) noexcept = default;

        float width() const;   // document units after parse, in px at 96 dpi
        float height() const;
        bool valid() const { return image != nullptr; }

        const NSVGimage* raw() const { return image.get(); }

    private:
        friend class SvgLoader;
        struct Deleter { void operator()(NSVGimage* img) const; };
        std::unique_ptr<NSVGimage, Deleter> image;
    };

    /// 8-bit RGBA, non-premultiplied, row-major, top-left origin.
    struct SvgImage
    {
        int width = 0;
        int height = 0;
        std::vector<unsigned char> rgba;
    };

    /// Alpha only, one byte per pixel. Colour-independent shape for tinted icons.
    struct SvgCoverage
    {
        int width = 0;
        int height = 0;
        std::vector<unsigned char> alpha;
    };

    class SvgLoader
    {
    public:
        /// Reads through UniversalFileAccessor. Empty optional on missing file or parse failure (logged).
        static std::optional<SvgDocument> parseFile(const std::string& path);
        /// nanosvg mutates its input, so this takes a copy on purpose.
        static std::optional<SvgDocument> parseString(std::string svgText);

        /// Scales uniformly so the longer document side equals targetPx; the shorter side is centred.
        static SvgImage rasterize(const SvgDocument& doc, int targetPx);
        static SvgCoverage rasterizeCoverage(const SvgDocument& doc, int targetPx);
    };
}
