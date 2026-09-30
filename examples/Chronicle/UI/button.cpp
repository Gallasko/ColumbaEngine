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

        std::string variantName(ButtonVariant v)
        {
            switch (v)
            {
            case ButtonVariant::Study: return "study";
            case ButtonVariant::Seal:  return "seal";
            case ButtonVariant::Quiet:
            default:                   return "quiet";
            }
        }

        // The theme defines "button.<variant>.<part>" for ground, frame, sheen, ink and cost, with
        // ".hover" leaves where a state differs and the root "disabled" mixin for the locked dim.
        std::string partElement(ButtonVariant v, const std::string& part, const std::string& state = "")
        {
            return "button." + variantName(v) + "." + part + state;
        }

        // The state suffix of a part: disabled dims, hover lights, otherwise the resting element.
        std::string stateSuffix(bool hovered, bool enabled)
        {
            if (not enabled)
                return ".disabled";

            return hovered ? ".hover" : "";
        }

        std::string monthsText(int months) { return std::to_string(months) + " mo"; }

        bool contains(const std::vector<_unique_id>& v, _unique_id id)
        {
            return std::find(v.begin(), v.end(), id) != v.end();
        }

        // Retarget an already-themed part; the ThemeComponent setter triggers the repaint.
        void rekey(EntityRef e, const std::string& element)
        {
            e->get<ThemeComponent>()->setElement(element);
        }
    }

    Button makeButton(EntitySystem* ecs, const ButtonSpec& spec)
    {
        auto* theme = ecs->getSystem<ThemeSystem>();

        const bool hasGlyph = not spec.glyph.empty();
        const bool hasCost = spec.months >= 0;
        const int z = spec.z;
        // The initial state, so a button that is born disabled draws dimmed before any event.
        const std::string s0 = stateSuffix(false, not spec.disabled);
        const std::string ink = partElement(spec.variant, "ink", s0);
        const std::string cost = partElement(spec.variant, "cost", s0);

        Button b;
        b.spec = spec;

        // Measure the label (and the cost figure) so the face can size to them.
        LabelSpec ls; ls.style = "control"; ls.text = spec.label; ls.z = z + 3;
        Label label = makeLabel(ecs, ls);
        const float labelW = label.entity->get<PositionComponent>()->width;

        float costTextW = 0.0f;
        std::optional<Label> costLabel;
        if (hasCost)
        {
            LabelSpec cs; cs.style = "tick"; cs.text = monthsText(spec.months); cs.z = z + 3;
            costLabel = makeLabel(ecs, cs);
            costTextW = costLabel->entity->get<PositionComponent>()->width;
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
        root.get<Prefab>()->addToPrefab(face, "face");

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

        // ground (rounded folio/tone), z. Colour and radius come from the theme element.
        auto ground = makeRoundedRect2DShape(ecs, theme->radius("radius-md"), 1.0f, 1.0f);
        ground.get<UiAnchor>()->fillIn(face->get<UiAnchor>());
        ground.get<UiAnchor>()->setZConstrain(PosConstrain{faceId, AnchorType::Z});
        ecs->attach<ThemeComponent>(ground.entity, partElement(spec.variant, "ground", s0));
        root.get<Prefab>()->addToPrefab(ground.entity);
        state->ground = ground.entity.id;

        // frame (1 px stroke, radius 3), z+1.
        auto frame = makeStrokeRect2DShape(ecs, 1.0f, 1.0f, {255.0f, 255.0f, 255.0f, 255.0f}, 1.0f);
        frame.get<UiAnchor>()->fillIn(face->get<UiAnchor>());
        frame.get<UiAnchor>()->setZConstrain(PosConstrain{faceId, AnchorType::Z, PosOpType::Add, 1.0f});
        ecs->attach<ThemeComponent>(frame.entity, partElement(spec.variant, "frame", s0));
        root.get<Prefab>()->addToPrefab(frame.entity);
        state->frame = frame.entity.id;

        // sheen (tonal only): a hover brighten/darken, alpha 0 until hovered.
        if (spec.variant != ButtonVariant::Quiet)
        {
            auto sheen = makeRoundedRect2DShape(ecs, theme->radius("radius-md"), 1.0f, 1.0f);
            sheen.get<UiAnchor>()->fillIn(face->get<UiAnchor>());
            sheen.get<UiAnchor>()->setZConstrain(PosConstrain{faceId, AnchorType::Z, PosOpType::Add, 1.0f});
            ecs->attach<ThemeComponent>(sheen.entity, partElement(spec.variant, "sheen"));
            root.get<Prefab>()->addToPrefab(sheen.entity);
            state->sheen = sheen.entity.id;
        }

        // focus ring: 2 px focus-ink, radius 5, 4 px outside the face, hidden until keyboard focus.
        auto ring = makeStrokeRect2DShape(ecs, 1.0f, 1.0f, {255.0f, 255.0f, 255.0f, 255.0f}, theme->border("border-rule"));
        {
            auto ra = ring.get<UiAnchor>();
            ra->setLeftAnchor(PosAnchor{faceId, AnchorType::Left});     ra->setLeftMargin(-4.0f);
            ra->setRightAnchor(PosAnchor{faceId, AnchorType::Right});   ra->setRightMargin(-4.0f);
            ra->setTopAnchor(PosAnchor{faceId, AnchorType::Top});       ra->setTopMargin(-4.0f);
            ra->setBottomAnchor(PosAnchor{faceId, AnchorType::Bottom}); ra->setBottomMargin(-4.0f);
            ra->setZConstrain(PosConstrain{faceId, AnchorType::Z, PosOpType::Add, 1.0f});
        }
        ring.get<PositionComponent>()->setVisible(false);
        ecs->attach<ThemeComponent>(ring.entity, "button.ring");
        root.get<Prefab>()->addToPrefab(ring.entity);
        state->ring = ring.entity.id;

        // face parts, z+2 (their texts z+3). Re-keyed to the variant's ink so a born-disabled button is dimmed.
        float x = PAD_X;
        if (hasGlyph)
        {
            Mark m = makeMark(ecs, {spec.glyph, MarkSize::S16, "ink", z + 2});
            anchorInFace(m.entity, x, 2, /*vcentre*/ true);
            rekey(m.entity, ink);
            b.glyph = m;
            state->inked.push_back(m.entity.id);
            x += GLYPH + GAP;
        }

        anchorInFace(label.entity, x, 3, /*vcentre*/ true);
        rekey(label.entity, ink);
        b.label = label;
        state->inked.push_back(label.entity.id);
        x += labelW;

        if (hasCost)
        {
            x += GAP;
            Mark cm = makeMark(ecs, {"time", MarkSize::S14, "ink", z + 2});
            anchorInFace(cm.entity, x, 2, /*vcentre*/ true);
            rekey(cm.entity, cost);
            b.costMark = cm;
            state->inked.push_back(cm.entity.id);
            state->costParts.push_back(cm.entity.id);
            x += COST_MARK + COST_GAP;

            anchorInFace(costLabel->entity, x, 3, /*vcentre*/ true);
            rekey(costLabel->entity, cost);
            b.cost = costLabel;
            state->inked.push_back(costLabel->entity.id);
            state->costParts.push_back(costLabel->entity.id);
        }

        // reason (created when there is reason text; shown only while disabled).
        if (hasReason)
        {
            LabelSpec rs; rs.style = "caption"; rs.text = spec.reason; rs.color = "ink-faint"; rs.z = z + 3;
            Label r = makeLabel(ecs, rs);
            auto ra = r.entity->get<UiAnchor>();
            ra->setLeftAnchor(PosAnchor{rootId, AnchorType::Left});
            ra->setTopAnchor(PosAnchor{faceId, AnchorType::Bottom}); ra->setTopMargin(REASON_GAP);
            ra->setZConstrain(PosConstrain{faceId, AnchorType::Z, PosOpType::Add, 3.0f});
            r.entity->get<PositionComponent>()->setVisible(spec.disabled);
            root.get<Prefab>()->addToPrefab(r.entity);
            b.reason = r;
            state->reasonBox = r.entity.id;
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
        if (not newReason.empty() and reason)
        {
            reason->setText(ecs, newReason);
            spec.reason = newReason;
        }
        // The reason band changes the root height.
        const bool showReason = disabled and reason.has_value();
        root->get<PositionComponent>()->setHeight(showReason ? FACE_H + REASON_GAP + REASON_H : FACE_H);
        ecs->getSystem<ButtonSystem>()->applyVisual(face);
    }

    void Button::setLabel(EntitySystem* ecs, const std::string& text)
    {
        label.setText(ecs, text);
        // Re-measure and regrow the face; the glyph/cost anchors reflow from the new label width.
        const float labelW = label.entity->get<PositionComponent>()->width;
        const bool hasGlyph = glyph.has_value();
        const bool hasCost = cost.has_value();
        float costTextW = hasCost ? cost->entity->get<PositionComponent>()->width : 0.0f;
        const float faceW = PAD_X + (hasGlyph ? GLYPH + GAP : 0.0f) + labelW
            + (hasCost ? GAP + COST_MARK + COST_GAP + costTextW : 0.0f) + PAD_X;
        root->get<PositionComponent>()->setWidth(faceW);

        // Shift the cost pair to follow the new label right edge.
        if (hasCost)
        {
            float x = PAD_X + (hasGlyph ? GLYPH + GAP : 0.0f) + labelW + GAP;
            costMark->entity->get<UiAnchor>()->setLeftMargin(x);
            x += COST_MARK + COST_GAP;
            cost->entity->get<UiAnchor>()->setLeftMargin(x);
        }
        spec.label = text;
    }

    void Button::setMonths(EntitySystem*, int months)
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
        auto st = face->get<ButtonState>();
        const bool enabled = not st->disabled;
        const std::string state = stateSuffix(st->hovered, enabled);

        if (auto g = ecsRef->getEntity(st->ground))
            rekey(g, partElement(st->variant, "ground", state));

        if (auto f = ecsRef->getEntity(st->frame))
            rekey(f, partElement(st->variant, "frame", state));

        // The sheen only lights on an enabled hover; it never dims, it is invisible at rest.
        if (st->sheen)
            if (auto s = ecsRef->getEntity(st->sheen))
                rekey(s, partElement(st->variant, "sheen", (st->hovered and enabled) ? ".hover" : ""));

        if (st->ring)
            if (auto r = ecsRef->getEntity(st->ring))
                r->get<PositionComponent>()->setVisible(st->keyboardFocus and enabled);

        for (auto id : st->inked)
            if (auto e = ecsRef->getEntity(id))
                rekey(e, partElement(st->variant, contains(st->costParts, id) ? "cost" : "ink", state));

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

            // A cancelled release (the press scrolled a list) is not a click
            if (inside and not event.cancelled)
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
        // The parts are keyed by state, so the theme system repaints them on its own; nothing to re-key.
    }
}
