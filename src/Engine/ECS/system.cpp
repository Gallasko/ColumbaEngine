#include "system.h"

#include "Compiler/vm.h"
#include "Compiler/chunk_serializer.h"
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
            componentOwners[componentName] = owner;

            LOG_INFO("StandardSystemImpl", "Registered component owner for: " << componentName);
        }

        for (auto [eventName, scriptName] : eventScriptCallbackList)
        {
            if (not checkCompiledScript(ecsRef, scriptName))
            {
                LOG_ERROR("StandardSystemImpl", "Cannot compile or open the script: " << eventName);
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

                // Todo add a interpret method in vm that does just that

                // Deserialize from cached memory (NO FILE I/O!)
                std::istringstream bytecodeStream(std::string(cachedBytecode.begin(), cachedBytecode.end()), std::ios::binary);

                Chunk chunk;
                if (!ChunkSerializer::deserialize(chunk, bytecodeStream, &vm))
                {
                    LOG_ERROR("StandardSystemImpl", "Failed to deserialize cached bytecode: " << scriptName);
                    return;
                }

                // Create function from deserialized chunk
                auto function = vm.createFunction();
                ObjFunction* funcObj = vm.asFunction(function);
                funcObj->chunk = chunk;

                auto closureValue = vm.createClosure(funcObj);
                Closure* closure = vm.asClosure(closureValue);
                vm.push(closureValue);

                // Set up event data
                auto value = serializeToTable(&vm, event);
                vm.globals["event"] = value;

                vm.call(closure, 0);

                InterpretResult result;
                try
                {
                    vm.pools.freezeConstantIndices();
                    result = vm.run();
                }
                catch(const std::exception& e)
                {
                    LOG_ERROR("StandardSystemImpl", "Event script handler error: " << e.what());
                    result = InterpretResult::RUNTIME_ERROR;
                }

                if (result != InterpretResult::OK)
                {
                    LOG_ERROR("StandardSystemImpl", "Event script handler error for: " << scriptName);
                }
            });
        }

        // data.eventCallback = [scriptName](StandardSystemHandle* sys, const StandardEvent& event) -> void {
        //     auto ecsRef = sys->getWorld();

        //     VM vm;

        //     ecsRef->setupVm(vm);

        //     auto sName = scriptName;

        //     if (not checkCompiledScript(ecsRef, sName))
        //     {
        //         return;
        //     }

        //     auto value = serializeToTable(&vm, event);

        //     vm.globals["event"] = value;

        //     // Compile the script once
        //     auto result = vm.interpretFromBytecodeFile(sName);

        //     if (result != InterpretResult::OK)
        //     {
        //         LOG_ERROR("StandardSystem", "Event script handler error.");
        //     }
        // };

        // Register event listeners
        for (const auto& eventName : listenedEvents)
        {
            registry->addStandardEventListener(eventName, this);
        }

        LOG_INFO("StandardSystemImpl", "System fully registered with " << componentOwners.size() << " components and " << listenedEvents.size() << " events");
    }
}