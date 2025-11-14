#include "standardsystem.h"
#include "system.h"
#include "entitysystem.h"
#include "componentregistry.h"

namespace pg
{
    // Simple base system - no templates, everything added manually
    // This system supports all features based on configuration
    class StandardSystemImpl : public AbstractSystem
    {
    public:
        StandardSystemImpl(const std::string& name,
                          const std::vector<std::string>& eventNames,
                          const std::vector<std::string>& componentNames,
                          bool saveLoadEnabled,
                          StandardSystemBuilder::InitCallback initCb,
                          StandardSystemBuilder::EventCallback eventCb,
                          StandardSystemBuilder::ExecuteCallback executeCb,
                          StandardSystemBuilder::SaveCallback saveCb,
                          StandardSystemBuilder::LoadCallback loadCb,
                          StandardSystemBuilder::InitCallback firstLoadCb)
            : systemName(name)
            , listenedEvents(eventNames)
            , ownedComponents(componentNames)
            , saveLoadEnabled(saveLoadEnabled)
            , initCallback(initCb)
            , eventCallback(eventCb)
            , executeCallback(executeCb)
            , saveCallback(saveCb)
            , loadCallback(loadCb)
            , firstLoadCallback(firstLoadCb)
        {
            handle._internalSystemPtr = this;
            if (saveLoadEnabled)
            {
                saveable = true;
            }
        }

        virtual ~StandardSystemImpl() override
        {
            removeFromRegistry();
        }

        void setRegistry(EntitySystem* ecs)
        {
            LOG_THIS_MEMBER("StandardSystemImpl");

            ecsRef = ecs;
            registry = &(ecs->registry);

            // Create and register Own<StandardComponent> for each component type
            for (const auto& componentName : ownedComponents)
            {
                auto* owner = new Own<StandardComponent>(componentName);
                owner->setRegistry(registry);
                componentOwners[componentName] = owner;

                LOG_INFO("StandardSystemImpl", "Registered component owner for: " << componentName);
            }

            // Register event listeners
            for (const auto& eventName : listenedEvents)
            {
                registry->addStandardEventListener(eventName, this);
            }

            LOG_INFO("StandardSystemImpl", "System fully registered with " << componentOwners.size() << " components and " << listenedEvents.size() << " events");
        }

        virtual void removeFromRegistry() override
        {
            LOG_THIS_MEMBER("StandardSystemImpl");

            // Unregister all components
            if (registry)
            {
                for (auto& [typeName, owner] : componentOwners)
                {
                    owner->unsetRegistry(registry);
                    delete owner;
                }
                componentOwners.clear();

                // Unregister event listeners
                for (const auto& eventName : listenedEvents)
                {
                    registry->removeStandardEventListener(eventName, this);
                }
            }
        }

        void onEvent(const StandardEvent& event)
        {
            LOG_THIS_MEMBER("StandardSystemImpl");

            // Call user event callback
            if (eventCallback)
            {
                eventCallback(&handle, event);
            }
        }

        virtual void onRegisterFinished() override
        {
            LOG_THIS_MEMBER("StandardSystemImpl");

            // Call user init callback
            if (initCallback)
            {
                initCallback(&handle);
            }
        }

        virtual void execute() override
        {
            LOG_THIS_MEMBER("StandardSystemImpl");

            // Call user execute callback
            if (executeCallback)
            {
                executeCallback(&handle);
            }
        }

        virtual std::string getSystemName() const override
        {
            return systemName;
        }

        StandardSystemHandle& getHandle() { return handle; }

        Own<StandardComponent>* getComponentOwner(const std::string& typeName)
        {
            auto it = componentOwners.find(typeName);
            return (it != componentOwners.end()) ? it->second : nullptr;
        }

        // Save/load methods (called when saveLoadEnabled is true)
        void save(Archive& archive)
        {
            LOG_THIS_MEMBER("StandardSystemImpl");

            if (saveCallback)
            {
                std::unordered_map<std::string, ElementType> saveData;
                saveCallback(&handle, saveData);

                // Serialize the save data
                for (const auto& [key, value] : saveData)
                {
                    serialize(archive, key, value);
                }
            }
        }

        void load(const UnserializedObject& serializedData)
        {
            LOG_THIS_MEMBER("StandardSystemImpl");

            if (loadCallback)
            {
                std::unordered_map<std::string, ElementType> loadData;

                // TODO: Implement deserialization based on your UnserializedObject structure
                // This is a placeholder - you'll need to adapt to your serialization format

                loadCallback(&handle, loadData);
            }
        }

        void firstLoad()
        {
            LOG_THIS_MEMBER("StandardSystemImpl");

            if (firstLoadCallback)
            {
                firstLoadCallback(&handle);
            }
        }

    private:
        std::string systemName;
        std::vector<std::string> listenedEvents;
        std::vector<std::string> ownedComponents;
        std::unordered_map<std::string, Own<StandardComponent>*> componentOwners;
        StandardSystemHandle handle;

        bool saveLoadEnabled;

        StandardSystemBuilder::InitCallback initCallback;
        StandardSystemBuilder::EventCallback eventCallback;
        StandardSystemBuilder::ExecuteCallback executeCallback;
        StandardSystemBuilder::SaveCallback saveCallback;
        StandardSystemBuilder::LoadCallback loadCallback;
        StandardSystemBuilder::InitCallback firstLoadCallback;
    };

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
        // TODO: Implement based on your component system
        // This would need integration with your component registry
        return nullptr;
    }

    void StandardSystemHandle::removeComponent(size_t entityId, const std::string& componentType)
    {
        // TODO: Implement based on your component system
    }

    StandardComponent* StandardSystemHandle::getComponent(size_t entityId, const std::string& componentType)
    {
        // TODO: Implement based on your component system
        return nullptr;
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

    AbstractSystem* StandardSystemBuilder::build()
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
