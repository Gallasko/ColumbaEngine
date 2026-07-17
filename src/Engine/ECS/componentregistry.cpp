#include "stdafx.h"

#include "componentregistry.h"

#include "Interpreter/interpretersystem.h"

#include "entitysystem.h"

namespace pg
{
    UniqueIdGenerator ComponentRegistry::globalIdGenerator;
    std::unordered_map<std::string, _unique_id> ComponentRegistry::globalStringIdGenerator;

    void Component::onCreation(EntityRef entity)
    {
        LOG_THIS_MEMBER("Component");

        ecsRef = entity->world();
        entityId = entity->id;
    }

    template <>
    void serialize(Archive& archive, const StandardEvent& value)
    {
        archive.startSerialization("StandardEvent");

        serialize(archive, "name", value.name);
        serialize(archive, "values", value.values);

        archive.endSerialization();
    }

    template <>
    StandardEvent deserialize(const UnserializedObject& serializedString)
    {
        LOG_THIS("Standard");

        std::string type = "";

        if (serializedString.isNull())
        {
            LOG_ERROR("Standard", "Element is null");
        }
        else
        {
            LOG_INFO("Standard", "Deserializing StandardEvent");

            StandardEvent data;

            defaultDeserialize(serializedString, "name", data.name);
            defaultDeserialize(serializedString, "values", data.values);

            return data;
        }

        return StandardEvent{};
    }

    void StandardComponent::sendStandardEvent(const StandardEvent& event)
    {
        ecsRef->sendEvent(event);
    }

    template <>
    void serialize(Archive& archive, const StandardComponent& value)
    {
        archive.startSerialization(StandardComponent::getType());

        serialize(archive, "typeName", value.typeName);
        serialize(archive, "properties", value.properties);

        archive.endSerialization();
    }

    template <>
    StandardComponent deserialize(const UnserializedObject& serializedString)
    {
        LOG_THIS("Standard");

        std::string type = "";

        if (serializedString.isNull())
        {
            LOG_ERROR("Standard", "Element is null");
        }
        else
        {
            LOG_INFO("Standard", "Deserializing StandardComponent");

            std::string typeName;
            defaultDeserialize(serializedString, "typeName", typeName);

            StandardComponent data(typeName);
            defaultDeserialize(serializedString, "properties", data.properties);

            return data;
        }

        return StandardComponent("");
    }

    ComponentRegistry::ComponentRegistry(EntitySystem *ecs) : ecsRef(ecs)
    {
        LOG_THIS_MEMBER("ComponentRegistry");

        systemSerializer.setFile("save/systems.sz");
    }

    ComponentRegistry::~ComponentRegistry()
    {
        LOG_THIS_MEMBER("Component Registry");

        LOG_INFO("Component Registry", "Deleting Component Registry...");

        for (auto group : groupStorageMap)
            delete static_cast<AbstractGroup*>(group.second);

        LOG_INFO("Component Registry", "Component Registry deleted !");
    }

    void ComponentRegistry::addEventListener(_unique_id eventId, InterpreterSystem *listener)
    {
        LOG_THIS_MEMBER("Component Registry");

        // Store the listerer using the listener pointer value to be able to delete it later
        eventStorageMap[eventId].emplace((intptr_t)listener, [eventId, listener](const std::any& event) {
            listener->onEvent(eventId, std::any_cast<const std::shared_ptr<ClassInstance>&>(event));
        });
    }

    void ComponentRegistry::removeEventListener(_unique_id eventId, InterpreterSystem *listener)
    {
        LOG_THIS_MEMBER("Component Registry");

        if (const auto& it = eventStorageMap[eventId].find((intptr_t)listener); it != eventStorageMap[eventId].end())
        {
            eventStorageMap[eventId].erase(it);
        }
    }

    void ComponentRegistry::removeTypeId(_unique_id id)
    {
        LOG_INFO("ID", "Removing Id: " << id);

        auto it = idMap.find(id);

        if (it == idMap.end())
        {
            idMap.erase(id);
        }
    }

    template<>
    void ComponentRegistry::processEvent(const StandardEvent& event)
    {
        LOG_THIS_MEMBER("Component Registry");

        for (auto& eventListener : standardEventStorageMap[event.name])
        {
            eventListener.second(event);
        }
    }

    void CompRef<StandardComponent>::operator=(const CompRef& rhs)
    {
        LOG_THIS_MEMBER("Comp ref");

        compName    = rhs.compName;
        ecsRef      = rhs.ecsRef;
        entityId    = rhs.entityId;
        initialized = rhs.initialized;
        component   = rhs.component;

        if (not initialized)
        {
            if (entityId != 0)
            {
                auto fetchComponent = rhs.ecsRef->getComponent(compName, entityId);

                if (fetchComponent)
                {
                    component   = fetchComponent;
                    initialized = true;
                }
                // Todo see if we propagate back the finding of the entity to the base ref !
                // rhs.entity = entity
                // rhs.initialized = true
                // Note that it needs to make the rhs not const or we need to make the member entity mutable !
            }
            else if (rhs.ecsRef != nullptr)
            {
                // ecsRef set with entityId == 0 is a real anomaly. A purely
                // default-constructed empty ref (ecsRef == nullptr) is legit
                // and stays silent — empty CompRefs are routinely held as
                // members and copied around.
                LOG_ERROR("Comp ref", "Copy of a reference to an invalid entity");
            }
        }
    }

    StandardComponent* CompRef<StandardComponent>::operator->()
    {
        if (initialized)
            return component;
        else
        {
            // Try to find the component in the ecs to update this ref
            auto comp = ecsRef->getComponent(compName, entityId);

            // Component found, updating this entity ref
            if (entityId != 0 and comp)
            {
                component = comp;
                initialized = true;
            }

           return component;
        }
    }

    CompRef<StandardComponent>::operator StandardComponent*()
    {
        if (initialized)
            return component;
        else
        {
            // Try to find the component in the ecs to update this ref
            auto comp = ecsRef->getComponent(compName, entityId);

            // Component found, updating this entity ref
            if (entityId != 0 and comp)
            {
                component = comp;
                initialized = true;
            }

           return component;
        }
    }

    Entity* CompRef<StandardComponent>::getEntity() const
    {
        if (entityId != 0)
        {
            return ecsRef->getEntity(entityId);
        }

        return nullptr;
    }

    // ============================================================================
    // StandardComponent Management Implementation
    // ============================================================================

    void ComponentRegistry::storeStandardComponent(const std::string& typeName, Own<StandardComponent>* owner)
    {
        LOG_THIS_MEMBER("Component Registry");

        // Generate a unique ID for this specific StandardComponent type name
        const auto id = idGenerator.generateId();

        // Register delete callback
        componentDeleteMap.emplace(id, [owner](Entity* entity) {
            owner->internalRemoveComponent(entity);
        });

        // Register serialize callback
        componentSerializeMap.emplace(id, [owner](Archive& archive, const Entity* entity) {
            serialize(archive, *(owner->getComponent(entity->id)));
        });

        // Store component type name for fast lookup
        componentTypeNameMap.emplace(id, typeName);

        // Register deserialize callback using the typeName
        componentDeserializeMap.emplace(typeName, [this, typeName](const UnserializedObject& serializedStr, EntityRef entity) {
            if (serializedStr.isNull())
                return;

            auto comp = deserialize<StandardComponent>(serializedStr);
            comp.entityId = entity.id;
            comp.ecsRef = entity.ecsRef;
            comp.typeName = typeName;

            ecsRef->_attach(entity, typeName, std::move(comp));
        });

        // Register detach callback using the typeName
        componentDetachMap.emplace(typeName, [this](EntityRef entity) {
            ecsRef->detach<StandardComponent>(entity);
        });

        // Store in both maps
        componentStorageMap.emplace(id, owner);
        standardComponentStorageMap[typeName] = owner;

        // Set the component ID on the owner
        owner->_componentId = id;
    }

    void ComponentRegistry::unstoreStandardComponent(const std::string& typeName)
    {
        LOG_THIS_MEMBER("Component Registry");

        // Find the owner to get its ID
        auto ownerIt = standardComponentStorageMap.find(typeName);
        if (ownerIt == standardComponentStorageMap.end())
        {
            LOG_WARNING("Component Registry", "Cannot unstore StandardComponent '" << typeName << "' - not found");
            return;
        }

        const auto id = ownerIt->second->_componentId;

        // Remove from all maps
        if (const auto& it = componentDeleteMap.find(id); it != componentDeleteMap.end())
        {
            componentDeleteMap.erase(it);
        }

        if (const auto& it = componentSerializeMap.find(id); it != componentSerializeMap.end())
        {
            componentSerializeMap.erase(it);
        }

        if (const auto& it = componentDeserializeMap.find(typeName); it != componentDeserializeMap.end())
        {
            componentDeserializeMap.erase(it);
        }

        if (const auto& it = componentDetachMap.find(typeName); it != componentDetachMap.end())
        {
            componentDetachMap.erase(it);
        }

        if (const auto& it = componentStorageMap.find(id); it != componentStorageMap.end())
        {
            componentStorageMap.erase(it);
        }

        // Remove from string-based map
        standardComponentStorageMap.erase(ownerIt);
    }

    Own<StandardComponent>* ComponentRegistry::retrieveStandardComponent(const std::string& typeName) const
    {
        LOG_THIS_MEMBER("Component Registry");

        auto it = standardComponentStorageMap.find(typeName);
        if (it != standardComponentStorageMap.end())
        {
            return it->second;
        }

        LOG_WARNING("Component Registry", "StandardComponent type '" << typeName << "' NOT FOUND in registry");
        return nullptr;
    }

    bool ComponentRegistry::hasStandardComponent(const std::string& typeName) const
    {
        return standardComponentStorageMap.find(typeName) != standardComponentStorageMap.end();
    }

    std::vector<std::string> ComponentRegistry::getStandardComponentTypes() const
    {
        std::vector<std::string> types;
        types.reserve(standardComponentStorageMap.size());
        for (const auto& [typeName, _] : standardComponentStorageMap)
        {
            types.push_back(typeName);
        }
        return types;
    }

}