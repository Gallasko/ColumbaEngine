/**
 * @file entitysystem.cpp
 * @author Pigeon Codeur (pigeoncodeur@gmail.com)
 * @brief Definition of the entity system
 * @version 0.1
 * @date 2022-08-06
 *
 * @copyright Copyright (c) 2022
 *
 */

#include "stdafx.h"

#include "entitysystem.h"

#include "system.h"

#include "Systems/coresystems.h"

#include "Interpreter/interpretersystem.h"

#ifdef PROFILE
std::mutex profileMutex;
// Profiling data

std::unordered_map<std::string, long long> _systemExecutionTimes;
std::unordered_map<std::string, size_t> _systemExecutionCounts;
#endif

namespace
{
    static constexpr char const * DOM = "ECS";

#ifdef DEBUG
    static constexpr size_t NBEXECUTORTHREADS = 1;
#else
    static constexpr size_t NBEXECUTORTHREADS = 3;
#endif
}

namespace pg
{
    // Todo maybe
    // template <>
    // void serialize(Archive& archive, const EntitySystem& ecs)
    // {
    //     LOG_THIS(DOM);
    // }

    // Todo set executor depending on the configuration / env !
    // Todo better save system init
    // Maybe put the number of executors in the save file
    EntitySystem::EntitySystem(const std::string& savePath) : registry(this), cmdDispatcher(this), saveManager(savePath), executor(NBEXECUTORTHREADS)
    {
        LOG_THIS_MEMBER(DOM);

        LOG_INFO(DOM, "Starting ecs...");

        saveManager.addToRegistry(&registry);

        LOG_INFO(DOM, "Added save manager in ecs");

        // Add the event and command dispatcher as the first element of the task flow
        basicTask = taskflow.emplace([this]() {
            static auto start = std::chrono::steady_clock::now();
            static auto end = std::chrono::steady_clock::now();
            static size_t nbExecution = 0;

            end = std::chrono::steady_clock::now();

            // During the command dispatcher no other system should be running
            // So it should be safe to allow for creation and deletion of entities/components on the spot
            running = false;

#ifdef PROFILE
            auto startTask = std::chrono::steady_clock::now();
#endif
            eventDispatcher.process();

            cmdDispatcher.process();

            if (not stopRequested)
                running = true;

            saveManager._execute();

#ifdef PROFILE
            // Record end time and compute elapsed time in nanoseconds.
            auto endTask = std::chrono::steady_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(endTask - startTask).count();

            // Log if the duration exceeds a threshold
            if (duration >= 3000000)
                std::cout << "System BasicTask execution time: " << duration << " ns" << std::endl;

            // Update profiling data in a thread-safe manner.
            {
                std::lock_guard<std::mutex> lock(profileMutex);
                std::string systemName = "BasicTask";
                _systemExecutionTimes[systemName] += duration;
                _systemExecutionCounts[systemName]++;
            }
#endif

            nbExecution++;
            totalNbOfExecution++;

            if (std::chrono::duration_cast<std::chrono::seconds>(end - start).count() >= 1)
            {
                LOG_MILE(DOM, "Number of execution of the system in the last seconds: " << nbExecution);
                currentNbOfExecution = nbExecution;
                nbExecution = 0;
                start = end;
            }

            }).name("Basic Task");

        LOG_INFO(DOM, "Ecs started !");
    }

    EntitySystem::~EntitySystem()
    {
        LOG_THIS_MEMBER(DOM);

        LOG_INFO(DOM, "Deleting Ecs...");

        stop();

        LOG_INFO(DOM, "Ecs stopped");

        for (auto& sys : systems)
        {
            sys.second->removeFromRegistry();

            delete sys.second;
        }

        LOG_INFO(DOM, "Ecs correctly deleted !");
    }

    EntityRef EntitySystem::createEntity()
    {
        LOG_THIS_MEMBER("ECS");

        if (running)
            return cmdDispatcher.createEntity();
        else
        {
            const auto& id = registry.idGenerator.generateId();
            return entityPool.addComponent(id, id, this);
        }
    }

    void EntitySystem::removeEntity(Entity* entity)
    {
        LOG_THIS_MEMBER("ECS");

        if (entity == nullptr)
        {
            LOG_ERROR("ECS", "Entity doesn't exists !");

            return;
        }

        if (running)
            cmdDispatcher.deleteEntity(entity);
        else
            deleteEntityFromPool(entity);
    }

    InterpreterSystem* EntitySystem::createInterpreterSystem(std::shared_ptr<Environment> env, std::shared_ptr<ClassInstance> sysInstance)
    {
        LOG_THIS_MEMBER("ECS");

        // Todo: add support for system creation during runtime
        if (running)
        {
            LOG_ERROR("ECS", "System creation during runtime is not supported");
            return nullptr;
        }

        auto system = new InterpreterSystem(env, sysInstance);
        system->_id = registry.idGenerator.generateId();

        system->ecsRef = this;

        systems.emplace(system->_id, system);

        system->addToRegistry(&registry);

        // Only add the system to the taskflow if the execution policy is set to sequential or independent !
        if (system->executionPolicy == ExecutionPolicy::Sequential)
        {
            auto task = taskflow.emplace([system]()
            {
#ifdef PROFILE
                // Todo time the whole exec of a run of the taskflow
                auto start = std::chrono::steady_clock::now();
#endif
                system->_execute();

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
            }).name(std::to_string(system->_id));

            // Put the task after every other basic task
            task.succeed(basicTask);

            // Register the task in case we need to call precede and succeed
            tasks[system->_id] = task;
        }
        else if (system->executionPolicy == ExecutionPolicy::Independent)
        {
            auto task = taskflow.emplace([system](){system->_execute();}).name(std::to_string(system->_id));

            // Register the task in case we need to call precede and succeed
            tasks[system->_id] = task;
        }

        return system;
    }

    void EntitySystem::deleteSystem(_unique_id id)
    {
        LOG_THIS_MEMBER("ECS");

        // Todo: add support for system deletion during runtime
        if (running)
        {
            LOG_ERROR("ECS", "System deletion during runtime is not supported");
            return;
        }

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

    void EntitySystem::executeOnce()
    {
        LOG_THIS_MEMBER("ECS");

        bool keepRunning = running;

        running = true;

        executor.run(taskflow).wait();

        running = keepRunning;
    }

    void EntitySystem::executeAll()
    {
        LOG_THIS_MEMBER(DOM);

        // runs the taskflow until we stop the system
        executor.run_until(taskflow, [&running = running](){ return not running; });
    }

    Entity* EntitySystem::getEntity(const std::string& name) const
    {
        LOG_THIS_MEMBER("ECS");

        return getEntity(getSystem<EntityNameSystem>()->getEntityId(name));
    }
    void EntitySystem::reportSystemProfiles()
    {
#ifdef PROFILE
        std::lock_guard<std::mutex> lock(profileMutex);

        std::string bottleneckSystem;
        long long maxAvgTime = 0;

        std::cout << "System execution times:" << _systemExecutionTimes.size() << std::endl;

        for (const auto& pair : _systemExecutionTimes) {
            std::string name = pair.first;
            long long totalTime = pair.second;
            size_t count = _systemExecutionCounts[name];
            long long avgTime = (count != 0) ? totalTime / count : 0;

            std::cout << "System " << name << " average execution: " << pair.second << ", " << avgTime << " ns ("
                      << count << " iterations)" << std::endl;

            if (avgTime > maxAvgTime) {
                maxAvgTime = avgTime;
                bottleneckSystem = name;
            }
        }

        std::cout << "Bottleneck system: " << bottleneckSystem
                  << " with average execution time: " << maxAvgTime << " ns" << std::endl;


        for (const auto& event : registry.eventCountMap)
        {
            std::cout << "Event ID: " << event.first << ", called: " << event.second << std::endl;
        }

        registry.eventCountMap.clear();

        // Optional: Reset the counters if you want per-interval reporting.
        _systemExecutionTimes.clear();
        _systemExecutionCounts.clear();
#endif
    }
}