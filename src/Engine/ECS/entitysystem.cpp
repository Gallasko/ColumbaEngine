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

#include "entitysystem.h"

#include "system.h"

#include "Systems/coresystems.h"

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
    template <>
    void serialize(Archive& archive, const EntitySystem& ecs)
    {
        LOG_THIS(DOM);
    }

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
            eventDispatcher.process(); 
                        
            cmdDispatcher.process();

            if (not stopRequested)
                running = true;

            saveManager.execute();

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
}