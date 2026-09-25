#include "label.h"

#include <cassert>

#include "ECS/entitysystem.h"
#include "2D/position.h"
#include "logger.h"

#include "paint.h"

using namespace pg;

namespace chronicle
{
    namespace
    {
        constexpr const char * const DOM = "Chronicle.Label";

        TextLayoutParams layoutParams(const Label& label)
        {
            TextLayoutParams params;
            params.maxWidth = label.spec.overflow != Overflow::Grow ? label.spec.width : 0.0f;
            params.spacing = label.lineSpacingPx;
            params.letterSpacing = label.letterSpacingPx;
            params.overflow = label.spec.overflow;
            params.align = label.spec.align;
            params.maxLines = label.spec.maxLines;
            return params;
        }

        // Sizes the entity now, from the same layout pass the renderer will run, so a
        // caller can read width/height synchronously; the engine's build then lands on
        // the same numbers. Height is the token line height times the line count - the
        // kit's stacking rhythm, and (unlike the atlas height) still correct for a
        // style whose font is absent, where the line count falls back to 1.
        void applyMeasure(EntitySystem* ecs, Label& label)
        {
            auto* ttf = ecs->getSystem<TTFTextSystem>();
            const TextMetrics metrics = ttf->measureText(label.fontAlias, label.spec.text, layoutParams(label));

            auto pos = label.entity->get<PositionComponent>();
            if (label.spec.overflow == Overflow::Grow)
                pos->setWidth(metrics.width);
            pos->setHeight(static_cast<float>(metrics.lineCount) * static_cast<float>(label.lineHeightPx));
        }
    }

    Label makeLabel(EntitySystem* ecs, const Tokens& tokens, const TextStyles& styles, const LabelSpec& specIn)
    {
        LabelSpec spec = specIn;

        // Wrap and Ellipsis need a width; without one, fall back to Grow.
        if ((spec.overflow == Overflow::Wrap or spec.overflow == Overflow::Ellipsis) and spec.width <= 0.0f)
        {
            assert(false and "Label Wrap/Ellipsis needs a width");
            LOG_ERROR(DOM, "Label '" << spec.style << "' overflow needs a width; falling back to Grow");
            spec.overflow = Overflow::Grow;
        }

        auto text = styles.makeText(ecs, spec.style, spec.text, tokens.colour(spec.colour), 0.0f, 0.0f, static_cast<float>(spec.z));
        ecs->attach<PaintComponent>(text.entity, spec.colour);

        auto ttf = text.get<TTFText>();

        if (spec.overflow != Overflow::Grow)
        {
            // The box width must be in place before the first glyph build; later
            // width changes re-fit through the engine's settled-width escalation.
            text.get<PositionComponent>()->setWidth(spec.width);
            ttf->setOverflow(spec.overflow);
            ttf->setMaxLines(spec.maxLines);
        }

        // Align only matters against a box width (Wrap/Ellipsis); on Grow the box
        // hugs the text and placement is the caller's anchors, as before.
        if (spec.align != Align::Left)
            ttf->setAlign(spec.align);

        const TextStyle& style = styles.get(spec.style);

        Label label;
        label.entity = text.entity;
        label.spec = spec;
        label.fontAlias = style.fontAlias;
        label.lineHeightPx = style.lineHeightPx;
        label.lineSpacingPx = style.lineSpacingPx;
        label.letterSpacingPx = style.letterSpacingPx;

        applyMeasure(ecs, label);

        return label;
    }

    void Label::setText(EntitySystem* ecs, const std::string& newText)
    {
        spec.text = newText;
        entity->get<TTFText>()->setText(newText);
        applyMeasure(ecs, *this);
    }

    void Label::setColour(EntitySystem*, const std::string& token)
    {
        entity->get<PaintComponent>()->setToken(token);
        spec.colour = token;
    }

    void Label::setAlign(EntitySystem*, Align align)
    {
        entity->get<TTFText>()->setAlign(align);
        spec.align = align;
    }

    void Label::setWidth(EntitySystem* ecs, float width)
    {
        spec.width = width;
        entity->get<PositionComponent>()->setWidth(width);
        applyMeasure(ecs, *this);
    }
}
