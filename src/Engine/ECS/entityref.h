#pragma once

#include "uniqueid.h"

namespace pg
{
    class Entity;
    class EntitySystem;

    template <typename Type>
    struct CompRef;

    // Todo remplace all occurence of EntityRef by CompRef<Entity> -> alias CompRef<Entity> EntityRef
    struct EntityRef
    {
        EntityRef() : initialized(false), entity(nullptr), id(0), ecsRef(nullptr) {}

        // Todo maybe create a constructor without the bool initialized that check in the ecsRef if the entity was actually initialized !
        EntityRef(Entity* ent, bool initialized = true);

        EntityRef(const EntityRef& rhs)
        {
            (*this) = rhs;
        }

        bool operator==(const EntityRef& rhs);

        void operator=(const EntityRef& rhs);

        void operator=(Entity* ent);

        bool operator<(const EntityRef& rhs) const { return id < rhs.id; }

        template <typename Comp>
        bool has() const;

        template <typename Comp>
        CompRef<Comp> get() const;

        template <typename Comp, typename... Args>
        CompRef<Comp> attach(Args&&... args);

        template <typename Comp, typename... Args>
        CompRef<Comp> attachGeneric(Args&&... args);

        Entity* operator->();

        operator Entity*();

        inline bool empty() const { return entity == nullptr; }

        bool initialized;
        Entity* entity;
        _unique_id id;
        EntitySystem* ecsRef;
    };
}