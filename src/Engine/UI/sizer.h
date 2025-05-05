#pragma once

#include "ECS/system.h"

#include "2D/texture.h"

namespace pg
{
    enum class LayoutOrientation
    {
        Horizontal,
        Vertical
    };

    struct ClearLayoutEvent
    {
        _unique_id id; LayoutOrientation orientation;
    };

    struct AddLayoutElementEvent
    {
        _unique_id id; _unique_id ui; LayoutOrientation orientation;
    };

    struct RemoveLayoutElementEvent
    {
        _unique_id id; _unique_id index; LayoutOrientation orientation;
    };

    struct UpdateLayoutVisibility
    {
        _unique_id id; bool visible; LayoutOrientation orientation;
    };

    struct UpdateLayoutScrollable
    {
        _unique_id id; bool scrollable; LayoutOrientation orientation;
    };

    template <typename Layout>
    size_t getNbVisibleElementsInLayout(Layout layout)
    {
        size_t nb = 0;

        for (auto& ent : layout->entities)
        {
            if (ent->template has<PositionComponent>())
            {
                auto pos = ent->template get<PositionComponent>();

                if (pos->visible)
                {
                    nb++;
                }
            }
        }

        return nb;
    }

    struct BaseLayout : public Ctor
    {
        virtual void onCreation(EntityRef entity) override
        {
            id = entity.id;
            ecsRef = entity.ecsRef;
        }

        void addEntity(EntityRef entity)
        {
            ecsRef->sendEvent(AddLayoutElementEvent{id, entity.id, orientation});
        }

        void removeEntity(EntityRef entity)
        {
            ecsRef->sendEvent(RemoveLayoutElementEvent{id, entity.id, orientation});
        }

        void removeEntity(_unique_id entityId)
        {
            ecsRef->sendEvent(RemoveLayoutElementEvent{id, entityId, orientation});
        }

        void setVisibility(bool vis)
        {
            ecsRef->sendEvent(UpdateLayoutVisibility{id, vis, orientation});
        }

        void setScrollable(bool scrollable)
        {
            if (this->scrollable != scrollable)
            {
                this->scrollable = scrollable;

                ecsRef->sendEvent(UpdateLayoutScrollable{id, scrollable, orientation});
            }
        }

        void clear()
        {
            ecsRef->sendEvent(ClearLayoutEvent{id, orientation});
        }

        bool fitToAxis = false;
        bool spaced = false;
        size_t spacing = 0;
        bool visible = true;

        // Scrollbar parameters
        EntityRef horizontalScrollBar, verticalScrollBar;
        float xOffset = 0.0f, yOffset = 0.0f;
        float contentWidth = 0.0f, contentHeight = 0.0f;
        float scrollSpeed = 25.0f;

        bool stickToEnd = false;

        // Private

        LayoutOrientation orientation = LayoutOrientation::Horizontal;

        bool scrollable = true;

        std::vector<EntityRef> entities;

        bool childrenAdded = false;

        _unique_id id;
        EntitySystem *ecsRef;
    };


    struct HorizontalLayout : public BaseLayout
    {
        HorizontalLayout() : BaseLayout()
        {
            orientation = LayoutOrientation::Horizontal;
        }
    };

    struct VerticalLayout : public BaseLayout
    {
        VerticalLayout() : BaseLayout()
        {
            orientation = LayoutOrientation::Vertical;
        }
    };

    struct LayoutSystem : public System<
        Listener<StandardEvent>,
        Listener<EntityChangedEvent>,
        Listener<AddLayoutElementEvent>,
        Listener<RemoveLayoutElementEvent>,
        Listener<UpdateLayoutVisibility>,
        Listener<ClearLayoutEvent>,
        Listener<UpdateLayoutScrollable>,
        Own<HorizontalLayout>,
        Own<VerticalLayout>,
        InitSys>
    {
        virtual std::string getSystemName() const override { return "Layout System"; }

        virtual void init() override;

        virtual void onEvent(const StandardEvent& event) override;

        virtual void onEvent(const AddLayoutElementEvent& event) override
        {
            addQueue.push(event);
        }

        virtual void onEvent(const RemoveLayoutElementEvent& event) override
        {
            removeQueue.push(event);
        }

        virtual void onEvent(const UpdateLayoutVisibility& event) override
        {
            visibilityQueue.push(event);
        }

        virtual void onEvent(const ClearLayoutEvent& event) override
        {
            clearQueue.push(event);
        }

        virtual void onEvent(const EntityChangedEvent& event) override
        {
            changedEntities.push(event);
        }

        virtual void onEvent(const UpdateLayoutScrollable& event) override
        {
            scrollableQueue.push(event);
        }

        virtual void execute() override;

        void processScroll();

        /**
         * Helper function for processScroll.
         *
         * If scrollable is true, a MouseWheelComponent is attached to the entity and all its children are
         * made to be clipped to the entity. If scrollable is false, the MouseWheelComponent is detached
         * and the children are no longer clipped to the entity.
         *
         * If the entity has a horizontal or vertical scrollbar and the scrollbar is visible, the
         * scrollbar is hidden.
         *
         * @param entity The entity to process.
         * @param view The layout view of the entity.
         * @param scrollable If true, the entity is scrollable, otherwise it is not.
         */
        void processScrollHelper(Entity* entity, BaseLayout* view, bool scrollable);

        void processClear();

        void processVisibility();

        void processAdd();

        void processRemove();

        void processChanged();

        void recalculateChildrenPos(EntityRef viewEnt, BaseLayout* view);

        void addEntity(EntityRef viewEnt, _unique_id ui, LayoutOrientation orientation);

        void removeEntity(BaseLayout* view, _unique_id index);

        void updateVisibility(EntityRef viewEnt, BaseLayout* view, bool visible);

        void clear(BaseLayout* view);

        std::queue<AddLayoutElementEvent> addQueue;
        std::queue<RemoveLayoutElementEvent> removeQueue;
        std::queue<ClearLayoutEvent> clearQueue;
        std::queue<UpdateLayoutVisibility> visibilityQueue;
        std::queue<EntityChangedEvent> changedEntities;
        std::queue<UpdateLayoutScrollable> scrollableQueue;

        std::set<EntityRef> layoutUpdate;

        std::set<_unique_id> entitiesInLayout;
    };

    template <typename Type>
    CompList<PositionComponent, UiAnchor, HorizontalLayout> makeHorizontalLayout(Type *ecs, float x, float y, float width, float height, bool scrollable = false)
    {
        auto entity = ecs->createEntity();

        auto view = ecs->template attach<HorizontalLayout>(entity);

        view->scrollable = scrollable;

        auto ui = ecs->template attach<PositionComponent>(entity);

        auto anchor = ecs->template attach<UiAnchor>(entity);

        ui->setX(x);
        ui->setY(y);
        ui->setWidth(width);
        ui->setHeight(height);

        return {entity, ui, anchor, view};
    }

    template <typename Type>
    CompList<PositionComponent, UiAnchor, VerticalLayout> makeVerticalLayout(Type *ecs, float x, float y, float width, float height, bool scrollable = false)
    {
        auto entity = ecs->createEntity();

        auto view = ecs->template attach<VerticalLayout>(entity);

        view->scrollable = scrollable;

        auto ui = ecs->template attach<PositionComponent>(entity);

        auto anchor = ecs->template attach<UiAnchor>(entity);

        ui->setX(x);
        ui->setY(y);
        ui->setWidth(width);
        ui->setHeight(height);

        return {entity, ui, anchor, view};
    }

}