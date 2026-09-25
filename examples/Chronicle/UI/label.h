#pragma once

#include <string>

#include "UI/ttftext.h"

#include "Core/tokens.h"
#include "Core/textstyle.h"

namespace chronicle
{
    // The engine owns overflow and alignment (TTFText.overflow / align / maxLines);
    // these aliases keep the kit's spelling.
    using Align = pg::TextAlign;
    using Overflow = pg::TextOverflow;

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

    // A single entity: PositionComponent + UiAnchor + ViewportComponent + TTFText +
    // PaintComponent. The engine lays out, wraps, elides and aligns; the label only
    // owns the colour token and the spec.
    struct Label
    {
        pg::EntityRef entity;
        LabelSpec spec;        // as built; `text` is the text as given (elision happens at layout)

        // Style values cached at build time so the mutators can size the entity
        // synchronously (callers read width/height right after) without a TextStyles.
        std::string fontAlias;
        int lineHeightPx = 0;
        float lineSpacingPx = 0.0f;
        float letterSpacingPx = 0.0f;

        void setText(pg::EntitySystem*, const std::string&);          // engine re-fits
        void setColour(pg::EntitySystem*, const std::string& token);
        void setAlign(pg::EntitySystem*, Align);
        void setWidth(pg::EntitySystem*, float);                      // engine re-wraps / re-elides
    };

    Label makeLabel(pg::EntitySystem*, const Tokens&, const TextStyles&, const LabelSpec&);
}
