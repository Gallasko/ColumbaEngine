#include "stdafx.h"

#include "standardsystem.h"
#include "system.h"
#include "entitysystem.h"
#include "componentregistry.h"

#include "Compiler/vm.h"

#include "Compiler/ecsserialization.h"

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

    ElementMap* StandardSystemHandle::getData()
    {
        if (!_internalSystemPtr)
            return nullptr;

        StandardSystemImpl* sys = static_cast<StandardSystemImpl*>(_internalSystemPtr);
        return &sys->getSystemData();
    }

    // ============================================================================
    // StandardSystemBuilder implementation
    // ============================================================================

    StandardSystemBuilder::StandardSystemBuilder(const std::string& systemName)
    {
        data.systemName = systemName;
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

    StandardSystemBuilder& StandardSystemBuilder::onInit(_S_InitCallback callback)
    {
        data.initCallback = callback;
        return *this;
    }

    StandardSystemBuilder& StandardSystemBuilder::onEvent(const std::string& eventName, _S_EventCallback callback)
    {
        data.eventCallbackList[eventName] = callback;
        return *this;
    }

    StandardSystemBuilder& StandardSystemBuilder::onExecute(_S_ExecuteCallback callback)
    {
        data.executeCallback = callback;
        return *this;
    }

    StandardSystemBuilder& StandardSystemBuilder::onSave(_S_SaveCallback callback)
    {
        data.saveCallback = callback;
        return *this;
    }

    StandardSystemBuilder& StandardSystemBuilder::onLoad(_S_LoadCallback callback)
    {
        data.loadCallback = callback;
        return *this;
    }

    StandardSystemBuilder& StandardSystemBuilder::onFirstLoad(_S_InitCallback callback)
    {
        data.firstLoadCallback = callback;
        return *this;
    }

    StandardSystemBuilder& StandardSystemBuilder::onEvent(const std::string& eventName, const std::string& scriptName)
    {
        data.scriptEventCallbackList[eventName] = scriptName;

        return *this;
    }

    StandardSystemBuilder& StandardSystemBuilder::onExecute(const std::string& scriptName)
    {
        data.executeScript = scriptName;

        return *this;
    }

    StandardSystemImpl* StandardSystemBuilder::build()
    {
        // Create a single StandardSystemImpl with all features
        auto* system = new StandardSystemImpl(
            data.systemName,
            data.componentNames,
            data.componentDefaultValues,
            data.saveLoadEnabled,
            data.initCallback,
            data.eventCallbackList,
            data.scriptEventCallbackList,
            data.executeCallback,
            data.executeScript,
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
