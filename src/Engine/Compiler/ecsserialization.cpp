#include "stdafx.h"

#include "ecsserialization.h"

#include "ECS/entitysystem.h"

namespace pg
{
    namespace
    {
        static constexpr char const * DOM = "ECS Serialization";

        // Name a component is stored under in an entity table, from its registered type name
        std::string getExposedTypeName(EntitySystem* ecsRef, const Entity* entity, const std::string& typeName)
        {
            // A component attached as a raw StandardComponent is exposed under its runtime type name
            if (typeName == "StandardComponent")
            {
                auto standardComp = ecsRef->getComponent<StandardComponent>(entity->id);

                if (standardComp)
                    return standardComp->typeName;
            }

            return typeName;
        }

        Value serializeComponent(VM* vm, EntitySystem* ecsRef, const Entity* entity, _unique_id componentId, const std::string& typeName)
        {
            auto registry = ecsRef->getComponentRegistry();
            auto metadata = ComponentProxyRegistry::instance().findMetadata(typeName);

            void* component = nullptr;

            if (metadata != nullptr and metadata->retriever)
            {
                component = metadata->retriever(ecsRef, entity->id);
            }
            else if (registry->hasStandardComponent(typeName))
            {
                // A script defined component is stored under its runtime name and served by the StandardComponent metadata
                auto owner = registry->retrieveStandardComponent(typeName);

                if (owner)
                    component = owner->getComponent(entity->id);

                if (metadata == nullptr)
                    metadata = ComponentProxyRegistry::instance().findMetadata("StandardComponent");
            }

            if (metadata != nullptr and component != nullptr)
                return ComponentProxy::createProxy(vm, metadata, component);

            // No proxy support for this type: the script gets a copy of the component
            LOG_MILE(DOM, "No proxy for component " << typeName << ", falling back to a table copy");

            InspectorArchive archive;

            registry->serializeComponentFromEntity(archive, entity, componentId);

            return archiveToTable(vm, archive);
        }

        NativeFn createHasFunction(Entity* entity, EntitySystem* ecsRef)
        {
            return [entity, ecsRef](VM* vm, int argCount, Value* args) -> Value {
                if (argCount != 1)
                    throw std::runtime_error("has expects exactly 1 argument (componentName)");

                if (not IS_STRING(args[0]))
                    throw std::runtime_error("has expects a string argument (component name)");

                auto componentName = vm->asString(args[0]);
                auto registry = ecsRef->getComponentRegistry();

                if (registry->hasStandardComponent(componentName))
                {
                    auto owner = registry->retrieveStandardComponent(componentName);

                    if (owner and owner->components.has(entity->id))
                        return makeBoolValue(true);
                }

                for (const auto& id : entity->componentList)
                {
                    if (getExposedTypeName(ecsRef, entity, registry->getComponentTypeName(id)) == componentName)
                        return makeBoolValue(true);
                }

                return makeBoolValue(false);
            };
        }
    }

    namespace detail
    {
        Value createEntityTable(VM* vm, EntitySystem* ecsRef, Entity* entity)
        {
            Value entityTableValue = vm->createTable();

            auto entityTable = vm->asInstance(entityTableValue);

            entityTable->setField("__entityId", makeIntValue(static_cast<int64_t>(entity->id)));

            // Both natives keep the entity pointer, so a script can use them without any entity lookup
            entityTable->setField("attachComp", vm->createNativeFunction(createAttachCompFunction(entity, ecsRef)));
            entityTable->setField("has", vm->createNativeFunction(createHasFunction(entity, ecsRef)));

            return entityTableValue;
        }
    }

    Value serializeComponentToTable(VM* vm, EntitySystem* ecsRef, const Entity* entity, _unique_id componentId)
    {
        return serializeComponent(vm, ecsRef, entity, componentId, ecsRef->getComponentRegistry()->getComponentTypeName(componentId));
    }

    Value serializeEntityToTable(VM* vm, EntitySystem* ecsRef, Entity* entity)
    {
        LOG_MILE(DOM, "Serializing entity ID " << entity->id);

        Value entityTableValue = detail::createEntityTable(vm, ecsRef, entity);

        auto entityTable = vm->asInstance(entityTableValue);
        auto registry = ecsRef->getComponentRegistry();

        for (const auto& id : entity->componentList)
        {
            const std::string typeName = registry->getComponentTypeName(id);

            entityTable->setField(getExposedTypeName(ecsRef, entity, typeName), serializeComponent(vm, ecsRef, entity, id, typeName));
        }

        return entityTableValue;
    }

    Value serializeEntityViewToTable(VM* vm, EntitySystem* ecsRef, _unique_id entityId, const std::vector<std::string>& componentNames)
    {
        Value entityTableValue = vm->createTable();

        auto entityTable = vm->asInstance(entityTableValue);

        entityTable->setField("__entityId", makeIntValue(static_cast<int64_t>(entityId)));

        Entity* entity = ecsRef->getEntity(entityId);

        // The entity was deleted since its id was collected: the script only gets the id
        if (not entity)
            return entityTableValue;

        auto registry = ecsRef->getComponentRegistry();

        // A requested component that the entity does not have simply leaves its field absent
        for (const auto& id : entity->componentList)
        {
            const std::string typeName = registry->getComponentTypeName(id);
            const std::string exposedName = getExposedTypeName(ecsRef, entity, typeName);

            if (std::find(componentNames.begin(), componentNames.end(), exposedName) == componentNames.end())
                continue;

            entityTable->setField(exposedName, serializeComponent(vm, ecsRef, entity, id, typeName));
        }

        return entityTableValue;
    }

    Value serializeEntitiesToTable(VM* vm, EntitySystem* ecsRef, const std::vector<Entity*>& entities)
    {
        Value entitiesTableValue = vm->createTable();

        auto entitiesTable = vm->asInstance(entitiesTableValue);

        size_t index = 0;

        for (auto entity : entities)
        {
            entitiesTable->setField(std::to_string(index), serializeEntityToTable(vm, ecsRef, entity));

            ++index;
        }

        entitiesTable->setField("count", makeIntValue(static_cast<int64_t>(entities.size())));

        return entitiesTableValue;
    }

    bool deserializeComponentFromTable(VM* vm, EntitySystem* ecsRef, EntityRef entity, Value componentTable, const std::string& componentTypeName)
    {
        if (not IS_INSTANCE(componentTable))
        {
            LOG_ERROR(DOM, "componentTable is not an instance");

            return false;
        }

        auto table = vm->asInstance(componentTable);

        std::string typeName = componentTypeName;

        if (typeName.empty() and table->hasField("__className"))
        {
            auto className = table->getField("__className");

            if (IS_STRING(className))
                typeName = vm->asString(className);
        }

        if (typeName.empty())
        {
            LOG_ERROR(DOM, "No component type name provided and no __className found in table");

            return false;
        }

        LOG_MILE(DOM, "Component type name: " << typeName);

        ecsRef->getComponentRegistry()->deserializeComponentToEntity(tableToUnserializedObject(vm, table, typeName), entity);

        return true;
    }

    EntityRef deserializeEntityFromTable(VM* vm, EntitySystem* ecsRef, Value entityTable, bool createNew)
    {
        if (not IS_INSTANCE(entityTable))
            throw std::runtime_error("entityTable is not an instance");

        auto table = vm->asInstance(entityTable);

        _unique_id specifiedId = 0;

        if (table->hasField("__entityId"))
        {
            auto idValue = table->getField("__entityId");

            if (IS_INT(idValue))
                specifiedId = static_cast<_unique_id>(AS_INT(idValue));
        }

        EntityRef entity;

        // Reuse the entity named by the table when asked to and when it still exists
        Entity* existingEntity = nullptr;

        if (not createNew and specifiedId != 0)
            existingEntity = ecsRef->getEntity(specifiedId);

        if (existingEntity)
        {
            entity = EntityRef(existingEntity);
        }
        else
        {
            entity = ecsRef->createEntity();
        }

        // Every field holding a table is a component, named after the field
        for (const auto& [key, index] : table->internedFields)
        {
            if (key == "__entityId" or key == "__className")
                continue;

            auto value = table->fieldValues[index];

            if (IS_INSTANCE(value))
                deserializeComponentFromTable(vm, ecsRef, entity, value, key);
        }

        return entity;
    }

    std::vector<EntityRef> deserializeEntitiesFromTable(VM* vm, EntitySystem* ecsRef, Value entitiesTable, bool createNew)
    {
        if (not IS_INSTANCE(entitiesTable))
            throw std::runtime_error("entitiesTable is not an instance");

        auto table = vm->asInstance(entitiesTable);

        std::vector<EntityRef> entities;

        for (const auto& [key, index] : table->internedFields)
        {
            // Entities are stored under numeric keys, anything else ("count", ...) is skipped
            if (key.empty() or key.find_first_not_of("0123456789") != std::string::npos)
                continue;

            auto value = table->fieldValues[index];

            if (IS_INSTANCE(value))
                entities.push_back(deserializeEntityFromTable(vm, ecsRef, value, createNew));
        }

        return entities;
    }
}
