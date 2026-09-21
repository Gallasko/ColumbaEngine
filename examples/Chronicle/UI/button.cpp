#include "button.h"

#ifdef __linux__
#include <SDL2/SDL.h>
#elif _WIN32
#include <SDL.h>
#endif

#include <algorithm>

#include "logger.h"

#include "ECS/callable.h"
#include "2D/position.h"
#include "2D/simple2dobject.h"
#include "2D/decoratedshapes.h"
#include "UI/prefab.h"
#include "UI/ttftext.h"

#include "paint.h"

using namespace pg;

namespace chronicle
{
    namespace
    {
        constexpr const char * const DOM = "Chronicle.Button";

        // No-op event so a face's auto-attached hover components carry a valid callable; the
        // system reacts to HoverChangedEvent, not to these.
        struct ButtonNoOp {};

        constexpr float FACE_H = 36.0f;   // space-2 padding + control line height (8 + 20 + 8)
        constexpr float PAD_X = 12.0f;    // space-3 side padding
        constexpr float GAP = 8.0f;       // space-2 between face parts
        constexpr float GLYPH = 16.0f;    // S16
        constexpr float COST_MARK = 14.0f;// S14 time mark
        constexpr float COST_GAP = 4.0f;  // space-1 between the time mark and its figure
        constexpr float REASON_GAP = 4.0f;
        constexpr float REASON_H = 15.0f; // caption line height

        std::string groundToken(ButtonVariant v, bool hovered, bool enabled)
        {
            switch (v)
            {
            case ButtonVariant::Study: return "lapis";
            case ButtonVariant::Seal:  return "vermilion";
            case ButtonVariant::Quiet:
            default:                   return (hovered and enabled) ? "vellum-tint" : "folio";
            }
        }

        std::string frameToken(ButtonVariant v)
        {
            switch (v)
            {
            case ButtonVariant::Study: return "lapis";
            case ButtonVariant::Seal:  return "vermilion";
            case ButtonVariant::Quiet:
            default:                   return "rule-ruled";
            }
        }

        std::string inkToken(ButtonVariant v)
        {
            switch (v)
            {
            case ButtonVariant::Study: return "on-lapis";
            case ButtonVariant::Seal:  return "on-vermilion";
            case ButtonVariant::Quiet:
            default:                   return "ink";
            }
        }

        std::string sheenToken(ButtonVariant v) { return v == ButtonVariant::Study ? "on-lapis" : "on-vermilion"; }

        // Quiet keeps its cost in the ochre time colour; the tonal variants use their on-tone ink.
        std::string costToken(ButtonVariant v) { return v == ButtonVariant::Quiet ? "status-time" : inkToken(v); }

        std::string monthsText(int months) { return std::to_string(months) + " mo"; }

        bool contains(const std::vector<_unique_id>& v, _unique_id id)
        {
            return std::find(v.begin(), v.end(), id) != v.end();
        }
    }

    Button makeButton(EntitySystem* ecs, const Tokens& tokens, const TextStyles& styles, const ButtonSpec& spec)
    {
        auto* paint = ecs->getSystem<PaintSystem>();
        const std::string ink = inkToken(spec.variant);
        const std::string cost = costToken(spec.variant);
        const bool hasGlyph = not spec.glyph.empty();
        const bool hasCost = spec.months >= 0;
        const int z = spec.z;
        // The initial dim, so a button that is born disabled draws dimmed before any event.
        const float a0 = spec.disabled ? tokens.opacity("opacity-locked") : 1.0f;

        Button b;
        b.spec = spec;
        b.styles = &styles;

        // Measure the label (and the cost figure) so the face can size to them.
        LabelSpec ls; ls.style = "control"; ls.text = spec.label; ls.colour = ink; ls.z = z + 2;
        Label label = makeLabel(ecs, tokens, styles, ls);
        const float labelW = label.box->get<PositionComponent>()->width;

        float costTextW = 0.0f;
        std::optional<Label> costLabel;
        if (hasCost)
        {
            LabelSpec cs; cs.style = "tick"; cs.text = monthsText(spec.months); cs.colour = cost; cs.z = z + 2;
            costLabel = makeLabel(ecs, tokens, styles, cs);
            costTextW = costLabel->box->get<PositionComponent>()->width;
        }

        const float faceW = PAD_X
            + (hasGlyph ? GLYPH + GAP : 0.0f)
            + labelW
            + (hasCost ? GAP + COST_MARK + COST_GAP + costTextW : 0.0f)
            + PAD_X;

        const bool hasReason = not spec.reason.empty();
        const float rootH = (spec.disabled and hasReason) ? FACE_H + REASON_GAP + REASON_H : FACE_H;

        // root
        auto root = makeAnchoredPrefab(ecs, 0.0f, 0.0f, static_cast<float>(z));
        root.get<PositionComponent>()->setWidth(faceW);
        root.get<PositionComponent>()->setHeight(rootH);
        const _unique_id rootId = root.id;
        b.root = root.entity;

        // face: the 36-tall hit area, full width, carrying the state and focus.
        auto face = ecs->createEntity();
        auto facePos = ecs->attach<PositionComponent>(face);
        facePos->setHeight(FACE_H);
        facePos->setZ(static_cast<float>(z));
        auto faceAnchor = ecs->attach<UiAnchor>(face);
        faceAnchor->setLeftAnchor(PosAnchor{rootId, AnchorType::Left});
        faceAnchor->setRightAnchor(PosAnchor{rootId, AnchorType::Right});
        faceAnchor->setTopAnchor(PosAnchor{rootId, AnchorType::Top});
        ecs->attach<FocusableComponent>(face);
        auto state = ecs->attach<ButtonState>(face);
        state->variant = spec.variant;
        state->disabled = spec.disabled;
        state->tag = spec.tag;
        const _unique_id faceId = face.id;
        b.face = face;
        root.get<Prefab>()->addToPrefab(face);

        // Join the kit-wide Tab order (buttons, tabs, later rows share one).
        if (auto* fo = ecs->getSystem<FocusOrderSystem>())
        {
            fo->add(faceId);
            fo->setEnabled(faceId, not spec.disabled);
        }

        // Attach a face child: anchor it into the face, constrain z, prefab it.
        auto anchorInFace = [&](EntityRef e, float leftMargin, int zOffset, bool vcentre)
        {
            auto a = e->get<UiAnchor>();
            a->setLeftAnchor(PosAnchor{faceId, AnchorType::Left});
            a->setLeftMargin(leftMargin);
            if (vcentre)
                a->setVerticalCenter(PosAnchor{faceId, AnchorType::VerticalCenter});
            a->setZConstrain(PosConstrain{faceId, AnchorType::Z, PosOpType::Add, static_cast<float>(zOffset)});
            root.get<Prefab>()->addToPrefab(e);
        };

        // ground (rounded folio/tone), z.
        auto ground = makeRoundedRect2DShape(ecs, tokens.radius("radius-md"), 1.0f, 1.0f,
            tokens.colour(groundToken(spec.variant, false, not spec.disabled)));
        ground.get<UiAnchor>()->fillIn(face->get<UiAnchor>());
        ground.get<UiAnchor>()->setZConstrain(PosConstrain{faceId, AnchorType::Z});
        paint->paint(ground.entity, groundToken(spec.variant, false, not spec.disabled), a0);
        root.get<Prefab>()->addToPrefab(ground.entity);
        state->ground = ground.entity.id;

        // frame (1 px stroke, radius 3), z+1.
        auto frame = makeStrokeRect2DShape(ecs, 1.0f, 1.0f, tokens.colour(frameToken(spec.variant)), 1.0f);
        frame.get<StrokeRect2DObject>()->setCornerRadius(tokens.radius("radius-md"));
        frame.get<UiAnchor>()->fillIn(face->get<UiAnchor>());
        frame.get<UiAnchor>()->setZConstrain(PosConstrain{faceId, AnchorType::Z, PosOpType::Add, 1.0f});
        paint->paint(frame.entity, frameToken(spec.variant), a0);
        root.get<Prefab>()->addToPrefab(frame.entity);
        state->frame = frame.entity.id;

        // sheen (tonal only): a hover brighten/darken, alpha 0 until hovered.
        if (spec.variant != ButtonVariant::Quiet)
        {
            auto sheen = makeRoundedRect2DShape(ecs, tokens.radius("radius-md"), 1.0f, 1.0f, tokens.colour(sheenToken(spec.variant)));
            sheen.get<UiAnchor>()->fillIn(face->get<UiAnchor>());
            sheen.get<UiAnchor>()->setZConstrain(PosConstrain{faceId, AnchorType::Z, PosOpType::Add, 1.0f});
            paint->paint(sheen.entity, sheenToken(spec.variant), 0.0f);
            root.get<Prefab>()->addToPrefab(sheen.entity);
            state->sheen = sheen.entity.id;
        }

        // focus ring: 2 px focus-ink, radius 5, 4 px outside the face, hidden until keyboard focus.
        auto ring = makeStrokeRect2DShape(ecs, 1.0f, 1.0f, tokens.colour("focus-ink"), tokens.border("border-rule"));
        ring.get<StrokeRect2DObject>()->setCornerRadius(5.0f);
        {
            auto ra = ring.get<UiAnchor>();
            ra->setLeftAnchor(PosAnchor{faceId, AnchorType::Left});     ra->setLeftMargin(-4.0f);
            ra->setRightAnchor(PosAnchor{faceId, AnchorType::Right});   ra->setRightMargin(-4.0f);
            ra->setTopAnchor(PosAnchor{faceId, AnchorType::Top});       ra->setTopMargin(-4.0f);
            ra->setBottomAnchor(PosAnchor{faceId, AnchorType::Bottom}); ra->setBottomMargin(-4.0f);
            ra->setZConstrain(PosConstrain{faceId, AnchorType::Z, PosOpType::Add, 1.0f});
        }
        ring.get<PositionComponent>()->setVisible(false);
        paint->paint(ring.entity, "focus-ink");
        root.get<Prefab>()->addToPrefab(ring.entity);
        state->ring = ring.entity.id;

        // face parts, z+2 (their texts z+3). Re-paint at a0 so a born-disabled button is dimmed.
        float x = PAD_X;
        if (hasGlyph)
        {
            Mark m = makeMark(ecs, tokens, {spec.glyph, MarkSize::S16, ink, z + 2});
            anchorInFace(m.entity, x, 2, /*vcentre*/ true);
            paint->paint(m.entity, ink, a0);
            b.glyph = m;
            state->inked.push_back(m.entity.id);
            x += GLYPH + GAP;
        }

        anchorInFace(label.box, x, 2, /*vcentre*/ true);
        paint->paint(label.text, ink, a0);
        b.label = label;
        state->inked.push_back(label.text.id);
        x += labelW;

        if (hasCost)
        {
            x += GAP;
            Mark cm = makeMark(ecs, tokens, {"time", MarkSize::S14, cost, z + 2});
            anchorInFace(cm.entity, x, 2, /*vcentre*/ true);
            paint->paint(cm.entity, cost, a0);
            b.costMark = cm;
            state->inked.push_back(cm.entity.id);
            state->costParts.push_back(cm.entity.id);
            x += COST_MARK + COST_GAP;

            anchorInFace(costLabel->box, x, 2, /*vcentre*/ true);
            paint->paint(costLabel->text, cost, a0);
            b.cost = costLabel;
            state->inked.push_back(costLabel->text.id);
            state->costParts.push_back(costLabel->text.id);
        }

        // reason (created when there is reason text; shown only while disabled).
        if (hasReason)
        {
            LabelSpec rs; rs.style = "caption"; rs.text = spec.reason; rs.colour = "ink-faint"; rs.z = z + 2;
            Label r = makeLabel(ecs, tokens, styles, rs);
            auto ra = r.box->get<UiAnchor>();
            ra->setLeftAnchor(PosAnchor{rootId, AnchorType::Left});
            ra->setTopAnchor(PosAnchor{faceId, AnchorType::Bottom}); ra->setTopMargin(REASON_GAP);
            ra->setZConstrain(PosConstrain{faceId, AnchorType::Z, PosOpType::Add, 2.0f});
            r.box->get<PositionComponent>()->setVisible(spec.disabled);
            root.get<Prefab>()->addToPrefab(r.box);
            b.reason = r;
            state->reasonBox = r.box.id;
        }

        return b;
    }

    // ── Button methods ─────────────────────────────────────────────────────────
    void Button::setDisabled(EntitySystem* ecs, bool disabled, const std::string& newReason)
    {
        spec.disabled = disabled;
        face->get<ButtonState>()->disabled = disabled;
        if (auto* fo = ecs->getSystem<FocusOrderSystem>())
            fo->setEnabled(face.id, not disabled);
        if (not newReason.empty() and reason and styles)
        {
            reason->setText(ecs, *styles, newReason);
            spec.reason = newReason;
        }
        // The reason band changes the root height.
        const bool showReason = disabled and reason.has_value();
        root->get<PositionComponent>()->setHeight(showReason ? FACE_H + REASON_GAP + REASON_H : FACE_H);
        ecs->getSystem<ButtonSystem>()->applyVisual(face);
    }

    void Button::setLabel(EntitySystem* ecs, const TextStyles& styles, const std::string& text)
    {
        label.setText(ecs, styles, text);
        // Re-measure and regrow the face; the glyph/cost anchors reflow from the new label width.
        const float labelW = label.box->get<PositionComponent>()->width;
        const bool hasGlyph = glyph.has_value();
        const bool hasCost = cost.has_value();
        float costTextW = hasCost ? cost->box->get<PositionComponent>()->width : 0.0f;
        const float faceW = PAD_X + (hasGlyph ? GLYPH + GAP : 0.0f) + labelW
            + (hasCost ? GAP + COST_MARK + COST_GAP + costTextW : 0.0f) + PAD_X;
        root->get<PositionComponent>()->setWidth(faceW);

        // Shift the cost pair to follow the new label right edge.
        if (hasCost)
        {
            float x = PAD_X + (hasGlyph ? GLYPH + GAP : 0.0f) + labelW + GAP;
            costMark->entity->get<UiAnchor>()->setLeftMargin(x);
            x += COST_MARK + COST_GAP;
            cost->box->get<UiAnchor>()->setLeftMargin(x);
        }
        spec.label = text;
    }

    void Button::setMonths(EntitySystem*, const TextStyles&, int months)
    {
        // Full add/remove of the cost pair is a rebuild; the gallery rebuilds instead.
        spec.months = months;
        LOG_WARNING(DOM, "setMonths after construction is not supported; rebuild the button");
    }

    float Button::faceWidth(EntitySystem* ecs) const
    {
        return ecs->getEntity(face.id)->get<PositionComponent>()->width;
    }

    // ── ButtonSystem ───────────────────────────────────────────────────────────
    ButtonSystem::ButtonSystem(const Tokens* tokens) : tokens(tokens) {}

    void ButtonSystem::init()
    {
        auto group = registerGroup<ButtonState>();

        group->addOnGroup([this](EntityRef entity) {
            // Participate in hover (no-op callables; we react to HoverChangedEvent), and record
            // the face in tab order.
            if (not entity->has<MouseEnterComponent>())
                ecsRef->attach<MouseEnterComponent>(entity, makeCallable<ButtonNoOp>(ButtonNoOp{}));
            if (not entity->has<MouseLeaveComponent>())
                ecsRef->attach<MouseLeaveComponent>(entity, makeCallable<ButtonNoOp>(ButtonNoOp{}));
        });
    }

    void ButtonSystem::applyVisual(EntityRef face)
    {
        auto* paint = ecsRef->getSystem<PaintSystem>();
        auto st = face->get<ButtonState>();
        const bool enabled = not st->disabled;
        const float alpha = st->disabled ? tokens->opacity("opacity-locked") : 1.0f;

        if (auto g = ecsRef->getEntity(st->ground))
            paint->paint(g, groundToken(st->variant, st->hovered, enabled), alpha);
        if (auto f = ecsRef->getEntity(st->frame))
            paint->paint(f, frameToken(st->variant), alpha);
        if (st->sheen)
            if (auto s = ecsRef->getEntity(st->sheen))
                paint->paint(s, sheenToken(st->variant), (st->hovered and enabled) ? tokens->opacity("opacity-ghost") : 0.0f);
        if (st->ring)
            if (auto r = ecsRef->getEntity(st->ring))
                r->get<PositionComponent>()->setVisible(st->keyboardFocus and enabled);

        const std::string ink = inkToken(st->variant);
        for (auto id : st->inked)
            if (auto e = ecsRef->getEntity(id))
                paint->paint(e, (st->variant == ButtonVariant::Quiet and contains(st->costParts, id)) ? "status-time" : ink, alpha);

        if (st->reasonBox)
            if (auto rb = ecsRef->getEntity(st->reasonBox))
                rb->get<PositionComponent>()->setVisible(st->disabled);
    }

    void ButtonSystem::activate(EntityRef face)
    {
        auto st = face->get<ButtonState>();
        if (st->disabled)
            return;
        ecsRef->sendEvent(ButtonActivatedEvent{face->id, st->tag});
    }

    void ButtonSystem::onEvent(const HoverChangedEvent& event)
    {
        for (auto id : event.entered)
            if (auto e = ecsRef->getEntity(id); e and e->has<ButtonState>())
            {
                e->get<ButtonState>()->hovered = true;
                applyVisual(e);
            }
        for (auto id : event.left)
            if (auto e = ecsRef->getEntity(id); e and e->has<ButtonState>())
            {
                auto st = e->get<ButtonState>();
                st->hovered = false;
                if (st->pressed)
                    st->pressed = false;   // a drag-off cancels
                applyVisual(e);
            }
    }

    void ButtonSystem::onEvent(const OnMouseClick& event)
    {
        if (event.button != 1)
            return;
        // Ring hiding is FocusOrderSystem's job (KeyboardFocusChangedEvent); we only press.
        for (auto* st : view<ButtonState>())
        {
            auto face = ecsRef->getEntity(st->entityId);
            if (not face)
                continue;
            if (st->hovered and not st->disabled)
            {
                st->pressed = true;
                applyVisual(face);
            }
        }
    }

    void ButtonSystem::onEvent(const OnMouseRelease& event)
    {
        if (event.button != 1)
            return;
        for (auto* st : view<ButtonState>())
        {
            auto face = ecsRef->getEntity(st->entityId);
            if (not face or not st->pressed)
                continue;
            const bool inside = st->hovered;
            st->pressed = false;
            applyVisual(face);
            if (inside)
                activate(face);
        }
    }

    void ButtonSystem::onEvent(const OnSDLScanCode& event)
    {
        // Only activation; Tab traversal lives in FocusOrderSystem.
        if (event.key != SDL_SCANCODE_RETURN and event.key != SDL_SCANCODE_SPACE)
            return;

        auto* fo = ecsRef->getSystem<FocusOrderSystem>();
        if (not fo)
            return;
        if (auto face = ecsRef->getEntity(fo->current()); face and face->has<ButtonState>())
            activate(face);
    }

    void ButtonSystem::onEvent(const KeyboardFocusChangedEvent& event)
    {
        for (auto* st : view<ButtonState>())
        {
            auto face = ecsRef->getEntity(st->entityId);
            if (not face)
                continue;
            const bool me = (st->entityId == event.face) and event.keyboard;
            st->focused = (st->entityId == event.face);
            st->keyboardFocus = me;
            applyVisual(face);
        }
    }

    void ButtonSystem::onEvent(const ThemeChangedEvent&)
    {
        for (auto* st : view<ButtonState>())
            if (auto face = ecsRef->getEntity(st->entityId))
                applyVisual(face);
    }
}
