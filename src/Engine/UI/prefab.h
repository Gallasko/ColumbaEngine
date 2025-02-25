#pragma once

#include "ECS/system.h"

// #include "uisystem.h"

#include "2D/position.h"

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

        void setCompForPrefab(EntityRef entity)
        {
            // Without a position component, we cannot work on adding the entity to the prefab ui, so we skip the rest
            if (not entity->has<PositionComponent>())
            {
                LOG_ERROR("Prefab", "Entity " << entity.id << " can't be added to prefab as it doesn't have a PositionComponent!");
                return;
            }

            auto pos = entity->get<PositionComponent>();

            if (pos->visible != visible)
            {
                pos->setVisibility(visible);
            }

            if (isClippedToWindow)
            {
                if (entity->has<ClippedTo>())
                {
                    entity->world()->detach<ClippedTo>(entity);
                }                
            }
            else
            {
                auto thisEnt = ecsRef->getEntity(id);
                if (not thisEnt or not thisEnt->has<ClippedTo>())
                {
                    LOG_ERROR("Prefab", "Prefab " << id << " has no ClippedTo component, but wants to clip its children!");
                }

                auto clip = thisEnt->get<ClippedTo>();

                if (entity->has<ClippedTo>())
                {
                    entity->get<ClippedTo>()->setNewClipper(clip->clipperId);
                }
                else
                {
                    ecsRef->attach<ClippedTo>(entity, clip->clipperId);    
                }
            }
        }

        void addToPrefab(EntityRef entity)
        {
            setCompForPrefab(entity);

            childrenIds.insert(entity.id);
        }

        void update()
        {
            for (const auto& id : childrenIds)
            {
                auto ent = ecsRef->getEntity(id);

                if (ent)
                {
                    setCompForPrefab(ent);
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
    
        bool visible = true;

        bool deleteEntityUponRelease = true;
    };

    struct PrefabSystem : public System<Own<Prefab>, Ref<PositionComponent>, Listener<EntityChangedEvent>, Listener<ClearPrefabEvent>, InitSys>
    {
        virtual void init() override
        {
            auto group = registerGroup<PositionComponent, Prefab>();

            group->addOnGroup([](EntityRef entity) {
                LOG_MILE("Prefab", "Add entity " << entity->id << " to ui - prefab group !");

                auto ui = entity->get<PositionComponent>();
                auto prefab = entity->get<Prefab>();

                prefab->visible = ui->visible;

                prefab->update();
            });

            auto clippedGroup = registerGroup<PositionComponent, Prefab, ClippedTo>();

            clippedGroup->addOnGroup([](EntityRef entity) {
                LOG_MILE("Prefab", "Add entity " << entity->id << " to pos - prefab - clip group !");

                auto ui = entity->get<PositionComponent>();
                auto prefab = entity->get<Prefab>();

                prefab->isClippedToWindow = false;

                prefab->visible = ui->visible;

                prefab->update();
            });

            clippedGroup->removeOfGroup([](EntitySystem* ecsRef, _unique_id id) {
                auto entity = ecsRef->getEntity(id);

                if (entity and entity->has<Prefab>())
                {
                    if (not entity->has<ClippedTo>())
                    {
                        auto prefab = entity->get<Prefab>();
                        
                        prefab->isClippedToWindow = true;
                        prefab->update();
                    }
                }
            });
        }

        virtual void onEvent(const EntityChangedEvent& event) override
        {
            auto entity = ecsRef->getEntity(event.id);

            if (not entity or not entity->has<Prefab>())
                return;

            bool modified = false;

            auto ui = entity->get<PositionComponent>();
            auto prefab = entity->get<Prefab>();

            if (ui->visible != prefab->visible)
            {
                prefab->visible = ui->visible;
                modified = true;
            }

            if (not prefab->isClippedToWindow)
            {
                // Todo find a way to find if the clip parent was updated and set modified flag only if it was updated
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
    CompList<PositionComponent, Prefab> makePrefab(Type *ecs, float x, float y)
    {
        LOG_THIS("Prefab System");

        auto entity = ecs->createEntity();

        auto ui = ecs->template attach<PositionComponent>(entity);

        ui->setX(x);
        ui->setY(y);

        auto prefab = ecs->template attach<Prefab>(entity);

        return CompList<PositionComponent, Prefab>(entity, ui, prefab);
    }

    template <typename Type>
    CompList<PositionComponent, UiAnchor, Prefab> makeAnchoredPrefab(Type *ecs, float x, float y)
    {
        LOG_THIS("Prefab System");

        auto entity = ecs->createEntity();

        auto ui = ecs->template attach<PositionComponent>(entity);

        ui->setX(x);
        ui->setY(y);

        auto anchor = ecs->template attach<UiAnchor>(entity);

        auto prefab = ecs->template attach<Prefab>(entity);

        return CompList<PositionComponent, UiAnchor, Prefab>(entity, ui, anchor, prefab);
    }

} // namespace pg
