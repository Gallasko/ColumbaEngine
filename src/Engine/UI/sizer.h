#pragma once

#include "ECS/system.h"

#include "2D/texture.h"

namespace pg
{
    struct ClearHorizontalLayoutEvent
    {
        _unique_id id;
    };

    struct AddHorizontalLayoutElementEvent
    {
        _unique_id id; _unique_id ui;
    };

    struct RemoveHorizontalLayoutElementEvent
    {
        _unique_id id; size_t index;
    };

    struct UpdateHorizontalLayoutVisibility
    {
        _unique_id id; bool visible;
    };

    struct HorizontalLayout: public Ctor
    {
        void onCreation(EntityRef entity) override
        {
            id = entity.id;
            ecsRef = entity.ecsRef;
        }

        void addEntity(EntityRef entity) { ecsRef->sendEvent(AddHorizontalLayoutElementEvent{id, entity.id}); }

        void setVisibility(bool visible) { ecsRef->sendEvent(UpdateHorizontalLayoutVisibility{id, visible}); }

        void clear() { ecsRef->sendEvent(ClearHorizontalLayoutEvent{id}); }

        bool fitToWidth = false;

        bool spacedInWidth = false;

        size_t spacing = 0;

        bool visible = true;

        std::vector<EntityRef> entities;

        _unique_id id;

        EntitySystem *ecsRef;
    };

    struct HorizontalLayoutSystem : public System<
        Listener<EntityChangedEvent>,
        Listener<AddHorizontalLayoutElementEvent>,
        Listener<RemoveHorizontalLayoutElementEvent>,
        Listener<UpdateHorizontalLayoutVisibility>,
        Listener<ClearHorizontalLayoutEvent>,
        Own<HorizontalLayout>,
        InitSys>
    {
        virtual std::string getSystemName() const override { return "Horizontal Layout System"; }

        virtual void init() override;

        virtual void onEvent(const AddHorizontalLayoutElementEvent& event) override
        {
            eventQueue.push(event);
        }

        virtual void onEvent(const RemoveHorizontalLayoutElementEvent& /*event*/) override
        {
            // Todo
        }

        virtual void onEvent(const ClearHorizontalLayoutEvent& event) override
        {
            clearQueue.push(event);
        }

        virtual void onEvent(const UpdateHorizontalLayoutVisibility& event) override
        {
            visibilityQueue.push(event);
        }

        virtual void onEvent(const EntityChangedEvent& event) override
        {
            auto ent = ecsRef->getEntity(event.id);

            if (not ent)
            {
                return;
            }

            if (ent->has<HorizontalLayout>())
            {
                auto layout = ent->get<HorizontalLayout>();
                auto pos = ent->get<PositionComponent>();

                if (layout->visible != pos->visible)
                {
                    layout->visible = pos->visible;

                    updateVisibility(ent, pos->visible);
                }
            }
        }

        virtual void execute() override
        {
            while (not clearQueue.empty())
            {
                const auto& event = clearQueue.front();

                auto ent = ecsRef->getEntity(event.id);

                if (not (ent->has<HorizontalLayout>()))
                {
                    LOG_ERROR("HorizontalLayout", "Entity requested doesn't have a list view component !");
                    return;
                }

                clear(ent->get<HorizontalLayout>());

                clearQueue.pop();
            }

            while (not visibilityQueue.empty())
            {
                const auto& event = visibilityQueue.front();

                auto ent = ecsRef->getEntity(event.id);

                if (not (ent->has<HorizontalLayout>()))
                {
                    LOG_ERROR("HorizontalLayout", "Entity requested doesn't have a list view component!");
                    return;
                }

                updateVisibility(ent, event.visible);

                visibilityQueue.pop();
            }

            while (not eventQueue.empty())
            {
                const auto& event = eventQueue.front();

                auto ent = ecsRef->getEntity(event.id);

                if (not (ent->has<HorizontalLayout>()))
                {
                    LOG_ERROR("HorizontalLayout", "Entity requested doesn't have a list view component !");
                    return;
                }

                addEntity(ent, event.ui);

                eventQueue.pop();
            }
        }

        void addEntity(EntityRef viewEnt, _unique_id ui);

        void recalculateChildrenPos(EntityRef viewEnt);

        void updateVisibility(EntityRef viewEnt, bool visible);

        void clear(CompRef<HorizontalLayout> view);

        std::queue<AddHorizontalLayoutElementEvent> eventQueue;

        std::queue<ClearHorizontalLayoutEvent> clearQueue;

        std::queue<UpdateHorizontalLayoutVisibility> visibilityQueue;
    };

    template <typename Type>
    CompList<PositionComponent, UiAnchor, HorizontalLayout> makeHorizontalLayout(Type *ecs, float x, float y, float width, float height)
    {
        auto entity = ecs->createEntity();

        auto ui = ecs->template attach<PositionComponent>(entity);

        auto anchor = ecs->template attach<UiAnchor>(entity);

        auto view = ecs->template attach<HorizontalLayout>(entity);

        ui->setX(x);
        ui->setY(y);
        ui->setWidth(width);
        ui->setHeight(height);

        return {entity, ui, anchor, view};
    }
}