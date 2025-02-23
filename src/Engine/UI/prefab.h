#pragma once

#include "ECS/system.h"

#include "uisystem.h"

namespace pg
{
    struct ClearPrefabEvent { std::set<_unique_id> ids; };

    // Todo fix prefab runtime
    // Currently prefabs only works in events has the prefab need to be realized before adding other components to it
    struct Prefab : public Ctor, public Dtor
    {
        virtual void onCreation(EntityRef entity) override
        {
            ecsRef = entity.ecsRef;

            id = entity.id;
        }

        virtual void onDeletion(EntityRef) override
        {
            if (ecsRef and deleteEntityUponRelease)
            {
                ecsRef->sendEvent(ClearPrefabEvent{childrenIds});
            }
        }

        void setCompForPrefab(CompRef<UiComponent> comp)
        {
            if (comp->isVisible() != visible)
            {
                comp->setVisibility(visible);
            }

            if (comp->isWindowClipped() == isClippedToWindow)
            {
                if (not isClippedToWindow and (comp->clipTopLeft != clipTopLeft or comp->clipBottomRight != clipBottomRight))
                {
                    comp->setClipRect(clipTopLeft, clipBottomRight);
                }
            }
            else
            {
                if (isClippedToWindow)
                {
                    comp->clipBackToWindow();
                }
                else
                {
                    if (comp->clipTopLeft != clipTopLeft or comp->clipBottomRight != clipBottomRight or comp->isWindowClipped() == true)
                    {
                        comp->setClipRect(clipTopLeft, clipBottomRight);
                    }
                }
            }
        }

        void addToPrefab(CompRef<UiComponent> comp)
        {
            setCompForPrefab(comp);

            childrenIds.insert(comp.entityId);
        }

        void update()
        {
            for (const auto& id : childrenIds)
            {
                auto ent = ecsRef->getEntity(id);

                if (ent and ent->has<UiComponent>())
                {
                    auto comp = ent->get<UiComponent>();

                    setCompForPrefab(comp);
                }
            }
        }

        void setVisibility(bool visible)
        {
            if (this->visible != visible)
            {
                this->visible = visible;
                update();
            }
        }

        EntitySystem *ecsRef = nullptr;

        _unique_id id = 0;

        std::set<_unique_id> childrenIds;

        // Data to keep track

        bool isClippedToWindow = true;

        // The top left point of the clip rectangle of this component
        UiComponent::Corner clipTopLeft;

        // The bottom right point of the clip rectangle of this component
        UiComponent::Corner clipBottomRight;

        bool visible = true;

        bool deleteEntityUponRelease = true;
    };

    struct PrefabSystem : public System<Own<Prefab>, Ref<UiComponent>, Listener<EntityChangedEvent>, Listener<ClearPrefabEvent>, InitSys>
    {
        virtual void init() override
        {
            auto group = registerGroup<UiComponent, Prefab>();

            group->addOnGroup([this](EntityRef entity) {
                LOG_MILE("Prefab", "Add entity " << entity->id << " to ui - prefab group !");

                auto ui = entity->get<UiComponent>();
                auto prefab = entity->get<Prefab>();

                prefab->isClippedToWindow = ui->isWindowClipped();

                prefab->clipTopLeft = ui->clipTopLeft;

                prefab->clipBottomRight = ui->clipBottomRight;

                prefab->visible = ui->isVisible();

                prefab->update();
            });
        }

        virtual void onEvent(const EntityChangedEvent& event) override
        {
            auto entity = ecsRef->getEntity(event.id);

            if (not entity or not entity->has<Prefab>())
                return;

            bool modified = false;

            auto ui = entity->get<UiComponent>();
            auto prefab = entity->get<Prefab>();

            if (ui->isVisible() != prefab->visible)
            {
                prefab->visible = ui->isVisible();
                modified = true;
            }

            if (ui->isWindowClipped() == prefab->isClippedToWindow)
            {
                if (not ui->isWindowClipped() and (ui->clipTopLeft != prefab->clipTopLeft or ui->clipBottomRight != prefab->clipBottomRight))
                {
                    prefab->clipTopLeft = ui->clipTopLeft;
                    prefab->clipBottomRight = ui->clipBottomRight;
                    modified = true;
                }
            }
            else
            {
                if (ui->clipTopLeft != prefab->clipTopLeft or ui->clipBottomRight != prefab->clipBottomRight or prefab->isClippedToWindow == true)
                {
                    prefab->clipTopLeft = ui->clipTopLeft;
                    prefab->clipBottomRight = ui->clipBottomRight;
                }

                prefab->isClippedToWindow = ui->isWindowClipped();

                modified = true;
            }

            if (modified)
            {
                prefab->update();
            }
        }

        virtual void onEvent(const ClearPrefabEvent& event) override
        {
            clearQueue.push(event);
        }

        virtual void execute() override
        {
            while (not clearQueue.empty())
            {
                const auto& event = clearQueue.front();

                for (const auto& id : event.ids)
                {
                    ecsRef->removeEntity(id);
                }

                clearQueue.pop();
            }
        }

        std::queue<ClearPrefabEvent> clearQueue;
    };

    template <typename Type>
    CompList<UiComponent, Prefab> makePrefab(Type *ecs, float x, float y)
    {
        LOG_THIS("Prefab System");

        auto entity = ecs->createEntity();

        auto ui = ecs->template attach<UiComponent>(entity);

        ui->setX(x);
        ui->setY(y);

        auto prefab = ecs->template attach<Prefab>(entity);

        return CompList<UiComponent, Prefab>(entity, ui, prefab);
    }

} // namespace pg
