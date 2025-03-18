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
        _unique_id id; _unique_id index;
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

        void removeEntity(EntityRef entity) { ecsRef->sendEvent(RemoveHorizontalLayoutElementEvent{id, entity.id}); }
        void removeEntity(_unique_id entityId) { ecsRef->sendEvent(RemoveHorizontalLayoutElementEvent{id, entityId}); }

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

        virtual void onEvent(const RemoveHorizontalLayoutElementEvent& event) override
        {
            removeQueue.push(event);
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
                hLayoutUpdated.insert(ent);
                return;
            }

            for (auto v : view<HorizontalLayout>())
            {
                const auto& it = std::find_if(v->entities.begin(), v->entities.end(), [ent](const EntityRef& ref) { return ref.id == ent->id; });

                if (it != v->entities.end())
                {
                    hLayoutUpdated.insert(ecsRef->getEntity(v->id));
                    // An entity should not be in multple layouts at the same time
                    return;
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

                hLayoutUpdated.insert(ent);

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

                hLayoutUpdated.insert(ent);

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

                hLayoutUpdated.insert(ent);

                eventQueue.pop();
            }

            while (not removeQueue.empty())
            {
                const auto& event = removeQueue.front();

                auto ent = ecsRef->getEntity(event.id);

                if (not (ent->has<HorizontalLayout>()))
                {
                    LOG_ERROR("HorizontalLayout", "Entity requested doesn't have a list view component !");
                    return;
                }

                removeEntity(ent, event.index);

                hLayoutUpdated.insert(ent);

                removeQueue.pop();
            }

            for (auto ent : hLayoutUpdated)
            {
                recalculateChildrenPos(ent);

                auto view = ent->get<HorizontalLayout>();

                updateVisibility(ent, view->visible);
            }

            hLayoutUpdated.clear();
        }

        void addEntity(EntityRef viewEnt, _unique_id ui);

        void removeEntity(EntityRef viewEnt, _unique_id);

        void recalculateChildrenPos(EntityRef viewEnt);

        void updateVisibility(EntityRef viewEnt, bool visible);

        void clear(CompRef<HorizontalLayout> view);

        std::queue<AddHorizontalLayoutElementEvent> eventQueue;

        std::queue<RemoveHorizontalLayoutElementEvent> removeQueue;

        std::queue<ClearHorizontalLayoutEvent> clearQueue;

        std::queue<UpdateHorizontalLayoutVisibility> visibilityQueue;

        std::set<EntityRef> hLayoutUpdated;
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

    struct ClearVerticalLayoutEvent
    {
        _unique_id id;
    };

    struct AddVerticalLayoutElementEvent
    {
        _unique_id id; _unique_id ui;
    };

    struct RemoveVerticalLayoutElementEvent
    {
        _unique_id id; _unique_id index;
    };

    struct UpdateVerticalLayoutVisibility
    {
        _unique_id id; bool visible;
    };

    struct VerticalLayout: public Ctor
    {
        void onCreation(EntityRef entity) override
        {
            id = entity.id;
            ecsRef = entity.ecsRef;
        }

        void addEntity(EntityRef entity) { ecsRef->sendEvent(AddVerticalLayoutElementEvent{id, entity.id}); }

        void removeEntity(EntityRef entity) { ecsRef->sendEvent(RemoveVerticalLayoutElementEvent{id, entity.id}); }
        void removeEntity(_unique_id entityId) { ecsRef->sendEvent(RemoveVerticalLayoutElementEvent{id, entityId}); }

        void setVisibility(bool visible) { ecsRef->sendEvent(UpdateVerticalLayoutVisibility{id, visible}); }

        void clear() { ecsRef->sendEvent(ClearVerticalLayoutEvent{id}); }

        bool fitToHeight = false;

        bool spacedInHeight = false;

        size_t spacing = 0;

        bool visible = true;

        std::vector<EntityRef> entities;

        _unique_id id;

        EntitySystem *ecsRef;
    };

    struct VerticalLayoutSystem : public System<
        Listener<EntityChangedEvent>,
        Listener<AddVerticalLayoutElementEvent>,
        Listener<RemoveVerticalLayoutElementEvent>,
        Listener<UpdateVerticalLayoutVisibility>,
        Listener<ClearVerticalLayoutEvent>,
        Own<VerticalLayout>,
        InitSys>
    {
        virtual std::string getSystemName() const override { return "Vertical Layout System"; }

        virtual void init() override;

        virtual void onEvent(const AddVerticalLayoutElementEvent& event) override
        {
            eventQueue.push(event);
        }

        virtual void onEvent(const RemoveVerticalLayoutElementEvent& event) override
        {
            removeQueue.push(event);
        }

        virtual void onEvent(const ClearVerticalLayoutEvent& event) override
        {
            clearQueue.push(event);
        }

        virtual void onEvent(const UpdateVerticalLayoutVisibility& event) override
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

            if (ent->has<VerticalLayout>())
            {
                vLayoutUpdated.insert(ent);
                return;
            }

            for (auto v : view<VerticalLayout>())
            {
                const auto& it = std::find_if(v->entities.begin(), v->entities.end(), [ent](const EntityRef& ref) { return ref.id == ent->id; });

                if (it != v->entities.end())
                {
                    vLayoutUpdated.insert(ecsRef->getEntity(v->id));
                    // An entity should not be in multiple layouts at the same time
                    return;
                }
            }
        }

        virtual void execute() override
        {
            while (not clearQueue.empty())
            {
                const auto& event = clearQueue.front();

                auto ent = ecsRef->getEntity(event.id);

                if (not (ent->has<VerticalLayout>()))
                {
                    LOG_ERROR("VerticalLayout", "Entity requested doesn't have a list view component !");
                    return;
                }

                clear(ent->get<VerticalLayout>());

                vLayoutUpdated.insert(ent);

                clearQueue.pop();
            }

            while (not visibilityQueue.empty())
            {
                const auto& event = visibilityQueue.front();

                auto ent = ecsRef->getEntity(event.id);

                if (not (ent->has<VerticalLayout>()))
                {
                    LOG_ERROR("VerticalLayout", "Entity requested doesn't have a list view component!");
                    return;
                }

                vLayoutUpdated.insert(ent);

                visibilityQueue.pop();
            }

            while (not eventQueue.empty())
            {
                const auto& event = eventQueue.front();

                auto ent = ecsRef->getEntity(event.id);

                if (not (ent->has<VerticalLayout>()))
                {
                    LOG_ERROR("VerticalLayout", "Entity requested doesn't have a list view component !");
                    return;
                }

                addEntity(ent, event.ui);

                vLayoutUpdated.insert(ent);

                eventQueue.pop();
            }

            while (not removeQueue.empty())
            {
                const auto& event = removeQueue.front();

                auto ent = ecsRef->getEntity(event.id);

                if (not (ent->has<VerticalLayout>()))
                {
                    LOG_ERROR("VerticalLayout", "Entity requested doesn't have a list view component !");
                    return;
                }

                removeEntity(ent, event.index);

                vLayoutUpdated.insert(ent);

                removeQueue.pop();
            }

            for (auto ent : vLayoutUpdated)
            {
                recalculateChildrenPos(ent);

                auto view = ent->get<VerticalLayout>();

                updateVisibility(ent, view->visible);
            }

            vLayoutUpdated.clear();
        }

        void addEntity(EntityRef viewEnt, _unique_id ui);

        void removeEntity(EntityRef viewEnt, _unique_id);

        void recalculateChildrenPos(EntityRef viewEnt);

        void updateVisibility(EntityRef viewEnt, bool visible);

        void clear(CompRef<VerticalLayout> view);

        std::queue<AddVerticalLayoutElementEvent> eventQueue;

        std::queue<RemoveVerticalLayoutElementEvent> removeQueue;

        std::queue<ClearVerticalLayoutEvent> clearQueue;

        std::queue<UpdateVerticalLayoutVisibility> visibilityQueue;

        std::set<EntityRef> vLayoutUpdated;
    };

    template <typename Type>
    CompList<PositionComponent, UiAnchor, VerticalLayout> makeVerticalLayout(Type *ecs, float x, float y, float width, float height)
    {
        auto entity = ecs->createEntity();

        auto ui = ecs->template attach<PositionComponent>(entity);

        auto anchor = ecs->template attach<UiAnchor>(entity);

        auto view = ecs->template attach<VerticalLayout>(entity);

        ui->setX(x);
        ui->setY(y);
        ui->setWidth(width);
        ui->setHeight(height);

        return {entity, ui, anchor, view};
    }
}