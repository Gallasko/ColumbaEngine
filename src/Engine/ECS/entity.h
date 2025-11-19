#pragma once

#include <set>
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

    struct EntityChangedEvent { _unique_id id; };

    class Entity
    {
    friend class EntitySystem;
    friend class CommandDispatcher;
    public:
        struct EntityHeld
        {
            union EntityHeldId
            {
                explicit EntityHeldId(Entity *ent) : entity(ent) {}
                explicit EntityHeldId(_unique_id id) : id(id) {}

                Entity *entity;
                _unique_id id;
            };

            enum class EntityHeldType
            {
                entity,
                id
            };

            explicit EntityHeld(Entity* entity) : entityHeldId(entity), entityHeldType(EntityHeldType::entity) { }
            explicit EntityHeld(const _unique_id& id) : entityHeldId(id), entityHeldType(EntityHeldType::id) { }
            explicit EntityHeld(const EntityHeld& other) : entityHeldId(other.entityHeldId), entityHeldType(other.entityHeldType) { }

            constexpr bool operator==(_unique_id id) const
            {
                if (entityHeldType == EntityHeldType::entity)
                    return entityHeldId.entity->id == id;
                else
                    return entityHeldId.id == id;
            }

            void operator=(Entity* entity)
            {
                entityHeldId.entity = entity;
                entityHeldType = EntityHeldType::entity;
            }

            void operator=(_unique_id id)
            {
                entityHeldId.id = id;
                entityHeldType = EntityHeldType::id;
            }

            void operator=(const EntityHeld& rhs)
            {
                entityHeldId = rhs.entityHeldId;
                entityHeldType = rhs.entityHeldType;
            }

            _unique_id getId() const
            {
                if (entityHeldType == EntityHeldType::entity)
                    return entityHeldId.entity->id;
                else
                    return entityHeldId.id;
            }

            operator _unique_id() const
            {
                return getId();
            }

            EntityHeldId entityHeldId;
            EntityHeldType entityHeldType;
        };

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
            return std::find(componentList.begin(), componentList.end(), otherId) != componentList.end();
        }

        template <typename Comp>
        inline bool has() const noexcept;

        template <typename Comp>
        inline CompRef<Comp> get() noexcept;

        inline EntitySystem* world() const noexcept { return ecsRef; }

        template <typename Comp, typename... Args>
        CompRef<Comp> attach(Args&&... args);

        template <typename Comp, typename... Args>
        CompRef<Comp> attachGeneric(Args&&... args);

        _unique_id id;

        // Todo make this mutable because it is only used for memoisation purposes
        // std::unordered_map<_unique_id, Entity*> componentList;
        std::set<EntityHeld> componentList;

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