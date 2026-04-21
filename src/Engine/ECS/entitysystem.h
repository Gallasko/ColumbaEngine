#pragma once

#include <vector>
#include <unordered_map>
#include <algorithm>

#include "componentregistry.h"
#include "entity.h"
#include "system.h"

#include "serialization.h"

#include "logger.h"
#include "Memory/memorypool.h"

#include "commanddispatcher.h"
#include "savemanager.h"

#include <iostream>

#ifdef PROFILE
#include <atomic>
#include <mutex>
extern std::mutex profileMutex;
// Profiling data

extern std::unordered_map<std::string, long long> _systemExecutionTimes;
extern std::unordered_map<std::string, size_t> _systemExecutionCounts;

#include "Profiler/profiler.h"
#endif

namespace pg
{
    enum class VmOptimizationLevel
    {
        O0 = 0,
        O1,
        O2,
        O3
    };

    // Todo create a queue that hold all entity id that got deleted to reattribute them later on

    // Todo Create a different id gen for systems so that components id are smaller and more packed

    // Forward declarations
    class ComponentRegistry;
    struct AbstractSystem;
    class InterpreterSystem;
    class Environment;
    class ClassInstance;
    class StandardSystemImpl;

    struct VM;
    typedef uint64_t Value;

    // Todo add batching for entity and component creation/deletion

    template <class T>
    class HasOnCreation
    {
        template <class U, class = typename std::enable_if<!std::is_member_pointer<decltype(&U::onCreation)>::value>::type>
            static std::true_type check(int);
        template <class>
            static std::false_type check(...);

    public:
        static constexpr bool value = decltype(check<T>(0))::value;
    };

    template<typename> inline constexpr bool always_false = false;

    // Helper to check if first argument is convertible to string (only if Args is non-empty)
    template<typename... Args>
    struct first_arg_is_string : std::false_type {};

    template<typename First, typename... Rest>
    struct first_arg_is_string<First, Rest...> : std::is_convertible<First, std::string> {};

    class EntitySystem
    {
    friend class Entity;
    friend struct EntityRef;
    friend class CommandDispatcher;
    friend struct CoreModule;
    friend struct InputModule;
    friend struct OnEventComponent;
    friend struct OnStandardEventComponent;
    friend class StandardSystemImpl;

    private:
        class EventDispatcher
        {
        public:
            EventDispatcher() {};

            inline bool enqueueEvent(std::function<void()>&& event)
            {
                return events.enqueue(event);
            }

            void process()
            {
                std::function<void()> event;

                while (events.try_dequeue(event))
                {
                    event();
                }
            }

        private:
            moodycamel::ConcurrentQueue<std::function<void()>> events;
        };

    public:
        EntitySystem(const std::string& savePath = "save/savedata.sz");
        ~EntitySystem();

        /**
         * @brief Start the running thread of the ecs and loop through the taskflow
         */
        inline void start()
        {
            LOG_THIS_MEMBER("ECS");

            if (running)
                return;

            stopRequested = false;
            running = true;

            runningThread = std::thread(&EntitySystem::executeAll, this);
        }

        /**
         * @brief Start the ecs without starting the execute thread loop, Used mainly for testing purposes
         *
         * @warning This function is mainly used for testing purposes, and it does not guarantee that the ECS will run properly. @see start() if you don't know what you're doing.
         */
        inline void fakeStart()
        {
            LOG_THIS_MEMBER("ECS");

            if (running)
                return;

            stopRequested = false;
            running = true;
        }

        /**
         * @brief Stop the running thread of the ECS
         */
        void stop();

        /**
         * @brief Save the underlaying registry to file
         */
        void saveSystems() const
        {
            registry.saveRegistry();
        }

        /**
         * @brief Generate a new unique identifier (on a 64bit generator)
         *
         * @return _unique_id A unique identifier for Systems and Entities
         */
        inline _unique_id generateId() noexcept
        {
            return registry.idGenerator.generateId();
        }

        /**
         * @brief Create a Entity object
         *
         * @return EntityRef A reference object to the entity created
         */
        EntityRef createEntity();

        /**
         * @brief Create a Entity object
         *
         * @param name Name of the entity
         *
         * @return EntityRef A reference object to the entity created
         */
        EntityRef createEntity(const std::string& name);

        /**
         * @brief Create multiple Entity objects at once
         *
         * When the ECS is not running, entity ids are allocated in a single contiguous
         * block via the id generator and all memory (sparse set, component pool) is
         * pre-reserved before the insertion loop, avoiding repeated reallocation.
         *
         * @param count Number of entities to create
         * @return std::vector<EntityRef> A vector of reference objects to the entities created, in creation order
         */
        std::vector<EntityRef> createEntities(size_t count);

        /**
         * @brief Remove an Entity object
         *
         * @param entity Pointer to the entity to delete from the ecs (Remove it from the entity pool)
         */
        void removeEntity(Entity* entity);

        /**
         * @brief Overload of the removeEntity function
         *
         * @param id Id of the entity to delete
         */
        void removeEntity(_unique_id id)
        {
            removeEntity(getEntity(id));
        }

        /**
         * @brief Create a new system in place and put it in the taskflow
         *
         * All the different option are set during contruction of the system check the ctor of System for more info
         *
         * @tparam Sys The type of the system to create
         * @tparam Args The types of the arguments of the system
         * @param args The arguments to pass to the ctor of the newly created system
         * @return Sys* A pointer to the newly created system
         */
        template <class Sys, bool Bypass = false, typename... Args>
        Sys* createSystem(const Args&... args)
        {
            LOG_THIS_MEMBER("ECS");

            // Todo: add support for system creation during runtime
            if (not Bypass and running)
            {
                LOG_ERROR("ECS", "System creation during runtime is not supported");
                return nullptr;
            }

            auto system = new Sys(args...);

            system->_id = registry.getTypeId<Sys>();

            system->ecsRef = this;

            systems.emplace(system->_id, system);

            system->addToRegistry(&registry);

            internalCreateSystem(system);

            return system;
        }

        /**
         * @brief Register a system previously by the user and put it in the taskflow
         *
         * StandardSystem should be use to create the system beforehand as all the helper and requiered functions are properly built in
         *
         * @param sys The system to add to the ecs
         * @return StandardSystemImpl The system given by the user
         */
        StandardSystemImpl* registerSystem(StandardSystemImpl* sys)
        {
            LOG_THIS_MEMBER("ECS");

            // Todo: add support for system registration during runtime
            if (running)
            {
                LOG_ERROR("ECS", "System registration during runtime is not supported");
                return sys;
            }

            auto name = sys->getSystemName();

            sys->_id = registry.getTypeId(name);

            sys->ecsRef = this;

            systems.emplace(sys->_id, sys);

            sys->addToRegistry(&registry);

            internalCreateSystem(sys);

            return sys;
        }

        template <class Sys>
        void deleteSystem()
        {
            LOG_THIS_MEMBER("ECS");

            // Todo: add support for system deletion during runtime
            if (running)
            {
                LOG_ERROR("ECS", "System deletion during runtime is not supported");
                return;
            }

            auto id = registry.getTypeId<Sys>();

            _deleteSystem(id);
        }

        template <class Sys, class DerivedSys, typename... Args>
        DerivedSys* createMockSystem(const Args&... args)
        {
            LOG_THIS_MEMBER("ECS");

            auto sys = new DerivedSys(args...);

            Sys* system = static_cast<Sys*>(sys);

            system->_id = registry.getTypeId<Sys>();

            system->ecsRef = this;

            systems.emplace(system->_id, system);

            system->addToRegistry(&registry);

            internalCreateSystem(system);

            return sys;
        }

        InterpreterSystem* createInterpreterSystem(std::shared_ptr<Environment> env, std::shared_ptr<ClassInstance> sysInstance);

        /**
         * Overload of deleteSystem mainly used for deleting Interpreter system
         *
         * @param id Id of the system to delete
         */
        void deleteSystem(_unique_id id);

        // Todo make proceed
        template <typename SysAfter, typename SysBefore>
        void succeed()
        {
            LOG_THIS_MEMBER("ECS");

            _unique_id sys1Id = registry.getTypeId<SysAfter>();
            _unique_id sys2Id = registry.getTypeId<SysBefore>();

            _succeed(sys1Id, sys2Id);
        }

        void succeed(const std::string& sysAfter, const std::string& sysBefore)
        {
            LOG_THIS_MEMBER("ECS");

            _unique_id sys1Id = registry.getTypeId(sysAfter);
            _unique_id sys2Id = registry.getTypeId(sysBefore);

            _succeed(sys1Id, sys2Id);
        }

        template <typename SysAfter>
        void succeed(const std::string& sysBefore)
        {
            LOG_THIS_MEMBER("ECS");

            _unique_id sys1Id = registry.getTypeId<SysAfter>();
            _unique_id sys2Id = registry.getTypeId(sysBefore);

            _succeed(sys1Id, sys2Id);
        }

        template <typename SysBefore>
        void precede(const std::string& sysAfter)
        {
            LOG_THIS_MEMBER("ECS");

            _unique_id sys1Id = registry.getTypeId(sysAfter);
            _unique_id sys2Id = registry.getTypeId<SysBefore>();

            _succeed(sys1Id, sys2Id);
        }

        /** Dump the taskflow graph in Graphviz DOT format.
         *  @param showEventNodes  When true (default), inject coloured event/group listener nodes.
         *  @param outputFile      When non-empty, write to this file path instead of stdout. */
        void dumbTaskflow(bool showEventNodes = true, const std::string& outputFile = "") const;

        //TODO make a template specialization capable of attaching an entity to an entity

        template <class Sys>
        inline Sys* getSystem() const noexcept
        {
            LOG_THIS_MEMBER("ECS");

            const auto& id = registry.getTypeId<Sys>();

            try
            {
                const auto& it = systems.find(id);

                if (it != systems.end())
                    return static_cast<Sys*>(systems.at(id));
                else
                    return nullptr;
            }
            catch (const std::exception& e)
            {
                LOG_ERROR("ECS", "Get system failed: " << e.what());
                return nullptr;
            }
        }

        // Todo fix attach doesn't work if an args is a const std::string&

        template <typename Type>
        auto registerFlagComponent()
        {
            return registry.registerFlagComponent<Type>();
        }

        template <typename Type, typename... Args>
        CompRef<Type> attach(EntityRef entity, Args&&... args)
        {
            // As component deriving from Ctor, check for Ctor presence is enough
            if constexpr(not std::is_base_of_v<Ctor, Type>)
            {
                static_assert(always_false<Type>,
                    "Not attaching a struct deriving from Component (or Ctor); use attachGeneric instead!");
            }

            return attachGeneric<Type>(entity, std::forward<Args>(args)...);
        }

        // Overload for StandardComponent - takes component name as first argument after entity
        template <typename... Args>
        CompRef<StandardComponent> attach(EntityRef entity, const std::string& componentName, Args&&... args)
        {
            return attachGeneric(entity, componentName, std::forward<Args>(args)...);
        }

        template <typename Type, typename... Args>
        CompRef<Type> attachGeneric(EntityRef entity, Args&&... args) noexcept
        {
            // Check if trying to attach StandardComponent without a name
            if constexpr (std::is_same_v<Type, StandardComponent>)
            {
                // return _attach<Type>(entity, std::forward<Args>(args)...);
                static_assert(always_false<Type>,
                    "Cannot attach StandardComponent without specifying component name! "
                    "Use: ecs.attachGeneric(entity, \"ComponentName\") instead of ecs.attachGeneric<StandardComponent>(entity)");
            }

            // Single combined lookup: avoids hasTypeId (find 1) + _attach→retrieve→getTypeId (find 2)
            auto* owner = registry.tryRetrieve<Type>();
            if (not owner)
            {
                LOG_WARNING("ECS", "Component [" << typeid(Type).name() << "] is not registered in the ECS, attaching it to the default flag system instead");
                LOG_WARNING("ECS", "This is a costly operation to do during runtime, you should register the component in the ECS using registerFlagComponent<Type>()");

                registerFlagComponent<Type>();
                owner = registry.retrieve<Type>();
            }

            return _attach<Type>(entity, owner, std::forward<Args>(args)...);
        }

        template <typename... Args>
        CompRef<StandardComponent> attachGeneric(EntityRef entity, const std::string& name, Args&&... args) noexcept
        {
            if (not registry.hasStandardComponent(name))
            {
                LOG_ERROR("ECS", "Trying to attach a non registered standard component: " << name);
            }

            return _attach(entity, name, std::forward<Args>(args)...);
        }

        template <typename Type, typename... Args,
                  typename = std::enable_if_t<!std::is_same_v<Type, StandardComponent> || !first_arg_is_string<Args...>::value>>
        CompRef<Type> _attach(EntityRef entity, Args&&... args) noexcept
        {
            try
            {
                Type* component;

                // Todo add lock a mutex for running to protect for race conditions or only build component with the cmdDispatcher
                if (running)
                {
                    component = cmdDispatcher.attachComp<Type>(entity, std::forward<Args>(args)...);

                    // Register in pending map for immediate access via get()
                    entity.entity->pendingComponents[getId<Type>()] = component;
                }
                else
                {
                    component = registry.retrieve<Type>()->internalCreateComponent(entity, std::forward<Args>(args)...);
                }

                auto res = CompRef<Type>(component, entity.id, this, not running);

                if constexpr(std::is_base_of_v<Ctor, Type>)
                    res->onCreation(entity);

                // Todo make the systems capable of triggering on a component creation

                return res;
            }
            catch (const std::exception& e)
            {
                LOG_ERROR("ECS", "Can't attach component [" << typeid(Type).name() << "]: " << e.what() << " (No system own this component ?)");
            }

            return CompRef<Type>();
        }

        // Overload that accepts a pre-fetched owner, bypassing the retrieve->getTypeId lookup
        template <typename Type, typename... Args>
        CompRef<Type> _attach(EntityRef entity, Own<Type>* owner, Args&&... args) noexcept
        {
            try
            {
                Type* component;

                if (running)
                {
                    component = cmdDispatcher.attachComp<Type>(entity, std::forward<Args>(args)...);

                    // Register in pending map for immediate access via get()
                    entity.entity->pendingComponents[getId<Type>()] = component;
                }
                else
                {
                    component = owner->internalCreateComponent(entity, std::forward<Args>(args)...);
                }

                auto res = CompRef<Type>(component, entity.id, this, not running);

                if constexpr(std::is_base_of_v<Ctor, Type>)
                    res->onCreation(entity);

                return res;
            }
            catch (const std::exception& e)
            {
                LOG_ERROR("ECS", "Can't attach component [" << typeid(Type).name() << "]: " << e.what() << " (No system own this component ?)");
            }

            return CompRef<Type>();
        }

        template <typename... Args>
        CompRef<StandardComponent> _attach(EntityRef entity, const std::string& compName, Args&&... args) noexcept
        {
            try
            {
                StandardComponent* component;

                // Todo add lock a mutex for running to protect for race conditions or only build component with the cmdDispatcher
                if (running)
                {
                    component = cmdDispatcher.attachComp<StandardComponent>(entity, compName, std::forward<Args>(args)...);

                    // Register in pending map for immediate access via get()
                    entity.entity->pendingComponents[registry.retrieveStandardComponent(compName)->getId()] = component;
                }
                else
                {
                    component = registry.retrieveStandardComponent(compName)->internalCreateComponent(entity, std::forward<Args>(args)...);
                }

                LOG_MILE("ECS", "Attached StandardComponent [" << compName << "] to entity [" << entity.id << "]");

                auto res = CompRef<StandardComponent>(component, entity.id, this, not running, compName);

                res->onCreation(entity);

                // Todo make the systems capable of triggering on a component creation
                return res;
            }
            catch (const std::exception& e)
            {
                LOG_ERROR("ECS", "Can't attach component [" << compName << "]: " << e.what() << " (No system own this component ?)");
            }

            return CompRef<StandardComponent>();
        }

        // template <typename Type, typename EntityHolderType, typename... Args>
        // CompRef<Type> attach(EntityHolderType entity, Args&&... args) noexcept { return attach(entity.entity, args...); }

        void deserializeComponent(EntityRef entity, const UnserializedObject& serializedObject) noexcept
        {
            registry.deserializeComponentToEntity(serializedObject, entity);
        }

        void detach(const std::string& name, EntityRef entity) noexcept
        {
            registry.detachComponentFromEntity(name, entity);
        }

        template <typename Type>
        void detach(Entity* entity) noexcept
        {
            if (not entity)
                return;

            // Single lookup replacing hasTypeId (find 1) + getTypeId (find 2)
            const auto id = registry.tryGetTypeId<Type>();
            if (id == 0)
            {
                LOG_ERROR("ECS", "Component [" << typeid(Type).name() << "] is not registered in the ECS");
                return;
            }

            try
            {
                if (running)
                {
                    cmdDispatcher.detachComp(entity, id);
                }
                else
                {
                    registry.detachComponentFromEntity(entity, id);
                }
            }
            catch (const std::exception& e)
            {
                LOG_ERROR("ECS", "Can't detach component [" << id << "] from entity [" << entity->id << "]: " << e.what());
            }
        }

        template <typename Event>
        void sendEvent(const Event& event, bool isDeferred = false)
        {
            LOG_THIS_MEMBER("ECS");

            // Select the appropriate dispatcher based on event type
            auto& dispatcher = isDeferred ? deferredEventDispatcher : eventDispatcher;

            // Dispatch the typed C++ event
            if (running)
            {
                dispatcher.enqueueEvent([event, this](){ LOG_THIS("ECS"); registry.processEvent(event); });
            }
            else
            {
                registry.processEvent(event);
            }

#ifdef PG_AUTO_CONVERT_EVENTS_TO_STANDARD
            // Auto-convert to StandardEvent if the event type supports it
            if constexpr (has_to_standard_event_v<Event>)
            {
                StandardEvent stdEvent = event.toStandardEvent();

                if (running)
                {
                    dispatcher.enqueueEvent([stdEvent, this](){
                        LOG_THIS("ECS");
                        registry.processEvent(stdEvent);
                    });
                }
                else
                {
                    registry.processEvent(stdEvent);
                }
            }
#endif
        }

        template <typename Comp>
        inline _unique_id getId() const noexcept { LOG_THIS_MEMBER("ECS"); return registry.getTypeId<Comp>(); }

        void executeOnce();

        void executeAll();

        /** Return the registry of the ECS, mainly for testing purposes */
        inline constexpr const ComponentRegistry* getComponentRegistry() const noexcept { return &registry; }

        inline ComponentRegistry* getComponentRegistry() noexcept { return &registry; }

        inline size_t getNbEntities() const { LOG_THIS_MEMBER("ECS"); return entityPool.nbElements() - 1; }

        inline Entity* getEntity(_unique_id id) const { LOG_THIS_MEMBER("ECS"); return entityPool.atEntity(id); }

        Entity* getEntity(const std::string& name) const;

        template <typename Comp>
        inline Comp* getComponent(_unique_id id) const
        {
            LOG_THIS_MEMBER("ECS");

            try
            {
                return registry.retrieve<Comp>()->getComponent(id);
            }
            catch (const std::exception& e)
            {
                LOG_WARNING("ECS", "Can't get component [" << typeid(Comp).name() << "] from entity [" << id << "]: " << e.what());
                return nullptr;
            }
        }

        inline StandardComponent* getComponent(const std::string& compName, _unique_id id) const
        {
            LOG_THIS_MEMBER("ECS");

            try
            {
                return registry.retrieveStandardComponent(compName)->getComponent(id);
            }
            catch (const std::exception& e)
            {
                LOG_WARNING("ECS", "Can't get component [" << compName << "] from entity [" << id << "]: " << e.what());
                return nullptr;
            }
        }

        inline ComponentSet<Entity>::ComponentSetList view() const
        {
            LOG_THIS_MEMBER("ECS");

            return entityPool.viewComponents();
        }

        inline const std::map<_unique_id, AbstractSystem*>& getSystems() const { return systems; }

        template <typename Comp>
        inline typename ComponentSet<Comp>::ComponentSetList view() const
        {
            LOG_THIS_MEMBER("System");

            return registry.retrieve<Comp>()->view();
        }

        inline ElementType getSavedData(const std::string& id) const { return saveManager.getValue(id); }

        /** Force an immediate save (used by browser lifecycle events in Emscripten). */
        inline void forceSaveNow()
        {
            saveManager.forceSave();

            registry.saveAllSystems();
        }

        inline bool isRunning() const { return running; }

        inline size_t getNbSystems() const { return systems.size(); }

        size_t getNbTasks() const;

        inline size_t getCurrentNbOfExecution() const { return currentNbOfExecution; }
        inline size_t getTotalNbOfExecution() const { return totalNbOfExecution; }

        void reportSystemProfiles();

        inline void setVMOptimizationLevel(const VmOptimizationLevel& level)
        {
            vmOptimizationLevel = level;
        }

        void setupVm(VM& vm);

        /**
         * @brief Register a custom VM module that will be added to all VMs created by this ECS
         *
         * This allows game-specific native modules to be available in all scripts (systems, events, etc.)
         * The module must be movable or copyable.
         *
         * @param name The name to import the module as (e.g., "particle" for import "particle")
         * @param module The native module instance (must be movable/copyable)
         *
         * Example:
         * @code
         * ecs.registerCustomVmModule("particle", ParticleModule{&ecs});
         * @endcode
         */
        template<typename ModuleType>
        void registerCustomVmModule(const std::string& name, ModuleType&& module);

        // Helper for storing module registration
        void addCustomVmModuleRegistrar(std::function<void(VM&)>&& registrar)
        {
            customVmModules.push_back(std::move(registrar));
        }

    private:
        // Todo maybe
        // friend void serialize<>(Archive& archive, const EntitySystem& ecs);

        void setOptimizationPasses(VM& vm);

        // Setup full build modules (implemented in entitysystem_full.cpp or entitysystem_minimal.cpp)
        void setupVmFullModules(VM& vm);

        void internalCreateSystem(AbstractSystem* system);

        void _deleteSystem(_unique_id id);

        void _succeed(_unique_id id1, _unique_id id2);

        void addEntityToPool(Entity* entity)
        {
            LOG_THIS_MEMBER("ECS");

            entityPool.addComponent(entity, *entity);
        }

        void deleteEntityFromPool(Entity* entity)
        {
            LOG_THIS_MEMBER("ECS");

            if (entity == nullptr)
            {
                LOG_ERROR("ECS", "Entity doesn't exists !");

                return;
            }

            if (entity->id == 0)
            {
                LOG_ERROR("ECS", "Trying do delete an entity that cannot exists !");

                return;
            }

            auto compList = entity->componentList;

            for (auto _id : compList)
            {
                try
                {
                    registry.detachComponentFromEntity(entity, _id);
                }
                catch (const std::exception& e)
                {
                    LOG_ERROR("ECS", "Can't detach component [" << _id << "] from entity [" << entity->id << "]: " << e.what());
                }
            }

            entityPool.removeComponent(entity);
        }

        template <typename Type>
        void addComponentToPool(EntityRef entity, Type* component)
        {
            LOG_THIS_MEMBER("ECS");

            if (component)
            {
                LOG_MILE("ECS", "addComponentToPool");

                // Todo add a mechanism to avoid creating a component that is already attached to the entity

                try
                {
                    registry.retrieve<Type>()->internalCreateComponent(entity, *component);
                }
                catch (const std::exception& e)
                {
                    LOG_ERROR("ECS", "Can't attach component [" << typeid(Type).name() << "]: " << e.what() << " (No system own this component ?)");
                }
            }
        }

        void addComponentToPool(EntityRef entity, StandardComponent* component)
        {
            LOG_THIS_MEMBER("ECS");

            if (component)
            {
                LOG_MILE("ECS", "addComponentToPool");

                // Todo add a mechanism to avoid creating a component that is already attached to the entity

                try
                {
                    registry.retrieveStandardComponent(component->typeName)->internalCreateComponent(entity, *component);
                }
                catch (const std::exception& e)
                {
                    LOG_ERROR("ECS", "Can't attach standard component [" << component->typeName << "]: " << e.what() << " (No system own this component ?)");
                }
            }
        }

        template <typename Type>
        void detachComponentFromPool(Entity* entity)
        {
            LOG_THIS_MEMBER("ECS");

            try
            {
                // TODO: Add this somewhere (ie in ecs.detach or in sparset.removeComponent)
                if constexpr(std::is_base_of_v<Dtor, Type>)
                {
                    auto res = getComponent<Type>(entity->id);
                    res->onDeletion(entity);
                }

                registry.retrieve<Type>()->internalRemoveComponent(entity);
            }
            catch (const std::exception& e)
            {
                LOG_ERROR("ECS", e.what());
            }
        }

        bool running = false;
        bool stopRequested = false;

        VmOptimizationLevel vmOptimizationLevel = VmOptimizationLevel::O3;

        /** Track the number of executed taskflows (for debug purposes) */
        size_t currentNbOfExecution = 0;
        size_t totalNbOfExecution = 0;

        ComponentRegistry registry;

        CommandDispatcher cmdDispatcher;

        EventDispatcher eventDispatcher;

        EventDispatcher deferredEventDispatcher;

        SaveManager saveManager;

        /** Store all systems added to the ECS */
        std::map<_unique_id, AbstractSystem*> systems;

        /** All the entities generated from the ECS */
        ComponentSet<Entity> entityPool;

        /** Running thread of the ECS */
        std::thread runningThread;

        /** Pimpl for taskflow types to reduce header compilation time */
        struct TaskflowImpl;
        std::unique_ptr<TaskflowImpl> taskflowImpl;

        /** Custom VM modules to be added to all VMs created by this ECS */
        std::vector<std::function<void(VM&)>> customVmModules;
    };

    template <typename Comp>
    inline bool Entity::has() const noexcept
    {
        LOG_THIS_MEMBER("Entity");

        if (not ecsRef)
        {
            LOG_ERROR("Entity", "Entity is not referenced in any ECS");

            return false;
        }

        const auto& componentId = ecsRef->getId<Comp>();

        return has(componentId);
    }

    template <typename Comp>
    bool EntityRef::has() const
    {
        if (initialized)
            return entity->template has<Comp>();
        else
        {
            // Try to find the entity in the ecs to update this ref
            auto ent = ecsRef->getEntity(id);

            return ent->template has<Comp>();
        }
    }

    template <typename Comp>
    CompRef<Comp> EntityRef::get() const
    {
        if (initialized)
            return entity->template get<Comp>();
        else
        {
            // Try to find the entity in the ecs to update this ref
            auto ent = ecsRef->getEntity(id);

            if (ent)
                return ent->template get<Comp>();
            else
            {
                return entity->template get<Comp>();
            }
        }
    }

    template <typename Comp>
    inline CompRef<Comp> Entity::get() noexcept
    {
        LOG_THIS_MEMBER("Entity");

        if (not ecsRef)
        {
            LOG_ERROR("Entity", "Entity is not referenced in any ECS");

            return CompRef<Comp>();
        }

        auto ent = ecsRef->getEntity(id);
        auto initialized = id != 0 and ent;

        const auto& componentId = ecsRef->getId<Comp>();

        // Fast path: entity is fully initialized, return the cached pointer directly without any lookup
        if (initialized)
            return CompRef<Comp>(static_cast<Comp*>(pendingComponents[componentId]), id, ecsRef, true);

        // Normal path: component is flushed and lives in the sparse set pool
        const auto& it = componentList.find(componentId);

        if (it != componentList.end())
        {
            return CompRef<Comp>(ecsRef->registry.retrieve<Comp>()->getComponent(id), id, ecsRef, initialized);
        }

        // Deferred path: component was attached while ECS is running but not yet flushed
        const auto pending = pendingComponents.find(componentId);
        if (pending != pendingComponents.end())
        {
            return CompRef<Comp>(static_cast<Comp*>(pending->second), id, ecsRef, false);
        }

        LOG_ERROR("Entity", "Entity doesn't have component: " << componentId);

        return CompRef<Comp>();
    }

    template <typename Comp, typename... Args>
    CompRef<Comp> EntityRef::attach(Args&&... args)
    {
        if (initialized)
            return entity->template attach<Comp>(std::forward<Args>(args)...);
        else
        {
            // Try to find the entity in the ecs to update this ref
            auto ent = ecsRef->getEntity(id);

            // Entity found, updating this entity ref
            if (id != 0 and ent)
            {
                entity = ent;
                initialized = true;
            }

            return entity->template attach<Comp>(std::forward<Args>(args)...);
        }
    }

    template <typename Comp, typename... Args>
    CompRef<Comp> EntityRef::attachGeneric(Args&&... args)
    {
        if (initialized)
            return entity->template attachGeneric<Comp>(std::forward<Args>(args)...);
        else
        {
            // Try to find the entity in the ecs to update this ref
            auto ent = ecsRef->getEntity(id);

            // Entity found, updating this entity ref
            if (id != 0 and ent)
            {
                entity = ent;
                initialized = true;
            }

            return entity->template attachGeneric<Comp>(std::forward<Args>(args)...);
        }
    }

    template <typename Comp, typename... Args>
    CompRef<Comp> Entity::attach(Args&&... args)
    {
        LOG_THIS_MEMBER("Entity");

        if (not ecsRef)
        {
            LOG_ERROR("Entity", "Entity is not referenced in any ECS");

            return CompRef<Comp>();
        }

        return ecsRef->template attach<Comp>(EntityRef(this, false), std::forward<Args>(args)...);
    }

    // Non-template overload for StandardComponent
    template <typename... Args>
    CompRef<StandardComponent> Entity::attach(const std::string& componentName, Args&&... args)
    {
        LOG_THIS_MEMBER("Entity");

        if (not ecsRef)
        {
            LOG_ERROR("Entity", "Entity is not referenced in any ECS");

            return CompRef<StandardComponent>();
        }

        return ecsRef->attach(EntityRef(this, false), componentName, std::forward<Args>(args)...);
    }

    template <typename Comp, typename... Args>
    CompRef<Comp> Entity::attachGeneric(Args&&... args)
    {
        LOG_THIS_MEMBER("Entity");

        if (not ecsRef)
        {
            LOG_ERROR("Entity", "Entity is not referenced in any ECS");

            return CompRef<Comp>();
        }

        return ecsRef->template attachGeneric<Comp>(EntityRef(this, false), std::forward<Args>(args)...);
    }

    template <typename Type>
    void ComponentRegistry::store(Own<Type>* owner) noexcept
    {
        LOG_THIS_MEMBER("Component Registry");

        const auto& id = getTypeId<Type>();

        // Todo see if there is a performance hit to keep this function or does it get optimized as it should be always false in production code
        // Block to find if a system is already registered in the ecs
#ifdef PROD
        if (const auto& it = componentStorageMap.find(id); it != componentStorageMap.end())
        {
            LOG_ERROR("Component Registry", "Trying to recreate a system that already existing with id: " << id << "Exiting");
            return;
        }
#endif

        componentDeleteMap.emplace(id, [owner](Entity* entity) {
            if constexpr(std::is_base_of_v<Dtor, Type>)
            {
                auto res = owner->getComponent(entity->id);
                res->onDeletion(entity);
            }

            owner->internalRemoveComponent(entity);
        });

        componentSerializeMap.emplace(id, [owner](Archive& archive, const Entity* entity) {
            if constexpr(HasStaticName<Type>::value)
            {
                serialize(archive, *(owner->getComponent(entity->id)));
            }
            else
            {
                (void)owner;
                (void)archive;
                (void)entity;
            }
        });

        // Store component type name for fast lookup
        if constexpr(HasStaticName<Type>::value)
        {
            componentTypeNameMap.emplace(id, Type::getType());

            componentDeserializeMap.emplace(Type::getType(), [this](const UnserializedObject& serializedStr, EntityRef entity) {
                if (serializedStr.isNull())
                    return;

                auto comp = deserialize<Type>(serializedStr);

                if constexpr(std::is_base_of_v<Component, Type>)
                {
                    comp.entityId = entity.id;
                    comp.ecsRef = entity.ecsRef;
                }

                ecsRef->attach<Type>(entity, comp);
            });

            componentDetachMap.emplace(Type::getType(), [this](EntityRef entity) {
                ecsRef->detach<Type>(entity);
            });
        }

        componentStorageMap.emplace(id, owner);

        owner->_componentId = id;
    }

    template <typename Type>
    void ComponentRegistry::unstore(Own<Type>*) noexcept
    {
        LOG_THIS_MEMBER("Component Registry");

        const auto& id = getTypeId<Type>();

        if (const auto& it = componentDeleteMap.find(id); it != componentDeleteMap.end())
        {
            componentDeleteMap.erase(it);
        }

        if (const auto& it = componentSerializeMap.find(id); it != componentSerializeMap.end())
        {
            componentSerializeMap.erase(it);
        }

        if constexpr(HasStaticName<Type>::value)
        {
            if (const auto& it = componentDeserializeMap.find(Type::getType()); it != componentDeserializeMap.end())
            {
                componentDeserializeMap.erase(it);
            }

            if (const auto& it = componentDetachMap.find(Type::getType()); it != componentDetachMap.end())
            {
                componentDetachMap.erase(it);
            }
        }

        if (const auto& it = componentStorageMap.find(id); it != componentStorageMap.end())
        {
            componentStorageMap.erase(it);
        }

        removeTypeId<Type>();
    }

    template <typename Type>
    auto ComponentRegistry::registerFlagComponent()
    {
        struct DummyFlagSys : public System<Own<Type>, StoragePolicy> {};

        return ecsRef->createSystem<DummyFlagSys, true>();
    }

    // Specialization for StandardComponent - does not create a dummy system
    template <>
    inline auto ComponentRegistry::registerFlagComponent<StandardComponent>()
    {
        LOG_ERROR("Component Registry", "Should never have to register a standard component flag ! Are you trying to attach a standard component without specifying the component name ?");

        // For StandardComponent, do nothing and return nullptr
        // StandardComponents are managed differently through the registry
        return static_cast<AbstractSystem*>(nullptr);
    }

    template <typename Comp>
    void CompRef<Comp>::operator=(const CompRef& rhs)
    {
        LOG_THIS_MEMBER("Comp ref");

        ecsRef      = rhs.ecsRef;
        entityId    = rhs.entityId;
        initialized = rhs.initialized;
        component   = rhs.component;

        if (not initialized)
        {
            if (entityId != 0)
            {
                auto fetchComponent = rhs.ecsRef->template getComponent<Comp>(entityId);

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
            else
            {
                LOG_ERROR("Comp ref", "Copy of a reference to an invalid entity(" << entityId << ")");
            }
        }
    }

    template <typename Comp>
    Comp* CompRef<Comp>::operator->()
    {
        if (initialized)
            return component;
        else
        {
            // Try to find the component in the ecs to update this ref
            auto comp = ecsRef->getComponent<Comp>(entityId);

            // Component found, updating this entity ref
            if (entityId != 0 and comp)
            {
                component = comp;
                initialized = true;
            }

           return component;
        }
    }

    template <typename Comp>
    Comp* CompRef<Comp>::operator->() const
    {
        if (initialized)
            return component;
        else
        {
            // Try to find the component in the ecs to update this ref
            auto comp = ecsRef->getComponent<Comp>(entityId);

            return comp;
        }
    }

    template <typename Comp>
    CompRef<Comp>::operator Comp*()
    {
        if (initialized)
            return component;
        else
        {
            // Try to find the component in the ecs to update this ref
            auto comp = ecsRef->getComponent<Comp>(entityId);

            // Component found, updating this entity ref
            if (entityId != 0 and comp)
            {
                component = comp;
                initialized = true;
            }

           return component;
        }

    }

    template <typename Comp>
    Entity* CompRef<Comp>::getEntity() const
    {
        if (entityId != 0)
        {
            return ecsRef->getEntity(entityId);
        }

        return nullptr;
    }

    template <typename Type, typename... Types>
    template <typename Set>
    void Group<Type, Types...>::addEventToSet(Set setN)
    {
        LOG_THIS_MEMBER("Ecs Group");

        // Todo fix this ( it is called multiple times when it should be only called once per set)
        // In case of texture it is called twice once for ui and once for tex comp
        setN->onComponentCreation.emplace(id, [](EntityRef entity) {
            LOG_MILE("Group", "On component creation for entity " << entity->id << ", sending event !");
            entity->world()->sendEvent(OnCompCreatedCheckForGroup<Group<Type, Types...>>{entity}, true);
        });

        setN->onComponentDeletion.emplace(id, [](EntityRef entity) {
            LOG_MILE("Group", "On component deletion for entity " << entity->id << ", sending event !");
            entity->world()->sendEvent(OnCompDeletionCheckForGroup<Group<Type, Types...>>{entity->id, entity->componentList}, true);
        });
    }

    template <typename Type, typename... Types>
    void Group<Type, Types...>::process()
    {
        LOG_THIS_MEMBER("Ecs Group");

        if (this->registry == nullptr)
            return;

        checkGroupTypeExistence<Type, Types...>();

        populateList(setList, 0, registry->retrieve<Type>(), registry->retrieve<Types>()...);

        size_t smallestSetIndex = 0;

        for (size_t i = 0; i < nbOfSets; ++i)
        {
            if (setList[i]->set->nbElements() < setList[smallestSetIndex]->set->nbElements())
                smallestSetIndex = i;
        }

        SetHolder<Type, Types...>* smallestSetHolder = setList[smallestSetIndex];
        const SparseSet* smallestSet = smallestSetHolder->set;

        setList[smallestSetIndex] = setList[nbOfSets - 1];

        setList[nbOfSets - 1] = smallestSetHolder;

        // const auto& elements = {registry->retrieve<Type>()->components, registry->retrieve<Types>()->components...};

        // const SparseSet& set = smallestSet(registry->retrieve<Type>()->components, registry->retrieve<Types>()->components...);

        LOG_INFO("Ecs Group", "Smallest set has: " + std::to_string(smallestSet->nbElements()) + " elements");

        // Todo add reserve and multiple emplace back in the component/sparse set
        // elements.reserve(smallestSet->nbElements()); // May need a -1

        // Todo check Branch: Parallel-Ecs to create a parallal implementation of grouping
        for (const auto& id : smallestSet->view())
        {
            GroupElement<Type, Types...> element(registry->world()->getEntity(id), this->world(), id);

            for (size_t j = 0; j < nbOfSets - 1; j++)
            {
                setList[j]->setElement(setList[j]->set, element, id);
            }

            if (not element.toBeDeleted)
                elements.addComponent(id, element);
        }

        // Todo add this on ~Group() !
        // for(size_t i = 0; i < nbOfSets; i++)
        //     delete setList[i];

        // Add support for thread pools by passing a pool in this function and add the task inside of this pool
        // checkEntityInGroup<Type, Types...>(this->registry->getThreadPool(), elements);

        // const auto& it = elements.viewComponents();

        // Remove all elements that miss at least one component from the group
        // Todo Do not delete the component but make the iterator skip element to be deleted !
        // for(size_t i = 1; i < elements.nbElements(); i++)
        // {
            // const auto& element = elements[i];
            // if(element->toBeDeleted)
                // elements.removeComponent(element->entityId);
        // }
        //std::remove_if(it.begin(), it.end(), [](const GroupElement<Type, Types...>& element) { return element.toBeDeleted; });

        // Todo sort the group
    }

    template <typename Type>
    void Component::setValue(Type& currentValue, const Type& value)
    {
        LOG_THIS_MEMBER("Component");

        if (currentValue != value)
        {
            currentValue = value;

            if (ecsRef)
            {
                ecsRef->sendEvent(EntityChangedEvent{entityId});
            }
        }
    }

    template <typename Type>
    void CommandDispatcher::ComponentCreateCommand::setupFunctions()
    {
        LOG_THIS_MEMBER("Command Dispatcher");

        addInEcs = [](EntitySystem* ecs, EntityRef entity, void* component) {
            ecs->addComponentToPool(entity, static_cast<Type*>(component));
            delete static_cast<Type*>(component);
        };
    }

    // ============================================================================
    // Component Serializer Registration System
    // ============================================================================

    struct ObjInstance;

    /**
     * @brief Type-erased function signature for component serializers
     *
     * The serializer receives a void* pointer which should be cast to the appropriate
     * component type inside the implementation. The component should provide its own
     * ecsRef and entityId/id members for generating setters.
     */
    using ComponentSerializerFunc = std::function<void(VM*, ObjInstance*, void*)>;

    /**
     * @brief Type-erased function signature for component retrieval
     *
     * This function retrieves a component from an entity given its ID.
     * Returns void* pointer to the component or nullptr if not found.
     */
    using ComponentRetrieverFunc = std::function<void*(EntitySystem*, _unique_id)>;

    /**
     * @brief Function type for creating component proxy instances
     *
     * Takes a VM pointer and a raw component pointer, returns a proxy Value.
     * This allows zero-copy component access from scripts.
     */
    using ComponentProxyFactoryFunc = std::function<Value(VM*, void*)>;

    /**
     * @brief Registration structure that handles type erasure for component serializers
     *
     * Similar to ComponentCreateCommand in the dispatcher, this structure stores
     * a type-erased serializer function that can be called with void* pointers.
     * It also stores a retriever function that knows how to get the component from an entity.
     * Additionally, stores a proxy factory function for creating zero-copy component proxies.
     */
    struct ComponentSerializerRegistration {
        ComponentSerializerRegistration() : serializer(nullptr), retriever(nullptr), proxyFactory(nullptr) {}

        template <typename ComponentType>
        ComponentSerializerRegistration(void(*func)(VM*, ObjInstance*, ComponentType*))
        {
            setupFunction<ComponentType>(func);
        }

        template <typename ComponentType>
        void setupFunction(void(*func)(VM*, ObjInstance*, ComponentType*))
        {
            // Wrap the typed function pointer in a lambda that handles the void* cast
            serializer = [func](VM* vm, ObjInstance* table, void* component) {
                func(vm, table, static_cast<ComponentType*>(component));
            };

            // Create a retriever lambda that knows how to get this component type from an entity
            retriever = [](EntitySystem* ecsRef, _unique_id entityId) -> void* {
                return ecsRef->getComponent<ComponentType>(entityId);
            };
        }

        // Helper to setup retriever separately (defined in .cpp where EntitySystem is complete)
        template <typename ComponentType>
        void setupRetriever();

        ComponentSerializerFunc serializer;
        ComponentRetrieverFunc retriever;
        ComponentProxyFactoryFunc proxyFactory;  // NEW: Zero-copy proxy factory
    };

    /**
     * @brief Registry for component serializers with setter generation
     *
     * This singleton registry allows registration of custom serializers that add
     * dynamic setter methods to component tables. Expert users can register custom
     * serializers for their components using the REGISTER_COMPONENT_SERIALIZER macro.
     *
     * Example:
     * ```cpp
     * void serializeMyComponentWithSetters(VM* vm, ObjInstance* table,
     *                                      const MyComponent& component) {
     *     // Generate setters using component.ecsRef and component.entityId
     * }
     *
     * REGISTER_COMPONENT_SERIALIZER(MyComponent, serializeMyComponentWithSetters);
     * ```
     */
    class ComponentSerializerRegistry
    {
    public:
        /**
         * @brief Get the singleton instance of the registry
         */
        static ComponentSerializerRegistry& instance()
        {
            static ComponentSerializerRegistry registry;
            return registry;
        }

        /**
         * @brief Register a typed serializer function for a component
         *
         * @tparam ComponentType The type of component this serializer handles
         * @param componentName The name of the component (usually the class name)
         * @param func Function pointer to the serializer implementation
         */
        template <typename ComponentType>
        void registerSerializer(const std::string& componentName,
                               void(*func)(VM*, ObjInstance*, ComponentType*))
        {
            ComponentSerializerRegistration reg;
            reg.setupFunction<ComponentType>(func);
            serializers_[componentName] = reg;
        }

        /**
         * @brief Check if a serializer is registered for a component
         *
         * @param componentName The name of the component to check
         * @return true if a serializer is registered, false otherwise
         */
        bool hasSerializer(const std::string& componentName) const
        {
            return serializers_.find(componentName) != serializers_.end();
        }

        /**
         * @brief Get the type-erased serializer function for a component
         *
         * @param componentName The name of the component
         * @return ComponentSerializerFunc The type-erased serializer function
         */
        ComponentSerializerFunc getSerializer(const std::string& componentName) const
        {
            auto it = serializers_.find(componentName);
            if (it != serializers_.end())
            {
                return it->second.serializer;
            }
            return nullptr;
        }

        /**
         * @brief Get the type-erased retriever function for a component
         *
         * @param componentName The name of the component
         * @return ComponentRetrieverFunc The type-erased retriever function
         */
        ComponentRetrieverFunc getRetriever(const std::string& componentName) const
        {
            auto it = serializers_.find(componentName);
            if (it != serializers_.end())
            {
                return it->second.retriever;
            }
            return nullptr;
        }

        /**
         * @brief Register a proxy factory function for a component
         *
         * This enables zero-copy component access from scripts by creating proxy
         * instances that directly reference C++ component memory.
         *
         * @param componentName The name of the component
         * @param factory Function that creates a proxy Value from a component pointer
         */
        void registerProxyFactory(const std::string& componentName, ComponentProxyFactoryFunc factory)
        {
            auto it = serializers_.find(componentName);

            if (it != serializers_.end())
            {
                it->second.proxyFactory = factory;
            }
            else
            {
                // Create new registration with just proxy factory
                ComponentSerializerRegistration reg;
                reg.proxyFactory = factory;
                serializers_[componentName] = reg;
            }
        }

        /**
         * @brief Check if a proxy factory is registered for a component
         *
         * @param componentName The name of the component to check
         * @return true if a proxy factory is registered, false otherwise
         */
        bool hasProxyFactory(const std::string& componentName) const
        {
            auto it = serializers_.find(componentName);

            return it != serializers_.end() and it->second.proxyFactory != nullptr;
        }

        /**
         * @brief Get the proxy factory function for a component
         *
         * @param componentName The name of the component
         * @return ComponentProxyFactoryFunc The proxy factory function, or nullptr if not found
         */
        ComponentProxyFactoryFunc getProxyFactory(const std::string& componentName) const
        {
            auto it = serializers_.find(componentName);

            if (it != serializers_.end())
            {
                return it->second.proxyFactory;
            }

            return nullptr;
        }

        /**
         * @brief Create a component proxy using the registered factory
         *
         * Convenience method that looks up the factory and calls it.
         *
         * @param componentName The name of the component
         * @param vm Pointer to the VM
         * @param componentPtr Raw pointer to the component
         * @return Value The proxy instance, or nil if no factory registered
         */
        Value createComponentProxy(const std::string& componentName, VM* vm, void* componentPtr) const;

    private:
        ComponentSerializerRegistry() = default;
        ComponentSerializerRegistry(const ComponentSerializerRegistry&) = delete;
        ComponentSerializerRegistry& operator=(const ComponentSerializerRegistry&) = delete;

        std::unordered_map<std::string, ComponentSerializerRegistration> serializers_;
    };

    /**
     * @brief Macro to easily register a component serializer
     *
     * This macro creates a static initializer that registers the serializer function
     * at program startup. Use this in any .cpp file to register a custom serializer.
     *
     * Example:
     * ```cpp
     * void serializeMyComponent(VM* vm, ObjInstance* table, const MyComponent& comp) {
     *     // Implementation
     * }
     *
     * REGISTER_COMPONENT_SERIALIZER(MyComponent, serializeMyComponent);
     * ```
     */
    #define REGISTER_COMPONENT_SERIALIZER(ComponentType, FunctionName) \
        template void ComponentSerializerRegistration::setupFunction<ComponentType>( \
            void(*)(VM*, ObjInstance*, ComponentType*)); \
        namespace { \
            struct ComponentType##SerializerRegistrar { \
                ComponentType##SerializerRegistrar() { \
                    pg::ComponentSerializerRegistry::instance() \
                        .registerSerializer<ComponentType>(#ComponentType, FunctionName); \
                } \
            }; \
            static ComponentType##SerializerRegistrar ComponentType##_registrar_instance; \
        }

    /**
     * @brief Macro to easily register a component proxy factory
     *
     * This macro creates a static initializer that registers a proxy factory function
     * for zero-copy component access from scripts. The proxy factory receives a raw
     * component pointer and returns a VM Value representing the proxy instance.
     *
     * Example:
     * ```cpp
     * Value createPositionComponentProxy(VM* vm, void* rawPtr) {
     *     PositionComponent* comp = static_cast<PositionComponent*>(rawPtr);
     *     return PositionComponentProxy::createProxy(vm, comp);
     * }
     *
     * REGISTER_COMPONENT_PROXY(PositionComponent, createPositionComponentProxy);
     * ```
     */
    #define REGISTER_COMPONENT_PROXY(ComponentType, FactoryFunction) \
        namespace { \
            struct ComponentType##ProxyRegistrar { \
                ComponentType##ProxyRegistrar() { \
                    pg::ComponentSerializerRegistry::instance() \
                        .registerProxyFactory(#ComponentType, FactoryFunction); \
                } \
            }; \
            static ComponentType##ProxyRegistrar ComponentType##_proxy_registrar_instance; \
        }
}