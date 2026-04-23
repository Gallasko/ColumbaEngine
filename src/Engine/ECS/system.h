#pragma once

#include <string>
#include <unordered_map>

#include "entity.h"
#include "componentregistry.h"
#include "group.h"
#include "eventlistener.h"

#include "logger.h"

// Todo
// Add an OnDelta caps that actually does all the listening to TickEvent and accumulation (like the standard sys)

namespace tf
{
    // Forward declaration
    class Taskflow;
}

namespace pg
{

    enum class ExecutionPolicy : uint8_t
    {
        Manual      = 0,
        Sequential  = 1,
        Parallel    = 2,
        Independent = 3,
        Storage     = 4
    };

    struct ManualPolicy { };

    struct ParallelPolicy
    {
        virtual ~ParallelPolicy() {}

        virtual void parallelExecute(tf::Taskflow&) = 0;
    };

    struct IndependentPolicy { };

    struct StoragePolicy { };

    struct InitSys
    {
        virtual ~InitSys() {}

        virtual void init() = 0;
    };

    struct SaveSys
    {
        virtual ~SaveSys() {}

        // Pure virtual function to save any data from the system
        virtual void save(Archive& archive) = 0;

        // Virtual function call when the sys is not present in the save file
        virtual void firstLoad() {};
        // Pure virtual function to load any data saved in the sys file
        virtual void load(const UnserializedObject& serializedString) = 0;
    };

    /**
     * @brief Abstract representation of a system
     */
    struct AbstractSystem
    {
        virtual ~AbstractSystem() { LOG_THIS_MEMBER("System"); }

        virtual void onRegisterFinished() {};

        virtual void execute() { LOG_THIS_MEMBER("System"); }

        // Todo
        inline void setPolicy(const ExecutionPolicy& policy) { executionPolicy = policy; }

        inline EntitySystem* world() const noexcept { return ecsRef; }

        ExecutionPolicy executionPolicy = ExecutionPolicy::Sequential;

        EntitySystem* ecsRef = nullptr;

        ComponentRegistry *registry = nullptr;

        _unique_id _id;

        bool saveable = false;

        virtual std::string getSystemName() const { return "UnNamed"; }

        std::vector<std::function<void()>> _executionQueue;
        void _execute()
        {
            for (const auto& func : _executionQueue)
                func();

            this->execute();
        }

        /**
         * @brief Remove a component from the registry
         */
        virtual void removeFromRegistry() = 0;

        std::string __name = "UnNamed";

        /** Events this system subscribes to.
         *  Each entry is "L:mangled_type" (Listener<T>) or "Q:mangled_type" (QueuedListener<T>). */
        std::vector<std::string> _listenerEventNames;

        /** Groups registered via registerGroup<>() – stored as mangled type names.
         *  Mutable because registerGroup() is a const method (also called from viewGroup()). */
        mutable std::vector<std::string> _registeredGroupNames;

        // Todo make function onAdd and onDelete of a component that default to nothing if not used
    };

    // Forward declaration for StandardSystemImpl
    class StandardSystemImpl;
    struct TickEvent;

    /**
     * @brief Handle for accessing standard system functionality
     */
    class StandardSystemHandle
    {
    public:
        StandardSystemHandle() = default;
        virtual ~StandardSystemHandle() = default;

        // Access to the ECS world
        EntitySystem* getWorld() const;

        // Send events
        void sendEvent(const StandardEvent& event);
        void sendEvent(const std::string& eventName);
        void sendEvent(const std::string& eventName, const std::string& key, const ElementType& value);

        // Component creation/removal (to be implemented based on your needs)
        // These will work with entities that have StandardComponent attached
        StandardComponent* createComponent(size_t entityId, const std::string& componentType);
        void removeComponent(size_t entityId, const std::string& componentType);
        StandardComponent* getComponent(size_t entityId, const std::string& componentType);

        // Access to system data storage
        ElementMap* getData();
        ElementType getData(const std::string& name);
        void setData(const std::string& name, const ElementType& value);

        // Internal use only - stores the actual StandardSystemImpl pointer
        StandardSystemImpl* _internalSystemPtr = nullptr;
    };

    // Set callbacks
    using _S_InitCallback = std::function<void(StandardSystemHandle*)>;
    using _S_EventCallback = std::function<void(StandardSystemHandle*, const StandardEvent&)>;
    using _S_ExecuteCallback = std::function<void(StandardSystemHandle*)>;
    using _S_SaveCallback = std::function<void(StandardSystemHandle*, ElementMap&)>;
    using _S_LoadCallback = std::function<void(StandardSystemHandle*, const ElementMap&)>;
    using _S_DeltaCallback = std::function<void(StandardSystemHandle*, float)>;

    using _S_EventMap = std::unordered_map<std::string, _S_EventCallback>;
    using _S_EventScriptMap = std::unordered_map<std::string, std::string>;

    // Simple base system - no templates, everything added manually
    // This system supports all features based on configuration
    class StandardSystemImpl : public AbstractSystem
    {
    public:
        StandardSystemImpl(const std::string& name,
                          const std::vector<std::string>& componentNames,
                          const std::unordered_map<std::string, ElementMap>& defaultComponentValues,
                          bool saveLoadEnabled,
                          _S_InitCallback initCb,
                          const std::string& initScriptPath,
                          _S_EventMap eventMap,
                          _S_EventScriptMap eventScriptMap,
                          _S_EventScriptMap deferredEventScriptMap,
                          _S_ExecuteCallback executeCb,
                          const std::string& executeScriptPath,
                          _S_SaveCallback saveCb,
                          _S_LoadCallback loadCb,
                          _S_InitCallback firstLoadCb,
                          _S_DeltaCallback deltaCb,
                          const std::string& deltaScriptPath) :
                          systemName(name), ownedComponents(componentNames), defaultComponentValues(defaultComponentValues),
                          initCallback(initCb), initScript(initScriptPath),
                          eventCallbackList(eventMap), eventScriptCallbackList(eventScriptMap),
                          deferredEventScriptCallbackList(deferredEventScriptMap),
                          executeCallback(executeCb), executeScript(executeScriptPath),
                          saveCallback(saveCb), loadCallback(loadCb), firstLoadCallback(firstLoadCb),
                          deltaCallback(deltaCb), deltaScript(deltaScriptPath)
        {
            for (auto [key, _] : eventMap)
            {
                listenedEvents.insert(key);
            }

            for (auto [key, _] : eventScriptMap)
            {
                listenedEvents.insert(key);
            }

            for (auto [key, _] : deferredEventScriptMap)
            {
                listenedDeferredEvents.insert(key);
            }

            handle._internalSystemPtr = this;
            if (saveLoadEnabled)
            {
                saveable = true;
            }

            if (deltaCallback or not deltaScript.empty())
            {
                needDelta = true;
            }
        }

        virtual ~StandardSystemImpl() override
        {
            removeFromRegistry();
        }

        void addToRegistry(ComponentRegistry *registry);

        virtual void removeFromRegistry() override;

        void onEvent(const StandardEvent& event)
        {
            LOG_THIS_MEMBER("StandardSystemImpl");

            // Deferred events are queued and processed later during _execute()
            if (listenedDeferredEvents.count(event.name))
            {
                _deferredEventQueue.push(event);
                return;
            }

            // Call user event callback
            auto it = eventCallbackList.find(event.name);

            if (it != eventCallbackList.end())
            {
                it->second(&handle, event);
            }

            auto it2 = eventCompiledScriptCallbackList.find(event.name);

            if (it2 != eventCompiledScriptCallbackList.end())
            {
                it2->second(&handle, event);
            }
        }

        void onProcessEvent(const StandardEvent& event)
        {
            LOG_THIS_MEMBER("StandardSystemImpl");

            auto it = deferredEventCompiledScriptCallbackList.find(event.name);

            if (it != deferredEventCompiledScriptCallbackList.end())
            {
                it->second(&handle, event);
            }
        }

        void onEvent(const TickEvent& event);

        virtual void onRegisterFinished() override
        {
            LOG_THIS_MEMBER("StandardSystemImpl");

            // Call user init callback
            if (initCallback)
            {
                initCallback(&handle);
            }

            // Call compiled init script callback
            if (compiledInitScriptCallback)
            {
                LOG_INFO("StandardSystemImpl", "Calling compiled init script for system: " << systemName);
                compiledInitScriptCallback(&handle);
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

            // Call compiled execute script callback
            if (compiledExecuteScriptCallback)
            {
                compiledExecuteScriptCallback(&handle);
            }

            if (needDelta and deltaTime > 0.0f)
            {
                if (deltaCallback)
                {
                    deltaCallback(&handle, deltaTime / 1000.0f);
                }

                if (compiledDeltaScriptCallback)
                {
                    compiledDeltaScriptCallback(&handle, deltaTime / 1000.0f);
                }

                deltaTime = 0;
            }
        }

        virtual std::string getSystemName() const override
        {
            return systemName;
        }

        StandardSystemHandle& getHandle() { return handle; }

        const std::unordered_map<std::string, ElementMap>& getDefaultCompValues() { return defaultComponentValues; }

        Own<StandardComponent>* getComponentOwner(const std::string& typeName)
        {
            auto it = componentOwners.find(typeName);
            return (it != componentOwners.end()) ? it->second : nullptr;
        }

        // Access to system data storage
        ElementMap& getSystemData() { return systemData; }

        // Save/load methods (called when saveLoadEnabled is true)
        void save(Archive& archive)
        {
            LOG_THIS_MEMBER("StandardSystemImpl");

            if (saveCallback)
            {
                ElementMap saveData;
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

            if (serializedData.isNull())
            {
                LOG_ERROR("StandardSystemImpl", "Serialized data is null");
                return;
            }

            if (loadCallback)
            {
                ElementMap loadData;

                // Iterate through all children in the serialized object
                // Each child is a key-value pair that was saved
                for (const auto& child : serializedData.children)
                {
                    const std::string& key = child.getObjectName();

                    // Deserialize the ElementType value
                    ElementType value;
                    defaultDeserialize(serializedData, key, value);

                    loadData[key] = value;
                }

                LOG_INFO("StandardSystemImpl", "Loaded " << loadData.size() << " values from save data");

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
        std::set<std::string> listenedEvents;
        std::set<std::string> listenedDeferredEvents;
        std::vector<std::string> ownedComponents;
        std::unordered_map<std::string, ElementMap> defaultComponentValues;
        std::unordered_map<std::string, Own<StandardComponent>*> componentOwners;
        StandardSystemHandle handle;

        // System data storage - allows system to store arbitrary key-value data
        ElementMap systemData;

        _S_InitCallback initCallback;
        std::string initScript;
        _S_InitCallback compiledInitScriptCallback;

        _S_EventMap eventCallbackList;
        _S_EventScriptMap eventScriptCallbackList;
        _S_EventMap eventCompiledScriptCallbackList;

        _S_EventScriptMap deferredEventScriptCallbackList;
        _S_EventMap deferredEventCompiledScriptCallbackList;
        std::queue<StandardEvent> _deferredEventQueue;

        _S_ExecuteCallback executeCallback;
        std::string executeScript;
        _S_ExecuteCallback compiledExecuteScriptCallback;
        _S_SaveCallback saveCallback;
        _S_LoadCallback loadCallback;
        _S_InitCallback firstLoadCallback;

        bool needDelta = false;
        float deltaTime = 0.0f;

        _S_DeltaCallback deltaCallback;
        std::string deltaScript;
        _S_DeltaCallback compiledDeltaScriptCallback;
    };

    template <typename... Comps>
    struct System;

    // ---------------------------------------------------------------
    // Single-dispatch helpers for registerComponents (C++17 fold)
    // ---------------------------------------------------------------

    template <typename Comp, typename Sys>
    void registerOneComponent(Sys *system, ComponentRegistry *registry, const tag<Own<Comp>>&)
    {
        LOG_THIS("System");
        LOG_INFO("System", "Registering an own to '" << typeid(Comp).name() << "' to the system.");
        static_cast<Own<Comp>*>(system)->setRegistry(registry);
    }

    template <typename Comp, typename Sys>
    void registerOneComponent(Sys *system, ComponentRegistry *registry, const tag<Ref<Comp>>&)
    {
        LOG_THIS("System");
        LOG_INFO("System", "Registering a ref to '" << typeid(Comp).name() << "' to the system.");
        static_cast<Ref<Comp>*>(system)->setRegistry(registry);
    }

    template <typename Event, typename Sys>
    void registerOneComponent(Sys *system, ComponentRegistry *registry, const tag<Listener<Event>>&)
    {
        LOG_THIS("System");
        LOG_INFO("System", "Registering a listener to event '" << typeid(Event).name() << "' to the system.");
        static_cast<Listener<Event>*>(system)->setRegistry(registry);
        system->_listenerEventNames.push_back(std::string("L:") + typeid(Event).name());
    }

    template <typename Event, typename Sys>
    void registerOneComponent(Sys *system, ComponentRegistry *registry, const tag<QueuedListener<Event>>&)
    {
        LOG_THIS("System");
        LOG_INFO("System", "Registering a queue listener to event '" << typeid(Event).name() << "' to the system.");

        system->_executionQueue.emplace_back([system]() {
            QueuedListener<Event>* castedSystem = static_cast<QueuedListener<Event>*>(system);
            while (not castedSystem->_eventQueue.empty())
            {
                const auto& event = castedSystem->_eventQueue.front();

                castedSystem->onProcessEvent(event);

                castedSystem->_eventQueue.pop();
            }
        });

        static_cast<QueuedListener<Event>*>(system)->setRegistry(registry);
        system->_listenerEventNames.push_back(std::string("Q:") + typeid(Event).name());
    }

    template <typename Sys>
    void registerOneComponent(Sys *system, ComponentRegistry *, const tag<StoragePolicy>&)
    {
        LOG_THIS("System");
        LOG_INFO("System", "Registering the system as a storage one");
        if (system->executionPolicy != ExecutionPolicy::Sequential)
            LOG_ERROR("System", "Trying to set two different execution policies !");
        system->setPolicy(ExecutionPolicy::Storage);
    }

    template <typename Sys>
    void registerOneComponent(Sys *system, ComponentRegistry *, const tag<ManualPolicy>&)
    {
        LOG_THIS("System");
        LOG_INFO("System", "Registering the system as a manual one");
        if (system->executionPolicy != ExecutionPolicy::Sequential)
            LOG_ERROR("System", "Trying to set two different execution policies !");
        system->setPolicy(ExecutionPolicy::Manual);
    }

    template <typename Sys>
    void registerOneComponent(Sys *system, ComponentRegistry *, const tag<ParallelPolicy>&)
    {
        LOG_THIS("System");
        LOG_INFO("System", "Registering the system as a parallel one");
        if (system->executionPolicy != ExecutionPolicy::Sequential)
            LOG_ERROR("System", "Trying to set two different execution policies !");
        system->setPolicy(ExecutionPolicy::Parallel);
    }

    template <typename Sys>
    void registerOneComponent(Sys *system, ComponentRegistry *, const tag<IndependentPolicy>&)
    {
        LOG_THIS("System");
        LOG_INFO("System", "Registering the system as a independent one");
        if (system->executionPolicy != ExecutionPolicy::Sequential)
            LOG_ERROR("System", "Trying to set two different execution policies !");
        system->setPolicy(ExecutionPolicy::Independent);
    }

    template <typename Sys>
    void registerOneComponent(Sys *system, ComponentRegistry *, const tag<InitSys>&)
    {
        LOG_THIS("System");
        LOG_INFO("System", "Running init");
        system->init();
    }

    template <typename Sys>
    void registerOneComponent(Sys *system, ComponentRegistry *registry, const tag<SaveSys>&)
    {
        LOG_THIS("System");
        LOG_INFO("System", "Loading system data...");

        auto name = system->__name;

        if (name != "UnNamed")
        {
            auto loaded = registry->loadSystem([system](const UnserializedObject& ss) { system->load(ss); }, name);

            if (not loaded)
                system->firstLoad();

            registry->registerSystemSaveSystem(name, [system, name](Archive& ar) {
                ar.startSerialization(name);

                system->save(ar);

                ar.endSerialization();
            });
        }
        else
        {
            LOG_ERROR("System", "Trying to load an unnamed system: " << typeid(Sys).name());
        }
    }

    // Fold-expression wrapper: one instantiation instead of N recursive ones
    template <typename Sys, typename... Comps>
    void registerComponents(Sys *system, ComponentRegistry *registry, const tag<Comps>&... comps)
    {
        (registerOneComponent(system, registry, comps), ...);
    }

    // ---------------------------------------------------------------
    // Single-dispatch helpers for unregisterComponents (C++17 fold)
    // ---------------------------------------------------------------

    template <typename Comp, typename Sys>
    void unregisterOneComponent(Sys *system, ComponentRegistry *registry, const tag<Own<Comp>>&)
    {
        LOG_THIS("System");
        LOG_INFO("System", "Unregistering an own to '" << typeid(Comp).name() << "' to the system.");
        // Todo also remove any group that is dependant to this owner
        static_cast<Own<Comp>*>(system)->unsetRegistry(registry);
    }

    template <typename Sys>
    void unregisterOneComponent(Sys *system, ComponentRegistry *registry, const tag<SaveSys>&)
    {
        LOG_THIS("System");
        LOG_INFO("System", "Saving system data...");

        auto name = system->__name;

        if (name != "UnNamed")
        {
            registry->saveSystem(name);
            registry->unregisterSystemSave(name);
        }
        else
        {
            LOG_ERROR("System", "Trying to save an unnamed system: " << typeid(Sys).name());
        }
    }

    template <typename Event, typename Sys>
    void unregisterOneComponent(Sys *system, ComponentRegistry *registry, const tag<Listener<Event>>&)
    {
        LOG_THIS("System");
        LOG_INFO("System", "Unregistering a listener to event '" << typeid(Event).name() << "' to the system.");
        static_cast<Listener<Event>*>(system)->unsetRegistry(registry);
    }

    template <typename Event, typename Sys>
    void unregisterOneComponent(Sys *system, ComponentRegistry *registry, const tag<QueuedListener<Event>>&)
    {
        LOG_THIS("System");
        LOG_INFO("System", "Unregistering a listener to event '" << typeid(Event).name() << "' to the system.");
        static_cast<QueuedListener<Event>*>(system)->unsetRegistry(registry);
    }

    // Catch-all for tags that don't need unregistration (Ref, policies, InitSys, etc.)
    template <typename Comp, typename Sys>
    void unregisterOneComponent(Sys *, ComponentRegistry *, const tag<Comp>&)
    {
        LOG_THIS("System");
        LOG_MILE("System", "Unregister is not needed for: " << typeid(Comp).name());
    }

    // Fold-expression wrapper: one instantiation instead of N recursive ones
    template <typename Sys, typename... Comps>
    void unregisterComponents(Sys *system, ComponentRegistry *registry, const tag<Comps>&... comps)
    {
        (unregisterOneComponent(system, registry, comps), ...);
    }

    template <typename... Comps>
    struct System : public AbstractSystem, public Comps...
    {
        System() : AbstractSystem(), Comps()...
        {
            LOG_THIS_MEMBER("System");
        }

        virtual ~System() override
        {
            LOG_THIS_MEMBER("System");
        }

        /**
         * @brief Send an event to the whole ECS
         *
         * @tparam Event Type of the event to send
         * @param event Struct holding everything about the event
         */
        template <typename Event>
        void sendEvent(const Event& event)
        {
            LOG_THIS_MEMBER("System");

            if (this->registry)
                this->registry->processEvent(event);
        }

        /**
         * @brief Register the current system to the registry
         *
         * @param registry Main registry of the current ECS
         */
        virtual void addToRegistry(ComponentRegistry *registry)
        {
            LOG_THIS_MEMBER("System");

            this->registry = registry;

            // Set the current name of the system
            __name = getSystemName();

            registerComponents(this, registry, tag<Comps>{}...);

            if ((executionPolicy == ExecutionPolicy::Manual or executionPolicy == ExecutionPolicy::Storage) and _executionQueue.size() > 0)
            {
                LOG_WARNING("System", "Trying to add a QueuedListener to a system(" << __name << ") that will not have an execute call. (Remove the Manual/Storage policy or call execute())");
            }

            onRegisterFinished();
        }

        /**
         * @brief Remove a component from the registry
         */
        virtual void removeFromRegistry() override
        {
            LOG_THIS_MEMBER("System");

            if (registry)
            {
                unregisterComponents(this, registry, tag<Comps>{}...);

                registry->removeTypeId(_id);

                registry = nullptr;
            }
        }

        /**
         * @brief Create a Component object
         *
         * @tparam Type Type of the component to create
         * @tparam Args Type of the arguments to pass to the component constructor
         * @param entity Entity to bind the component to
         * @param args Argument to pass to the component constructor
         * @return Type* A pointer to the component created
         *
         * @todo Move those function (create component and delete component) in the Own and Ref struct
         * And call it create component so the system inherit of the correct one
         */
        template <typename Type, typename... Args>
        inline Type* createComponent(Entity* entity, Args&&... args)
        {
            LOG_THIS_MEMBER("System");

            return this->createRefferedComponent<Type>(entity, std::forward<Args>(args)...);
        }

        template <typename Type, typename... Args>
        inline Type* createOwnedComponent(Entity* entity, Args&&... args)
        {
            LOG_THIS_MEMBER("System");

            return this->Own<Type>::internalCreateComponent(entity, std::forward<Args>(args)...);
        }

        template <typename Type, typename... Args>
        inline Type* createRefferedComponent(Entity* entity, Args&&... args)
        {
            LOG_THIS_MEMBER("System");

            return this->Ref<Type>::internalCreateComponent(entity, std::forward<Args>(args)...);
        }

        template <typename Type>
        inline void removeComponent(Entity* entity)
        {
            LOG_THIS_MEMBER("System");

            this->removeRefferedComponent(entity);
        }

        template <typename Type>
        void removeRefferedComponent(Entity* entity)
        {
            LOG_THIS_MEMBER("System");

            this->Ref<Type>::internalRemoveComponent(entity);
        }

        template <typename Type>
        inline void removeOwnedComponent(Entity* entity)
        {
            LOG_THIS_MEMBER("System");

            this->Own<Type>::internalRemoveComponent(entity);
        }

        template <typename Comp>
        Comp* atEntity(_unique_id id) const
        {
            LOG_THIS_MEMBER("System");

            // Todo enable this only if the system own this comp !
            return this->Own<Comp>::getComponent(id);
        }

        template <typename Type>
        inline typename ComponentSet<Type>::ComponentSetList view() const
        {
            LOG_THIS_MEMBER("System");

            return this->Ref<Type>::view();
        }

        template <typename Type, typename... Types>
        Group<Type, Types...>* registerGroup() const
        {
            LOG_THIS_MEMBER("System");

            if (registry == nullptr)
            {
                LOG_ERROR("System", "No registry specified, can't create a group");
                return nullptr;
            }

            // Todo remove this as those are just used for debug and log, but still polutes the system with registry stuff
            // Track this group type once (viewGroup() calls us on every execute, so deduplicate)
            const char* groupTypeName = typeid(Group<Type, Types...>).name();
            bool alreadyTracked = false;
            for (const auto& g : _registeredGroupNames)
                if (g == groupTypeName) { alreadyTracked = true; break; }
            if (not alreadyTracked)
                _registeredGroupNames.push_back(groupTypeName);

            const auto& groupId = registry->getTypeId<Group<Type, Types...>>();

            if (registry->hasGroup(groupId))
                return registry->retrieveGroup<Type, Types...>();
            else
            {
                LOG_INFO("System", "Creating new group");

                auto group = new Group<Type, Types...>(groupId);
                //
                group->setRegistry(registry);
                group->process();

                return group;
            }
        }

        template <typename Type, typename... Types>
        inline typename ComponentSet<GroupElement<Type, Types...>>::ComponentSetList viewGroup() const
        {
            LOG_THIS_MEMBER("System");

            return this->registerGroup<Type, Types...>()->elements.viewComponents();
        }
    };
}