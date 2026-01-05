#pragma once

#include <string>
#include <vector>
#include <functional>
#include <unordered_map>

#include "Memory/elementtype.h"

// Those include are not necessary here, but anyone that wants to use a standard system
// Should use at least one of them.
#include "component.h"
#include "standardevent.h"

namespace pg
{
    // Forward declarations to avoid including heavy headers
    class EntitySystem;
    class ComponentRegistry;
    struct AbstractSystem;
    struct StandardEvent;
    struct StandardComponent;
    class StandardSystemHandle;
    class StandardSystemImpl;

    /**
     * @brief A simplified interface for creating systems that work with StandardEvent
     *
     * This wrapper hides all the ECS boilerplate (system.h, component registration, etc.)
     * and provides a clean interface for creating systems that listen to named events
     * and work with named components (both using name + map<string, ElementType>).
     *
     * Benefits:
     * - Fast compilation (doesn't include system.h)
     * - Simple interface (no template complexity)
     * - Automatic registration of events and components
     * - Works with dynamic StandardEvent and StandardComponent types
     *
     * Usage example:
     * @code
     * auto mySystem = StandardSystemBuilder("MyGameSystem")
     *     .listenToEvents({"PlayerJump", "PlayerDamage", "GameStart"})
     *     .ownComponents({"Health", "Position"})
     *     .onInit([](StandardSystemHandle* sys) {
     *         // Initialize system (C++ callback)
     *     })
     *     .onInit("scripts/init.pgs")  // Or use a script
     *     .onEvent([](StandardSystemHandle* sys, const StandardEvent& event) {
     *         if (event.name == "PlayerJump") {
     *             // Handle jump (C++ callback)
     *         }
     *     })
     *     .onEvent("PlayerDamage", "scripts/damage.pgs")  // Or use a script for events
     *     .onExecute([](StandardSystemHandle* sys) {
     *         // Called every frame (C++ callback)
     *     })
     *     .onExecute("scripts/update.pgs")  // Or use a script
     *     .build();
     * @endcode
     */

    // Set callbacks
    using _S_InitCallback = std::function<void(StandardSystemHandle*)>;
    using _S_EventCallback = std::function<void(StandardSystemHandle*, const StandardEvent&)>;
    using _S_ExecuteCallback = std::function<void(StandardSystemHandle*)>;
    using _S_SaveCallback = std::function<void(StandardSystemHandle*, ElementMap&)>;
    using _S_LoadCallback = std::function<void(StandardSystemHandle*, const ElementMap&)>;
    using _S_DeltaCallback = std::function<void(StandardSystemHandle*, float)>;

    using _S_EventMap = std::unordered_map<std::string, _S_EventCallback>;
    using _S_EventScriptMap = std::unordered_map<std::string, std::string>;

    /**
     * @brief Builder for creating standard systems
     */
    class StandardSystemBuilder
    {
    public:
        StandardSystemBuilder(const std::string& systemName);

        // Configure which components this system owns
        StandardSystemBuilder& ownComponents(const std::vector<std::string>& componentNames);
        StandardSystemBuilder& ownComponent(const std::string& componentName);

        StandardSystemBuilder& ownComponent(const std::string& componentName, const std::string& key, const ElementType& value)
        {
            data.componentDefaultValues[componentName][key] = value;
            return ownComponent(componentName);
        }

        template <typename... Args>
        StandardSystemBuilder& ownComponent(const std::string& componentName, const std::string& key, const ElementType& value, Args... args)
        {
            data.componentDefaultValues[componentName][key] = value;
            return ownComponent(componentName, args...);
        }

        // Set execution policy
        StandardSystemBuilder& useStoragePolicy(); // No automatic execute()
        StandardSystemBuilder& useManualPolicy();   // Manual execution only
        StandardSystemBuilder& useParallelPolicy(); // Parallel execution
        StandardSystemBuilder& useSequentialPolicy(); // Default: sequential

        // Enable save/load
        StandardSystemBuilder& enableSaveLoad();

        StandardSystemBuilder& onInit(_S_InitCallback callback);
        StandardSystemBuilder& onEvent(const std::string& eventName, _S_EventCallback callback);
        StandardSystemBuilder& onExecute(_S_ExecuteCallback callback);
        StandardSystemBuilder& onSave(_S_SaveCallback callback);
        StandardSystemBuilder& onLoad(_S_LoadCallback callback);
        StandardSystemBuilder& onFirstLoad(_S_InitCallback callback);
        StandardSystemBuilder& onDelta(_S_DeltaCallback callback);

        // Scripts overload
        StandardSystemBuilder& onInit(const std::string& scriptName);
        StandardSystemBuilder& onEvent(const std::string& eventName, const std::string& scriptName);
        StandardSystemBuilder& onExecute(const std::string& scriptName);
        StandardSystemBuilder& onDelta(const std::string& scriptName);

        // Build and return the system (returns StandardSystemImpl* that can be registered)
        StandardSystemImpl* build();

    private:
        struct BuilderData
        {
            std::string systemName;
            std::vector<std::string> componentNames;
            std::unordered_map<std::string, ElementMap> componentDefaultValues;
            std::string executionPolicy = "sequential"; // sequential, storage, manual, parallel
            bool saveLoadEnabled = false;

            _S_InitCallback initCallback;
            std::string initScript;

            _S_EventMap eventCallbackList;
            _S_EventScriptMap scriptEventCallbackList;

            _S_ExecuteCallback executeCallback;
            std::string executeScript;
            _S_SaveCallback saveCallback;
            _S_LoadCallback loadCallback;
            _S_InitCallback firstLoadCallback;

            _S_DeltaCallback deltaCallback;
            std::string deltaScript;
        };

        BuilderData data;
    };

    /**
     * @brief Helper function to create a standard system inline
     *
     * @param systemName Name of the system
     * @return A builder to configure the system
     */
    inline StandardSystemBuilder createStandardSystem(const std::string& systemName)
    {
        return StandardSystemBuilder(systemName);
    }
}