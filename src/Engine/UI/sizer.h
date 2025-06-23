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
        std::vector<_unique_id> entityIds;
    };

    struct AddLayoutElementEvent
    {
        _unique_id id; _unique_id ui; LayoutOrientation orientation;
    };

    struct InsertLayoutElementEvent
    {
        _unique_id id; _unique_id ui; LayoutOrientation orientation; int index;
    };

    struct RemoveLayoutElementEvent
    {
        _unique_id id; _unique_id index; LayoutOrientation orientation;
    };

    struct RemoveLayoutElementAtEvent
    {
        _unique_id id; int index; LayoutOrientation orientation;
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

    struct BaseLayout : public Ctor, public Dtor
    {
        virtual void onCreation(EntityRef entity) override
        {
            id = entity.id;
            ecsRef = entity.ecsRef;
        }

        virtual void onDeletion(EntityRef) override
        {
            if (clearOnDeletion)
                clear();
        }

        void addEntity(EntityRef entity)
        {
            ecsRef->sendEvent(AddLayoutElementEvent{id, entity.id, orientation});
        }

        void insertEntity(EntityRef entity, int index)
        {
            ecsRef->sendEvent(InsertLayoutElementEvent{id, entity.id, orientation, index});
        }

        void removeEntity(EntityRef entity)
        {
            ecsRef->sendEvent(RemoveLayoutElementEvent{id, entity.id, orientation});
        }

        void removeEntity(_unique_id entityId)
        {
            ecsRef->sendEvent(RemoveLayoutElementEvent{id, entityId, orientation});
        }

        void removeAt(int index)
        {
            ecsRef->sendEvent(RemoveLayoutElementAtEvent{id, index, orientation});
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
            std::vector<_unique_id> entityIds;

            entityIds.reserve(entities.size());

            for (const auto& ent : entities)
            {
                entityIds.push_back(ent.id);
            }

            ecsRef->sendEvent(ClearLayoutEvent{entityIds});

            entities.clear();
        }

        bool fitToAxis = false;
        bool spaced = false;
        size_t spacing = 0;

        // Scrollbar parameters
        EntityRef horizontalScrollBar, verticalScrollBar;
        float xOffset = 0.0f, yOffset = 0.0f;
        float contentWidth = 0.0f, contentHeight = 0.0f;
        float scrollSpeed = 25.0f;

        bool stickToEnd = false;

        bool clearOnDeletion = true;

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
        QueuedListener<EntityChangedEvent>,
        QueuedListener<AddLayoutElementEvent>,
        QueuedListener<InsertLayoutElementEvent>,
        QueuedListener<RemoveLayoutElementEvent>,
        QueuedListener<RemoveLayoutElementAtEvent>,
        QueuedListener<ClearLayoutEvent>,
        QueuedListener<UpdateLayoutScrollable>,
        Own<HorizontalLayout>,
        Own<VerticalLayout>,
        InitSys>
    {
        virtual std::string getSystemName() const override { return "Layout System"; }

        virtual void init() override;

        virtual void onEvent(const StandardEvent& event) override;

        virtual void onProcessEvent(const AddLayoutElementEvent& event) override;

        virtual void onProcessEvent(const InsertLayoutElementEvent& event) override;

        virtual void onProcessEvent(const RemoveLayoutElementEvent& event) override;

        virtual void onProcessEvent(const RemoveLayoutElementAtEvent& event) override;

        virtual void onProcessEvent(const ClearLayoutEvent& event) override;

        virtual void onProcessEvent(const EntityChangedEvent& event) override;

        virtual void onProcessEvent(const UpdateLayoutScrollable& event) override;

        virtual void execute() override;

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

        // Todo calculate the lesser axis of the view (Biggest width for Vertical for example)
        void recalculateChildrenPos(EntityRef viewEnt, BaseLayout* view);

        void addEntity(EntityRef viewEnt, _unique_id ui, LayoutOrientation orientation, int index = -1);

        void removeEntity(BaseLayout* view, _unique_id index);

        void removeEntityAt(BaseLayout* view, int index);

        void updateVisibility(EntityRef viewEnt, BaseLayout* view);

        void clear(const std::vector<_unique_id>& entityIds);

        void updateLayout(EntityRef viewEnt, BaseLayout* view);

        void adjustOffsets(EntityRef viewEnt, BaseLayout* view, bool childrenAdded);

        void updateScrollBars(EntityRef viewEnt, BaseLayout* view);

        void updateHorizontalScrollBar(PositionComponent* viewUi, BaseLayout* view, PositionComponent* sbPos);

        void updateVerticalScrollBar(PositionComponent* viewUi, BaseLayout* view, PositionComponent* sbPos);

        void layoutWithoutSpacing(EntityRef viewEnt, BaseLayout* view);

        void layoutWithSpacing(EntityRef viewEnt, BaseLayout* view);

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