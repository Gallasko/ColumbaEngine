#pragma once

#include "ECS/system.h"

#include "2D/position.h"

#include "Helpers/functionregistry.h"

namespace pg
{
    struct ClearPrefabEvent { std::set<_unique_id> ids; };

    struct SetMainEntityEvent { _unique_id prefabId; _unique_id entityId; };

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
                LOG_MILE("Prefab", "Entity " << entity.id << " can't be added to prefab as it doesn't have a PositionComponent!");
                return;
            }

            auto pos = entity->get<PositionComponent>();

            if (pos->visible != visible)
            {
                pos->setVisibility(visible);
            }

            if (pos->observable != observable)
            {
                pos->setObservable(observable);
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

        void addToPrefab(EntityRef entity, const std::string& name)
        {
            addToPrefab(entity);

            namedChildrenIds[name] = entity;
        }

        EntityRef getEntity(const std::string& name)
        {
            const auto it = namedChildrenIds.find(name);

            if (it == namedChildrenIds.end())
            {
                LOG_ERROR("Prefab", "Couldn't find entity with name: " << name << " in prefab: " << id);
                return nullptr;
            }

            return namedChildrenIds[name];
        }

        void setMainEntity(EntityRef entity)
        {
            ecsRef->sendEvent(SetMainEntityEvent{id, entity->id});
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

        void setObservable(bool observable)
        {
            if (this->observable != observable)
            {
                this->observable = observable;
                update();
            }
        }

        template<typename R, typename... Args>
        void addHelper(const std::string& name, std::function<R(Args...)> func);

        template <typename HelperType>
        void addHelper(const std::string& name, HelperType func);

        template <typename... Args>
        std::any callHelper(const std::string& name, Args... args);

        EntitySystem *ecsRef = nullptr;

        _unique_id id = 0;

        // Todo make it simpler to find a specific child in a prefab
        std::set<_unique_id> childrenIds;
        std::map<std::string, EntityRef> namedChildrenIds;

        // Data to keep track

        bool isClippedToWindow = true;

        bool visible = true;
        bool observable = true;

        bool deleteEntityUponRelease = true;
    };

    struct PrefabSystem : public System<Own<Prefab>, Ref<PositionComponent>, Listener<EntityChangedEvent>, QueuedListener<ClearPrefabEvent>, QueuedListener<SetMainEntityEvent>, InitSys>
    {
        virtual void init() override
        {
            auto group = registerGroup<PositionComponent, Prefab>();

            group->addOnGroup([](EntityRef entity) {
                LOG_MILE("Prefab", "Add entity " << entity->id << " to ui - prefab group !");

                auto ui = entity->get<PositionComponent>();
                auto prefab = entity->get<Prefab>();

                prefab->visible = ui->visible;
                prefab->observable = ui->observable;

                prefab->update();
            });

            auto clippedGroup = registerGroup<PositionComponent, Prefab, ClippedTo>();

            clippedGroup->addOnGroup([](EntityRef entity) {
                LOG_MILE("Prefab", "Add entity " << entity->id << " to pos - prefab - clip group !");

                auto prefab = entity->get<Prefab>();
                prefab->isClippedToWindow = false;

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

            if (ui->observable != prefab->observable)
            {
                prefab->observable = ui->observable;
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

        virtual void onProcessEvent(const ClearPrefabEvent& event) override
        {
            for (const auto& id : event.ids)
            {
                ecsRef->removeEntity(id);
            }
        }

        virtual void onProcessEvent(const SetMainEntityEvent& event) override
        {
            auto prefabEnt = ecsRef->getEntity(event.prefabId);
            auto ent = ecsRef->getEntity(event.entityId);

            // Todo support this for non anchored prefabs

            if (not ent or not prefabEnt or not ent->has<UiAnchor>() or not prefabEnt->has<UiAnchor>() or not prefabEnt->has<Prefab>())
            {
                LOG_ERROR("Prefab System", "Failed to set main entity for prefab " << event.prefabId << " and entity " << event.entityId << " !");
                return;
            }

            auto prefabAnchor = prefabEnt->get<UiAnchor>();
            auto entAnchor = ent->get<UiAnchor>();

            prefabAnchor->setWidthConstrain(PosConstrain{event.entityId, AnchorType::Width});
            prefabAnchor->setHeightConstrain(PosConstrain{event.entityId, AnchorType::Height});

            entAnchor->fillIn(prefabAnchor);
            entAnchor->setZConstrain(PosConstrain{event.prefabId, AnchorType::Z});

            auto prefab = prefabEnt->get<Prefab>();

            prefab->addToPrefab(ent);
        }

        virtual void execute() override
        {
        }

        template<typename R, typename... Args>
        void addHelper(_unique_id id, const std::string& name, std::function<R(Args...)> func)
        {
            helperRegistry[id].add(name, func);
        }


        template <typename HelperType>
        void addHelper(_unique_id id, const std::string& name, HelperType func)
        {
            helperRegistry[id].add(name, func);
        }

        template <typename... Args>
        std::any callHelper(_unique_id id, const std::string& name, Args... args)
        {
            return helperRegistry[id].call(name, args...);
        }

        std::unordered_map<_unique_id, FunctionRegistry> helperRegistry;
    };

    template<typename R, typename... Args>
    void Prefab::addHelper(const std::string& name, std::function<R(Args...)> func)
    {
        ecsRef->getSystem<PrefabSystem>()->addHelper(id, name, std::move(func));
    }

    template <typename HelperType>
    void Prefab::addHelper(const std::string& name, HelperType func)
    {
        ecsRef->getSystem<PrefabSystem>()->addHelper(id, name, std::move(func));
    }

    template <typename... Args>
    std::any Prefab::callHelper(const std::string& name, Args... args)
    {
        return ecsRef->getSystem<PrefabSystem>()->callHelper(id, name, this, std::forward<Args>(args)...);
    }

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
    CompList<PositionComponent, UiAnchor, Prefab> makeAnchoredPrefab(Type *ecs, float x = 0.0f, float y = 0.0f, float z = 0.0f)
    {
        LOG_THIS("Prefab System");

        auto entity = ecs->createEntity();

        auto ui = ecs->template attach<PositionComponent>(entity);

        ui->setX(x);
        ui->setY(y);
        ui->setZ(z);

        auto anchor = ecs->template attach<UiAnchor>(entity);

        auto prefab = ecs->template attach<Prefab>(entity);

        return CompList<PositionComponent, UiAnchor, Prefab>(entity, ui, anchor, prefab);
    }

} // namespace pg
