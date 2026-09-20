#pragma once

#include <cstdint>
#include <string>

#include "UI/ttftext.h"

#include "Core/tokens.h"
#include "Core/textstyle.h"

namespace chronicle
{
    enum class Align : uint8_t { Left, Centre, Right };

    enum class Overflow : uint8_t
    {
        Grow,      // box width = measured text width; `width` ignored
        Wrap,      // box width = `width`; text wraps; height = lines x lineHeight; maxLines truncates with an ellipsis
        Ellipsis   // box width = `width`; one line; text shortened to fit with U+2026
    };

    struct LabelSpec
    {
        std::string style  = "body";
        std::string text;
        std::string colour = "ink";   // token name - never an RGBA; the paint system owns colours
        Align align        = Align::Left;
        Overflow overflow  = Overflow::Grow;
        float width        = 0.0f;    // required for Wrap and Ellipsis
        int maxLines       = 0;       // Wrap only; 0 = unlimited
        int z              = 0;
    };

    struct Label
    {
        pg::EntityRef box;     // PositionComponent + UiAnchor + Prefab - anchor and size this
        pg::EntityRef text;    // the TTFText child
        LabelSpec spec;        // as built, with `text` the text as given (not the fitted one)
        std::string fitted;    // the text actually set on the TTFText

        void setText(pg::EntitySystem*, const TextStyles&, const std::string&);
        void setColour(pg::EntitySystem*, const std::string& token);
        void setAlign(pg::EntitySystem*, Align);
        void setWidth(pg::EntitySystem*, const TextStyles&, float);

        float boxWidth(pg::EntitySystem*) const;
        float boxHeight(pg::EntitySystem*) const;
    };

    Label makeLabel(pg::EntitySystem*, const Tokens&, const TextStyles&, const LabelSpec&);

    // Pure helpers (no ECS), exposed for tests and for the tooltip sizing later.
    std::string fitEllipsis(const pg::TTFTextSystem&, const TextStyle&, const std::string& text, float width);
    std::string clampLines(const pg::TTFTextSystem&, const TextStyle&, const std::string& text, float width, int maxLines);
    int countLines(const pg::TTFTextSystem&, const TextStyle&, const std::string& text, float width);
}
