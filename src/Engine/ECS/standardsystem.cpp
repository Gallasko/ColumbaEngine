#include "standardsystem.h"
#include "system.h"
#include "entitysystem.h"
#include "componentregistry.h"

namespace pg
{
    // Internal implementation of the standard system
    // This hides all the template complexity from the user
    class StandardSystemImpl : public System<Listener<StandardEvent>, InitSys>
    {
    public:
        StandardSystemImpl(const std::string& name, const std::vector<std::string>& eventNames, StandardSystemBuilder::InitCallback initCb,
            StandardSystemBuilder::EventCallback eventCb, StandardSystemBuilder::ExecuteCallback executeCb) :
            systemName(name), listenedEvents(eventNames), initCallback(initCb), eventCallback(eventCb), executeCallback(executeCb)
        {
            handle._internalSystemPtr = this;
        }

        virtual ~StandardSystemImpl() override = default;

        virtual void init() override
        {
            LOG_THIS_MEMBER("StandardSystemImpl");

            // Register for all requested standard events
            for (const auto& eventName : listenedEvents)
            {
                addListenerToStandardEvent(eventName);
            }

            // Call user init callback
            if (initCallback)
            {
                initCallback(&handle);
            }
        }

        virtual void onEvent(const StandardEvent& event) override
        {
            LOG_THIS_MEMBER("StandardSystemImpl");

            // Call user event callback
            if (eventCallback)
            {
                eventCallback(&handle, event);
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

    private:
        std::string systemName;
        std::vector<std::string> listenedEvents;
        StandardSystemHandle handle;

        StandardSystemBuilder::InitCallback initCallback;
        StandardSystemBuilder::EventCallback eventCallback;
        StandardSystemBuilder::ExecuteCallback executeCallback;
    };

    // Saveable variant
    class StandardSystemImplSaveable : public System<Listener<StandardEvent>, InitSys, SaveSys>
    {
    public:
        StandardSystemImplSaveable(const std::string& name,
                                  const std::vector<std::string>& eventNames,
                                  StandardSystemBuilder::InitCallback initCb,
                                  StandardSystemBuilder::EventCallback eventCb,
                                  StandardSystemBuilder::ExecuteCallback executeCb,
                                  StandardSystemBuilder::SaveCallback saveCb,
                                  StandardSystemBuilder::LoadCallback loadCb,
                                  StandardSystemBuilder::InitCallback firstLoadCb)
            : systemName(name)
            , listenedEvents(eventNames)
            , initCallback(initCb)
            , eventCallback(eventCb)
            , executeCallback(executeCb)
            , saveCallback(saveCb)
            , loadCallback(loadCb)
            , firstLoadCallback(firstLoadCb)
        {
            handle._internalSystemPtr = this;
        }

        virtual ~StandardSystemImplSaveable() override = default;

        virtual void init() override
        {
            LOG_THIS_MEMBER("StandardSystemImplSaveable");

            for (const auto& eventName : listenedEvents)
            {
                addListenerToStandardEvent(eventName);
            }

            if (initCallback)
            {
                initCallback(&handle);
            }
        }

        virtual void onEvent(const StandardEvent& event) override
        {
            LOG_THIS_MEMBER("StandardSystemImplSaveable");

            if (eventCallback)
            {
                eventCallback(&handle, event);
            }
        }

        virtual void execute() override
        {
            LOG_THIS_MEMBER("StandardSystemImplSaveable");

            if (executeCallback)
            {
                executeCallback(&handle);
            }
        }

        virtual void save(Archive& archive) override
        {
            LOG_THIS_MEMBER("StandardSystemImplSaveable");

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

        virtual void load(const UnserializedObject& serializedData) override
        {
            LOG_THIS_MEMBER("StandardSystemImplSaveable");

            if (loadCallback)
            {
                std::unordered_map<std::string, ElementType> loadData;

                // Deserialize the data
                // TODO: Implement deserialization based on your UnserializedObject structure
                // This is a placeholder - you'll need to adapt to your serialization format

                loadCallback(&handle, loadData);
            }
        }

        virtual void firstLoad() override
        {
            LOG_THIS_MEMBER("StandardSystemImplSaveable");

            if (firstLoadCallback)
            {
                firstLoadCallback(&handle);
            }
        }

        virtual std::string getSystemName() const override
        {
            return systemName;
        }

        StandardSystemHandle& getHandle() { return handle; }

    private:
        std::string systemName;
        std::vector<std::string> listenedEvents;
        StandardSystemHandle handle;

        StandardSystemBuilder::InitCallback initCallback;
        StandardSystemBuilder::EventCallback eventCallback;
        StandardSystemBuilder::ExecuteCallback executeCallback;
        StandardSystemBuilder::SaveCallback saveCallback;
        StandardSystemBuilder::LoadCallback loadCallback;
        StandardSystemBuilder::InitCallback firstLoadCallback;
    };

    // Storage policy variant
    class StandardSystemImplStorage : public System<Listener<StandardEvent>, StoragePolicy, InitSys>
    {
    public:
        StandardSystemImplStorage(const std::string& name,
                                 const std::vector<std::string>& eventNames,
                                 StandardSystemBuilder::InitCallback initCb,
                                 StandardSystemBuilder::EventCallback eventCb)
            : systemName(name)
            , listenedEvents(eventNames)
            , initCallback(initCb)
            , eventCallback(eventCb)
        {
            handle._internalSystemPtr = this;
        }

        virtual ~StandardSystemImplStorage() override = default;

        virtual void init() override
        {
            LOG_THIS_MEMBER("StandardSystemImplStorage");

            for (const auto& eventName : listenedEvents)
            {
                addListenerToStandardEvent(eventName);
            }

            if (initCallback)
            {
                initCallback(&handle);
            }
        }

        virtual void onEvent(const StandardEvent& event) override
        {
            LOG_THIS_MEMBER("StandardSystemImplStorage");

            if (eventCallback)
            {
                eventCallback(&handle, event);
            }
        }

        virtual std::string getSystemName() const override
        {
            return systemName;
        }

        StandardSystemHandle& getHandle() { return handle; }

    private:
        std::string systemName;
        std::vector<std::string> listenedEvents;
        StandardSystemHandle handle;

        StandardSystemBuilder::InitCallback initCallback;
        StandardSystemBuilder::EventCallback eventCallback;
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
        AbstractSystem* system = nullptr;

        // Create the appropriate system variant based on configuration
        if (data.saveLoadEnabled)
        {
            system = new StandardSystemImplSaveable(
                data.systemName,
                data.eventNames,
                data.initCallback,
                data.eventCallback,
                data.executeCallback,
                data.saveCallback,
                data.loadCallback,
                data.firstLoadCallback
            );
        }
        else if (data.executionPolicy == "storage")
        {
            system = new StandardSystemImplStorage(
                data.systemName,
                data.eventNames,
                data.initCallback,
                data.eventCallback
            );
        }
        else
        {
            // Default: sequential with normal execution
            system = new StandardSystemImpl(
                data.systemName,
                data.eventNames,
                data.initCallback,
                data.eventCallback,
                data.executeCallback
            );
        }

        // Apply execution policy if not storage (storage is handled in type)
        if (data.executionPolicy == "manual" && !data.saveLoadEnabled)
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

        return system;
    }
}
