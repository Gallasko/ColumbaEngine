#include "progressrule.h"

#include <algorithm>
#include <cmath>

#include "logger.h"

#include "2D/position.h"
#include "2D/simple2dobject.h"
#include "2D/decoratedshapes.h"
#include "UI/prefab.h"
#include "Systems/tween.h"
#include "UI/themesystem.h"

#include "Core/motion.h"

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

        // The resolved track height: an explicit trackHeight wins; otherwise 6 (small) or 10.
        float trackH(const ProgressRuleSpec& spec)
        {
            if (spec.trackHeight > 0.0f)
                return spec.trackHeight;
            return spec.small ? 6.0f : 10.0f;
        }
    }

    ProgressRule makeProgressRule(EntitySystem* ecs, const ProgressRuleSpec& specIn)
    {
        ProgressRuleSpec spec = specIn;
        spec.percent = clampPct(spec.percent);
        spec.forecastPercent = clampPct(spec.forecastPercent);

        const float W = spec.width;
        const float H = trackH(spec);
        const int z = spec.z;

        ProgressRule p;
        p.spec = spec;
        p.shown = spec.percent;

        auto root = makeAnchoredPrefab(ecs, 0.0f, 0.0f, static_cast<float>(z));
        root.get<PositionComponent>()->setWidth(W);
        root.get<PositionComponent>()->setHeight(H);   // grown below if there is a caption
        const _unique_id rootId = root.id;
        p.root = root.entity;

        // track
        auto track = makeUiSimple2DShape(ecs, Shape2D::Square, W, H);
        track.get<UiAnchor>()->setLeftAnchor(PosAnchor{rootId, AnchorType::Left});
        track.get<UiAnchor>()->setTopAnchor(PosAnchor{rootId, AnchorType::Top});
        track.get<UiAnchor>()->setZConstrain(PosConstrain{rootId, AnchorType::Z});
        ecs->attach<ThemeComponent>(track.entity, "progress.track");
        root.get<Prefab>()->addToPrefab(track.entity);
        p.track = track.entity;

        // fill (progress-ink), inside the frame
        auto fill = makeUiSimple2DShape(ecs, Shape2D::Square, 1.0f, H - 2.0f);
        {
            auto fa = fill.get<UiAnchor>();
            fa->setLeftAnchor(PosAnchor{rootId, AnchorType::Left}); fa->setLeftMargin(1.0f);
            fa->setTopAnchor(PosAnchor{rootId, AnchorType::Top});   fa->setTopMargin(1.0f);
            fa->setZConstrain(PosConstrain{rootId, AnchorType::Z, PosOpType::Add, 1.0f});
        }
        fill.get<PositionComponent>()->setHeight(H - 2.0f);
        ecs->attach<ThemeComponent>(fill.entity, "progress.fill");
        root.get<Prefab>()->addToPrefab(fill.entity);
        p.fill = fill.entity;

        // forecast (hatch), starting where the fill ends
        auto forecast = makeHatchRect2DShape(ecs, 1.0f, H - 2.0f, {255.0f, 255.0f, 255.0f, 255.0f}, 6.0f, 2.0f);
        forecast.get<HatchRect2DObject>()->setAngle(45.0f);
        {
            auto fca = forecast.get<UiAnchor>();
            fca->setLeftAnchor(PosAnchor{rootId, AnchorType::Left}); fca->setLeftMargin(1.0f);
            fca->setTopAnchor(PosAnchor{rootId, AnchorType::Top});   fca->setTopMargin(1.0f);
            fca->setZConstrain(PosConstrain{rootId, AnchorType::Z, PosOpType::Add, 2.0f});
        }
        forecast.get<PositionComponent>()->setHeight(H - 2.0f);
        ecs->attach<ThemeComponent>(forecast.entity, "progress.forecast");
        root.get<Prefab>()->addToPrefab(forecast.entity);
        p.forecast = forecast.entity;

        // frame (above fill and forecast so the hairline is never covered)
        auto frame = makeStrokeRect2DShape(ecs, 1.0f, 1.0f, {255.0f, 255.0f, 255.0f, 255.0f}, 1.0f);
        frame.get<UiAnchor>()->fillIn(track.get<UiAnchor>());
        frame.get<UiAnchor>()->setZConstrain(PosConstrain{rootId, AnchorType::Z, PosOpType::Add, 3.0f});
        ecs->attach<ThemeComponent>(frame.entity, "progress.frame");
        root.get<Prefab>()->addToPrefab(frame.entity);
        p.frame = frame.entity;

        // nib
        if (spec.nib)
        {
            Mark m = makeMark(ecs, {"quill", MarkSize::S14, "ink", z + 4});
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
            LabelSpec cs; cs.style = "caption"; cs.text = spec.caption; cs.color = "ink-muted";
            cs.overflow = Overflow::Wrap; cs.width = W; cs.z = z + 2;
            Label c = makeLabel(ecs, cs);
            auto ca = c.entity->get<UiAnchor>();
            ca->setLeftAnchor(PosAnchor{rootId, AnchorType::Left});
            ca->setTopAnchor(PosAnchor{rootId, AnchorType::Top}); ca->setTopMargin(H + CAP_GAP);
            ca->setZConstrain(PosConstrain{rootId, AnchorType::Z, PosOpType::Add, 2.0f});
            root.get<Prefab>()->addToPrefab(c.entity);
            p.caption = c;
            root.get<PositionComponent>()->setHeight(H + CAP_GAP + c.entity->get<PositionComponent>()->height);
        }

        p.layoutAt(p.shown);
        return p;
    }

    void ProgressRule::layoutAt(float shownPct)
    {
        const float H = trackH(spec);
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

    void ProgressRule::setGlide(EntitySystem* ecs, float percent, float ms)
    {
        percent = clampPct(percent);

        if (auto e = ecs->getEntity(fill.id); e and e->has<TweenComponent>())
            ecs->detach<TweenComponent>(e);

        // Held: the fill stays where it is drawn. A glide never runs back, and it is motion
        if (ms <= 0.0f or Motion::reduced() or percent <= shown)
            return;

        ProgressRule* self = this;
        ecs->attach<TweenComponent>(ecs->getEntity(fill.id), TweenComponent{
            TweenValue{shown}, TweenValue{percent}, ms,
            [self](const TweenValue& v) { self->shown = std::get<float>(v); self->layoutAt(self->shown); },
            nullptr, 1, false, false, TweenLinear});
    }

    void ProgressRule::setForecast(EntitySystem*, float forecastPercent)
    {
        spec.forecastPercent = clampPct(forecastPercent);
        layoutAt(shown);   // never animated: a forecast is a statement, not a motion
    }

    void ProgressRule::setCaption(EntitySystem* ecs, const std::string& text)
    {
        const float H = trackH(spec);
        if (text.empty())
        {
            if (caption)
            {
                ecs->removeEntity(caption->entity.id);
                caption.reset();
            }
            spec.caption.clear();
            root->get<PositionComponent>()->setHeight(H);
            return;
        }

        if (caption)
        {
            caption->setText(ecs, text);
        }
        else
        {
            LabelSpec cs; cs.style = "caption"; cs.text = text; cs.color = "ink-muted";
            cs.overflow = Overflow::Wrap; cs.width = spec.width; cs.z = spec.z + 2;
            Label c = makeLabel(ecs, cs);
            auto ca = c.entity->get<UiAnchor>();
            ca->setLeftAnchor(PosAnchor{root.id, AnchorType::Left});
            ca->setTopAnchor(PosAnchor{root.id, AnchorType::Top}); ca->setTopMargin(H + CAP_GAP);
            ca->setZConstrain(PosConstrain{root.id, AnchorType::Z, PosOpType::Add, 2.0f});
            root->get<Prefab>()->addToPrefab(c.entity);
            caption = c;
        }
        spec.caption = text;
        root->get<PositionComponent>()->setHeight(H + CAP_GAP + caption->entity->get<PositionComponent>()->height);
    }

    void ProgressRule::setNib(EntitySystem* ecs, bool on)
    {
        if (on and not nib)
        {
            const float H = trackH(spec);
            Mark m = makeMark(ecs, {"quill", MarkSize::S14, "ink", spec.z + 4});
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

    void ProgressRule::setWidth(EntitySystem* ecs, float width)
    {
        spec.width = width;
        const float H = trackH(spec);
        track->get<PositionComponent>()->setWidth(width);   // frame fillIn follows
        root->get<PositionComponent>()->setWidth(width);

        // The caption wraps at the rule's width
        if (caption)
        {
            caption->setWidth(ecs, width);
            root->get<PositionComponent>()->setHeight(H + CAP_GAP + caption->entity->get<PositionComponent>()->height);
        }

        layoutAt(shown);
    }
}
