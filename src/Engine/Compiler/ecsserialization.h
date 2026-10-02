#pragma once

/**
 * @file ecsserialization.h
 * @brief Expose ECS entities to the scripting VM, and build entities back from script tables
 *
 * An entity is given to a script as a table:
 *
 *     {
 *         "__entityId": 42,
 *         "attachComp": native function,
 *         "has": native function,
 *         "PositionComponent": component,
 *         "Velocity": component
 *     }
 *
 * Each component is a proxy on the live C++ component (see componentproxy.h): reading or writing one of its
 * properties goes straight to the component. A component type without proxy metadata is given as a copy
 * (see tableserialization.h), in which case writes stay in the script.
 *
 * The other pieces of the script bridge are included from here:
 * - componentproxy.h: the proxy class and the per-type property metadata
 * - componentattach.h: the attachComp() handlers
 * - tableserialization.h: plain copies between C++ objects and script tables
 */

#include <string>
#include <vector>

#include "ECS/entitysystem_fwd.h"
#include "ECS/entity.h"

#include "componentattach.h"
#include "componentproxy.h"
#include "tableserialization.h"

namespace pg
{
    /**
     * @brief Give a script access to one component of an entity
     *
     * @param vm VM that will own the returned value
     * @param ecsRef Entity system owning the entity
     * @param entity Entity owning the component
     * @param componentId Id of the component type
     * @return A proxy on the component, or a table copy if the type has no proxy metadata
     */
    Value serializeComponentToTable(VM* vm, EntitySystem* ecsRef, const Entity* entity, _unique_id componentId);

    /**
     * @brief Build the script table of an entity, with all its components
     *
     * @param vm VM that will own the table
     * @param ecsRef Entity system owning the entity
     * @param entity Entity to expose
     * @return The entity table
     */
    Value serializeEntityToTable(VM* vm, EntitySystem* ecsRef, Entity* entity);

    /**
     * @brief Build a reduced script table of an entity, holding only the requested components
     *
     * Used by the entity loop lowering (__ecsEntityView): no attachComp nor has native is added.
     * An entity that no longer exists gives a table holding only "__entityId".
     *
     * @param vm VM that will own the table
     * @param ecsRef Entity system owning the entity
     * @param entityId Id of the entity
     * @param componentNames Component type names to include
     * @return The entity table
     */
    Value serializeEntityViewToTable(VM* vm, EntitySystem* ecsRef, _unique_id entityId, const std::vector<std::string>& componentNames);

    /**
     * @brief Build the table holding several entities
     *
     * The entity tables are stored under "0", "1", ... and their number under "count".
     */
    Value serializeEntitiesToTable(VM* vm, EntitySystem* ecsRef, const std::vector<Entity*>& entities);

    /**
     * @brief Attach a component described by a script table to an entity
     *
     * @param vm VM owning the table
     * @param ecsRef Entity system owning the entity
     * @param entity Entity to attach the component to
     * @param componentTable Table holding the component fields
     * @param componentTypeName Type of the component, read from the "__className" field of the table when empty
     * @return false if the value is not a table or if no type name could be found
     */
    bool deserializeComponentFromTable(VM* vm, EntitySystem* ecsRef, EntityRef entity, Value componentTable, const std::string& componentTypeName = "");

    /**
     * @brief Build an entity from a script table
     *
     * Every field holding a table is attached as a component named after the field.
     *
     * @param vm VM owning the table
     * @param ecsRef Entity system that receives the entity
     * @param entityTable Table describing the entity
     * @param createNew If false, the entity named by "__entityId" is reused when it exists
     * @return The created, or reused, entity
     */
    EntityRef deserializeEntityFromTable(VM* vm, EntitySystem* ecsRef, Value entityTable, bool createNew = false);

    /**
     * @brief Build the entities stored under the numeric keys of a script table
     */
    std::vector<EntityRef> deserializeEntitiesFromTable(VM* vm, EntitySystem* ecsRef, Value entitiesTable, bool createNew = true);

    namespace detail
    {
        /**
         * @brief Create an entity table holding "__entityId" and the attachComp / has natives, without any component
         */
        Value createEntityTable(VM* vm, EntitySystem* ecsRef, Entity* entity);

        template <typename Comp, typename... Comps>
        void serializeCompListComponent(VM* vm, ObjInstance* entityTable, const CompList<Comps...>& compList)
        {
            CompRef<Comp> comp = compList.template get<Comp>();

            if (not comp)
                return;

            const std::string typeName = Comp::getType();

            auto metadata = ComponentProxyRegistry::instance().findMetadata(typeName);

            if (metadata != nullptr)
            {
                entityTable->setField(typeName, ComponentProxy::createProxy(vm, metadata, comp.operator->()));
            }
            else
            {
                entityTable->setField(typeName, serializeToTable(vm, *comp));
            }
        }
    }

    /**
     * @brief Build the script table of an entity from a CompList, holding only the components of the list
     *
     * @code
     * auto tex = make2DTexture(ecs, 100, 100, "texture.png");
     *
     * Value table = serializeEntityToTable(vm, ecs, tex);
     * @endcode
     *
     * @tparam Comps The component types of the CompList
     * @param vm VM that will own the table
     * @param compList Entity and component references, as returned by the make functions
     * @return The entity table
     */
    template <typename... Comps>
    Value serializeEntityToTable(VM* vm, EntitySystem*, const CompList<Comps...>& compList)
    {
        if (compList.entity.empty())
        {
            LOG_ERROR("ECS Serialization", "Cannot serialize a CompList without entity");

            return vm->createTable();
        }

        Entity* entity = compList.entity.entity;

        Value entityTableValue = detail::createEntityTable(vm, entity->world(), entity);

        auto entityTable = vm->asInstance(entityTableValue);

        (detail::serializeCompListComponent<Comps>(vm, entityTable, compList), ...);

        return entityTableValue;
    }
}
