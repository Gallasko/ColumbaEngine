#pragma once

#include "ECS/system.h"

#include "2D/position.h"

#include "Input/inputcomponent.h"

namespace pg {

    // ScrollableComponent – attach this to any UI entity with a LayoutComponent.
    struct ScrollableComponent
    {
        // The entity id of the inner layout (scrollable content)
        _unique_id innerLayoutId = 0;

        // Current scroll offset (in pixels).
        // For vertical scrolling, this would be the number of pixels scrolled from the top.
        float scrollOffset = 0.0f;

        // Content size, computed from the inner layout.
        float contentHeight = 0.0f;
        float contentWidth  = 0.0f;

        // Orientation (by default vertical scrolling).
        enum class Orientation : uint8_t { Vertical, Horizontal } orientation = Orientation::Vertical;

        // Multiplier for scrolling speed.
        float scrollSpeed = 1.0f;
    };


    struct ScrollableSystem : public System<Own<ScrollableComponent>, Listener<OnSDLMouseWheel>, InitSys, SaveSys>
    {
        virtual std::string getSystemName() const override { return "Scrollable System"; }

        // virtual void init() override
        // {
        //     // Register group for all entities with a ScrollableComponent.
        //     // (Assuming that such entities also have a LayoutComponent or UiAnchor on their inner layout.)
        //     addListenerToStandardEvent("mouse_wheel");

        //     // If necessary, you could register this system to a specific group here.
        // }

        // virtual void onEvent(const OnSDLMouseWheel& event) override
        // {
        //     // Iterate through all scrollable entities and update their scroll offset.
        //     for (auto scrollable : view<ScrollableComponent>())
        //     {
        //         // For vertical scrolling: assume event.y holds the scroll delta (positive for scrolling up).
        //         float delta = event.y * scrollable->scrollSpeed;
        //         scrollable->scrollOffset += delta;

        //         // Clamp the scroll offset (using a helper; you might need to know the viewport size).
        //         clampScroll(scrollable);

        //         // Update the inner layout margins to reflect the new offset.
        //         updateInnerLayoutMargin(scrollable);
        //     }
        // }

        // virtual void execute() override
        // {
        //     // Optionally, recalc content size from the inner layout.
        //     for (auto scrollable : view<ScrollableComponent>())
        //     {
        //         recalcContentSize(scrollable);
        //     }
        // }

        // virtual void save(Archive& archive) override
        // {
        //     // Save any necessary data.
        //     std::vector<ScrollableComponent> scrollables;
        //     for (const auto& s : view<ScrollableComponent>())
        //         scrollables.push_back(*s);
        //     serialize(archive, "scrollables", scrollables);
        // }

        // virtual void load(const UnserializedObject& serializedString) override
        // {
        //     std::vector<ScrollableComponent> scrollables;
        //     defaultDeserialize(serializedString, "scrollables", scrollables);
        //     for (const auto& s : scrollables)
        //     {
        //         auto ent = ecsRef->createEntity();
        //         ecsRef->attach<ScrollableComponent>(ent, s);
        //     }
        // }

    protected:
        // void clampScroll(ScrollableComponent* scrollable)
        // {
        //     // For vertical scrolling, assume we get the viewport height from some parent or container.
        //     // Replace 500.0f with your actual viewport height.
        //     float viewportHeight = 500.0f;

        //     if (scrollable->scrollOffset < 0)
        //         scrollable->scrollOffset = 0;
        //     else if (scrollable->scrollOffset > scrollable->contentHeight - viewportHeight)
        //         scrollable->scrollOffset = std::max(0.0f, scrollable->contentHeight - viewportHeight);
        // }

        // void updateInnerLayoutMargin(ScrollableComponent* scrollable)
        // {
        //     // Get the inner layout entity from its stored id.
        //     EntityRef innerEntity = ecsRef->getEntity(scrollable->innerLayoutId);
        //     if (innerEntity && innerEntity->has<UiAnchor>())
        //     {
        //         // Update the top margin to shift the content upward
        //         // (For example, moving the content by setting its top margin to -scrollOffset)
        //         innerEntity->get<UiAnchor>()->setTopMargin(-scrollable->scrollOffset);

        //         // You might need to trigger a reposition event for the inner layout.
        //         ecsRef->sendEvent(PositionComponentChangedEvent{scrollable->innerLayoutId});
        //     }
        // }

        // void recalcContentSize(ScrollableComponent* scrollable)
        // {
        //     // Assuming the inner layout entity has a LayoutComponent that computes contentHeight and contentWidth.
        //     EntityRef innerEntity = ecsRef->getEntity(scrollable->innerLayoutId);
        //     if (innerEntity && innerEntity->has<LayoutComponent>())
        //     {
        //         auto layout = innerEntity->get<LayoutComponent>();
        //         scrollable->contentHeight = layout->contentHeight;
        //         scrollable->contentWidth = layout->contentWidth;
        //     }
        // }
    };

} // namespace pg