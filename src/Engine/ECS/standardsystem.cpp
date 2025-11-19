#include "standardsystem.h"
#include "system.h"
#include "entitysystem.h"
#include "componentregistry.h"

namespace pg
{
    // ============================================================================
    // StandardSystemHandle implementation
    // ============================================================================

    EntitySystem* StandardSystemHandle::getWorld() const
    {
        if (_internalSystemPtr)
        {
            AbstractSystem* sys = static_cast<AbstractSystem*>(_internalSystemPtr);
            return sys->world();
        }
        return nullptr;
    }

    void StandardSystemHandle::sendEvent(const StandardEvent& event)
    {
        if (_internalSystemPtr)
        {
            AbstractSystem* sys = static_cast<AbstractSystem*>(_internalSystemPtr);
            if (sys->registry)
            {
                sys->registry->processEvent(event);
            }
        }
    }

    void StandardSystemHandle::sendEvent(const std::string& eventName)
    {
        sendEvent(StandardEvent(eventName));
    }

    void StandardSystemHandle::sendEvent(const std::string& eventName, const std::string& key, const ElementType& value)
    {
        StandardEvent event(eventName);
        event.values[key] = value;
        sendEvent(event);
    }

    StandardComponent* StandardSystemHandle::createComponent(size_t entityId, const std::string& componentType)
    {
        if (!_internalSystemPtr)
            return nullptr;

        StandardSystemImpl* sys = static_cast<StandardSystemImpl*>(_internalSystemPtr);

        // Get the component owner for this type
        auto* owner = sys->getComponentOwner(componentType);
        if (!owner)
        {
            LOG_WARNING("StandardSystemHandle", "Component type '" << componentType << "' is not owned by this system");
            return nullptr;
        }

        // Get the entity
        auto* entity = sys->world()->getEntity(entityId);
        if (!entity)
        {
            LOG_ERROR("StandardSystemHandle", "Entity " << entityId << " does not exist");
            return nullptr;
        }

        // Create the component with the typeName set
        auto* comp = owner->internalCreateComponent(entity, componentType);

        LOG_INFO("StandardSystemHandle", "Created StandardComponent '" << componentType << "' for entity " << entityId);

        return comp;
    }

    void StandardSystemHandle::removeComponent(size_t entityId, const std::string& componentType)
    {
        if (!_internalSystemPtr)
            return;

        StandardSystemImpl* sys = static_cast<StandardSystemImpl*>(_internalSystemPtr);

        // Get the component owner for this type
        auto* owner = sys->getComponentOwner(componentType);
        if (!owner)
        {
            LOG_WARNING("StandardSystemHandle", "Component type '" << componentType << "' is not owned by this system");
            return;
        }

        // Get the entity
        auto* entity = sys->world()->getEntity(entityId);
        if (!entity)
        {
            LOG_ERROR("StandardSystemHandle", "Entity " << entityId << " does not exist");
            return;
        }

        // Remove the component
        owner->internalRemoveComponent(entity);

        LOG_INFO("StandardSystemHandle", "Removed StandardComponent '" << componentType << "' from entity " << entityId);
    }

    StandardComponent* StandardSystemHandle::getComponent(size_t entityId, const std::string& componentType)
    {
        if (!_internalSystemPtr)
            return nullptr;

        StandardSystemImpl* sys = static_cast<StandardSystemImpl*>(_internalSystemPtr);

        // Get the component owner for this type
        auto* owner = sys->getComponentOwner(componentType);
        if (!owner)
        {
            LOG_WARNING("StandardSystemHandle", "Component type '" << componentType << "' is not owned by this system");
            return nullptr;
        }

        // Get the component for this entity
        auto* comp = owner->getComponent(entityId);

        return comp;
    }

    // ============================================================================
    // StandardSystemBuilder implementation
    // ============================================================================

    StandardSystemBuilder::StandardSystemBuilder(const std::string& systemName)
    {
        data.systemName = systemName;
    }

    StandardSystemBuilder& StandardSystemBuilder::listenToEvents(const std::vector<std::string>& eventNames)
    {
        data.eventNames = eventNames;
        return *this;
    }

    StandardSystemBuilder& StandardSystemBuilder::listenToEvent(const std::string& eventName)
    {
        data.eventNames.push_back(eventName);
        return *this;
    }

    StandardSystemBuilder& StandardSystemBuilder::ownComponents(const std::vector<std::string>& componentNames)
    {
        data.componentNames = componentNames;
        return *this;
    }

    StandardSystemBuilder& StandardSystemBuilder::ownComponent(const std::string& componentName)
    {
        data.componentNames.push_back(componentName);
        return *this;
    }

    StandardSystemBuilder& StandardSystemBuilder::useStoragePolicy()
    {
        data.executionPolicy = "storage";
        return *this;
    }

    StandardSystemBuilder& StandardSystemBuilder::useManualPolicy()
    {
        data.executionPolicy = "manual";
        return *this;
    }

    StandardSystemBuilder& StandardSystemBuilder::useParallelPolicy()
    {
        data.executionPolicy = "parallel";
        return *this;
    }

    StandardSystemBuilder& StandardSystemBuilder::useSequentialPolicy()
    {
        data.executionPolicy = "sequential";
        return *this;
    }

    StandardSystemBuilder& StandardSystemBuilder::enableSaveLoad()
    {
        data.saveLoadEnabled = true;
        return *this;
    }

    StandardSystemBuilder& StandardSystemBuilder::onInit(InitCallback callback)
    {
        data.initCallback = callback;
        return *this;
    }

    StandardSystemBuilder& StandardSystemBuilder::onEvent(EventCallback callback)
    {
        data.eventCallback = callback;
        return *this;
    }

    StandardSystemBuilder& StandardSystemBuilder::onExecute(ExecuteCallback callback)
    {
        data.executeCallback = callback;
        return *this;
    }

    StandardSystemBuilder& StandardSystemBuilder::onSave(SaveCallback callback)
    {
        data.saveCallback = callback;
        return *this;
    }

    StandardSystemBuilder& StandardSystemBuilder::onLoad(LoadCallback callback)
    {
        data.loadCallback = callback;
        return *this;
    }

    StandardSystemBuilder& StandardSystemBuilder::onFirstLoad(InitCallback callback)
    {
        data.firstLoadCallback = callback;
        return *this;
    }

    StandardSystemImpl* StandardSystemBuilder::build()
    {
        // Create a single StandardSystemImpl with all features
        auto* system = new StandardSystemImpl(
            data.systemName,
            data.eventNames,
            data.componentNames,
            data.saveLoadEnabled,
            data.initCallback,
            data.eventCallback,
            data.executeCallback,
            data.saveCallback,
            data.loadCallback,
            data.firstLoadCallback
        );

        // Apply execution policy
        if (data.executionPolicy == "storage")
        {
            system->setPolicy(ExecutionPolicy::Storage);
        }
        else if (data.executionPolicy == "manual")
        {
            system->setPolicy(ExecutionPolicy::Manual);
        }
        else if (data.executionPolicy == "parallel")
        {
            system->setPolicy(ExecutionPolicy::Parallel);
        }
        else if (data.executionPolicy == "independent")
        {
            system->setPolicy(ExecutionPolicy::Independent);
        }
        // else: defaults to Sequential

        return system;
    }
}
