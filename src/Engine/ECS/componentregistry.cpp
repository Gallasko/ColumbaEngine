#include "componentregistry.h"

#include "Interpreter/interpretersystem.h"

namespace pg
{
    UniqueIdGenerator ComponentRegistry::globalIdGenerator;

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
        LOG_THIS("IncreaseFact");

        std::string type = "";

        if (serializedString.isNull())
        {
            LOG_ERROR("AddFact", "Element is null");
        }
        else
        {
            LOG_INFO("AddFact", "Deserializing IncreaseFact");

            StandardEvent data;

            defaultDeserialize(serializedString, "name", data.name);
            defaultDeserialize(serializedString, "values", data.values);

            return data;
        }

        return StandardEvent{};
    }

    ComponentRegistry::ComponentRegistry(EntitySystem *ecs) : ecsRef(ecs)
    {
        LOG_THIS_MEMBER("ComponentRegistry");

        systemSerializer.setFile("systems.sz");
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

        LOG_INFO("CReg", "Received standard event");

        for (auto& eventListener : standardEventStorageMap[event.name])
        {
            eventListener.second(event);
        }
    }

}