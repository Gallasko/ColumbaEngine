#pragma once

#include "ECS/system.h"
#include "ECS/entitysystem.h"

#include "2D/position.h"

#include "Helpers/functionregistry.h"

namespace pg
{
    struct ClearPrefabEvent { std::set<_unique_id> ids; };

    // Carries EntityRef rather than ids so the handler still works when this fires
    // for entities created during a running ECS tick (pending in cmdDispatcher,
    // not yet in entityPool → ecsRef->getEntity(id) would return null).
    struct SetMainEntityEvent { EntityRef prefabEnt; EntityRef ent; };

    struct PrefabChangedEvent { _unique_id prefabId; };

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

        void addToPrefab(EntityRef entity)
        {
            // setCompForPrefab(entity);

            childrenIds.insert(entity.id);

            ecsRef->sendEvent(PrefabChangedEvent{id});
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
                return EntityRef{};
            }

            return namedChildrenIds[name];
        }

        // Two-argument form: caller passes the prefab container's own EntityRef so the
        // wiring handler doesn't need to round-trip through entityPool — important when
        // the container itself is still pending in cmdDispatcher.
        void setMainEntity(EntityRef self, EntityRef entity)
        {
            addToPrefab(entity, "MainEntity");

            ecsRef->sendEvent(SetMainEntityEvent{self, entity}, true);
        }

        // Convenience overload for callers whose prefab entity is already flushed into
        // entityPool (typical for non-runtime creation paths like editor / tests / init).
        // For entities created during a running ECS tick, prefer the two-arg form so the
        // wiring works even before the dispatcher syncs.
        void setMainEntity(EntityRef entity)
        {
            setMainEntity(EntityRef{ecsRef->getEntity(id)}, entity);
        }

        template<typename R, typename... Args>
        void addHelper(const std::string& name, std::function<R(Args...)> func);

        template <typename HelperType>
        void addHelper(const std::string& name, HelperType func);

        template <typename... Args>
        std::any callHelper(const std::string& name, Args... args);

        template <typename ReturnType, typename... Args>
        ReturnType callHelper(const std::string& name, Args... args);

        EntitySystem *ecsRef = nullptr;

        _unique_id id = 0;

        // Todo make it simpler to find a specific child in a prefab
        std::set<_unique_id> childrenIds;
        std::map<std::string, EntityRef> namedChildrenIds;

        // Data to keep track

        bool deleteEntityUponRelease = true;
    };

    struct PrefabSystem : public System<Own<Prefab>, Ref<PositionComponent>, QueuedListener<PositionComponentChangedEvent>, QueuedListener<ClearPrefabEvent>, Listener<SetMainEntityEvent>, InitSys>
    {
        virtual void init() override
        {
            auto group = registerGroup<PositionComponent, Prefab>();

            group->addOnGroup([this](EntityRef entity) {
                LOG_MILE("Prefab", "Add entity " << entity->id << " to ui - prefab group !");

                updateAllPrefabEntities(entity);
            });

            auto clippedGroup = registerGroup<PositionComponent, Prefab, ClippedTo>();

            clippedGroup->addOnGroup([this](EntityRef entity) {
                LOG_MILE("Prefab", "Add entity " << entity->id << " to pos - prefab - clip group !");

                updateAllPrefabEntities(entity);
            });

            clippedGroup->removeOfGroup([this](EntitySystem* ecsRef, _unique_id id) {
                auto entity = ecsRef->getEntity(id);

                if (entity and entity->has<Prefab>() and entity->has<PositionComponent>())
                {
                    updateAllPrefabEntities(entity);
                }
            });
        }

        virtual void onProcessEvent(const PositionComponentChangedEvent& event) override
        {
            auto entity = ecsRef->getEntity(event.id);

            if (not entity or not entity->has<Prefab>())
                return;

            updateAllPrefabEntities(entity);
        }

        virtual void onProcessEvent(const ClearPrefabEvent& event) override
        {
            for (const auto& id : event.ids)
            {
                ecsRef->removeEntity(id);
            }
        }

        virtual void onEvent(const SetMainEntityEvent& event) override
        {
            // Use the EntityRefs from the event directly — their cached Entity* is
            // valid even while the entities are still pending in cmdDispatcher, so
            // has<>()/get<>() find UiAnchor via pendingComponents.
            EntityRef prefabEnt = event.prefabEnt;
            EntityRef ent       = event.ent;

            // Todo support this for non anchored prefabs

            if (not ent or not prefabEnt or not ent->has<UiAnchor>() or not prefabEnt->has<UiAnchor>() or not prefabEnt->has<Prefab>())
            {
                LOG_ERROR("Prefab System", "Failed to set main entity for prefab " << prefabEnt.id << " and entity " << ent.id << " !");
                return;
            }

            auto prefabAnchor = prefabEnt->get<UiAnchor>();
            auto entAnchor = ent->get<UiAnchor>();

            prefabAnchor->setWidthConstrain(PosConstrain{ent.id, AnchorType::Width});
            prefabAnchor->setHeightConstrain(PosConstrain{ent.id, AnchorType::Height});

            entAnchor->setTopAnchor(PosAnchor{prefabEnt.id, AnchorType::Top});
            entAnchor->setLeftAnchor(PosAnchor{prefabEnt.id, AnchorType::Left});
            entAnchor->setZConstrain(PosConstrain{prefabEnt.id, AnchorType::Z});
        }

        virtual void execute() override
        {
        }

        virtual std::string getSystemName() const override { return "Prefab System"; }

        void updatePrefabEntity(EntityRef prefabEnt, EntityRef targetEnt)
        {
            auto prefabPos = prefabEnt->get<PositionComponent>();

            // Without a position component, we cannot work on adding the entity to the prefab ui, so we skip the rest
            if (not targetEnt->has<PositionComponent>())
            {
                LOG_MILE("Prefab", "Entity " << targetEnt.id << " can't be added to prefab as it doesn't have a PositionComponent!");
                return;
            }

            auto pos = targetEnt->get<PositionComponent>();

            bool renderable = prefabPos->isRenderable();

            if (pos->observable != renderable)
            {
                pos->setObservable(renderable);
            }

            if (not prefabEnt->has<ClippedTo>())
            {
                if (targetEnt->has<ClippedTo>())
                {
                    ecsRef->detach<ClippedTo>(targetEnt);
                }
            }
            else
            {
                auto clip = prefabEnt->get<ClippedTo>();

                if (targetEnt->has<ClippedTo>())
                {
                    targetEnt->get<ClippedTo>()->setNewClipper(clip->clipperId);
                }
                else
                {
                    ecsRef->attach<ClippedTo>(targetEnt, clip->clipperId);
                }
            }
        }

        void updateAllPrefabEntities(EntityRef prefabEnt)
        {
            auto prefab = prefabEnt->get<Prefab>();

            for (const auto& id : prefab->childrenIds)
            {
                auto ent = ecsRef->getEntity(id);

                if (ent)
                {
                    updatePrefabEntity(prefabEnt, ent);
                }
            }
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

        template <typename ReturnType, typename... Args>
        ReturnType callHelper(_unique_id id, const std::string& name, Args... args)
        {
            return helperRegistry[id].call<ReturnType>(name, args...);
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

    template <typename ReturnType, typename... Args>
    ReturnType Prefab::callHelper(const std::string& name, Args... args)
    {
        return ecsRef->getSystem<PrefabSystem>()->callHelper<ReturnType>(id, name, this, std::forward<Args>(args)...);
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
