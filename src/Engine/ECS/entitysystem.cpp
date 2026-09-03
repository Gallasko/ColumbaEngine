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

// For type-name demangling in dumbTaskflow()
#if defined(__GNUC__) || defined(__clang__)
#include <cxxabi.h>
#endif

#include "system.h"
#include "scriptregistry.h"

#include "Systems/coresystems.h"

#ifndef PG_MINIMAL_BUILD
#include "Renderer/renderer.h"
#endif // PG_MINIMAL_BUILD

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
    #ifdef __EMSCRIPTEN__
        static constexpr size_t NBEXECUTORTHREADS = 2;
    #else
        static  size_t NBEXECUTORTHREADS = std::thread::hardware_concurrency() > 1 ? std::thread::hardware_concurrency() - 1 : 1;
    #endif
#endif
}

// Include for the vm setup
#include "Compiler/vm.h"
#include "ecsnativemodule.h"
#include "Helpers/mathmodule.h"
#include "Helpers/algorithmmodule.h"
#include "Helpers/stringmodule.h"
#include "Files/filemodule.h"

// Full build modules are now in entitysystem_full.cpp or entitysystem_minimal.cpp

// Include for vm optimization pass
#include "Compiler/pass/long_jump_optimization_pass.h"
#include "Compiler/pass/popping_jump_pass.h"
#include "Compiler/pass/basic_operator_local_indexing.h"
#include "Compiler/ast/pass/loop_invariant_hoisting.h"
#include "Compiler/pass/comparison_local_indexing.h"
#include "Compiler/pass/remove_def_get_global_redunduncy.h"
#include "Compiler/pass/constant_var_access.h"
#include "Compiler/pass/fuse_op_pop.h"
#include "Compiler/pass/set_local_pop_fusion.h"
#include "Compiler/pass/constant_folding.h"
#include "Compiler/pass/increment_optimization_pass.h"
#include "Compiler/pass/simplify_constant_pass.h"
#include "Compiler/pass/remove_useless_jump_pass.h"
#include "Compiler/pass/loop_rotation_pass.h"

namespace pg
{
    // Storage for the BasicTask-thread marker declared in entitysystem.h.
    // Each worker thread keeps its own copy; set to true at the start of the
    // BasicTask iteration and false at the end.
    thread_local bool EntitySystem::inBasicTask = false;

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
        scriptRegistry(std::make_unique<ScriptRegistry>(this)),
        saveManager(savePath), taskflowImpl(std::make_unique<TaskflowImpl>())
    {
        LOG_THIS_MEMBER(DOM);

        LOG_INFO(DOM, "Starting ecs...");
        LOG_INFO(DOM, "Number of executor threads: " << NBEXECUTORTHREADS);

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

            // Mark this worker thread as the BasicTask thread for the duration
            // of this iteration. sendEvent uses these flags to decide whether
            // the direct-dispatch path is safe. inBasicTask is thread_local
            // (only this thread sees true). basicTaskInProgress is atomic so
            // other threads know a BasicTask iteration is currently active and
            // must always enqueue.
            inBasicTask = true;
            basicTaskInProgress.store(true, std::memory_order_release);

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

            // Hot reload: swap staged script bytecode while no system is
            // executing, so no VM can be running the old version mid-swap
            scriptRegistry->applyPendingSwaps();

#ifdef PROFILE
            PROFILE_END("CommandDispatch", "Command");

            PROFILE_BEGIN("GroupEventDispatch", "Event");
#endif
            deferredEventDispatcher.process();

#ifdef PROFILE
            PROFILE_END("GroupEventDispatch", "Event");
#endif

            if (not stopRequested)
                running = true;

            // Clear the BasicTask markers before parallel systems start running
            // (they might land on this same thread later). Release on the atomic
            // so other threads observing basicTaskInProgress=false also see all
            // listener queue mutations made above.
            inBasicTask = false;
            basicTaskInProgress.store(false, std::memory_order_release);

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

    void EntitySystem::dumbTaskflow(bool showEventNodes, const std::string& outputFile) const
    {
        LOG_THIS_MEMBER("ECS");

        // --- local helpers ---

        // Demangle a C++ mangled type name and strip all "pg::" namespace prefixes.
        auto prettyName = [](const char* mangled) -> std::string {
#if defined(__GNUC__) || defined(__clang__)
            int status = 0;
            char* buf = abi::__cxa_demangle(mangled, nullptr, nullptr, &status);
            std::string s = (status == 0 && buf) ? buf : mangled;
            if (buf) free(buf);
#else
            std::string s = mangled;
#endif
            std::string clean;
            for (size_t i = 0; i < s.size(); )
            {
                if (s.compare(i, 4, "pg::") == 0) i += 4;
                else clean += s[i++];
            }
            return clean;
        };

        // Decode a _listenerEventNames entry ("L:mangled" or "Q:mangled")
        // Returns {kind_prefix, pretty_event_name}
        auto decodeEntry = [&prettyName](const std::string& entry)
            -> std::pair<std::string, std::string>
        {
            if (entry.size() > 2 && entry[1] == ':')
                return { entry.substr(0, 2), prettyName(entry.c_str() + 2) };
            return { "", prettyName(entry.c_str()) };
        };

        // --- capture raw DOT ---
        std::ostringstream oss;
        taskflowImpl->taskflow.dump(oss);
        std::string dot = oss.str();

        // --- find "Basic Task" node ID ---
        const std::string basicTaskLabel = "[label=\"Basic Task\" ]";
        auto labelPos = dot.find(basicTaskLabel);
        if (labelPos == std::string::npos)
        {
            std::cout << dot;
            return;
        }
        auto lineStart = dot.rfind('\n', labelPos);
        lineStart = (lineStart == std::string::npos) ? 0 : lineStart + 1;
        const std::string basicTaskId = dot.substr(lineStart, labelPos - lineStart);
        const std::string edgePrefix    = basicTaskId + " -> ";
        const std::string basicNodeDef  = basicTaskId + "["; // node definition line to drop

        // --- parse DOT: build nodeId↔name maps, strip Basic Task node + edges ---
        std::vector<std::string> basicTaskTargets;
        std::unordered_map<std::string, std::string> nodeIdToName;
        std::string result;
        std::istringstream stream(dot);
        std::string line;

        while (std::getline(stream, line))
        {
            // Detect node definition:  p0xABCD[label="Name" ];
            auto labelMark = line.find("[label=\"");
            if (labelMark != std::string::npos)
            {
                std::string nid = line.substr(0, labelMark);
                auto last = nid.find_last_not_of(" \t");
                if (last != std::string::npos) nid = nid.substr(0, last + 1);

                auto ns = labelMark + 8; // len("[label=\"")
                auto ne = line.find('"', ns);
                if (ne != std::string::npos)
                    nodeIdToName[nid] = line.substr(ns, ne - ns);
            }

            // Drop Basic Task node definition and all its outgoing edges
            if (line.rfind(basicNodeDef, 0) == 0 || line.rfind(edgePrefix, 0) == 0)
            {
                if (line.rfind(edgePrefix, 0) == 0)
                {
                    auto ts = line.find("-> ") + 3;
                    auto te = line.find(';', ts);
                    if (te != std::string::npos)
                        basicTaskTargets.push_back(line.substr(ts, te - ts));
                }
                // drop this line in both cases
            }
            else
            {
                result += line + '\n';
            }
        }

        // Build reverse: system name → DOT node id
        std::unordered_map<std::string, std::string> nameToNodeId;
        for (const auto& [nid, name] : nodeIdToName)
            nameToNodeId[name] = nid;

        // --- build event / group → [target dot nodes] maps ---
        // key for events: "L:<pretty>" or "Q:<pretty>"
        std::unordered_map<std::string, std::vector<std::string>> eventToNodes;
        std::unordered_map<std::string, std::vector<std::string>> groupToNodes;

        for (const auto& [sysId, sys] : systems)
        {
            auto it = nameToNodeId.find(sys->__name);
            if (it == nameToNodeId.end()) continue; // Storage/Manual – not in taskflow
            const std::string& nodeId = it->second;

            for (const auto& entry : sys->_listenerEventNames)
            {
                auto [prefix, name] = decodeEntry(entry);
                eventToNodes[prefix + name].push_back(nodeId);
            }

            for (const auto& mangled : sys->_registeredGroupNames)
                groupToNodes[prettyName(mangled.c_str())].push_back(nodeId);
        }

        // --- inject new nodes + edges into the DOT (optional) ---
        if (showEventNodes)
        {
            std::string inject;
            int idx = 0;

            // One coloured node per unique listened event (root nodes – no predecessor)
            for (const auto& [key, targetNodes] : eventToNodes)
            {
                bool isQueued = (key.size() >= 2 && key[0] == 'Q' && key[1] == ':');
                std::string label = key.substr(2) + (isQueued ? " (queued)" : "");
                std::string color = isQueued ? "lightyellow" : "lightblue";
                std::string nid   = "evtNode" + std::to_string(idx++);

                inject += nid + "[label=\"" + label + "\" style=filled fillcolor=" + color + "];\n";
                for (const auto& tn : targetNodes)
                    inject += nid + " -> " + tn + ";\n";
            }

            // One coloured node per unique registered group (deferred/group events, root nodes)
            for (const auto& [key, targetNodes] : groupToNodes)
            {
                std::string nid = "grpNode" + std::to_string(idx++);
                inject += nid + "[label=\"" + key + "\" style=filled fillcolor=lightgreen];\n";
                for (const auto& tn : targetNodes)
                    inject += nid + " -> " + tn + ";\n";
            }

            // Insert inside the subgraph, before its closing '}'
            auto insertPos = result.rfind('}');
            if (insertPos != std::string::npos and insertPos > 0)
                insertPos = result.rfind('}', insertPos - 1);
            if (insertPos != std::string::npos)
                result.insert(insertPos, inject);
        }

        // --- output: file or stdout ---
        if (outputFile.empty())
        {
            std::cout << result;
        }
        else
        {
            std::ofstream f(outputFile);
            if (f.is_open())
            {
                f << result;
                LOG_INFO("ECS", "Taskflow graph written to: " << outputFile);
            }
            else
            {
                LOG_ERROR("ECS", "dumbTaskflow: could not open file: " << outputFile);
                std::cout << result; // fallback to stdout
            }
        }
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

    std::vector<EntityRef> EntitySystem::createEntities(size_t count)
    {
        LOG_THIS_MEMBER("ECS");

        std::vector<EntityRef> result;
        result.reserve(count);

        if (running)
        {
            for (size_t i = 0; i < count; ++i)
                result.push_back(cmdDispatcher.createEntity());
        }
        else
        {
            const auto idList = registry.idGenerator.generateIdList(count);

            // Use parallel construction when there are enough entities to amortise
            // the task-submission overhead and more than one executor thread is available.
            constexpr size_t parallelThreshold = 64;
            if (count > parallelThreshold and NBEXECUTORTHREADS > 1)
            {
                entityPool.addComponentsParallel(idList, std::back_inserter(result),
                    [this](size_t n, auto body)
                    {
                        tf::Taskflow tfLocal;
                        const size_t chunkSize = (n + NBEXECUTORTHREADS - 1) / NBEXECUTORTHREADS;
                        for (size_t t = 0; t < NBEXECUTORTHREADS and t * chunkSize < n; ++t)
                        {
                            const size_t start = t * chunkSize;
                            const size_t end   = std::min(start + chunkSize, n);
                            tfLocal.emplace([body, start, end]()
                            {
                                for (size_t i = start; i < end; ++i)
                                    body(i);
                            });
                        }
                        taskflowImpl->executor.run(tfLocal).wait();
                    },
                    this);
            }
            else
            {
                entityPool.addComponents(idList, std::back_inserter(result), this);
            }
        }

        return result;
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
            {
                name = std::to_string(system->_id);
                system->__name = name; // keep __name in sync with the DOT task label
            }

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

    // createInterpreterSystem is now implemented in entitysystem_full.cpp or entitysystem_minimal.cpp

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

    ScriptRegistry& EntitySystem::scripts()
    {
        return *scriptRegistry;
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
        vm.addNativeModule("ecs", EcsCompiledModule{this});

        // Setup full build modules (implemented in entitysystem_full.cpp or entitysystem_minimal.cpp)
        setupVmFullModules(vm);

        // Register any custom VM modules that were added via registerCustomVmModule
        for (const auto& registerModule : customVmModules)
        {
            registerModule(vm);
        }

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
                else if (IS_NAT_FUNC(value))
                    std::cout << "<native function>";
                else if (IS_CLOSURE(value))
                    std::cout << "<closure>";
                else if (IS_BOUND_METHOD(value))
                    std::cout << "<bound method>";
                else if (IS_UPVALUE(value))
                    std::cout << "<upvalue>";
                else if (IS_CUSTOM_PTR(value))
                    std::cout << "<custom pointer>";
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
                for (const auto& [key, v] : table->internedFields)
                {
                    auto value = table->fieldValues[v];

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

            for (const auto& [key, slot] : vm->globalSlots)
            {
                const VM::GlobalCell& globalCell = vm->globalCells[slot];
                if (not globalCell.defined)
                    continue;
                const Value value = globalCell.value;
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

        // Compiler front-end choice (Pratt vs AST) propagates to every VM
        // this ECS creates, including ScriptRegistry compilations
        vm.setFrontEnd(vmFrontEnd);

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

            // Operand-elision specialization (the old BasicOperatorLocal-
            // Indexing / ComparisonLocalIndexing / SetLocalPopFusion passes)
            // now happens at DECODE time (decoded_fusion.h) — the bytecode
            // stays generic. The remaining passes are genuine bytecode
            // peepholes: jump shrinking, folding, redundancy removal.
            vm.enableDecodeFusion = true;

            vm.addOptimizationPass(std::make_unique<LongJumpOptimizationPass>());
            vm.addOptimizationPass(std::make_unique<PoppingJumpPass>());
            vm.addOptimizationPass(std::make_unique<RemoveUselessJumpPass>());

            vm.addOptimizationPass(std::make_unique<RemoveDefGetGlobalRedunduncy>());

            // IncrementOptimization matches `Get_Local + Constant + Add +
            // Set_Local + Pop` — must run BEFORE FuseOpPop merges adjacent
            // Pops into PopN, otherwise the trailing single Pop is gone.
            vm.addOptimizationPass(std::make_unique<IncrementOptimizationPass>());

            vm.addOptimizationPass(std::make_unique<FuseOpPop>());

            vm.addOptimizationPass(std::make_unique<ConstantFoldingPass>());

            vm.addOptimizationPass(std::make_unique<ConstantVarAccess>());

            // This doesn't work if there is a closure capturing the constant variable.
            vm.addOptimizationPass(std::make_unique<SimplifyConstantToShort>());

            // Loop rotation runs LAST: it consumes the popping/shrunk jump forms
            // and rewrites test-at-top loops (while / for-in) into test-at-bottom
            // form, dropping the unconditional OP_Loop. No later pass observes the
            // new OP_Jump_If_True_Popping opcode.
            vm.addOptimizationPass(std::make_unique<LoopRotationPass>());

            // AST-level passes: only run on the AST front-end path (between
            // parse and emission); the Pratt front-end never sees them
            vm.addAstPass(std::make_unique<LoopInvariantHoistingPass>());
        }
        else if (vmOptimizationLevel == VmOptimizationLevel::O0)
        {
            vm.disableBytecodeOptimization();
            vm.enableDecodeFusion = false;
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

    void EntitySystem::autoSucceedMasterRenderer(BaseAbstractRenderer* abr, _unique_id subId)
    {
#ifndef PG_MINIMAL_BUILD
        MasterRenderer* mr = abr->getMasterRenderer();

        if (mr == nullptr or mr->_id == 0)
        {
            LOG_ERROR("ECS", "Sub-renderer " << subId << " created with no registered MasterRenderer; skipping auto-succeed");
            return;
        }

        _succeed(mr->_id, subId);
#else
        // The minimal engine has no renderer; nothing to succeed.
        (void) abr;
        (void) subId;
#endif // PG_MINIMAL_BUILD
    }

    Value ComponentSerializerRegistry::createComponentProxy(const std::string& componentName, VM* vm, void* componentPtr) const
    {
        auto factory = getProxyFactory(componentName);

        if (factory)
        {
            return factory(vm, componentPtr);
        }

        return INT_VAL(-1);  // Return sentinel value if no factory
    }
}