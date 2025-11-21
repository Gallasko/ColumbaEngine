#include "system.h"

#include "Compiler/vm.h"
#include "Compiler/ecsserialization.h"
#include <sstream>

namespace pg
{
    // Todo move this in a vm helper header
    bool checkCompiledScript(EntitySystem* ecsRef, std::string& scriptName)
    {
        if (scriptName.size() >= 3 && scriptName.substr(scriptName.size() - 3) == ".pg")
        {
            // Compile .pg script and cache to .pgc
            VM compiler;
            ecsRef->setupVm(compiler);

            auto result = compiler.interpretFromFile(scriptName, true, scriptName + "c");

            if (result != InterpretResult::OK)
            {
                LOG_ERROR("CollisionHandleScript", "Failed to compile script: " << scriptName);
            }

            scriptName += "c";

            return true;
        }
        else if (scriptName.size() >= 4 && scriptName.substr(scriptName.size() - 4) == ".pgc")
        {
            LOG_MILE("CollisionHandleScript", "Loading precompiled script: " << scriptName);

            return true;
        }
        else
        {
            LOG_ERROR("CollisionHandleScript", "Invalid script file extension. Must be .pg or .pgc: " << scriptName);
        }

        return false;
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
            if (not checkCompiledScript(ecsRef, scriptName))
            {
                LOG_ERROR("StandardSystemImpl", "Cannot compile or open the script: " << scriptName);
                continue;
            }

            // Read the bytecode file once into memory
            std::ifstream file(scriptName, std::ios::binary);
            if (!file)
            {
                LOG_ERROR("StandardSystemImpl", "Failed to open bytecode file: " << scriptName);
                continue;
            }

            // Get file size and read entire file
            file.seekg(0, std::ios::end);
            size_t fileSize = file.tellg();
            file.seekg(0, std::ios::beg);

            std::vector<char> cachedBytecode(fileSize);
            file.read(cachedBytecode.data(), fileSize);

            if (!file)
            {
                LOG_ERROR("StandardSystemImpl", "Failed to read bytecode file: " << scriptName);
                continue;
            }

            LOG_MILE("StandardSystemImpl", "Cached bytecode for event '" << eventName << "': " << scriptName << " (" << fileSize << " bytes)");

            // Register the event handler with cached bytecode (captured by value)
            eventCompiledScriptCallbackList.emplace(eventName, [cachedBytecode, scriptName](StandardSystemHandle* sys, const StandardEvent& event) {
                auto ecsRef = sys->getWorld();

                VM vm;
                ecsRef->setupVm(vm);

                // Set up event data before interpreting
                auto value = serializeToTable(&vm, event);
                vm.globals["event"] = value;

                // ========================================================================
                // System Data Setup - Expose system's persistent data storage to scripts
                // ========================================================================
                // The system data (ElementMap) is serialized to a VM table called "sysData"
                // Scripts can read/write to this table using standard field access:
                //   sysData.myCounter = sysData.myCounter + 1
                //   local x = sysData.someValue
                //
                // After script execution, changes are copied back to C++ ElementMap
                // This allows systems to maintain state between script invocations
                //
                // TODO: If immediate synchronization is needed during script execution,
                //       consider implementing setter methods (see StandardComponent setters)
                // ========================================================================
                if (sys->_internalSystemPtr)
                {
                    StandardSystemImpl* sysImpl = static_cast<StandardSystemImpl*>(sys->_internalSystemPtr);
                    ElementMap& sysData = sysImpl->getSystemData();

                    // Create a VM table to hold system data
                    auto it = vm.globals.find("__Table");
                    if (it != vm.globals.end())
                    {
                        Klass* tableClass = vm.asClass(it->second);
                        Value dataTableValue = vm.createInstance(tableClass);
                        ObjInstance* dataTable = vm.asInstance(dataTableValue);

                        // Copy all C++ ElementMap entries to VM table
                        for (const auto& [key, elemValue] : sysData)
                        {
                            dataTable->fields[key] = vm.retainValue(vm.elementToValue(elemValue));
                        }

                        vm.globals["sysData"] = dataTableValue;
                    }
                }

                // Interpret cached bytecode
                InterpretResult result = vm.interpretFromCachedBytecode(cachedBytecode, 0);

                // ========================================================================
                // System Data Synchronization - Copy script changes back to C++
                // ========================================================================
                // After script execution, any changes made to sysData table are copied
                // back to the C++ ElementMap so they persist across script invocations
                // ========================================================================
                if (sys->_internalSystemPtr && result == InterpretResult::OK)
                {
                    StandardSystemImpl* sysImpl = static_cast<StandardSystemImpl*>(sys->_internalSystemPtr);
                    ElementMap& sysData = sysImpl->getSystemData();

                    auto it = vm.globals.find("sysData");
                    if (it != vm.globals.end() && IS_INSTANCE(it->second))
                    {
                        ObjInstance* dataTable = vm.asInstance(it->second);

                        // Copy all fields from VM table back to C++ ElementMap
                        // This overwrites existing keys and adds new ones
                        for (const auto& [key, vmValue] : dataTable->fields)
                        {
                            // Skip internal VM fields
                            if (key != "__className" && !key.empty())
                            {
                                sysData[key] = vm.valueToElement(vmValue);
                            }
                        }
                    }
                }

                if (result != InterpretResult::OK)
                {
                    LOG_ERROR("StandardSystemImpl", "Event script handler error for: " << scriptName);
                    LOG_ERROR("StandardSystemImpl", "Interpret result: " << (result == InterpretResult::COMPILE_ERROR ? "COMPILE_ERROR" : "RUNTIME_ERROR"));
                    LOG_ERROR("StandardSystemImpl", "Check VM error messages above for details");
                }
            });
        }

        // Compile and cache execute script if provided
        if (!executeScript.empty())
        {
            if (not checkCompiledScript(ecsRef, executeScript))
            {
                LOG_ERROR("StandardSystemImpl", "Cannot compile or open the execute script: " << executeScript);
            }
            else
            {
                // Read the bytecode file once into memory
                std::ifstream file(executeScript, std::ios::binary);
                if (!file)
                {
                    LOG_ERROR("StandardSystemImpl", "Failed to open execute bytecode file: " << executeScript);
                }
                else
                {
                    // Get file size and read entire file
                    file.seekg(0, std::ios::end);
                    size_t fileSize = file.tellg();
                    file.seekg(0, std::ios::beg);

                    std::vector<char> cachedBytecode(fileSize);
                    file.read(cachedBytecode.data(), fileSize);

                    if (!file)
                    {
                        LOG_ERROR("StandardSystemImpl", "Failed to read execute bytecode file: " << executeScript);
                    }
                    else
                    {
                        LOG_MILE("StandardSystemImpl", "Cached bytecode for execute script: " << executeScript << " (" << fileSize << " bytes)");

                        // Register the execute handler with cached bytecode (captured by value)
                        compiledExecuteScriptCallback = [this, cachedBytecode, scriptName = executeScript](StandardSystemHandle* sys) {
                            auto ecsRef = sys->getWorld();

                            VM vm;
                            ecsRef->setupVm(vm);

                            for (auto [compName, owner] : componentOwners)
                            {
                                auto v = owner->view();

                                int i = 0;

                                for (auto val : v)
                                {
                                    // Use specialized overload for StandardComponent that generates setters
                                    // The component already has ecsRef and entityId set
                                    auto value = serializeToTable(&vm, *val);
                                    vm.globals[compName + "_" + std::to_string(i)] = value;

                                    i++;
                                }
                            }

                            // ========================================================================
                            // System Data Setup - Expose system's persistent data storage to scripts
                            // ========================================================================
                            // The system data (ElementMap) is serialized to a VM table called "sysData"
                            // Scripts can read/write to this table using standard field access:
                            //   sysData.myCounter = sysData.myCounter + 1
                            //   local x = sysData.someValue
                            //
                            // After script execution, changes are copied back to C++ ElementMap
                            // This allows systems to maintain state between script invocations
                            //
                            // TODO: If immediate synchronization is needed during script execution,
                            //       consider implementing setter methods (see StandardComponent setters)
                            // ========================================================================
                            if (sys->_internalSystemPtr)
                            {
                                StandardSystemImpl* sysImpl = static_cast<StandardSystemImpl*>(sys->_internalSystemPtr);
                                ElementMap& sysData = sysImpl->getSystemData();

                                // Create a VM table to hold system data
                                auto it = vm.globals.find("__Table");
                                if (it != vm.globals.end())
                                {
                                    Klass* tableClass = vm.asClass(it->second);
                                    Value dataTableValue = vm.createInstance(tableClass);
                                    ObjInstance* dataTable = vm.asInstance(dataTableValue);

                                    // Copy all C++ ElementMap entries to VM table
                                    for (const auto& [key, elemValue] : sysData)
                                    {
                                        dataTable->fields[key] = vm.retainValue(vm.elementToValue(elemValue));
                                    }

                                    vm.globals["sysData"] = dataTableValue;
                                }
                            }

                            // Interpret cached bytecode
                            InterpretResult result = vm.interpretFromCachedBytecode(cachedBytecode, 0);

                            // ========================================================================
                            // System Data Synchronization - Copy script changes back to C++
                            // ========================================================================
                            // After script execution, any changes made to sysData table are copied
                            // back to the C++ ElementMap so they persist across script invocations
                            // ========================================================================
                            if (sys->_internalSystemPtr && result == InterpretResult::OK)
                            {
                                StandardSystemImpl* sysImpl = static_cast<StandardSystemImpl*>(sys->_internalSystemPtr);
                                ElementMap& sysData = sysImpl->getSystemData();

                                auto it = vm.globals.find("sysData");
                                if (it != vm.globals.end() && IS_INSTANCE(it->second))
                                {
                                    ObjInstance* dataTable = vm.asInstance(it->second);

                                    // Copy all fields from VM table back to C++ ElementMap
                                    // This overwrites existing keys and adds new ones
                                    for (const auto& [key, vmValue] : dataTable->fields)
                                    {
                                        // Skip internal VM fields
                                        if (key != "__className" && !key.empty())
                                        {
                                            sysData[key] = vm.valueToElement(vmValue);
                                        }
                                    }
                                }
                            }

                            if (result != InterpretResult::OK)
                            {
                                LOG_ERROR("StandardSystemImpl", "Execute script handler error for: " << scriptName);
                                LOG_ERROR("StandardSystemImpl", "Interpret result: " << (result == InterpretResult::COMPILE_ERROR ? "COMPILE_ERROR" : "RUNTIME_ERROR"));
                                LOG_ERROR("StandardSystemImpl", "Check VM error messages above for details");
                            }
                        };
                    }
                }
            }
        }

        // Register event listeners
        for (const auto& eventName : listenedEvents)
        {
            registry->addStandardEventListener(eventName, this);
        }

        LOG_INFO("StandardSystemImpl", "System fully registered with " << componentOwners.size() << " components and " << listenedEvents.size() << " events");
    }
}