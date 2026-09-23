#include "tabs.h"

#ifdef __linux__
#include <SDL2/SDL.h>
#elif _WIN32
#include <SDL.h>
#endif

#include <algorithm>
#include <string>

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
        constexpr const char * const DOM = "Chronicle.Tabs";

        constexpr float ROW_H = 44.0f;      // space-2 + tab line + space-3 (8 + 24 + 12)
        constexpr float PAD_TOP = 8.0f;     // space-2
        constexpr float TAB_LINE = 24.0f;   // tab line height
        constexpr float GAP = 16.0f;        // space-4 between tabs
        constexpr float GLYPH = 16.0f;      // S16
        constexpr float GLYPH_GAP = 8.0f;   // space-2
        constexpr float BADGE_GAP = 8.0f;   // space-2 before the badge
        constexpr float BADGE_PAD = 4.0f;   // space-1 side padding
        constexpr float BADGE_H = 15.0f;    // caption line height
        constexpr float UNDERLINE = 3.0f;   // border-frame

        struct TabsNoOp {};

        std::string ink(bool lit) { return lit ? "ink" : "ink-muted"; }
    }

    Tabs makeTabs(EntitySystem* ecs, const Tokens& tokens, const TextStyles& styles, const TabsSpec& spec)
    {
        auto* ttf = ecs->getSystem<TTFTextSystem>();
        const int z = spec.z;

        if (spec.items.size() > 6)
        {
            static bool warned = false;
            if (not warned) { LOG_WARNING(DOM, "Tabs: more than six items"); warned = true; }
        }

        Tabs t;
        t.spec = spec;

        auto root = makeAnchoredPrefab(ecs, 0.0f, 0.0f, static_cast<float>(z));
        root.get<PositionComponent>()->setHeight(ROW_H);
        const _unique_id rootId = root.id;
        t.root = root.entity;

        const TextStyle& tabStyle = styles.get("tab");
        const TextStyle& capStyle = styles.get("caption");

        float x = 0.0f;
        for (size_t i = 0; i < spec.items.size(); ++i)
        {
            const TabItem& item = spec.items[i];
            const bool active = (static_cast<int>(i) == spec.active);
            const bool hasGlyph = not item.glyph.empty();
            const bool hasBadge = item.badge > 0;

            const float glyphAdvance = hasGlyph ? GLYPH + GLYPH_GAP : 0.0f;
            const float labelW = ttf->measureText(tabStyle.fontAlias, item.label, 1.0f, 0.0f, 0.0f, tabStyle.letterSpacingPx).width;
            float badgeTextW = 0.0f, badgeFrameW = 0.0f, badgeAdvance = 0.0f;
            if (hasBadge)
            {
                badgeTextW = ttf->measureText(capStyle.fontAlias, std::to_string(item.badge), 1.0f, 0.0f, 0.0f, capStyle.letterSpacingPx).width;
                badgeFrameW = badgeTextW + 2.0f * BADGE_PAD;
                badgeAdvance = BADGE_GAP + badgeFrameW;
            }
            const float faceW = glyphAdvance + labelW + badgeAdvance;

            Tabs::Tab tab;

            // face
            auto face = ecs->createEntity();
            auto fp = ecs->attach<PositionComponent>(face);
            fp->setWidth(faceW); fp->setHeight(ROW_H); fp->setZ(static_cast<float>(z));
            auto fa = ecs->attach<UiAnchor>(face);
            fa->setLeftAnchor(PosAnchor{rootId, AnchorType::Left}); fa->setLeftMargin(x);
            fa->setTopAnchor(PosAnchor{rootId, AnchorType::Top});
            ecs->attach<FocusableComponent>(face);
            auto st = ecs->attach<TabState>(face);
            st->tabs = rootId; st->index = static_cast<int>(i); st->tag = spec.tag; st->active = active;
            const _unique_id faceId = face.id;
            tab.face = face;
            root.get<Prefab>()->addToPrefab(face);
            if (auto* fo = ecs->getSystem<FocusOrderSystem>())
                fo->add(faceId);

            // glyph
            if (hasGlyph)
            {
                Mark m = makeMark(ecs, tokens, {item.glyph, MarkSize::S16, ink(active), z + 2});
                auto ma = m.entity->get<UiAnchor>();
                ma->setLeftAnchor(PosAnchor{faceId, AnchorType::Left});
                ma->setTopAnchor(PosAnchor{faceId, AnchorType::Top}); ma->setTopMargin(PAD_TOP + (TAB_LINE - GLYPH) / 2.0f);
                ma->setZConstrain(PosConstrain{faceId, AnchorType::Z, PosOpType::Add, 2.0f});
                root.get<Prefab>()->addToPrefab(m.entity);
                tab.glyph = m;
                st->inked.push_back(m.entity.id);
            }

            // label
            LabelSpec ls; ls.style = "tab"; ls.text = item.label; ls.colour = ink(active); ls.z = z + 2;
            Label label = makeLabel(ecs, tokens, styles, ls);
            auto la = label.box->get<UiAnchor>();
            la->setLeftAnchor(PosAnchor{faceId, AnchorType::Left}); la->setLeftMargin(glyphAdvance);
            la->setTopAnchor(PosAnchor{faceId, AnchorType::Top}); la->setTopMargin(PAD_TOP);
            la->setZConstrain(PosConstrain{faceId, AnchorType::Z, PosOpType::Add, 2.0f});
            root.get<Prefab>()->addToPrefab(label.box);
            tab.label = label;
            st->inked.push_back(label.text.id);

            // badge
            if (hasBadge)
            {
                const float bx = glyphAdvance + labelW + BADGE_GAP;
                auto frame = makeStrokeRect2DShape(ecs, badgeFrameW, BADGE_H, tokens.colour("rule-hair"), 1.0f);
                auto bfa = frame.get<UiAnchor>();
                bfa->setLeftAnchor(PosAnchor{faceId, AnchorType::Left}); bfa->setLeftMargin(bx);
                bfa->setTopAnchor(PosAnchor{faceId, AnchorType::Top}); bfa->setTopMargin(PAD_TOP + (TAB_LINE - BADGE_H) / 2.0f);
                bfa->setZConstrain(PosConstrain{faceId, AnchorType::Z, PosOpType::Add, 2.0f});
                ecs->attach<PaintComponent>(frame.entity, "rule-hair");
                root.get<Prefab>()->addToPrefab(frame.entity);
                tab.badgeFrame = frame.entity;

                LabelSpec bs; bs.style = "caption"; bs.text = std::to_string(item.badge); bs.colour = "ink-muted"; bs.z = z + 2;
                Label badge = makeLabel(ecs, tokens, styles, bs);
                auto ba = badge.box->get<UiAnchor>();
                ba->setLeftAnchor(PosAnchor{faceId, AnchorType::Left}); ba->setLeftMargin(bx + BADGE_PAD);
                ba->setTopAnchor(PosAnchor{faceId, AnchorType::Top}); ba->setTopMargin(PAD_TOP + (TAB_LINE - BADGE_H) / 2.0f);
                ba->setZConstrain(PosConstrain{faceId, AnchorType::Z, PosOpType::Add, 2.0f});
                root.get<Prefab>()->addToPrefab(badge.box);
                tab.badge = badge;
            }

            // underline (3 px, face-wide, at the face bottom; overlaps the rule by 1 px)
            auto underline = makeUiSimple2DShape(ecs, Shape2D::Square, 1.0f, UNDERLINE, tokens.colour("vermilion"));
            auto ua = underline.get<UiAnchor>();
            ua->setLeftAnchor(PosAnchor{faceId, AnchorType::Left});
            ua->setRightAnchor(PosAnchor{faceId, AnchorType::Right});
            ua->setBottomAnchor(PosAnchor{faceId, AnchorType::Bottom});
            ua->setZConstrain(PosConstrain{faceId, AnchorType::Z, PosOpType::Add, 1.0f});
            underline.get<PositionComponent>()->setHeight(UNDERLINE);
            underline.get<PositionComponent>()->setVisible(active);
            ecs->attach<PaintComponent>(underline.entity, "vermilion");
            root.get<Prefab>()->addToPrefab(underline.entity);
            tab.underline = underline.entity;
            st->underline = underline.entity.id;

            // ring (square, 2 px focus-ink, face + 8, at -4)
            auto ring = makeStrokeRect2DShape(ecs, 1.0f, 1.0f, tokens.colour("focus-ink"), tokens.border("border-rule"));
            auto ra = ring.get<UiAnchor>();
            ra->setLeftAnchor(PosAnchor{faceId, AnchorType::Left});     ra->setLeftMargin(-4.0f);
            ra->setRightAnchor(PosAnchor{faceId, AnchorType::Right});   ra->setRightMargin(-4.0f);
            ra->setTopAnchor(PosAnchor{faceId, AnchorType::Top});       ra->setTopMargin(-4.0f);
            ra->setBottomAnchor(PosAnchor{faceId, AnchorType::Bottom}); ra->setBottomMargin(-4.0f);
            ra->setZConstrain(PosConstrain{faceId, AnchorType::Z, PosOpType::Add, 1.0f});
            ring.get<PositionComponent>()->setVisible(false);
            ecs->attach<PaintComponent>(ring.entity, "focus-ink");
            root.get<Prefab>()->addToPrefab(ring.entity);
            tab.ring = ring.entity;
            st->ring = ring.entity.id;

            t.tabs.push_back(tab);
            x += faceW + GAP;
        }

        const float contentW = spec.items.empty() ? 0.0f : x - GAP;
        const float rowW = spec.width > 0.0f ? spec.width : contentW;
        root.get<PositionComponent>()->setWidth(rowW);

        // the row rule at the bottom
        auto rule = makeUiSimple2DShape(ecs, Shape2D::Square, rowW, 1.0f, tokens.colour("rule-ruled"));
        auto rla = rule.get<UiAnchor>();
        rla->setLeftAnchor(PosAnchor{rootId, AnchorType::Left});
        rla->setTopAnchor(PosAnchor{rootId, AnchorType::Top}); rla->setTopMargin(ROW_H - 1.0f);
        rla->setZConstrain(PosConstrain{rootId, AnchorType::Z});
        rule.get<PositionComponent>()->setWidth(rowW);
        ecs->attach<PaintComponent>(rule.entity, "rule-ruled");
        root.get<Prefab>()->addToPrefab(rule.entity);
        t.rule = rule.entity;

        return t;
    }

    int Tabs::active() const { return spec.active; }

    void Tabs::setActive(EntitySystem* ecs, int index)
    {
        spec.active = index;
        auto* sys = ecs->getSystem<TabsSystem>();
        for (size_t i = 0; i < tabs.size(); ++i)
        {
            tabs[i].face->get<TabState>()->active = (static_cast<int>(i) == index);
            if (sys)
                sys->applyVisual(tabs[i].face);
        }
    }

    void Tabs::setBadge(EntitySystem* ecs, const TextStyles& styles, int index, int count)
    {
        if (index < 0 or index >= static_cast<int>(tabs.size()))
            return;
        Tab& tab = tabs[index];

        const bool had = tab.badge.has_value();
        if (count <= 0)
        {
            if (had)
            {
                ecs->removeEntity(tab.badge->box.id);
                ecs->removeEntity(tab.badgeFrame.id);
                tab.badge.reset();
                tab.badgeFrame = EntityRef{};
                // Narrow the face back to glyph + label.
                const float glyphAdvance = tab.glyph ? GLYPH + GLYPH_GAP : 0.0f;
                const float labelW = tab.label.box->get<PositionComponent>()->width;
                tab.face->get<PositionComponent>()->setWidth(glyphAdvance + labelW);
            }
            return;
        }

        if (had)
        {
            tab.badge->setText(ecs, styles, std::to_string(count));
            const TextStyle& capStyle = styles.get("caption");
            const float badgeTextW = ecs->getSystem<TTFTextSystem>()
                ->measureText(capStyle.fontAlias, std::to_string(count), 1.0f, 0.0f, 0.0f, capStyle.letterSpacingPx).width;
            const float badgeFrameW = badgeTextW + 2.0f * BADGE_PAD;
            tab.badgeFrame->get<PositionComponent>()->setWidth(badgeFrameW);

            const float glyphAdvance = tab.glyph ? GLYPH + GLYPH_GAP : 0.0f;
            const float labelW = tab.label.box->get<PositionComponent>()->width;
            tab.face->get<PositionComponent>()->setWidth(glyphAdvance + labelW + BADGE_GAP + badgeFrameW);
        }
        // Adding a badge where none existed is a rebuild; the gallery rebuilds instead.
    }

    // ── TabsSystem ─────────────────────────────────────────────────────────────
    TabsSystem::TabsSystem(const Tokens* tokens) : tokens(tokens) {}

    void TabsSystem::init()
    {
        auto group = registerGroup<TabState>();
        group->addOnGroup([this](EntityRef entity) {
            if (not entity->has<MouseEnterComponent>())
                ecsRef->attach<MouseEnterComponent>(entity, makeCallable<TabsNoOp>(TabsNoOp{}));
            if (not entity->has<MouseLeaveComponent>())
                ecsRef->attach<MouseLeaveComponent>(entity, makeCallable<TabsNoOp>(TabsNoOp{}));
        });
    }

    void TabsSystem::applyVisual(EntityRef face)
    {
        auto st = face->get<TabState>();
        const bool lit = st->hovered or st->active;

        if (auto u = ecsRef->getEntity(st->underline))
            u->get<PositionComponent>()->setVisible(st->active);
        if (auto r = ecsRef->getEntity(st->ring))
            r->get<PositionComponent>()->setVisible(st->keyboardFocus);
        for (auto id : st->inked)
            if (auto e = ecsRef->getEntity(id))
                e->get<PaintComponent>()->setToken(ink(lit));
    }

    void TabsSystem::select(EntityRef face)
    {
        auto st = face->get<TabState>();
        if (st->active)
            return;
        const _unique_id row = st->tabs;
        const int index = st->index;
        const std::string tag = st->tag;

        for (auto* other : view<TabState>())
            if (other->tabs == row)
                if (auto f = ecsRef->getEntity(other->entityId))
                {
                    other->active = (other->entityId == face->id);
                    applyVisual(f);
                }
        ecsRef->sendEvent(TabSelectedEvent{row, tag, index});
    }

    void TabsSystem::onEvent(const HoverChangedEvent& event)
    {
        for (auto id : event.entered)
            if (auto e = ecsRef->getEntity(id); e and e->has<TabState>())
            {
                e->get<TabState>()->hovered = true;
                applyVisual(e);
            }
        for (auto id : event.left)
            if (auto e = ecsRef->getEntity(id); e and e->has<TabState>())
            {
                auto st = e->get<TabState>();
                st->hovered = false;
                st->pressed = false;
                applyVisual(e);
            }
    }

    void TabsSystem::onEvent(const OnMouseClick& event)
    {
        if (event.button != 1)
            return;
        for (auto* st : view<TabState>())
            if (st->hovered)
                st->pressed = true;
    }

    void TabsSystem::onEvent(const OnMouseRelease& event)
    {
        if (event.button != 1)
            return;
        for (auto* st : view<TabState>())
        {
            if (not st->pressed)
                continue;
            const bool inside = st->hovered;
            st->pressed = false;
            if (inside)
                if (auto f = ecsRef->getEntity(st->entityId))
                    select(f);
        }
    }

    void TabsSystem::onEvent(const OnSDLScanCode& event)
    {
        auto* fo = ecsRef->getSystem<FocusOrderSystem>();
        if (not fo)
            return;
        auto face = ecsRef->getEntity(fo->current());
        if (not face or not face->has<TabState>())
            return;

        if (event.key == SDL_SCANCODE_RETURN or event.key == SDL_SCANCODE_SPACE)
        {
            select(face);
            return;
        }

        const bool left = (event.key == SDL_SCANCODE_LEFT);
        const bool right = (event.key == SDL_SCANCODE_RIGHT);
        if (not left and not right)
            return;

        auto st = face->get<TabState>();
        const _unique_id row = st->tabs;
        const int target = st->index + (right ? 1 : -1);

        for (auto* other : view<TabState>())
            if (other->tabs == row and other->index == target)
                if (auto f = ecsRef->getEntity(other->entityId))
                {
                    fo->focus(other->entityId);   // move keyboard focus (keeps FocusOrder in sync)
                    select(f);
                }
    }

    void TabsSystem::onEvent(const KeyboardFocusChangedEvent& event)
    {
        for (auto* st : view<TabState>())
        {
            auto face = ecsRef->getEntity(st->entityId);
            if (not face)
                continue;
            st->keyboardFocus = (st->entityId == event.face) and event.keyboard;
            applyVisual(face);
        }
    }

    void TabsSystem::onEvent(const ThemeChangedEvent&)
    {
        for (auto* st : view<TabState>())
            if (auto face = ecsRef->getEntity(st->entityId))
                applyVisual(face);
    }
}
