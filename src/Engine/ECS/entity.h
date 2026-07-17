#pragma once

#include <unordered_set>
#include <unordered_map>
#include <algorithm>

#include "entityref.h"

#include "serialization.h"

#include "uniqueid.h"

namespace pg
{
    template <typename T, size_t N>
    class AllocatorPool;

    // Todo find the correct place for this
    template <class T>struct tag{using type=T;};

    class EntitySystem;

    template <typename Type>
    struct CompRef;

    struct StandardComponent;

    struct EntityChangedEvent { _unique_id id; };

    class Entity
    {
    friend class EntitySystem;
    friend class CommandDispatcher;
    public:
        // Default copy and move constructor
        // Entity(Entity& mE)              = default;
        // Entity& operator=(Entity& mE)   = default;
        // Entity(Entity&& mE)             = default;
        // Entity& operator=(Entity&& mE)  = default;

        // Todo remove this as it is only for testing purposes
        // 0 means that it can't be a valid entity !
        Entity() : id(0), ecsRef(nullptr) {}

        Entity(_unique_id id, EntitySystem *const ecs) noexcept : id(id), ecsRef(ecs) {}

        ~Entity() noexcept { }

        inline bool has(const _unique_id& otherId) const noexcept
        {
            // Also check pendingComponents so that components attached during a running ECS
            // tick are visible to has<>() the same way they are to get<>(). Without this,
            // a sequence like `attach<UiAnchor>(e); e->has<UiAnchor>()` returns false while
            // running (the attach is queued through the cmdDispatcher and only committed at
            // the next sync point), breaking any caller that uses has<>() as a guard before
            // setting up anchors / wiring a Prefab — e.g. applyAnchorsToEntity and
            // PrefabSystem::onEvent(SetMainEntityEvent).
            return componentList.find(otherId) != componentList.end()
                or pendingComponents.find(otherId) != pendingComponents.end();
        }

        template <typename Comp>
        inline bool has() const noexcept;

        bool has(const std::string& compName) const noexcept;

        template <typename Comp>
        inline CompRef<Comp> get() noexcept;

        inline EntitySystem* world() const noexcept { return ecsRef; }

        template <typename Comp, typename... Args>
        CompRef<Comp> attach(Args&&... args);

        // Non-template overload for StandardComponent
        template <typename... Args>
        CompRef<StandardComponent> attach(const std::string& componentName, Args&&... args);

        template <typename Comp, typename... Args>
        CompRef<Comp> attachGeneric(Args&&... args);

        _unique_id id;

        // Todo make this mutable because it is only used for memoisation purposes
        // std::unordered_map<_unique_id, Entity*> componentList;
        std::unordered_set<_unique_id> componentList;

        // Components that have been attached while the ECS is running but not yet flushed
        // Maps component type id -> raw pointer to the heap-allocated pending component
        std::unordered_map<_unique_id, void*> pendingComponents;

        //Todo overload operator delete to call ecsRef->deleteEntity(this);

    protected:
        friend void serialize<>(Archive& archive, const Entity& entity);

        // Todo use this destructor but set ecsRef to nullptr when calling it from deleteEntity of the ecs to not destroy the entity multiple time
        // ~Entity() { if(ecsRef) ecsRef->deleteEntity(this); }

        EntitySystem *const ecsRef = nullptr;
    };

    template <>
    void serialize(Archive& archive, const Entity& entity);

    template <typename Comp>
    struct CompListGetter
    {
        CompListGetter(CompRef<Comp> comp) : comp(comp) {}

        inline CompRef<Comp> get() const { return comp; }

        CompRef<Comp> comp;
    };

    template <typename... Comps>
    struct CompList : public CompListGetter<Comps>...
    {
        CompList(EntityRef entity, CompRef<Comps>... comps) : CompListGetter<Comps>(comps)..., entity(entity), id(entity.id) { }

        template <typename Comp>
        inline CompRef<Comp> get() const { return static_cast<const CompListGetter<Comp>*>(this)->get(); }

        template <typename Comp, typename... Args>
        CompRef<Comp> attach(Args&&... args) { return entity->template attach<Comp>(std::forward<Args>(args)...); }

        template <typename Comp, typename... Args>
        CompRef<Comp> attachGeneric(Args&&... args) { return entity->template attachGeneric<Comp>(std::forward<Args>(args)...); }

        operator EntityRef()
        {
            return entity;
        }

        EntityRef entity;
        _unique_id id;
    };
}