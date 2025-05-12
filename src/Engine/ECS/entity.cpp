#include "entity.h"

#include "entitysystem.h"

#include "logger.h"

namespace pg
{
    namespace
    {
        constexpr static const char * const DOM = "Entity";
    }

    template <>
    void serialize<>(Archive& archive, const Entity& entity)
    {
        archive.startSerialization("Entity");

        serialize(archive, "id", entity.id);

        auto ecs = entity.world();

        size_t refIdCount = 0;

        for (const auto& comp : entity.componentList)
        {
            if (comp.entityHeldType == Entity::EntityHeld::EntityHeldType::id)
            {
                ecs->getComponentRegistry()->serializeComponentFromEntity(archive, &entity, comp.getId());
            }
            else
            {
                serialize(archive, "idRef" + std::to_string(refIdCount++), comp.getId());
            }
        }

        serialize(archive, "nbRefId", refIdCount);

        archive.endSerialization();
    }

    bool EntityRef::operator==(const EntityRef& rhs)
    {
        return id == rhs.id;
    }

    void EntityRef::operator=(const EntityRef& rhs)
    {
        LOG_THIS_MEMBER(DOM);

        if (rhs.initialized)
        {
            initialized = rhs.initialized;
            entity      = rhs.entity;
            id          = rhs.id;
            ecsRef      = rhs.ecsRef;
        }
        else
        {
            id = rhs.id;

            Entity* ent = nullptr;

            if (id != 0)
                ent = rhs.ecsRef->getEntity(id);

            if (ent)
            {
                entity = ent;
                ecsRef = rhs.ecsRef;
                initialized = true;

                // Todo see if we propagate back the finding of the entity to the base ref !
                // rhs.entity = entity
                // rhs.initialized = true
                // Note that it needs to make the rhs not const or we need to make the member entity mutable !
            }
            else
            {
                initialized = rhs.initialized;
                entity      = rhs.entity;
                ecsRef      = rhs.ecsRef;
            }
        }
    }

    void EntityRef::operator=(Entity* ent)
    {
        LOG_THIS_MEMBER(DOM);

        // Check first if the entity exist
        if (not ent)
        {
            // If not make this entity ref a dummy one
            entity = nullptr;
            id = 0;
            ecsRef = nullptr;
            initialized = false;

            return;
        }

        // Get id and ecs
        id = ent->id;
        ecsRef = ent->world();

        // Try to get the entity from the ecs
        auto ecsEnt = ecsRef->getEntity(id);

        if (id != 0 and ecsEnt)
        {
            // If the entity is in the ecs grab it's pointer from there

            entity = ecsEnt;
            initialized = true;
        }
        else
        {
            // Else store the pointer given to us but stay uninitialized

            entity = ent;
            initialized = false;
        }
    }

    Entity* EntityRef::operator->()
    {
        LOG_THIS_MEMBER(DOM);

        if (initialized)
            return entity;
        else
        {
            // Try to find the entity in the ecs to update this ref
            auto ent = ecsRef->getEntity(id);

            // Entity found, updating this entity ref
            if (id != 0 and ent)
            {
                entity = ent;
                initialized = true;
            }

            return entity;
        }
    }

    EntityRef::operator Entity*()
    {
        LOG_THIS_MEMBER(DOM);

        if (initialized)
            return entity;
        else
        {
            // Try to find the entity in the ecs to update this ref
            auto ent = ecsRef->getEntity(id);

            // Entity found, updating this entity ref
            if (id != 0 and ent)
            {
                entity = ent;
                initialized = true;
            }

            return entity;
        }
    }
}