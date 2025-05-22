#pragma once

#include <string>
#include <unordered_map>

#include "entity.h"
#include "componentregistry.h"
#include "group.h"
#include "eventlistener.h"

#include "logger.h"

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

        // Todo make function onAdd and onDelete of a component that default to nothing if not used
    };

    template <typename... Comps>
    struct System;

    template <typename Sys>
    void registerComponents(Sys*, ComponentRegistry*) { LOG_THIS("System"); }

    template <typename Comp, typename... Comps, typename Sys>
    void registerComponents(Sys *system, ComponentRegistry *registry, const tag<Own<Comp>>&, const Comps&... comps)
    {
        LOG_THIS("System");

        LOG_INFO("System", "Registering an own to '" << typeid(Comp).name() << "' to the system.");

        static_cast<Own<Comp>*>(system)->setRegistry(registry);
        registerComponents(system, registry, comps...);
    }

    template <typename Comp, typename... Comps, typename Sys>
    void registerComponents(Sys *system, ComponentRegistry *registry, const tag<Ref<Comp>>&, const Comps&... comps)
    {
        LOG_THIS("System");

        LOG_INFO("System", "Registering a ref to '" << typeid(Comp).name() << "' to the system.");

        static_cast<Ref<Comp>*>(system)->setRegistry(registry);
        registerComponents(system, registry, comps...);
    }

    template <typename Event, typename... Comps, typename Sys>
    void registerComponents(Sys *system, ComponentRegistry *registry, const tag<Listener<Event>>&, const Comps&... comps)
    {
        LOG_THIS("System");

        LOG_INFO("System", "Registering a listener to event '" << typeid(Event).name() << "' to the system.");

        static_cast<Listener<Event>*>(system)->setRegistry(registry);
        registerComponents(system, registry, comps...);
    }

    template <typename Event, typename... Comps, typename Sys>
    void registerComponents(Sys *system, ComponentRegistry *registry, const tag<QueuedListener<Event>>&, const Comps&... comps)
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
        registerComponents(system, registry, comps...);
    }

    template <typename... Comps, typename Sys>
    void registerComponents(Sys *system, ComponentRegistry *registry, const tag<StoragePolicy>&, const Comps&... comps)
    {
        LOG_THIS("System");

        LOG_INFO("System", "Registering the system as a storage one");

        if (system->executionPolicy != ExecutionPolicy::Sequential)
        {
            LOG_ERROR("System", "Trying to set two different execution policies !");
        }

        system->setPolicy(ExecutionPolicy::Storage);

        registerComponents(system, registry, comps...);
    }

    template <typename... Comps, typename Sys>
    void registerComponents(Sys *system, ComponentRegistry *registry, const tag<ManualPolicy>&, const Comps&... comps)
    {
        LOG_THIS("System");

        LOG_INFO("System", "Registering the system as a manual one");

        if (system->executionPolicy != ExecutionPolicy::Sequential)
        {
            LOG_ERROR("System", "Trying to set two different execution policies !");
        }

        system->setPolicy(ExecutionPolicy::Manual);

        registerComponents(system, registry, comps...);
    }

    template <typename... Comps, typename Sys>
    void registerComponents(Sys *system, ComponentRegistry *registry, const tag<ParallelPolicy>&, const Comps&... comps)
    {
        LOG_THIS("System");

        LOG_INFO("System", "Registering the system as a parallel one");

        if (system->executionPolicy != ExecutionPolicy::Sequential)
        {
            LOG_ERROR("System", "Trying to set two different execution policies !");
        }

        system->setPolicy(ExecutionPolicy::Parallel);

        registerComponents(system, registry, comps...);
    }

    template <typename... Comps, typename Sys>
    void registerComponents(Sys *system, ComponentRegistry *registry, const tag<IndependentPolicy>&, const Comps&... comps)
    {
        LOG_THIS("System");

        LOG_INFO("System", "Registering the system as a independent one");

        if (system->executionPolicy != ExecutionPolicy::Sequential)
        {
            LOG_ERROR("System", "Trying to set two different execution policies !");
        }

        system->setPolicy(ExecutionPolicy::Independent);

        registerComponents(system, registry, comps...);
    }

    template <typename... Comps, typename Sys>
    void registerComponents(Sys *system, ComponentRegistry *registry, const tag<InitSys>&, const Comps&... comps)
    {
        LOG_THIS("System");

        LOG_INFO("System", "Running init");

        system->init();

        registerComponents(system, registry, comps...);
    }

    template <typename... Comps, typename Sys>
    void registerComponents(Sys *system, ComponentRegistry *registry, const tag<SaveSys>&, const Comps&... comps)
    {
        LOG_THIS("System");

        LOG_INFO("System", "Loading system data...");

        auto name = system->__name;

        if (name != "UnNamed")
        {
            auto loaded = registry->loadSystem([system](const UnserializedObject& ss) { system->load(ss); }, name);

            if (not loaded)
                system->firstLoad();
        }
        else
        {
            LOG_ERROR("System", "Trying to load an unnamed system: " << typeid(Sys).name());
        }

        registerComponents(system, registry, comps...);
    }

    template <typename Sys>
    void unregisterComponents(Sys*, ComponentRegistry*) { LOG_THIS("System"); }

    template <typename Comp, typename... Comps, typename Sys>
    void unregisterComponents(Sys *system, ComponentRegistry *registry, const tag<Own<Comp>>&, const Comps&... comps)
    {
        LOG_THIS("System");

        LOG_INFO("System", "Unregistering an own to '" << typeid(Comp).name() << "' to the system.");

        // Todo also remove any group that is dependant to this owner

        static_cast<Own<Comp>*>(system)->unsetRegistry(registry);
        unregisterComponents(system, registry, comps...);
    }

    template <typename... Comps, typename Sys>
    void unregisterComponents(Sys *system, ComponentRegistry *registry, const tag<SaveSys>&, const Comps&... comps)
    {
        LOG_THIS("System");

        LOG_INFO("System", "Saving system data...");

        auto name = system->__name;
        if (name != "UnNamed")
        {
            registry->saveSystem([system, name](Archive& ar) {
                ar.startSerialization(name);

                system->save(ar);

                ar.endSerialization();
            }, name);
        }
        else
        {
            LOG_ERROR("System", "Trying to save an unnamed system: " << typeid(Sys).name());
        }

        unregisterComponents(system, registry, comps...);
    }

    template <typename Event, typename... Comps, typename Sys>
    void unregisterComponents(Sys *system, ComponentRegistry *registry, const tag<Listener<Event>>&, const Comps&... comps)
    {
        LOG_THIS("System");

        LOG_INFO("System", "Unregistering a listener to event '" << typeid(Event).name() << "' to the system.");

        static_cast<Listener<Event>*>(system)->unsetRegistry(registry);
        unregisterComponents(system, registry, comps...);
    }

    template <typename Event, typename... Comps, typename Sys>
    void unregisterComponents(Sys *system, ComponentRegistry *registry, const tag<QueuedListener<Event>>&, const Comps&... comps)
    {
        LOG_THIS("System");

        LOG_INFO("System", "Unregistering a listener to event '" << typeid(Event).name() << "' to the system.");

        static_cast<QueuedListener<Event>*>(system)->unsetRegistry(registry);
        unregisterComponents(system, registry, comps...);
    }

    template <typename Comp, typename... Comps, typename Sys>
    void unregisterComponents(Sys *system, ComponentRegistry *registry, const tag<Comp>&, const Comps&... comps)
    {
        LOG_THIS("System");

        LOG_MILE("System", "Unregister is not needed for: " << typeid(Comp).name());

        unregisterComponents(system, registry, comps...);
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
                LOG_WARNING("System", "Trying to add a QueuedListener to a system that will not have an execute call. (Remove the Manual/Storage policy or call execute())");
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