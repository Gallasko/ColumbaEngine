#pragma once

#include "ECS/system.h"

#include "uisystem.h"

namespace pg
{
    struct ClearListViewEvent
    {
        _unique_id id;
    };

    struct AddListViewElementEvent
    {
        _unique_id id; CompRef<UiComponent> ui;
    };

    struct RemoveListViewElementEvent
    {
        _unique_id id; size_t index;
    };

    struct ListViewElement : public Dtor 
    {
        ListViewElement(_unique_id viewId, size_t index) : viewId(viewId), index(index) {}
        ListViewElement(const ListViewElement& other) : viewId(other.viewId), index(other.index) {}

        virtual void onDeletion(EntityRef entity) override
        {
            entity.ecsRef->sendEvent(RemoveListViewElementEvent{entity.id, index});
        };

        /** Id of the list view entity */
        _unique_id viewId = 0;

        /** Position of this entity inside the list view */
        size_t index = 0;
    };

    struct ListView : public Ctor
    {
        void onCreation(EntityRef entity) override
        {
            id = entity.id;
            ecsRef = entity.ecsRef;
        }

        void addEntity(CompRef<UiComponent> ui) { ecsRef->sendEvent(AddListViewElementEvent{id, ui}); }

        void removeEntity(EntityRef entity);

        void updateVisibility();

        void clear() { ecsRef->sendEvent(ClearListViewEvent{id}); }

        CompRef<UiComponent> viewUi;

        // Todo currently only supports vertical list view
        // Todo make it work for horizontal or both hori/verti slider (2 sliders at the same time)
        EntityRef cursor;
        UiSize cursorHeight;

        UiSize listReelHeight;

        EntityRef slider;

        /** Spacing between each entity of the list */
        float spacing = 5;

        std::vector<CompRef<UiComponent>> entities;

        _unique_id id;

        EntitySystem *ecsRef;
    };

    struct ListViewSystem : public System<Listener<AddListViewElementEvent>, Listener<ClearListViewEvent>, Own<ListView>, NamedSystem, InitSys>
    {
        virtual std::string getSystemName() const override { return "ListView System"; }

        virtual void init() override;

        virtual void onEvent(const AddListViewElementEvent& event) override
        {
            eventQueue.push(event);
        }

        virtual void onEvent(const ClearListViewEvent& event) override
        {
            clearQueue.push(event);
        }

        virtual void execute() override
        {
            while (not clearQueue.empty())
            {
                const auto& event = clearQueue.front();

                auto ent = ecsRef->getEntity(event.id);

                if (not (ent->has<ListView>()))
                {
                    LOG_ERROR("ListView", "Entity requested doesn't have a list view component !");
                    return;
                }

                clear(ent->get<ListView>());

                clearQueue.pop();
            }

            while (not eventQueue.empty())
            {
                const auto& event = eventQueue.front();

                auto ent = ecsRef->getEntity(event.id);

                if (not (ent->has<ListView>()))
                {
                    LOG_ERROR("ListView", "Entity requested doesn't have a list view component !");
                    return;
                }

                addEntity(ent->get<ListView>(), event.ui);

                eventQueue.pop();
            }
        }

        void addEntity(CompRef<ListView> view, CompRef<UiComponent> ui);

        void removeEntity(EntityRef /* entity */) {}

        void calculateListSize(CompRef<ListView> view);

        void updateCursorSize(CompRef<ListView> view, const UiSize& maxPos);

        void clear(CompRef<ListView> view);

        std::queue<AddListViewElementEvent> eventQueue;

        std::queue<ClearListViewEvent> clearQueue;
    };

    /** Helper that create an entity with an Ui component and a Texture component */
    CompList<UiComponent, ListView> makeListView(EntitySystem *ecs, float x, float y, float width, float height);

}