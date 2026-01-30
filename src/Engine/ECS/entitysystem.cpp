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

// Include taskflow here instead of in header for compilation time optimization
#include "taskflow/taskflow.hpp"

#include "system.h"

#include "Systems/coresystems.h"

// #include "Interpreter/interpretersystem.h"

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

// Include for the vm setup
#include "Compiler/vm.h"
#include "Helpers/mathmodule.h"
#include "Helpers/algorithmmodule.h"
#include "Helpers/stringmodule.h"
#include "Files/filemodule.h"

#ifndef PG_MINIMAL_BUILD
#include "ecsmodule.h"
#include "Helpers/randommodule.h"
#include "Helpers/inputmodule_vm.h"
#include "Input/inputcomponent.h"
#include "2D/texturemodule.h"
#include "UI/uimodule.h"
#endif

// Include for vm optimization pass
#include "Compiler/pass/long_jump_optimization_pass.h"
#include "Compiler/pass/basic_operator_local_indexing.h"
#include "Compiler/pass/remove_def_get_global_redunduncy.h"
#include "Compiler/pass/constant_var_access.h"
#include "Compiler/pass/fuse_op_pop.h"
#include "Compiler/pass/constant_folding.h"
#include "Compiler/pass/increment_optimization_pass.h"
#include "Compiler/pass/simplify_constant_pass.h"

namespace pg
{
    // Pimpl implementation for taskflow to reduce header compilation time
    struct EntitySystem::TaskflowImpl
    {
        /** Taskflow of all the system of the ecs */
        tf::Taskflow taskflow;

        /** Main executor of the ecs */
        tf::Executor executor;

        /** Map of all the task associated to systems */
        std::unordered_map<_unique_id, tf::Task> tasks;

        /** Last task of the mandatory ecs base systems */
        tf::Task basicTask;

        TaskflowImpl() : executor(NBEXECUTORTHREADS) {}
    };

    // Todo maybe
    // template <>
    // void serialize(Archive& archive, const EntitySystem& ecs)
    // {
    //     LOG_THIS(DOM);
    // }

    // Todo set executor depending on the configuration / env !
    // Todo better save system init
    // Maybe put the number of executors in the save file
    EntitySystem::EntitySystem(const std::string& savePath) : registry(this), cmdDispatcher(this),
        saveManager(savePath), taskflowImpl(std::make_unique<TaskflowImpl>())
    {
        LOG_THIS_MEMBER(DOM);

        LOG_INFO(DOM, "Starting ecs...");

        saveManager.addToRegistry(&registry);

        LOG_INFO(DOM, "Added save manager in ecs");

        // Add the event and command dispatcher as the first element of the task flow
        taskflowImpl->basicTask = taskflowImpl->taskflow.emplace([this]() {
            static auto start = std::chrono::steady_clock::now();
            static auto end = std::chrono::steady_clock::now();
            static size_t nbExecution = 0;

            end = std::chrono::steady_clock::now();

            // During the command dispatcher no other system should be running
            // So it should be safe to allow for creation and deletion of entities/components on the spot
            running = false;

#ifdef PROFILE
            auto startTask = std::chrono::steady_clock::now();

            PROFILE_BEGIN("EventDispatch", "Event");
#endif
            eventDispatcher.process();

#ifdef PROFILE
            PROFILE_END("EventDispatch", "Event");

            PROFILE_BEGIN("CommandDispatch", "Command");
#endif
            cmdDispatcher.process();

#ifdef PROFILE
            PROFILE_END("CommandDispatch", "Command");
#endif

            if (not stopRequested)
                running = true;

#ifdef PROFILE
            PROFILE_BEGIN("SaveManager", "System");
#endif
            saveManager._execute();

#ifdef PROFILE
            PROFILE_END("SaveManager", "System");

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

    void EntitySystem::stop()
    {
        LOG_THIS_MEMBER("ECS");

        stopRequested = true;
        running = false;

        taskflowImpl->executor.wait_for_all();

        if (runningThread.joinable())
            runningThread.join();
    }

    void EntitySystem::dumbTaskflow() const
    {
        LOG_THIS_MEMBER("ECS");

        taskflowImpl->taskflow.dump(std::cout);
    }

    size_t EntitySystem::getNbTasks() const
    {
        return taskflowImpl->tasks.size();
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

    EntityRef EntitySystem::createEntity(const std::string& name)
    {
        LOG_THIS_MEMBER("ECS");

        if (running)
        {
            auto ent = cmdDispatcher.createEntity();
            ent->attach<EntityName>(name);
            return ent;
        }
        else
        {
            const auto& id = registry.idGenerator.generateId();
            auto ent = entityPool.addComponent(id, id, this);
            ent->attach<EntityName>(name);
            return ent;
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

    void EntitySystem::internalCreateSystem(AbstractSystem* system)
    {
        // Only add the system to the taskflow if the execution policy is set to sequential or independent !
        if (system->executionPolicy == ExecutionPolicy::Sequential)
        {
            auto name = system->getSystemName();

            if (name == "UnNamed")
                name = std::to_string(system->_id);

            auto task = taskflowImpl->taskflow.emplace([system, name]()
            {
#ifdef PROFILE
                // Todo time the whole exec of a run of the taskflow
                auto start = std::chrono::steady_clock::now();

                PROFILE_SCOPE(system->getSystemName(), "System");
#endif

                try
                {
                    system->_execute();
                }
                catch (const std::exception& e)
                {
                    LOG_ERROR("ECS", "Exception thrown whhile execution sys: " << name << ", error: " << e.what());
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
            task.succeed(taskflowImpl->basicTask);

            // Register the task in case we need to call precede and succeed
            taskflowImpl->tasks[system->_id] = task;
        }
        else if (system->executionPolicy == ExecutionPolicy::Independent)
        {
            auto name = system->getSystemName();

            if (name == "UnNamed")
                name = std::to_string(system->_id);

            auto task = taskflowImpl->taskflow.emplace([system]()
            {
#ifdef PROFILE
                // Todo time the whole exec of a run of the taskflow
                auto start = std::chrono::steady_clock::now();

                PROFILE_SCOPE(system->getSystemName(), "System");
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
            }).name(name);

            // Register the task in case we need to call precede and succeed
            taskflowImpl->tasks[system->_id] = task;
        }
    }

#ifndef PG_MINIMAL_BUILD
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

        internalCreateSystem(system);

        return system;
    }
#else
    InterpreterSystem* EntitySystem::createInterpreterSystem(std::shared_ptr<Environment>, std::shared_ptr<ClassInstance>)
    {
        return nullptr;
    }
#endif



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
                if (auto itTask = taskflowImpl->tasks.find(id); itTask != taskflowImpl->tasks.end())
                {
                    // Remove the task from the taskflow
                    const auto& task = itTask->second;
                    taskflowImpl->taskflow.erase(task);
                    taskflowImpl->tasks.erase(itTask);
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

        taskflowImpl->executor.run(taskflowImpl->taskflow).wait();

        running = keepRunning;
    }

    void EntitySystem::executeAll()
    {
        LOG_THIS_MEMBER(DOM);

        // runs the taskflow until we stop the system
        taskflowImpl->executor.run_until(taskflowImpl->taskflow, [&running = running](){ return not running; });
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

    void EntitySystem::setupVm(VM& vm)
    {
        LOG_THIS_MEMBER("ECS");

        // Start timing if profiling is enabled
        std::chrono::steady_clock::time_point setupStart;
        if (vm.profiler.isEnabled())
        {
            setupStart = std::chrono::steady_clock::now();
        }

        vm.addNativeModule("math", MathModule{});
        vm.addNativeModule("algorithm", AlgorithmModule{});
        vm.addNativeModule("string", StringModule{});
        vm.addNativeModule("file", FileModule{});

#ifndef PG_MINIMAL_BUILD
        vm.addNativeModule("random", RandomModule{});
        vm.addNativeModule("ecs", EcsCompiledModule{this});
        vm.addNativeModule("texture", TextureModule{this});
        vm.addNativeModule("ui", UIModule{this});

        // Todo change this
        // Get the Input handler from the MouseClickSystem
        Input* inputHandler = nullptr;
        auto mouseClickSys = getSystem<MouseClickSystem>();
        if (mouseClickSys)
        {
            inputHandler = mouseClickSys->inputHandler;
        }

        // Add input module if we have an input handler
        if (inputHandler)
        {
            vm.addNativeModule("input", InputModuleVM{inputHandler});
        }
#endif

        // Print function - outputs to stdout
        vm.registerNative("print", [](VM *vm, int argCount, Value* args) -> Value {
            for (int i = 0; i < argCount; i++)
            {
                if (i > 0) std::cout << " ";  // Space between arguments

                Value value = args[i];
                if (IS_STRING(value))
                    std::cout << vm->asString(value);
                else if (IS_INT(value))
                    std::cout << AS_INT(value);
                else if (IS_DOUBLE(value))
                    std::cout << AS_DOUBLE(value);
                else if (IS_BOOL(value))
                    std::cout << (AS_BOOL(value) ? "true" : "false");
                else if (IS_FUNC(value))
                    std::cout << "<function>";
                else if (IS_CLASS(value))
                    std::cout << "<class " << vm->asClass(value)->name << ">";
                else if (IS_INSTANCE(value))
                    std::cout << "<instance>";
                else if (IS_VECTOR(value))
                    std::cout << "<vector>";
                else
                    std::cout << "<value>";
            }
            std::cout << std::endl;
            return makeIntValue(0);
        });

        vm.registerNative("debugTable", [](VM *vm, int argCount, Value* args) -> Value {
            if (argCount != 1) return makeBoolValue(false);

            if (IS_INSTANCE(args[0]))
            {
                ObjInstance* table = vm->asInstance(args[0]);
                LOG_INFO("Script", "Table contents:");
                for (const auto& [key, value] : table->fields)
                {
                    std::string valStr;
                    if (IS_STRING(value))
                        valStr = vm->asString(value);
                    else if (IS_INT(value))
                        valStr = std::to_string(AS_INT(value));
                    else if (IS_DOUBLE(value))
                        valStr = std::to_string(AS_DOUBLE(value));
                    else if (IS_BOOL(value))
                        valStr = AS_BOOL(value) ? "true" : "false";
                    else
                        valStr = "<complex type>";

                    LOG_INFO("Script", "  " << key << " : " << valStr);
                }
            }
            else if (IS_VECTOR(args[0]))
            {
                ObjVector* vector = vm->asVector(args[0]);
                LOG_INFO("Script", "Vector contents:");
                for (size_t i = 0; i < vector->fields.size(); i++)
                {
                    Value value = vector->fields[i];
                    std::string valStr;
                    if (IS_STRING(value))
                        valStr = vm->asString(value);
                    else if (IS_INT(value))
                        valStr = std::to_string(AS_INT(value));
                    else if (IS_DOUBLE(value))
                        valStr = std::to_string(AS_DOUBLE(value));
                    else if (IS_BOOL(value))
                        valStr = AS_BOOL(value) ? "true" : "false";
                    else
                        valStr = "<complex type>";

                    LOG_INFO("Script", "  [" << i << "] : " << valStr);
                }
            }
            else
            {
                LOG_INFO("Script", "Value is not a table or a vector instance");
            }

            return makeBoolValue(true);
        });

        vm.registerNative("debugGlobal", [](VM *vm, int argCount, Value*) -> Value {
            if (argCount != 0) return makeBoolValue(false);

            for (const auto& [key, value] : vm->globals)
            {
                std::string valStr;
                if (IS_STRING(value))
                    valStr = vm->asString(value);
                else if (IS_INT(value))
                    valStr = std::to_string(AS_INT(value));
                else if (IS_DOUBLE(value))
                    valStr = std::to_string(AS_DOUBLE(value));
                else if (IS_BOOL(value))
                    valStr = AS_BOOL(value) ? "true" : "false";
                else
                    valStr = "<complex type>";

                LOG_INFO("Script", "Global " << key << " : " << valStr);
            }

            return makeBoolValue(true);
        });

        vm.registerNative("logInfo", [](VM *vm, int argCount, Value* args) -> Value {
            if (argCount != 1) return makeBoolValue(false);

            auto value = args[0];

            std::string valStr;
            if (IS_STRING(value))
                valStr = vm->asString(value);
            else if (IS_INT(value))
                valStr = std::to_string(AS_INT(value));
            else if (IS_DOUBLE(value))
                valStr = std::to_string(AS_DOUBLE(value));
            else if (IS_BOOL(value))
                valStr = AS_BOOL(value) ? "true" : "false";
            else
                valStr = "<complex type>";

            LOG_INFO("Script", "Logged value: " << valStr);

            return makeBoolValue(true);
        });

        setOptimizationPasses(vm);

        // Todo add a flag to enable this
        // vm.enableOptimizationDebugging();

        // Setup the VM with necessary bindings and references
        // For example, bind the ECS reference to the VM for script access
        // This is a placeholder implementation; actual implementation may vary
        // depending on the VM and its API

        // Example:
        // vm.bindECS(this);

        // Record setupVm time if profiling is enabled
        if (vm.profiler.isEnabled())
        {
            auto setupEnd = std::chrono::steady_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(setupEnd - setupStart).count();
            vm.profiler.recordSetupVmTime(duration);
        }
    }

    void EntitySystem::setOptimizationPasses(VM &vm)
    {
        if (vmOptimizationLevel == VmOptimizationLevel::O3)
        {
            vm.enableBytecodeOptimization();

            vm.addOptimizationPass(std::make_unique<BasicOperatorLocalIndexingPass>());
            vm.addOptimizationPass(std::make_unique<LongJumpOptimizationPass>());
            vm.addOptimizationPass(std::make_unique<RemoveDefGetGlobalRedunduncy>());
            vm.addOptimizationPass(std::make_unique<FuseOpPop>());

            vm.addOptimizationPass(std::make_unique<ConstantFoldingPass>());

            vm.addOptimizationPass(std::make_unique<ConstantVarAccess>());

            vm.addOptimizationPass(std::make_unique<IncrementOptimizationPass>());

            vm.addOptimizationPass(std::make_unique<SimplifyConstantToShort>());
        }
        else if (vmOptimizationLevel == VmOptimizationLevel::O0)
        {
            vm.disableBytecodeOptimization();
        }
    }

    void EntitySystem::_deleteSystem(_unique_id id)
    {
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
                if (auto itTask = taskflowImpl->tasks.find(id); itTask != taskflowImpl->tasks.end())
                {
                    // Remove the task from the taskflow
                    const auto& task = itTask->second;
                    taskflowImpl->taskflow.erase(task);
                    taskflowImpl->tasks.erase(itTask);
                }
            }

            // Delete the system
            delete system;
            systems.erase(it);
        }
    }

    void EntitySystem::_succeed(_unique_id sys1Id, _unique_id sys2Id)
    {
        auto it1 = taskflowImpl->tasks.find(sys1Id);
        auto it2 = taskflowImpl->tasks.find(sys2Id);

        if (it1 != taskflowImpl->tasks.end() and it2 != taskflowImpl->tasks.end())
        {
            it1->second.succeed(it2->second);
            LOG_INFO("ECS", "System " << sys1Id << " will run after system " << sys2Id << " !");
        }
        else if (it1 == taskflowImpl->tasks.end() and it2 != taskflowImpl->tasks.end())
        {
            LOG_ERROR("ECS", "Systems " << sys1Id << " is not a registered task in ecs can't reorder task !");
        }
        else if (it1 != taskflowImpl->tasks.end() and it2 == taskflowImpl->tasks.end())
        {
            LOG_ERROR("ECS", "Systems " << sys2Id << " is not a registered task in ecs can't reorder task !");
        }
        else
        {
            LOG_ERROR("ECS", "Both systems " << sys1Id << " and " << sys2Id << " are not registered task in ecs can't reorder their task !");
        }
    }
}