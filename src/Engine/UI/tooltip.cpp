#include "stdafx.h"

#include "tooltip.h"

#include "ECS/callable.h"

#include "2D/simple2dobject.h"
#include "UI/prefab.h"
#include "UI/ttftext.h"

namespace pg
{
    namespace
    {
        constexpr const char * const DOM = "Tooltip System";

        // Cursor offset and screen margin for placement, matching common tooltip feel.
        constexpr float CURSOR_OFFSET_X = 14.0f;
        constexpr float CURSOR_OFFSET_Y = 18.0f;
        constexpr float SCREEN_MARGIN = 4.0f;
        constexpr float PADDING = 8.0f;
        constexpr float TOOLTIP_Z = 200.0f;

        // No-op event so the auto-attached hover components carry a valid callable.
        struct TooltipNoOp {};
    }

    void TooltipSystem::init()
    {
        LOG_THIS_MEMBER(DOM);

        auto group = registerGroup<TooltipComponent, PositionComponent>();

        group->addOnGroup([this](EntityRef entity) {
            // The entity must participate in hover; attach no-op hover components if absent.
            if (not entity->has<MouseEnterComponent>())
                ecsRef->attach<MouseEnterComponent>(entity, makeCallable<TooltipNoOp>(TooltipNoOp{}));

            if (not entity->has<MouseLeaveComponent>())
                ecsRef->attach<MouseLeaveComponent>(entity, makeCallable<TooltipNoOp>(TooltipNoOp{}));
        });
    }

    void TooltipSystem::registerStyle(const std::string& name, TooltipBuilder builder)
    {
        builders[name] = std::move(builder);
    }

    void TooltipSystem::setDefaultFont(const std::string& fontAlias, float scale)
    {
        defaultFont = fontAlias;
        defaultScale = scale;

        registerStyle("default", [this](EntitySystem& ecs, const TooltipComponent& tip) -> EntityRef
        {
            float textWidth = 0.0f;
            float textHeight = 0.0f;

            auto* ttf = ecs.getSystem<TTFTextSystem>();
            if (ttf)
            {
                const TextMetrics metrics = ttf->measureText(defaultFont, tip.text, defaultScale);
                textWidth = metrics.width;
                textHeight = metrics.height;
            }

            const float boxWidth = textWidth + 2.0f * PADDING;
            const float boxHeight = textHeight + 2.0f * PADDING;

            auto root = makeAnchoredPrefab(&ecs, 0.0f, 0.0f, 0.0f);
            root.get<PositionComponent>()->setWidth(boxWidth);
            root.get<PositionComponent>()->setHeight(boxHeight);
            auto prefab = root.get<Prefab>();

            // Neutral folio-ish background; games register their own style for a themed look.
            auto background = makeRoundedRect2DShape(&ecs, 3.0f, boxWidth, boxHeight, {240.0f, 240.0f, 240.0f, 255.0f});
            background.get<UiAnchor>()->fillIn(*root.get<UiAnchor>());
            background.get<UiAnchor>()->setZConstrain(PosConstrain{root.id, AnchorType::Z, PosOpType::Add, 1.0f});
            prefab->addToPrefab(background, "BG");

            auto label = makeTTFText(&ecs, 0.0f, 0.0f, 0.0f, defaultFont, tip.text, defaultScale, {20.0f, 20.0f, 20.0f, 255.0f});
            auto labelAnchor = label.get<UiAnchor>();
            labelAnchor->setTopAnchor(PosAnchor{root.id, AnchorType::Top});
            labelAnchor->setLeftAnchor(PosAnchor{root.id, AnchorType::Left});
            labelAnchor->setTopMargin(PADDING);
            labelAnchor->setLeftMargin(PADDING);
            labelAnchor->setZConstrain(PosConstrain{root.id, AnchorType::Z, PosOpType::Add, 2.0f});
            prefab->addToPrefab(label, "Text");

            return root.entity;
        });
    }

    void TooltipSystem::onEvent(const HoverChangedEvent& event)
    {
        LOG_THIS_MEMBER(DOM);

        // 1. Leaving the shown or pending target cancels it.
        for (const auto& id : event.left)
        {
            if (id == shownFor)
                hide();

            if (id == shownFor or id == pendingTarget)
            {
                pendingTarget = 0;
                pendingMs = 0;
            }
        }

        // 2. Entering a tooltipped entity starts a fresh pending timer.
        for (const auto& id : event.entered)
        {
            auto entity = ecsRef->getEntity(id);

            if (entity and entity->has<TooltipComponent>())
            {
                pendingTarget = id;
                pendingMs = 0;
                lastPos = event.pos;
                break;
            }
        }

        // 3. Track the cursor: re-place while showing, otherwise just remember it.
        if (shownFor != 0)
        {
            lastPos = event.pos;
            place(current, event.pos);
        }
        else if (pendingTarget != 0)
        {
            lastPos = event.pos;
        }
    }

    void TooltipSystem::onEvent(const OnMouseClick&)
    {
        LOG_THIS_MEMBER(DOM);

        hide();

        pendingTarget = 0;
        pendingMs = 0;
    }

    void TooltipSystem::onEvent(const ResizeEvent& event)
    {
        screenWidth = event.width;
        screenHeight = event.height;
    }

    void TooltipSystem::onProcessEvent(const TickEvent& event)
    {
        if (pendingTarget == 0)
            return;

        pendingMs += static_cast<uint32_t>(event.tick);

        auto entity = ecsRef->getEntity(pendingTarget);
        if (not entity or not entity->has<TooltipComponent>())
        {
            pendingTarget = 0;
            pendingMs = 0;
            return;
        }

        if (pendingMs >= entity->get<TooltipComponent>()->delayMs)
        {
            show(pendingTarget, lastPos);

            pendingTarget = 0;
            pendingMs = 0;
        }
    }

    void TooltipSystem::show(_unique_id target, Point2D at)
    {
        LOG_THIS_MEMBER(DOM);

        auto entity = ecsRef->getEntity(target);
        if (not entity or not entity->has<TooltipComponent>())
            return;

        auto tip = entity->get<TooltipComponent>();

        auto it = builders.find(tip->style);
        if (it == builders.end())
            it = builders.find("default");

        if (it == builders.end())
        {
            LOG_ERROR(DOM, "No tooltip builder for style '" << tip->style << "' and no default registered");
            return;
        }

        // Any previous tooltip is replaced.
        hide();

        current = it->second(*ecsRef, *tip);
        shownFor = target;

        // Tooltips live in the Tooltip z band, on the target's own viewport.
        if (not current.empty() and current->has<ViewportComponent>() and entity->has<ViewportComponent>())
            current->get<ViewportComponent>()->viewport = entity->get<ViewportComponent>()->viewport;

        place(current, at);
    }

    void TooltipSystem::hide()
    {
        if (not current.empty())
            ecsRef->removeEntity(current.id);

        current = EntityRef{};
        shownFor = 0;
    }

    void TooltipSystem::place(EntityRef root, Point2D at)
    {
        if (root.empty() or not root->has<PositionComponent>())
            return;

        auto pos = root->get<PositionComponent>();
        const float width = pos->width;
        const float height = pos->height;

        float x = at.x + CURSOR_OFFSET_X;
        float y = at.y + CURSOR_OFFSET_Y;

        // Clamp to the right/bottom edges.
        if (screenWidth > 0.0f and x + width > screenWidth - SCREEN_MARGIN)
            x = screenWidth - SCREEN_MARGIN - width;

        if (screenHeight > 0.0f and y + height > screenHeight - SCREEN_MARGIN)
            y = at.y - CURSOR_OFFSET_Y - height;   // flip above the cursor

        // Keep it on-screen at the top/left.
        if (x < SCREEN_MARGIN)
            x = SCREEN_MARGIN;
        if (y < SCREEN_MARGIN)
            y = SCREEN_MARGIN;

        pos->setX(x);
        pos->setY(y);
        pos->setZ(TOOLTIP_Z);
    }
}
