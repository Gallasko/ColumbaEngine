#pragma once

#include <string>
#include <vector>

#include "UI/ttftext.h"

#include "tokens.h"

namespace chronicle
{
    struct TextStyle
    {
        std::string name;          // "body", "figure-xl", ...
        std::string family;        // "display" | "text"
        int sizePx = 16;           // exact rasterisation size
        int lineHeightPx = 24;     // the tokens' lineHeight
        int weight = 400;
        bool italic = false;
        float letterSpacingPx = 0; // tokens letterSpacing (em) x sizePx, rounded to 0.25 px
        std::string fontAlias;     // "chr-" + name - the TTFTextSystem font name
        std::string fontFile;      // from fontFiles(), relative to the font root
        float lineSpacingPx = 0;   // filled by registerAll: lineHeightPx - atlas line height
    };

    class TextStyles
    {
    public:
        static TextStyles fromTokens(const Tokens& tokens);

        bool ok() const;
        const std::vector<std::string>& errors() const;

        const TextStyle& get(const std::string& name) const;   // unknown -> logs once, returns "body"
        bool has(const std::string& name) const;
        const std::vector<TextStyle>& all() const;             // file order

        // Registers every style as its own atlas: ttf->registerFont(root + "/" + fontFile, fontAlias, sizePx).
        // Then fills lineSpacingPx from ttf->measureText(alias, "Hg").lineHeight.
        // Returns the number registered; a style whose file is missing is logged and skipped.
        size_t registerAll(pg::TTFTextSystem* ttf, const std::string& fontRoot);

        // A TTFText configured for a style: fontPath = alias, scale = 1, letterSpacing,
        // spacing = lineSpacingPx, colors = colour.
        pg::CompList<pg::PositionComponent, pg::UiAnchor, pg::ViewportComponent, pg::TTFText>
        makeText(pg::EntitySystem* ecs, const std::string& style, const std::string& text,
                 const pg::constant::Vector4D& colour, float x = 0, float y = 0, float z = 0) const;

    private:
        std::vector<TextStyle> styles;
        std::vector<std::string> errorList;
        mutable std::set<std::string> reportedUnknown;
    };
}
