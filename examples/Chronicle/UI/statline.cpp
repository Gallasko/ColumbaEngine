#include "statline.h"

#include <algorithm>
#include <string>

#include "logger.h"

#include "2D/position.h"
#include "2D/simple2dobject.h"
#include "UI/prefab.h"
#include "UI/ttftext.h"

#include "Core/textmetrics.h"
#include "paint.h"
#include "gloss.h"

using namespace pg;

namespace chronicle
{
    namespace
    {
        constexpr const char * const DOM = "Chronicle.StatLine";

        constexpr float HEAD = 22.0f;      // the `figure` line box
        constexpr float GAP1 = 4.0f;       // space-1
        constexpr float TRACK = 8.0f;      // the groove track
        constexpr float BASE_H = HEAD + GAP1 + TRACK;   // 34 (no note)

        const std::string PROJ = "\xE2\x86\x92 ";   // U+2192 + space: "-> "

        // Upper-case ASCII only; non-ASCII bytes (é, …) pass through untouched — `label` is a caps
        // style and the four MVP labels are ASCII; this documents the limit rather than transliterating.
        std::string asciiUpper(const std::string& s)
        {
            std::string r = s;
            for (char& c : r)
                if (c >= 'a' and c <= 'z')
                    c = static_cast<char>(c - 'a' + 'A');
            return r;
        }

        // The "-> p" forecast label, right-anchored to the root and baseline-aligned to the figure.
        Label buildProjection(EntitySystem* ecs, const Tokens& tokens, const TextStyles& styles,
                              EntityRef root, int z, float ascFig, float ascTick, int p)
        {
            LabelSpec ps;
            ps.style = "tick";
            ps.text = PROJ + std::to_string(p);
            ps.colour = "progress-forecast";
            ps.overflow = Overflow::Grow;
            ps.z = z + 2;

            Label pl = makeLabel(ecs, tokens, styles, ps);
            auto pa = pl.entity->get<UiAnchor>();
            pa->setRightAnchor(PosAnchor{root.id, AnchorType::Right});
            pa->setTopAnchor(PosAnchor{root.id, AnchorType::Top});
            pa->setTopMargin(ascFig - ascTick);
            pa->setZConstrain(PosConstrain{root.id, AnchorType::Z, PosOpType::Add, 2.0f});
            root->get<Prefab>()->addToPrefab(pl.entity);
            return pl;
        }

        // The note under the groove: wrapped `caption`, ink-faint, at groove.bottom + 4.
        Label buildNote(EntitySystem* ecs, const Tokens& tokens, const TextStyles& styles,
                        EntityRef root, int z, float width, const std::string& text)
        {
            LabelSpec ns;
            ns.style = "caption";
            ns.text = text;
            ns.colour = "ink-faint";
            ns.overflow = Overflow::Wrap;
            ns.width = width;
            ns.z = z + 2;

            Label nl = makeLabel(ecs, tokens, styles, ns);
            auto na = nl.entity->get<UiAnchor>();
            na->setLeftAnchor(PosAnchor{root.id, AnchorType::Left});
            na->setTopAnchor(PosAnchor{root.id, AnchorType::Top});
            na->setTopMargin(HEAD + GAP1 + TRACK + GAP1);   // 38
            na->setZConstrain(PosConstrain{root.id, AnchorType::Z, PosOpType::Add, 2.0f});
            root->get<Prefab>()->addToPrefab(nl.entity);
            return nl;
        }

        void setRootHeight(StatLine& sl)
        {
            float h = BASE_H;
            if (sl.note)
                h += GAP1 + sl.note->entity->get<PositionComponent>()->height;
            sl.root->get<PositionComponent>()->setHeight(h);
        }
    }

    StatLine makeStatLine(EntitySystem* ecs, const Tokens& tokens, const TextStyles& styles, const StatLineSpec& specIn)
    {
        StatLineSpec spec = specIn;
        spec.max = std::max(1, spec.max);
        spec.value = std::clamp(spec.value, 0, spec.max);
        if (spec.projected >= 0)
            spec.projected = std::clamp(spec.projected, 0, spec.max);
        if (spec.threshold != 0)
            spec.threshold = std::clamp(spec.threshold, 1, spec.max);

        const float W = spec.width;
        const int z = spec.z;
        const bool hasProj = spec.projected > spec.value;
        auto pct = [&](int v) { return 100.0f * static_cast<float>(v) / static_cast<float>(spec.max); };

        StatLine sl;
        sl.spec = spec;
        sl.tokens = &tokens;
        sl.styles = &styles;

        auto root = makeAnchoredPrefab(ecs, 0.0f, 0.0f, static_cast<float>(z));
        root.get<PositionComponent>()->setWidth(W);
        sl.root = root.entity;
        const _unique_id rootId = root.id;

        const float ascFig = ascenderOf(ecs, styles, "figure");
        const float ascLbl = ascenderOf(ecs, styles, "label");
        const float ascTick = ascenderOf(ecs, styles, "tick");

        // ── figure (right edge is the anchor; the figure does not reflow) ──────
        LabelSpec fs;
        fs.style = "figure"; fs.text = std::to_string(spec.value); fs.colour = "ink";
        fs.align = Align::Right; fs.overflow = Overflow::Grow; fs.z = z + 2;
        Label figure = makeLabel(ecs, tokens, styles, fs);
        {
            auto fa = figure.entity->get<UiAnchor>();
            fa->setTopAnchor(PosAnchor{rootId, AnchorType::Top});
            fa->setZConstrain(PosConstrain{rootId, AnchorType::Z, PosOpType::Add, 2.0f});
        }
        root.get<Prefab>()->addToPrefab(figure.entity);
        sl.figure = figure;

        // ── name: mark S16 + label style, both ink-muted, baseline on the figure ──
        MarkedLabelSpec ns;
        ns.mark = spec.glyph;
        ns.label = {"label", asciiUpper(spec.label), "ink-muted"};
        ns.z = z + 1;
        MarkedLabel name = makeMarkedLabel(ecs, tokens, styles, ns);
        if (name.mark)
            name.mark->setSize(ecs, MarkSize::S16);   // a stat name's mark is 16, not the label's 14
        {
            auto na = name.root->get<UiAnchor>();
            na->setLeftAnchor(PosAnchor{rootId, AnchorType::Left});
            na->setTopAnchor(PosAnchor{rootId, AnchorType::Top});
            na->setTopMargin(ascFig - ascLbl);
            na->setZConstrain(PosConstrain{rootId, AnchorType::Z, PosOpType::Add, 1.0f});
        }
        root.get<Prefab>()->addToPrefab(name.root);
        sl.name = name;

        // ── projection (only when above the value) ────────────────────────────
        if (hasProj)
            sl.projected = buildProjection(ecs, tokens, styles, sl.root, z, ascFig, ascTick, spec.projected);
        sl.placeFigureRight(ecs);

        // ── groove: a ProgressRule, nib off, 8 px track ───────────────────────
        ProgressRuleSpec gs;
        gs.width = W; gs.trackHeight = TRACK;
        gs.percent = pct(spec.value);
        gs.forecastPercent = hasProj ? pct(spec.projected) : 0.0f;
        gs.nib = false; gs.z = z + 1;
        ProgressRule groove = makeProgressRule(ecs, tokens, styles, gs);
        {
            auto ga = groove.root->get<UiAnchor>();
            ga->setLeftAnchor(PosAnchor{rootId, AnchorType::Left});
            ga->setTopAnchor(PosAnchor{rootId, AnchorType::Top});
            ga->setTopMargin(HEAD + GAP1);
            ga->setZConstrain(PosConstrain{rootId, AnchorType::Z, PosOpType::Add, 1.0f});
        }
        root.get<Prefab>()->addToPrefab(groove.root);
        sl.groove = groove;

        // ── threshold tick: 2 x 14 status-time, standing on the track (z+6) ───
        auto th = makeUiSimple2DShape(ecs, Shape2D::Square, 2.0f, 14.0f, tokens.colour("status-time"));
        {
            auto ta = th.get<UiAnchor>();
            ta->setLeftAnchor(PosAnchor{rootId, AnchorType::Left});
            ta->setTopAnchor(PosAnchor{rootId, AnchorType::Top});
            ta->setTopMargin(HEAD + GAP1 - 3.0f);   // 3 px proud of the track above and below
            ta->setZConstrain(PosConstrain{rootId, AnchorType::Z, PosOpType::Add, 6.0f});
        }
        ecs->attach<PaintComponent>(th.entity, "status-time");
        root.get<Prefab>()->addToPrefab(th.entity);
        sl.threshold = th.entity;
        sl.placeThreshold();

        // ── note ──────────────────────────────────────────────────────────────
        if (not spec.note.empty())
            sl.note = buildNote(ecs, tokens, styles, sl.root, z, W, spec.note);

        setRootHeight(sl);

        // ── gloss on the whole line ───────────────────────────────────────────
        if (not spec.glossKey.empty())
            attachGloss(ecs, sl.root, spec.glossKey);

        return sl;
    }

    void StatLine::placeFigureRight(EntitySystem*)
    {
        auto fa = figure.entity->get<UiAnchor>();
        if (projected)
        {
            fa->setRightAnchor(PosAnchor{projected->entity.id, AnchorType::Left});
            fa->setRightMargin(4.0f);
        }
        else
        {
            fa->setRightAnchor(PosAnchor{root.id, AnchorType::Right});
            fa->setRightMargin(0.0f);
        }
    }

    void StatLine::placeThreshold()
    {
        auto pos = threshold->get<PositionComponent>();
        if (spec.threshold <= 0)
        {
            pos->setVisibility(false);
            return;
        }
        pos->setVisibility(true);

        const float inner = spec.width - 2.0f;   // between the hairlines, like the fill
        const float lm = inner * static_cast<float>(spec.threshold) / static_cast<float>(spec.max);
        threshold->get<UiAnchor>()->setLeftMargin(lm);
    }

    void StatLine::setValue(EntitySystem* ecs, const TextStyles& styles, int value, bool animate)
    {
        value = std::clamp(value, 0, spec.max);
        spec.value = value;

        // The figure is the fact and updates at once; the bar is catching up.
        figure.setText(ecs, std::to_string(value));
        groove.setPercent(ecs, 100.0f * static_cast<float>(value) / static_cast<float>(spec.max), animate);

        // A projection at or below the value is not a projection.
        if (projected and spec.projected <= value)
            setProjected(ecs, styles, -1);
    }

    void StatLine::setProjected(EntitySystem* ecs, const TextStyles& styles, int p)
    {
        if (p >= 0)
            p = std::clamp(p, 0, spec.max);
        spec.projected = p;

        const bool show = p > spec.value;
        if (show)
        {
            if (not projected)
            {
                const float ascFig = ascenderOf(ecs, styles, "figure");
                const float ascTick = ascenderOf(ecs, styles, "tick");
                projected = buildProjection(ecs, *tokens, styles, root, spec.z, ascFig, ascTick, p);
            }
            else
            {
                projected->setText(ecs, PROJ + std::to_string(p));
            }
            groove.setForecast(ecs, 100.0f * static_cast<float>(p) / static_cast<float>(spec.max));
        }
        else
        {
            if (projected)
            {
                ecs->removeEntity(projected->entity.id);
                projected.reset();
            }
            groove.setForecast(ecs, 0.0f);
        }

        placeFigureRight(ecs);
    }

    void StatLine::setThreshold(EntitySystem*, int t)
    {
        if (t != 0)
            t = std::clamp(t, 1, spec.max);
        spec.threshold = t;
        placeThreshold();
    }

    void StatLine::setNote(EntitySystem* ecs, const TextStyles& styles, const std::string& text)
    {
        if (text.empty())
        {
            if (note)
            {
                ecs->removeEntity(note->entity.id);
                note.reset();
            }
            spec.note.clear();
        }
        else if (note)
        {
            note->setText(ecs, text);
            spec.note = text;
        }
        else if (tokens)
        {
            note = buildNote(ecs, *tokens, styles, root, spec.z, spec.width, text);
            spec.note = text;
        }

        setRootHeight(*this);
    }

    float StatLine::height(EntitySystem* ecs) const
    {
        return ecs->getEntity(root.id)->get<PositionComponent>()->height;
    }
}
