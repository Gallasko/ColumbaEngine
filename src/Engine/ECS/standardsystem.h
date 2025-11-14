#pragma once

#include <string>
#include <vector>
#include <functional>
#include <unordered_map>

#include "Memory/elementtype.h"

namespace pg
{
    // Forward declarations to avoid including heavy headers
    class EntitySystem;
    class ComponentRegistry;
    struct AbstractSystem;
    struct StandardEvent;
    struct StandardComponent;

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
     *         // Initialize system
     *     })
     *     .onEvent([](StandardSystemHandle* sys, const StandardEvent& event) {
     *         if (event.name == "PlayerJump") {
     *             // Handle jump
     *         }
     *     })
     *     .onExecute([](StandardSystemHandle* sys) {
     *         // Called every frame
     *     })
     *     .build();
     * @endcode
     */

    /**
     * @brief Handle for accessing standard system functionality
     */
    class StandardSystemHandle
    {
    public:
        StandardSystemHandle() = default;
        virtual ~StandardSystemHandle() = default;

        // Access to the ECS world
        EntitySystem* getWorld() const;

        // Send events
        void sendEvent(const StandardEvent& event);
        void sendEvent(const std::string& eventName);
        void sendEvent(const std::string& eventName, const std::string& key, const ElementType& value);

        // Component creation/removal (to be implemented based on your needs)
        // These will work with entities that have StandardComponent attached
        StandardComponent* createComponent(size_t entityId, const std::string& componentType);
        void removeComponent(size_t entityId, const std::string& componentType);
        StandardComponent* getComponent(size_t entityId, const std::string& componentType);

        // Internal use only - stores the actual ECS system pointer
        void* _internalSystemPtr = nullptr;
    };

    /**
     * @brief Builder for creating standard systems
     */
    class StandardSystemBuilder
    {
    public:
        StandardSystemBuilder(const std::string& systemName);

        // Configure which events to listen to
        StandardSystemBuilder& listenToEvents(const std::vector<std::string>& eventNames);
        StandardSystemBuilder& listenToEvent(const std::string& eventName);

        // Configure which components this system owns
        StandardSystemBuilder& ownComponents(const std::vector<std::string>& componentNames);
        StandardSystemBuilder& ownComponent(const std::string& componentName);

        // Set execution policy
        StandardSystemBuilder& useStoragePolicy(); // No automatic execute()
        StandardSystemBuilder& useManualPolicy();   // Manual execution only
        StandardSystemBuilder& useParallelPolicy(); // Parallel execution
        StandardSystemBuilder& useSequentialPolicy(); // Default: sequential

        // Enable save/load
        StandardSystemBuilder& enableSaveLoad();

        // Set callbacks
        using InitCallback = std::function<void(StandardSystemHandle*)>;
        using EventCallback = std::function<void(StandardSystemHandle*, const StandardEvent&)>;
        using ExecuteCallback = std::function<void(StandardSystemHandle*)>;
        using SaveCallback = std::function<void(StandardSystemHandle*, std::unordered_map<std::string, ElementType>&)>;
        using LoadCallback = std::function<void(StandardSystemHandle*, const std::unordered_map<std::string, ElementType>&)>;

        StandardSystemBuilder& onInit(InitCallback callback);
        StandardSystemBuilder& onEvent(EventCallback callback);
        StandardSystemBuilder& onExecute(ExecuteCallback callback);
        StandardSystemBuilder& onSave(SaveCallback callback);
        StandardSystemBuilder& onLoad(LoadCallback callback);
        StandardSystemBuilder& onFirstLoad(InitCallback callback);

        // Build and return the system (returns AbstractSystem* that can be registered)
        AbstractSystem* build();

    private:
        struct BuilderData
        {
            std::string systemName;
            std::vector<std::string> eventNames;
            std::vector<std::string> componentNames;
            std::string executionPolicy = "sequential"; // sequential, storage, manual, parallel
            bool saveLoadEnabled = false;

            InitCallback initCallback;
            EventCallback eventCallback;
            ExecuteCallback executeCallback;
            SaveCallback saveCallback;
            LoadCallback loadCallback;
            InitCallback firstLoadCallback;
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