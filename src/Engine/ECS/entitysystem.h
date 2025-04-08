#pragma once

#include <vector>
#include <unordered_map>
#include <algorithm>

#include <taskflow.hpp>

#include "componentregistry.h"
#include "entity.h"
#include "system.h"
#include "commanddispatcher.h"
#include "savemanager.h"

#include "serialization.h"

#include "logger.h"
#include "Memory/memorypool.h"

#ifdef PROFILE
#include <atomic>
#include <mutex>
extern std::mutex profileMutex;
// Profiling data

extern std::unordered_map<std::string, long long> _systemExecutionTimes;
extern std::unordered_map<std::string, size_t> _systemExecutionCounts;
#endif

namespace pg
{
    // Todo create a queue that hold all entity id that got deleted to reattribute them later on

    // Todo Create a different id gen for systems so that components id are smaller and more packed

    // Forward declarations
    class ComponentRegistry;
    struct AbstractSystem;
    class InterpreterSystem;
    class Environment;
    class ClassInstance;

    // Todo add this in a window dependancy
    // This event is fired when the window is resized
    struct ResizeEvent { float width, height; };

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

    class EntitySystem
    {
    friend class Entity;
    friend class CommandDispatcher;
    friend struct CoreModule;
    friend struct InputModule;
    friend struct OnEventComponent;
    friend struct OnStandardEventComponent;

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
        inline void stop()
        {
            LOG_THIS_MEMBER("ECS");

            // if (not running)
            //     return;

            stopRequested = true;
            running = false;

            executor.wait_for_all();

            if (runningThread.joinable())
                runningThread.join();
        }

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
        template <class Sys, typename... Args>
        Sys* createSystem(const Args&... args)
        {
            LOG_THIS_MEMBER("ECS");

            // Todo: add support for system creation during runtime
            if (running)
            {
                LOG_ERROR("ECS", "System creation during runtime is not supported");
                return nullptr;
            }

            auto system = new Sys(args...);

            system->_id = registry.getTypeId<Sys>();

            system->ecsRef = this;

            systems.emplace(system->_id, system);

            system->addToRegistry(&registry);

            // Only add the system to the taskflow if the execution policy is set to sequential or independent !
            if (system->executionPolicy == ExecutionPolicy::Sequential)
            {
                auto name = system->getSystemName();

                if (name == "UnNamed")
                    name = std::to_string(system->_id);

                auto task = taskflow.emplace([system]()
                {
#ifdef PROFILE
                    // Todo time the whole exec of a run of the taskflow
                    auto start = std::chrono::steady_clock::now();
#endif

                    try
                    {
                        system->execute();
                    }
                    catch (const std::exception& e)
                    {
                        LOG_ERROR("ECS", "Exception thrown whhile execution sys: " << typeid(Sys).name() << ", error: " << e.what());
                    }

#ifdef PROFILE
                    // Record end time and compute elapsed time in nanoseconds.
                    auto end = std::chrono::steady_clock::now();
                    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();

                    // Log if the duration exceeds a threshold
                    if (duration >= 3000000)
                        std::cout << "System " << system->getSystemName() << " execution time: " << duration << " ns" << std::endl;

                    // Update profiling data in a thread-safe manner.
                    {
                        std::lock_guard<std::mutex> lock(profileMutex);
                        std::string systemName = system->getSystemName();
                        _systemExecutionTimes[systemName] += duration;
                        _systemExecutionCounts[systemName]++;

                        // std::cout << "Updated " << systemName
                        // << " total time = " << _systemExecutionTimes[systemName]
                        // << ", count = " << _systemExecutionCounts[systemName] << std::endl;
                    }
#endif
                }).name(name);

                // Put the task after every other basic task
                task.succeed(basicTask);

                // Register the task in case we need to call precede and succeed
                tasks[system->_id] = task;
            }
            else if (system->executionPolicy == ExecutionPolicy::Independent)
            {
                auto name = system->getSystemName();

                if (name == "UnNamed")
                    name = std::to_string(system->_id);

                auto task = taskflow.emplace([system]()
                {
#ifdef PROFILE
                    // Todo time the whole exec of a run of the taskflow
                    auto start = std::chrono::steady_clock::now();
#endif

                    system->execute();

#ifdef PROFILE
                    // Record end time and compute elapsed time in nanoseconds.
                    auto end = std::chrono::steady_clock::now();
                    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();

                    // Log if the duration exceeds a threshold
                    if (duration >= 3000000)
                        std::cout << "System " << system->getSystemName() << " execution time: " << duration << " ns" << std::endl;

                    // Update profiling data in a thread-safe manner.
                    {
                        std::lock_guard<std::mutex> lock(profileMutex);
                        std::string systemName = system->getSystemName();
                        _systemExecutionTimes[systemName] += duration;
                        _systemExecutionCounts[systemName]++;
                    }
#endif
                }).name(name);

                // Register the task in case we need to call precede and succeed
                tasks[system->_id] = task;
            }

            return system;
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

            if (auto it = systems.find(id); it == systems.end())
            {
                LOG_ERROR("ECS", "System [" << id << "] is not registered so it cannot be deleted");
                return;
            }
            else
            {
                auto system = it->second;

                if (not system)
                {
                    LOG_ERROR("ECS", "System [" << id << "] is already deleted");
                    systems.erase(it);
                    return;
                }

                // Remove the system task from the taskflow
                if (system->executionPolicy == ExecutionPolicy::Sequential or system->executionPolicy == ExecutionPolicy::Independent)
                {
                    // Try to find the task in the task list
                    if (auto itTask = tasks.find(id); itTask != tasks.end())
                    {
                        // Remove the task from the taskflow
                        const auto& task = itTask->second;
                        taskflow.erase(task);
                        tasks.erase(itTask);
                    }
                }

                // Delete the system
                delete system;
                systems.erase(it);
            }
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

            // Only add the system to the taskflow if the execution policy is set to sequential or independent !
            if (system->executionPolicy == ExecutionPolicy::Sequential)
            {
                auto name = system->getSystemName();

                if (name == "UnNamed")
                    name = std::to_string(system->_id);

                auto task = taskflow.emplace([system]()
                {
#ifdef PROFILE
                    // Todo time the whole exec of a run of the taskflow
                    auto start = std::chrono::steady_clock::now();
#endif

                    system->execute();

#ifdef PROFILE
                    // Record end time and compute elapsed time in nanoseconds.
                    auto end = std::chrono::steady_clock::now();
                    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();

                    // Log if the duration exceeds a threshold
                    if (duration >= 3000000)
                        std::cout << "System " << system->getSystemName() << " execution time: " << duration << " ns" << std::endl;

                    // Update profiling data in a thread-safe manner.
                    {
                        std::lock_guard<std::mutex> lock(profileMutex);
                        std::string systemName = system->getSystemName();
                        _systemExecutionTimes[systemName] += duration;
                        _systemExecutionCounts[systemName]++;
                    }
#endif
                }).name(name);

                // Put the task after every other basic task
                task.succeed(basicTask);

                // Register the task in case we need to call precede and succeed
                tasks[system->_id] = task;
            }
            else if (system->executionPolicy == ExecutionPolicy::Independent)
            {
                auto name = system->getSystemName();

                if (name == "UnNamed")
                    name = std::to_string(system->_id);

                auto task = taskflow.emplace([system](){system->execute();}).name(name);

                // Register the task in case we need to call precede and succeed
                tasks[system->_id] = task;
            }

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

            auto sys1Id = registry.getTypeId<SysAfter>();
            auto sys2Id = registry.getTypeId<SysBefore>();

            auto it1 = tasks.find(sys1Id);
            auto it2 = tasks.find(sys2Id);

            if (it1 != tasks.end() and it2 != tasks.end())
            {
                it1->second.succeed(it2->second);
                LOG_INFO("ECS", "System " << sys1Id << " will run after system " << sys2Id << " !");
            }
            else if (it1 == tasks.end() and it2 != tasks.end())
            {
                LOG_ERROR("ECS", "Systems " << sys1Id << " is not a registered task in ecs can't reorder task !");
            }
            else if (it1 != tasks.end() and it2 == tasks.end())
            {
                LOG_ERROR("ECS", "Systems " << sys2Id << " is not a registered task in ecs can't reorder task !");
            }
            else
            {
                LOG_ERROR("ECS", "Both systems " << sys1Id << " and " << sys2Id << " are not registered task in ecs can't reorder their task !");
            }
        }

        inline void dumbTaskflow() const
        {
            LOG_THIS_MEMBER("ECS");

            taskflow.dump(std::cout);
        }

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
        template <typename Type, typename... Args>
        CompRef<Type> attach(EntityRef entity, Args&&... args) noexcept
        {
            LOG_THIS_MEMBER("ECS");

            try
            {
                Type* component;

                // Todo add lock a mutex for running to protect for race conditions or only build component with the cmdDispatcher
                if (running)
                {
                    component = cmdDispatcher.attachComp<Type>(entity, std::forward<Args>(args)...);
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
                LOG_ERROR("ECS", "Can't attach component [" << typeid(Type).name() << "]: " << e.what());
            }

            return CompRef<Type>();
        }

        // template <typename Type, typename EntityHolderType, typename... Args>
        // CompRef<Type> attach(EntityHolderType entity, Args&&... args) noexcept { return attach(entity.entity, args...); }

        void deserializeComponent(EntityRef entity, const UnserializedObject& serializedObject) noexcept
        {
            registry.deserializeComponentToEntity(serializedObject, entity);
        }

        template <typename Type>
        void detach(Entity* entity) noexcept
        {
            if (not entity)
                return;

            auto id = registry.getTypeId<Type>();

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
        void sendEvent(const Event& event)
        {
            LOG_THIS_MEMBER("ECS");

            if (running)
            {
                eventDispatcher.enqueueEvent([event, this](){ LOG_THIS("ECS"); registry.processEvent(event); });
            }
            else
            {
                registry.processEvent(event);
            }
        }

        template <typename Comp>
        inline _unique_id getId() const noexcept { LOG_THIS_MEMBER("ECS"); return registry.getTypeId<Comp>(); }

        void executeOnce();

        void executeAll();

        /** Return the registry of the ECS, mainly for testing purposes */
        inline constexpr const ComponentRegistry* getComponentRegistry() const noexcept { return &registry; }

        inline size_t getNbEntities() const { LOG_THIS_MEMBER("ECS"); return entityPool.nbElements() - 1; }

        inline Entity* getEntity(_unique_id id) const { LOG_THIS_MEMBER("ECS"); return entityPool.atEntity(id); }

        Entity* getEntity(const std::string& name) const;

        template <typename Comp>
        inline Comp* getComponent(_unique_id id) const { LOG_THIS_MEMBER("ECS"); return registry.retrieve<Comp>()->getComponent(id); }

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

        inline bool isRunning() const { return running; }

        inline size_t getNbSystems() const { return systems.size(); }

        inline size_t getNbTasks() const { return tasks.size(); }

        inline size_t getCurrentNbOfExecution() const { return currentNbOfExecution; }
        inline size_t getTotalNbOfExecution() const { return totalNbOfExecution; }

        void reportSystemProfiles();

    private:
        friend void serialize<>(Archive& archive, const EntitySystem& ecs);

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

            while (not entity->componentList.empty())
            {
                const auto& comp = *entity->componentList.begin();

                if (comp.entityHeldType == Entity::EntityHeld::EntityHeldType::id)
                {
                    try
                    {
                        registry.detachComponentFromEntity(entity, comp.getId());
                    }
                    catch (const std::exception& e)
                    {
                        LOG_ERROR("ECS", "Can't detach component [" << comp.getId() << "] from entity [" << entity->id << "]: " << e.what());
                    }
                }
                else
                {
                    entity->componentList.erase(entity->componentList.begin());
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

                registry.retrieve<Type>()->internalCreateComponent(entity, *component);
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

        /** Track the number of executed taskflows (for debug purposes) */
        size_t currentNbOfExecution = 0;
        size_t totalNbOfExecution = 0;

        ComponentRegistry registry;

        CommandDispatcher cmdDispatcher;

        EventDispatcher eventDispatcher;

        SaveManager saveManager;

        /** Store all systems added to the ECS */
        std::map<_unique_id, AbstractSystem*> systems;

        /** All the entities generated from the ECS */
        ComponentSet<Entity> entityPool;

        /** Running thread of the ECS */
        std::thread runningThread;

        /** Taskflow of all the system of the ecs */
        tf::Taskflow taskflow;

        /** Main executor of the ecs */
        tf::Executor executor;

        /** Map of all the task associated to systems */
        std::unordered_map<_unique_id, tf::Task> tasks;

        /** Last task of the mandatory ecs base systems */
        tf::Task basicTask;
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

            return ent->template get<Comp>();
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

        const auto& componentId = ecsRef->getId<Comp>();

        const auto& it = std::find(componentList.begin(), componentList.end(), componentId);

        if (it != componentList.end())
        {
            auto ent = ecsRef->getEntity(id);
            auto initialized = id != 0 and ent;

            // Todo add memoisation if we run into performance issues here
            return CompRef<Comp>(ecsRef->registry.retrieve<Comp>()->getComponent(id), id, ecsRef, initialized);
        }

        LOG_ERROR("Entity", "Entity doesn't have component: " << componentId);

        return CompRef<Comp>();
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
            serialize(archive, *(owner->getComponent(entity->id)));
        });

        if constexpr(HasStaticName<Type>::value)
        {
            componentDeserializeMap.emplace(Type::getType(), [this](const UnserializedObject& serializedStr, EntityRef entity) {
                if (serializedStr.isNull())
                    return;

                auto comp = deserialize<Type>(serializedStr);

                ecsRef->attach<Type>(entity, comp);
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
        }

        if (const auto& it = componentStorageMap.find(id); it != componentStorageMap.end())
        {
            componentStorageMap.erase(it);
        }

        removeTypeId<Type>();
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
                LOG_ERROR("Comp ref", "Copy of a reference to an invalid entity");
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
            entity->world()->sendEvent(OnCompCreatedCheckForGroup<Group<Type, Types...>>{entity});
        });

        setN->onComponentDeletion.emplace(id, [](EntityRef entity) {
            LOG_MILE("Group", "On component deletion for entity " << entity->id << ", sending event !");
            entity->world()->sendEvent(OnCompDeletionCheckForGroup<Group<Type, Types...>>{entity->id, entity->componentList});
        });
    }

    template <typename Type, typename... Types>
    void Group<Type, Types...>::process()
    {
        LOG_THIS_MEMBER("Ecs Group");

        if (this->registry == nullptr)
            return;

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
    void CommandDispatcher::ComponentCreateCommand::setupFunctions()
    {
        LOG_THIS_MEMBER("Command Dispatcher");

        addInEcs = [](EntitySystem* ecs, EntityRef entity, void* component) {
            ecs->addComponentToPool(entity, static_cast<Type*>(component));
            delete static_cast<Type*>(component);
        };
    }
}