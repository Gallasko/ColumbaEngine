#include "progressrule.h"

#include <algorithm>
#include <cmath>

#include "logger.h"

#include "2D/position.h"
#include "2D/simple2dobject.h"
#include "2D/decoratedshapes.h"
#include "UI/prefab.h"
#include "Systems/tween.h"

#include "Core/motion.h"
#include "paint.h"

using namespace pg;

namespace chronicle
{
    namespace
    {
        constexpr const char * const DOM = "Chronicle.Progress";

        constexpr float NIB = 14.0f;        // quill S14
        constexpr float NIB_DX = 0.4f * NIB; // translate(-40%)
        constexpr float NIB_DY = 0.6f * NIB; // translate(-60%)
        constexpr float CAP_GAP = 4.0f;     // space-1 below the track

        float clampPct(float p) { return std::max(0.0f, std::min(100.0f, p)); }
        float trackH(bool small) { return small ? 6.0f : 10.0f; }
    }

    ProgressRule makeProgressRule(EntitySystem* ecs, const Tokens& tokens, const TextStyles& styles, const ProgressRuleSpec& specIn)
    {
        ProgressRuleSpec spec = specIn;
        spec.percent = clampPct(spec.percent);
        spec.forecastPercent = clampPct(spec.forecastPercent);

        const float W = spec.width;
        const float H = trackH(spec.small);
        const int z = spec.z;

        ProgressRule p;
        p.spec = spec;
        p.shown = spec.percent;
        p.tokens = &tokens;
        p.styles = &styles;

        auto root = makeAnchoredPrefab(ecs, 0.0f, 0.0f, static_cast<float>(z));
        root.get<PositionComponent>()->setWidth(W);
        root.get<PositionComponent>()->setHeight(H);   // grown below if there is a caption
        const _unique_id rootId = root.id;
        p.root = root.entity;

        // track
        auto track = makeUiSimple2DShape(ecs, Shape2D::Square, W, H, tokens.colour("progress-track"));
        track.get<UiAnchor>()->setLeftAnchor(PosAnchor{rootId, AnchorType::Left});
        track.get<UiAnchor>()->setTopAnchor(PosAnchor{rootId, AnchorType::Top});
        track.get<UiAnchor>()->setZConstrain(PosConstrain{rootId, AnchorType::Z});
        ecs->attach<PaintComponent>(track.entity, "progress-track");
        root.get<Prefab>()->addToPrefab(track.entity);
        p.track = track.entity;

        // fill (progress-ink), inside the frame
        auto fill = makeUiSimple2DShape(ecs, Shape2D::Square, 1.0f, H - 2.0f, tokens.colour("progress-ink"));
        {
            auto fa = fill.get<UiAnchor>();
            fa->setLeftAnchor(PosAnchor{rootId, AnchorType::Left}); fa->setLeftMargin(1.0f);
            fa->setTopAnchor(PosAnchor{rootId, AnchorType::Top});   fa->setTopMargin(1.0f);
            fa->setZConstrain(PosConstrain{rootId, AnchorType::Z, PosOpType::Add, 1.0f});
        }
        fill.get<PositionComponent>()->setHeight(H - 2.0f);
        ecs->attach<PaintComponent>(fill.entity, "progress-ink");
        root.get<Prefab>()->addToPrefab(fill.entity);
        p.fill = fill.entity;

        // forecast (hatch), starting where the fill ends
        auto forecast = makeHatchRect2DShape(ecs, 1.0f, H - 2.0f, tokens.colour("progress-forecast"), 6.0f, 2.0f);
        forecast.get<HatchRect2DObject>()->setAngle(45.0f);
        {
            auto fca = forecast.get<UiAnchor>();
            fca->setLeftAnchor(PosAnchor{rootId, AnchorType::Left}); fca->setLeftMargin(1.0f);
            fca->setTopAnchor(PosAnchor{rootId, AnchorType::Top});   fca->setTopMargin(1.0f);
            fca->setZConstrain(PosConstrain{rootId, AnchorType::Z, PosOpType::Add, 2.0f});
        }
        forecast.get<PositionComponent>()->setHeight(H - 2.0f);
        ecs->attach<PaintComponent>(forecast.entity, "progress-forecast", tokens.opacity("opacity-hatch"));
        root.get<Prefab>()->addToPrefab(forecast.entity);
        p.forecast = forecast.entity;

        // frame (above fill and forecast so the hairline is never covered)
        auto frame = makeStrokeRect2DShape(ecs, 1.0f, 1.0f, tokens.colour("rule-hair"), 1.0f);
        frame.get<StrokeRect2DObject>()->setCornerRadius(tokens.radius("radius-sm"));
        frame.get<UiAnchor>()->fillIn(track.get<UiAnchor>());
        frame.get<UiAnchor>()->setZConstrain(PosConstrain{rootId, AnchorType::Z, PosOpType::Add, 3.0f});
        ecs->attach<PaintComponent>(frame.entity, "rule-hair");
        root.get<Prefab>()->addToPrefab(frame.entity);
        p.frame = frame.entity;

        // nib
        if (spec.nib)
        {
            Mark m = makeMark(ecs, tokens, {"quill", MarkSize::S14, "ink", z + 4});
            auto ma = m.entity->get<UiAnchor>();
            ma->setLeftAnchor(PosAnchor{rootId, AnchorType::Left});
            ma->setTopAnchor(PosAnchor{rootId, AnchorType::Top}); ma->setTopMargin(H / 2.0f - NIB_DY);
            ma->setZConstrain(PosConstrain{rootId, AnchorType::Z, PosOpType::Add, 4.0f});
            root.get<Prefab>()->addToPrefab(m.entity);
            p.nib = m;
        }

        // caption
        if (not spec.caption.empty())
        {
            LabelSpec cs; cs.style = "caption"; cs.text = spec.caption; cs.colour = "ink-muted";
            cs.overflow = Overflow::Wrap; cs.width = W; cs.z = z + 1;
            Label c = makeLabel(ecs, tokens, styles, cs);
            auto ca = c.box->get<UiAnchor>();
            ca->setLeftAnchor(PosAnchor{rootId, AnchorType::Left});
            ca->setTopAnchor(PosAnchor{rootId, AnchorType::Top}); ca->setTopMargin(H + CAP_GAP);
            ca->setZConstrain(PosConstrain{rootId, AnchorType::Z, PosOpType::Add, 1.0f});
            root.get<Prefab>()->addToPrefab(c.box);
            p.caption = c;
            root.get<PositionComponent>()->setHeight(H + CAP_GAP + c.box->get<PositionComponent>()->height);
        }

        p.layoutAt(p.shown);
        return p;
    }

    void ProgressRule::layoutAt(float shownPct)
    {
        const float H = trackH(spec.small);
        const float inner = spec.width - 2.0f;
        const float fillW = inner * shownPct / 100.0f;

        fill->get<PositionComponent>()->setWidth(fillW);

        const float fW = (spec.forecastPercent > shownPct) ? inner * (spec.forecastPercent - shownPct) / 100.0f : 0.0f;
        forecast->get<UiAnchor>()->setLeftMargin(1.0f + fillW);
        forecast->get<PositionComponent>()->setWidth(fW);

        if (nib)
            nib->entity->get<UiAnchor>()->setLeftMargin(1.0f + fillW - NIB_DX);
        (void)H;
    }

    void ProgressRule::setPercent(EntitySystem* ecs, float percent, bool animate)
    {
        percent = clampPct(percent);
        spec.percent = percent;

        // Always drop any running tween on the fill first.
        if (auto e = ecs->getEntity(fill.id); e and e->has<TweenComponent>())
            ecs->detach<TweenComponent>(e);

        if (not animate or Motion::reduced() or percent == shown)
        {
            shown = percent;
            layoutAt(shown);
            return;
        }

        const float dur = std::abs(percent - shown) * Motion::kMsPerPercent;
        ProgressRule* self = this;
        ecs->attach<TweenComponent>(ecs->getEntity(fill.id), TweenComponent{
            TweenValue{shown}, TweenValue{percent}, dur,
            [self](const TweenValue& v) { self->shown = std::get<float>(v); self->layoutAt(self->shown); },
            nullptr, 1, false, false, TweenLinear});
    }

    void ProgressRule::setForecast(EntitySystem*, float forecastPercent)
    {
        spec.forecastPercent = clampPct(forecastPercent);
        layoutAt(shown);   // never animated: a forecast is a statement, not a motion
    }

    void ProgressRule::setCaption(EntitySystem* ecs, const TextStyles& s, const std::string& text)
    {
        const float H = trackH(spec.small);
        if (text.empty())
        {
            if (caption)
            {
                ecs->removeEntity(caption->box.id);
                caption.reset();
            }
            spec.caption.clear();
            root->get<PositionComponent>()->setHeight(H);
            return;
        }

        if (caption)
        {
            caption->setText(ecs, s, text);
        }
        else if (tokens)
        {
            LabelSpec cs; cs.style = "caption"; cs.text = text; cs.colour = "ink-muted";
            cs.overflow = Overflow::Wrap; cs.width = spec.width; cs.z = spec.z + 1;
            Label c = makeLabel(ecs, *tokens, s, cs);
            auto ca = c.box->get<UiAnchor>();
            ca->setLeftAnchor(PosAnchor{root.id, AnchorType::Left});
            ca->setTopAnchor(PosAnchor{root.id, AnchorType::Top}); ca->setTopMargin(H + CAP_GAP);
            ca->setZConstrain(PosConstrain{root.id, AnchorType::Z, PosOpType::Add, 1.0f});
            root->get<Prefab>()->addToPrefab(c.box);
            caption = c;
        }
        spec.caption = text;
        root->get<PositionComponent>()->setHeight(H + CAP_GAP + caption->box->get<PositionComponent>()->height);
    }

    void ProgressRule::setNib(EntitySystem* ecs, const Tokens& tokens, bool on)
    {
        if (on and not nib)
        {
            const float H = trackH(spec.small);
            Mark m = makeMark(ecs, tokens, {"quill", MarkSize::S14, "ink", spec.z + 4});
            auto ma = m.entity->get<UiAnchor>();
            ma->setLeftAnchor(PosAnchor{root.id, AnchorType::Left});
            ma->setTopAnchor(PosAnchor{root.id, AnchorType::Top}); ma->setTopMargin(H / 2.0f - NIB_DY);
            ma->setZConstrain(PosConstrain{root.id, AnchorType::Z, PosOpType::Add, 4.0f});
            root->get<Prefab>()->addToPrefab(m.entity);
            nib = m;
            layoutAt(shown);
        }
        else if (not on and nib)
        {
            ecs->removeEntity(nib->entity.id);
            nib.reset();
        }
        spec.nib = on;
    }

    void ProgressRule::setWidth(EntitySystem*, float width)
    {
        spec.width = width;
        const float H = trackH(spec.small);
        track->get<PositionComponent>()->setWidth(width);   // frame fillIn follows
        root->get<PositionComponent>()->setWidth(width);
        (void)H;
        layoutAt(shown);
    }
}
