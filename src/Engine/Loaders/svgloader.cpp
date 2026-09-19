#include "stdafx.h"

#include "svgloader.h"

// nanosvg is vendored C code: exactly this translation unit defines its
// implementation, and its float comparisons trip -Wfloat-equal/-pedantic,
// so the includes are isolated behind a diagnostic guard.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wfloat-equal"
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wpedantic"
#define NANOSVG_IMPLEMENTATION
#define NANOSVGRAST_IMPLEMENTATION
#include "Loaders/nanosvg/nanosvg.h"
#include "Loaders/nanosvg/nanosvgrast.h"
#pragma GCC diagnostic pop

namespace pg
{
    namespace
    {
        constexpr const char * const DOM = "SvgLoader";

        constexpr float SVG_DPI = 96.0f;
    }

    void SvgDocument::Deleter::operator()(NSVGimage* img) const
    {
        if (img)
            nsvgDelete(img);
    }

    float SvgDocument::width() const
    {
        return image ? image->width : 0.0f;
    }

    float SvgDocument::height() const
    {
        return image ? image->height : 0.0f;
    }

    std::optional<SvgDocument> SvgLoader::parseFile(const std::string& path)
    {
        TextFile file = UniversalFileAccessor::openTextFile(path);

        if (file.data.empty())
        {
            LOG_ERROR(DOM, "Failed to open SVG file: " << path);
            return std::nullopt;
        }

        return parseString(std::move(file.data));
    }

    std::optional<SvgDocument> SvgLoader::parseString(std::string svgText)
    {
        // nsvgParse mutates the buffer and needs it null-terminated; svgText is
        // our own copy and std::string storage is guaranteed null-terminated.
        NSVGimage* image = nsvgParse(svgText.data(), "px", SVG_DPI);

        if (not image)
        {
            LOG_ERROR(DOM, "Failed to parse SVG document");
            return std::nullopt;
        }

        // nanosvg returns a non-null image even for non-SVG input, but with no
        // shapes and zero dimensions; treat that as a parse failure.
        const bool empty = image->shapes == nullptr or image->width <= 0.0f or image->height <= 0.0f;
        if (empty)
        {
            LOG_ERROR(DOM, "SVG document has no drawable shapes");
            nsvgDelete(image);
            return std::nullopt;
        }

        SvgDocument doc;
        doc.image.reset(image);

        return doc;
    }

    SvgImage SvgLoader::rasterize(const SvgDocument& doc, int targetPx)
    {
        SvgImage result;

        if (not doc.valid() or targetPx <= 0)
        {
            LOG_ERROR(DOM, "Cannot rasterise an invalid SVG document");
            return result;
        }

        // SvgLoader is a friend of SvgDocument; get() yields a mutable pointer
        // even from a const document, which nsvgRasterize requires.
        NSVGimage* image = doc.image.get();

        const float longestSide = std::max(image->width, image->height);
        const float scale = static_cast<float>(targetPx) / longestSide;

        // Centre the shorter side inside the square target.
        const float tx = (static_cast<float>(targetPx) - image->width * scale) * 0.5f;
        const float ty = (static_cast<float>(targetPx) - image->height * scale) * 0.5f;

        result.width = targetPx;
        result.height = targetPx;
        result.rgba.resize(static_cast<size_t>(targetPx) * targetPx * 4, 0);

        NSVGrasterizer* rast = nsvgCreateRasterizer();
        if (not rast)
        {
            LOG_ERROR(DOM, "Failed to create SVG rasteriser");
            return SvgImage{};
        }

        nsvgRasterize(rast, image, tx, ty, scale, result.rgba.data(), targetPx, targetPx, targetPx * 4);

        nsvgDeleteRasterizer(rast);

        return result;
    }

    SvgCoverage SvgLoader::rasterizeCoverage(const SvgDocument& doc, int targetPx)
    {
        SvgImage rgbaImage = rasterize(doc, targetPx);

        SvgCoverage coverage;
        coverage.width = rgbaImage.width;
        coverage.height = rgbaImage.height;
        coverage.alpha.resize(static_cast<size_t>(coverage.width) * coverage.height, 0);

        // Coverage is the alpha channel of the RGBA raster; colour is ignored.
        for (size_t i = 0; i < coverage.alpha.size(); ++i)
        {
            coverage.alpha[i] = rgbaImage.rgba[i * 4 + 3];
        }

        return coverage;
    }
}
