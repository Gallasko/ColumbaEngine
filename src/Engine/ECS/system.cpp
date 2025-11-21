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

                // Interpret cached bytecode
                InterpretResult result = vm.interpretFromCachedBytecode(cachedBytecode, 0);

                if (result != InterpretResult::OK)
                {
                    LOG_ERROR("StandardSystemImpl", "Event script handler error for: " << scriptName);
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

                            // Interpret cached bytecode
                            InterpretResult result = vm.interpretFromCachedBytecode(cachedBytecode, 0);

                            if (result != InterpretResult::OK)
                            {
                                LOG_ERROR("StandardSystemImpl", "Execute script handler error for: " << scriptName);
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