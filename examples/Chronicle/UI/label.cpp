#include "label.h"

#include <cassert>
#include <vector>

#include "ECS/entitysystem.h"
#include "2D/position.h"
#include "UI/prefab.h"
#include "logger.h"

#include "paint.h"

using namespace pg;

namespace chronicle
{
    namespace
    {
        constexpr const char * const DOM = "Chronicle.Label";

        const std::string ELLIPSIS = "\xE2\x80\xA6";   // U+2026

        // Byte offset of each code point start, plus a final entry at text.size().
        std::vector<size_t> codePointStarts(const std::string& text)
        {
            std::vector<size_t> starts;

            for (size_t i = 0; i < text.size(); )
            {
                starts.push_back(i);
                const unsigned char c = static_cast<unsigned char>(text[i]);
                if (c >= 0xF0)
                    i += 4;
                else if (c >= 0xE0)
                    i += 3;
                else if (c >= 0xC0)
                    i += 2;
                else
                    i += 1;
            }

            starts.push_back(text.size());
            return starts;
        }

        void trimTrailingSpaces(std::string& s)
        {
            while (not s.empty() and s.back() == ' ')
                s.pop_back();
        }

        float measureWidth(const TTFTextSystem& ttf, const TextStyle& style, const std::string& s)
        {
            return ttf.measureText(style.fontAlias, s, 1.0f, 0.0f, 0.0f, style.letterSpacingPx).width;
        }
    }

    int countLines(const TTFTextSystem& ttf, const TextStyle& style, const std::string& text, float width)
    {
        return ttf.measureText(style.fontAlias, text, 1.0f, width, style.lineSpacingPx, style.letterSpacingPx).lineCount;
    }

    std::string fitEllipsis(const TTFTextSystem& ttf, const TextStyle& style, const std::string& text, float width)
    {
        if (measureWidth(ttf, style, text) <= width)
            return text;

        // Not even the ellipsis fits: an empty label (a box too narrow for one glyph is the caller's bug).
        if (measureWidth(ttf, style, ELLIPSIS) > width)
            return "";

        const std::vector<size_t> starts = codePointStarts(text);
        const int n = static_cast<int>(starts.size()) - 1;

        int lo = 0;
        int hi = n;
        int best = 0;

        while (lo <= hi)
        {
            const int mid = (lo + hi) / 2;
            std::string prefix = text.substr(0, starts[mid]);
            trimTrailingSpaces(prefix);

            if (measureWidth(ttf, style, prefix + ELLIPSIS) <= width)
            {
                best = mid;
                lo = mid + 1;
            }
            else
            {
                hi = mid - 1;
            }
        }

        std::string prefix = text.substr(0, starts[best]);
        trimTrailingSpaces(prefix);
        return prefix + ELLIPSIS;
    }

    std::string clampLines(const TTFTextSystem& ttf, const TextStyle& style, const std::string& text, float width, int maxLines)
    {
        if (maxLines <= 0 or countLines(ttf, style, text, width) <= maxLines)
            return text;

        const std::vector<size_t> starts = codePointStarts(text);
        const int n = static_cast<int>(starts.size()) - 1;

        int lo = 0;
        int hi = n;
        int best = 0;
        while (lo <= hi)
        {
            const int mid = (lo + hi) / 2;
            std::string prefix = text.substr(0, starts[mid]);
            trimTrailingSpaces(prefix);

            if (countLines(ttf, style, prefix + ELLIPSIS, width) <= maxLines)
            {
                best = mid;
                lo = mid + 1;
            }
            else
            {
                hi = mid - 1;
            }
        }

        std::string prefix = text.substr(0, starts[best]);
        trimTrailingSpaces(prefix);
        return prefix + ELLIPSIS;
    }

    Label makeLabel(EntitySystem* ecs, const Tokens& tokens, const TextStyles& styles, const LabelSpec& specIn)
    {
        LabelSpec spec = specIn;

        auto* ttf = ecs->getSystem<TTFTextSystem>();
        const TextStyle& style = styles.get(spec.style);

        // Wrap and Ellipsis need a width; without one, fall back to Grow.
        if ((spec.overflow == Overflow::Wrap or spec.overflow == Overflow::Ellipsis) and spec.width <= 0.0f)
        {
            assert(false and "Label Wrap/Ellipsis needs a width");
            LOG_ERROR(DOM, "Label '" << spec.style << "' overflow needs a width; falling back to Grow");
            spec.overflow = Overflow::Grow;
        }

        std::string fitted;
        if (spec.overflow == Overflow::Ellipsis)
            fitted = fitEllipsis(*ttf, style, spec.text, spec.width);
        else if (spec.overflow == Overflow::Wrap)
            fitted = clampLines(*ttf, style, spec.text, spec.width, spec.maxLines);
        else
            fitted = spec.text;

        auto box = makeAnchoredPrefab(ecs, 0.0f, 0.0f, static_cast<float>(spec.z));
        auto text = styles.makeText(ecs, spec.style, fitted, tokens.colour(spec.colour), 0.0f, 0.0f, static_cast<float>(spec.z + 1));
        ecs->attach<PaintComponent>(text.entity, spec.colour);

        box.get<Prefab>()->addToPrefab(text.entity);

        auto textAnchor = text.get<UiAnchor>();
        textAnchor->setTopAnchor(PosAnchor{box.id, AnchorType::Top});
        textAnchor->setZConstrain(PosConstrain{box.id, AnchorType::Z, PosOpType::Add, 1.0f});

        // Height in token line heights, so styles stack on a common rhythm.
        float boxHeight = static_cast<float>(style.lineHeightPx);
        if (spec.overflow == Overflow::Grow)
        {
            const int lines = ttf->measureText(style.fontAlias, fitted, 1.0f, 0.0f, style.lineSpacingPx, style.letterSpacingPx).lineCount;
            boxHeight = static_cast<float>(lines) * static_cast<float>(style.lineHeightPx);
        }
        else if (spec.overflow == Overflow::Wrap)
        {
            boxHeight = static_cast<float>(countLines(*ttf, style, fitted, spec.width)) * static_cast<float>(style.lineHeightPx);
        }

        float boxWidth = (spec.overflow == Overflow::Grow) ? measureWidth(*ttf, style, fitted) : spec.width;

        if (spec.overflow == Overflow::Wrap)
        {
            // The wrap width is read from the text's own width at glyph-build time
            // (maxWidth = wrap ? ui->width : 0). Set it now: the anchors below settle it
            // to the same value over the next ticks, but a later width change only triggers
            // a position-only update, never a re-wrap — so the first build must already
            // see the constraint, or the text lays out as a single unwrapped line.
            text.get<PositionComponent>()->setWidth(spec.width);
            text.get<TTFText>()->setWrap(true);
            // Both anchors stretch the child to the box width; the engine wraps at it.
            textAnchor->setLeftAnchor(PosAnchor{box.id, AnchorType::Left});
            textAnchor->setRightAnchor(PosAnchor{box.id, AnchorType::Right});

            if (spec.align != Align::Left)
            {
                static bool warned = false;
                if (not warned)
                {
                    LOG_WARNING(DOM, "Label: per-line alignment inside wrapped text is not supported; use Left");
                    warned = true;
                }
            }
        }
        else
        {
            switch (spec.align)
            {
            case Align::Left:
                textAnchor->setLeftAnchor(PosAnchor{box.id, AnchorType::Left});
                break;

            case Align::Centre:
                textAnchor->setHorizontalCenter(PosAnchor{box.id, AnchorType::HorizontalCenter});
                break;

            case Align::Right:
                textAnchor->setRightAnchor(PosAnchor{box.id, AnchorType::Right});
                break;
            }
        }

        auto boxPos = box.get<PositionComponent>();
        boxPos->setWidth(boxWidth);
        boxPos->setHeight(boxHeight);

        Label label;
        label.box = box.entity;
        label.text = text.entity;
        label.spec = spec;
        label.fitted = fitted;
        return label;
    }

    void Label::setText(EntitySystem* ecs, const TextStyles& styles, const std::string& newText)
    {
        const TextStyle& style = styles.get(spec.style);
        auto* ttf = ecs->getSystem<TTFTextSystem>();

        spec.text = newText;

        if (spec.overflow == Overflow::Ellipsis)
            fitted = fitEllipsis(*ttf, style, newText, spec.width);
        else if (spec.overflow == Overflow::Wrap)
            fitted = clampLines(*ttf, style, newText, spec.width, spec.maxLines);
        else
            fitted = newText;

        text->get<TTFText>()->setText(fitted);

        auto boxPos = box->get<PositionComponent>();
        if (spec.overflow == Overflow::Grow)
            boxPos->setWidth(measureWidth(*ttf, style, fitted));
        else if (spec.overflow == Overflow::Wrap)
            boxPos->setHeight(static_cast<float>(countLines(*ttf, style, fitted, spec.width)) * static_cast<float>(style.lineHeightPx));
    }

    void Label::setColour(EntitySystem*, const std::string& token)
    {
        text->get<PaintComponent>()->setToken(token);
        spec.colour = token;
    }

    void Label::setAlign(EntitySystem* ecs, Align align)
    {
        if (spec.overflow == Overflow::Wrap)
        {
            static bool warned = false;
            if (not warned)
            {
                LOG_WARNING(DOM, "Label: per-line alignment inside wrapped text is not supported; use Left");
                warned = true;
            }

            return;
        }

        auto anchor = text->get<UiAnchor>();
        anchor->clearLeftAnchor();
        anchor->clearRightAnchor();
        anchor->clearHorizontalCenter();

        switch (align)
        {
        case Align::Left:
            anchor->setLeftAnchor(PosAnchor{box.id, AnchorType::Left});
            break;

        case Align::Centre:
            anchor->setHorizontalCenter(PosAnchor{box.id, AnchorType::HorizontalCenter});
            break;

        case Align::Right:
            anchor->setRightAnchor(PosAnchor{box.id, AnchorType::Right});
            break;
        }

        spec.align = align;
    }

    void Label::setWidth(EntitySystem* ecs, const TextStyles& styles, float width)
    {
        const TextStyle& style = styles.get(spec.style);
        auto* ttf = ecs->getSystem<TTFTextSystem>();

        spec.width = width;
        box->get<PositionComponent>()->setWidth(width);

        if (spec.overflow == Overflow::Ellipsis)
        {
            fitted = fitEllipsis(*ttf, style, spec.text, width);
            text->get<TTFText>()->setText(fitted);
        }
        else if (spec.overflow == Overflow::Wrap)
        {
            fitted = clampLines(*ttf, style, spec.text, width, spec.maxLines);
            text->get<TTFText>()->setText(fitted);
            box->get<PositionComponent>()->setHeight(static_cast<float>(countLines(*ttf, style, fitted, width)) * static_cast<float>(style.lineHeightPx));
        }
    }

    float Label::boxWidth(EntitySystem* ecs) const
    {
        return ecs->getEntity(box.id)->get<PositionComponent>()->width;
    }

    float Label::boxHeight(EntitySystem* ecs) const
    {
        return ecs->getEntity(box.id)->get<PositionComponent>()->height;
    }
}
