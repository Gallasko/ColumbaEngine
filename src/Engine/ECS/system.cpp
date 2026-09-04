#include "system.h"

#include "entitysystem.h"
#include "scriptregistry.h"

#include "Compiler/vm.h"
#include "Compiler/ecsserialization.h"
#include "ECS/sysmodule.h"

#include <sstream>

#include "Systems/coresystems.h"

namespace pg
{
    InterpretResult interpretWithSysData(StandardSystemHandle* sys, VM& vm, const std::vector<char>& cachedBytecode, const std::string& scriptName = "")
    {
        // ========================================================================
        // System Data Setup - Expose system's persistent data storage to scripts
        // ========================================================================
        // The system data (ElementMap) is serialized to a VM table called "sysData"
        // Scripts can read/write to this table using standard field access:
        //   sysData.myCounter = sysData.myCounter + 1
        //   var x = sysData.someValue
        //
        // After script execution, changes are copied back to C++ ElementMap
        // This allows systems to maintain state between script invocations
        //
        // TODO: If immediate synchronization is needed during script execution,
        //       consider implementing setter methods (see StandardComponent setters)
        // ========================================================================
        if (sys->_internalSystemPtr)
        {
            ElementMap& sysData = sys->_internalSystemPtr->getSystemData();

            // Create a VM table to hold system data
            VM::GlobalCell* cell = vm.findGlobalCell("__Table");
            if (cell != nullptr and cell->defined)
            {
                Klass* tableClass = vm.asClass(cell->value);
                Value dataTableValue = vm.createInstance(tableClass);
                ObjInstance* dataTable = vm.asInstance(dataTableValue);

                // Copy all C++ ElementMap entries to VM table
                for (const auto& [key, elemValue] : sysData)
                {
                    dataTable->setField(key, vm.retainValue(vm.elementToValue(elemValue)));
                }

                vm.defineGlobal("sysData", dataTableValue);
            }
        }

        // Interpret cached bytecode
        InterpretResult result = vm.interpretFromCachedBytecode(cachedBytecode, 0, scriptName);

        // ========================================================================
        // System Data Synchronization - Copy script changes back to C++
        // ========================================================================
        // After script execution, any changes made to sysData table are copied
        // back to the C++ ElementMap so they persist across script invocations
        // ========================================================================
        if (sys->_internalSystemPtr and result == InterpretResult::OK)
        {
            ElementMap& sysData = sys->_internalSystemPtr->getSystemData();

            VM::GlobalCell* cell = vm.findGlobalCell("sysData");
            if (cell != nullptr and cell->defined and IS_INSTANCE(cell->value))
            {
                ObjInstance* dataTable = vm.asInstance(cell->value);

                // Copy all fields from VM table back to C++ ElementMap
                // This overwrites existing keys and adds new ones
                for (const auto& [key, v] : dataTable->internedFields)
                {
                    auto vmValue = dataTable->fieldValues[v];

                    // Skip internal VM fields
                    if (key != "__className" and not key.empty())
                    {
                        sysData[key] = vm.valueToElement(vmValue);
                    }
                }
            }
        }

        return result;
    }

    void StandardSystemImpl::addToRegistry(ComponentRegistry *registry)
    {
        LOG_THIS_MEMBER("StandardSystemImpl");

        this->registry = registry;

        // Create and register Own<StandardComponent> for each component type
        for (const auto& componentName : ownedComponents)
        {
            auto* owner = new Own<StandardComponent>(componentName);
            owner->setRegistry(registry);

            // Trying to push the default values of the component
            auto it = defaultComponentValues.find(componentName);
            if (it != defaultComponentValues.end())
                owner->setDefaultValue(it->second);

            componentOwners[componentName] = owner;

            LOG_INFO("StandardSystemImpl", "Registered component owner for: " << componentName);
        }

        for (auto [eventName, scriptName] : eventScriptCallbackList)
        {
            auto script = ecsRef->scripts().load(scriptName, this);

            if (not script)
                continue;

            // Create a copy of scriptName for lambda capture (structured bindings can't be captured)
            std::string capturedScriptName = scriptName;

            // Register the event handler with the shared script handle (hot reloadable)
            eventCompiledScriptCallbackList.emplace(eventName, [this, script, capturedScriptName](StandardSystemHandle* sys, const StandardEvent& event) {
                // Pin this run's version of the bytecode
                auto code = script->bytecode();

                if (not code)
                    return;

                PROFILE_SCOPE(capturedScriptName, "Script");

                auto ecsRef = sys->getWorld();

                PROFILE_BEGIN(capturedScriptName + " [setup]", "Script");

                VM vm;
                ecsRef->setupVm(vm);

                // Add sys module for accessing system's entities by component
                vm.addNativeModule("sys", SystemModule{this});

                // Set up event data before interpreting
                auto value = serializeToTable(&vm, event);
                vm.defineGlobal("event", value);

                PROFILE_END(capturedScriptName + " [setup]", "Script");

                auto result = interpretWithSysData(sys, vm, *code, capturedScriptName);

                if (result != InterpretResult::OK)
                {
                    LOG_ERROR("StandardSystemImpl", "Event script handler error for: " << capturedScriptName);
                    LOG_ERROR("StandardSystemImpl", "Interpret result: " << (result == InterpretResult::COMPILE_ERROR ? "COMPILE_ERROR" : "RUNTIME_ERROR"));
                    LOG_ERROR("StandardSystemImpl", "Check VM error messages above for details");
                }
            });
        }

        // Compile and cache init script if provided
        if (not initScript.empty())
        {
            auto script = ecsRef->scripts().load(initScript);

            if (not script)
            {
                LOG_ERROR("StandardSystemImpl", "Cannot compile or open the init script: " << initScript);
            }
            else
            {
                // Register the init handler with the shared script handle
                compiledInitScriptCallback = [script, scriptName = initScript](StandardSystemHandle* sys) {
                    // Pin this run's version of the bytecode
                    auto code = script->bytecode();

                    if (not code)
                        return;

                    PROFILE_SCOPE(scriptName, "Script");

                    auto ecsRef = sys->getWorld();

                    PROFILE_BEGIN(scriptName + " [setup]", "Script");

                    VM vm;
                    ecsRef->setupVm(vm);

                    PROFILE_END(scriptName + " [setup]", "Script");

                    auto result = interpretWithSysData(sys, vm, *code, scriptName);

                    if (result != InterpretResult::OK)
                    {
                        LOG_ERROR("StandardSystemImpl", "Init script handler error for: " << scriptName);
                        LOG_ERROR("StandardSystemImpl", "Interpret result: " << (result == InterpretResult::COMPILE_ERROR ? "COMPILE_ERROR" : "RUNTIME_ERROR"));
                        LOG_ERROR("StandardSystemImpl", "Check VM error messages above for details");
                    }
                };
            }
        }

        // Compile and cache execute script if provided
        if (not executeScript.empty())
        {
            auto script = ecsRef->scripts().load(executeScript, this);

            if (not script)
            {
                LOG_ERROR("StandardSystemImpl", "Cannot compile or open the execute script: " << executeScript);
            }
            else
            {
                // Register the execute handler with the shared script handle (hot reloadable)
                compiledExecuteScriptCallback = [this, script, scriptName = executeScript](StandardSystemHandle* sys) {
                    // Pin this run's version of the bytecode
                    auto code = script->bytecode();

                    if (not code)
                        return;

                    PROFILE_SCOPE(scriptName, "Script");

                    auto ecsRef = sys->getWorld();

                    PROFILE_BEGIN(scriptName + " [setup]", "Script");

                    VM vm;
                    ecsRef->setupVm(vm);

                    // Add sys module for accessing system's entities by component
                    vm.addNativeModule("sys", SystemModule{this});

                    PROFILE_END(scriptName + " [setup]", "Script");

                    auto result = interpretWithSysData(sys, vm, *code, scriptName);

                    if (result != InterpretResult::OK)
                    {
                        LOG_ERROR("StandardSystemImpl", "Execute script handler error for: " << scriptName);
                        LOG_ERROR("StandardSystemImpl", "Interpret result: " << (result == InterpretResult::COMPILE_ERROR ? "COMPILE_ERROR" : "RUNTIME_ERROR"));
                        LOG_ERROR("StandardSystemImpl", "Check VM error messages above for details");
                    }
                };
            }
        }

        if (not deltaScript.empty())
        {
            auto script = ecsRef->scripts().load(deltaScript, this);

            if (not script)
            {
                LOG_ERROR("StandardSystemImpl", "Cannot compile or open the execute script: " << deltaScript);
            }
            else
            {
                // Register the deltaTime handler with the shared script handle (hot reloadable)
                compiledDeltaScriptCallback = [this, script, scriptName = deltaScript](StandardSystemHandle* sys, float deltaTime) {
                    // Pin this run's version of the bytecode
                    auto code = script->bytecode();

                    if (not code)
                        return;

                    PROFILE_SCOPE(scriptName, "Script");

                    auto ecsRef = sys->getWorld();

                    PROFILE_BEGIN(scriptName + " [setup]", "Script");

                    VM vm;
                    ecsRef->setupVm(vm);

                    // Add sys module for accessing system's entities by component
                    vm.addNativeModule("sys", SystemModule{this});

                    vm.defineGlobal("deltaTime", vm.elementToValue(deltaTime));

                    PROFILE_END(scriptName + " [setup]", "Script");

                    auto result = interpretWithSysData(sys, vm, *code, scriptName);

                    if (result != InterpretResult::OK)
                    {
                        LOG_ERROR("StandardSystemImpl", "Delta script handler error for: " << scriptName);
                        LOG_ERROR("StandardSystemImpl", "Interpret result: " << (result == InterpretResult::COMPILE_ERROR ? "COMPILE_ERROR" : "RUNTIME_ERROR"));
                        LOG_ERROR("StandardSystemImpl", "Check VM error messages above for details");
                    }
                };
            }
        }

        if (needDelta)
        {
            registry->addEventListener<TickEvent>(this);
        }

        for (auto [eventName, scriptName] : deferredEventScriptCallbackList)
        {
            auto script = ecsRef->scripts().load(scriptName, this);

            if (not script)
                continue;

            std::string capturedScriptName = scriptName;

            deferredEventCompiledScriptCallbackList.emplace(eventName, [this, script, capturedScriptName](StandardSystemHandle* sys, const StandardEvent& event) {
                // Pin this run's version of the bytecode
                auto code = script->bytecode();

                if (not code)
                    return;

                PROFILE_SCOPE(capturedScriptName, "Script");

                auto ecsRef = sys->getWorld();

                PROFILE_BEGIN(capturedScriptName + " [setup]", "Script");

                VM vm;
                ecsRef->setupVm(vm);

                vm.addNativeModule("sys", SystemModule{this});

                auto value = serializeToTable(&vm, event);
                vm.defineGlobal("event", value);

                PROFILE_END(capturedScriptName + " [setup]", "Script");

                auto result = interpretWithSysData(sys, vm, *code, capturedScriptName);

                if (result != InterpretResult::OK)
                {
                    LOG_ERROR("StandardSystemImpl", "Deferred event script handler error for: " << capturedScriptName);
                    LOG_ERROR("StandardSystemImpl", "Interpret result: " << (result == InterpretResult::COMPILE_ERROR ? "COMPILE_ERROR" : "RUNTIME_ERROR"));
                    LOG_ERROR("StandardSystemImpl", "Check VM error messages above for details");
                }
            });
        }

        // Register immediate event listeners
        for (const auto& eventName : listenedEvents)
        {
            registry->addStandardEventListener(eventName, this);
        }

        // Register deferred event listeners via the same standard path.
        // onEvent() will push them onto _deferredEventQueue instead of processing immediately.
        for (const auto& eventName : listenedDeferredEvents)
        {
            registry->addStandardEventListener(eventName, this);
        }

        // Drain the deferred queue during _execute(), after cmdDispatcher has committed entity changes.
        if (not listenedDeferredEvents.empty())
        {
            _executionQueue.emplace_back([this]()
            {
                while (not _deferredEventQueue.empty())
                {
                    auto event = _deferredEventQueue.front();
                    _deferredEventQueue.pop();
                    onProcessEvent(event);
                }
            });
        }

        LOG_INFO("StandardSystemImpl", "System fully registered with " << componentOwners.size() << " components and " << listenedEvents.size() << " events");

        // Call onRegisterFinished to trigger init callbacks and scripts
        onRegisterFinished();
    }

    void StandardSystemImpl::removeFromRegistry()
    {
        LOG_THIS_MEMBER("StandardSystemImpl");

        // The script registry keeps this system as a compile context for hot
        // reloads; clear it so a reload after our destruction can't use a
        // dangling pointer
        if (ecsRef)
        {
            ecsRef->scripts().onSystemRemoved(this);
        }

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

            if (needDelta)
            {
                registry->removeEventListener<TickEvent>(this);
            }
        }
    }

    void StandardSystemImpl::onEvent(const TickEvent& event)
    {
        deltaTime += event.tick;
    }
}