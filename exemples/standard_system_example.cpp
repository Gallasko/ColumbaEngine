/**
 * @file standard_system_example.cpp
 * @brief Example demonstrating how to use the StandardSystem wrapper
 *
 * This example shows how to create systems using the StandardSystemBuilder
 * without needing to include system.h or deal with template complexity.
 */

#include "ECS/standardsystem.h"
#include "ECS/entitysystem.h"

namespace pg
{
    /**
     * Example 1: Simple event-based system
     * This system listens to "PlayerJump" and "PlayerLand" events
     */
    AbstractSystem* createPlayerMovementSystem()
    {
        return createStandardSystem("PlayerMovement")
            .listenToEvents({"PlayerJump", "PlayerLand", "PlayerMove"})
            .onInit([](StandardSystemHandle* sys) {
                // Initialize system
                LOG_INFO("PlayerMovement", "System initialized");
            })
            .onEvent([](StandardSystemHandle* sys, const StandardEvent& event) {
                if (event.name == "PlayerJump")
                {
                    LOG_INFO("PlayerMovement", "Player jumped!");

                    // Access event data
                    if (event.values.find("jumpPower") != event.values.end())
                    {
                        float power = event.values.at("jumpPower").get<float>();
                        LOG_INFO("PlayerMovement", "Jump power: " << power);
                    }

                    // Send response event
                    sys->sendEvent("PlayerInAir");
                }
                else if (event.name == "PlayerLand")
                {
                    LOG_INFO("PlayerMovement", "Player landed!");
                    sys->sendEvent("PlayerGrounded");
                }
                else if (event.name == "PlayerMove")
                {
                    // Handle movement
                    if (event.values.find("x") != event.values.end() &&
                        event.values.find("y") != event.values.end())
                    {
                        float x = event.values.at("x").get<float>();
                        float y = event.values.at("y").get<float>();
                        LOG_INFO("PlayerMovement", "Moving to (" << x << ", " << y << ")");
                    }
                }
            })
            .useStoragePolicy() // Only react to events, no execute() needed
            .build();
    }

    /**
     * Example 2: System with execute loop (runs every frame)
     */
    AbstractSystem* createHealthRegenerationSystem()
    {
        return createStandardSystem("HealthRegeneration")
            .listenToEvent("TickEvent")
            .onInit([](StandardSystemHandle* sys) {
                LOG_INFO("HealthRegen", "Health regeneration system ready");
            })
            .onEvent([](StandardSystemHandle* sys, const StandardEvent& event) {
                if (event.name == "TickEvent")
                {
                    // Event carries tick duration
                    if (event.values.find("tick") != event.values.end())
                    {
                        float deltaTime = event.values.at("tick").get<float>();
                        // Process regeneration based on delta time
                    }
                }
            })
            .onExecute([](StandardSystemHandle* sys) {
                // This runs every frame
                // You can iterate through entities here and regenerate health
                LOG_INFO("HealthRegen", "Regenerating health for all players");
            })
            .useSequentialPolicy() // Default, but shown explicitly
            .build();
    }

    /**
     * Example 3: System with save/load support
     */
    AbstractSystem* createGameProgressSystem()
    {
        return createStandardSystem("GameProgress")
            .listenToEvents({"LevelComplete", "AchievementUnlocked", "GameSave"})
            .enableSaveLoad()
            .onInit([](StandardSystemHandle* sys) {
                LOG_INFO("GameProgress", "Game progress system initialized");
            })
            .onEvent([](StandardSystemHandle* sys, const StandardEvent& event) {
                if (event.name == "LevelComplete")
                {
                    int level = event.values.at("level").get<int>();
                    LOG_INFO("GameProgress", "Level " << level << " completed!");

                    // Trigger save
                    sys->sendEvent("GameSave");
                }
                else if (event.name == "AchievementUnlocked")
                {
                    std::string achievement = event.values.at("name").get<std::string>();
                    LOG_INFO("GameProgress", "Achievement unlocked: " << achievement);
                }
            })
            .onSave([](StandardSystemHandle* sys, std::unordered_map<std::string, ElementType>& saveData) {
                // Save your data
                saveData["currentLevel"] = ElementType{5};
                saveData["playerScore"] = ElementType{1000.0f};
                saveData["completedLevels"] = ElementType{3};
                LOG_INFO("GameProgress", "Saving game progress...");
            })
            .onLoad([](StandardSystemHandle* sys, const std::unordered_map<std::string, ElementType>& loadData) {
                // Load your data
                if (loadData.find("currentLevel") != loadData.end())
                {
                    int level = loadData.at("currentLevel").get<int>();
                    LOG_INFO("GameProgress", "Loaded level: " << level);
                }
                if (loadData.find("playerScore") != loadData.end())
                {
                    float score = loadData.at("playerScore").get<float>();
                    LOG_INFO("GameProgress", "Loaded score: " << score);
                }
            })
            .onFirstLoad([](StandardSystemHandle* sys) {
                // Initialize default values when no save exists
                LOG_INFO("GameProgress", "No save file found, starting new game");
            })
            .build();
    }

    /**
     * Example 4: Complex game system with multiple responsibilities
     */
    auto* createInventorySystem()
    {
        // This could be stored somewhere accessible by callbacks if needed
        struct InventoryData
        {
            std::unordered_map<std::string, int> items;
            int maxSlots = 20;
        };

        // Note: For stateful systems, you might want to capture shared state
        // via a shared_ptr in the lambdas

        return createStandardSystem("Inventory")
            .listenToEvents({
                "AddItem",
                "RemoveItem",
                "UseItem",
                "DropItem",
                "InventoryFull"
            })
            .enableSaveLoad()
            .onInit([](StandardSystemHandle* sys) {
                LOG_INFO("Inventory", "Inventory system ready");

                // Register for UI update events
                sys->sendEvent("InventoryInitialized");
            })
            .onEvent([](StandardSystemHandle* sys, const StandardEvent& event) {
                if (event.name == "AddItem")
                {
                    std::string itemName = event.values.at("itemName").get<std::string>();
                    int quantity = event.values.at("quantity").get<int>();

                    LOG_INFO("Inventory", "Adding " << quantity << "x " << itemName);

                    // Check if inventory is full
                    // If full, send InventoryFull event
                    // sys->sendEvent("InventoryFull");

                    // Update UI
                    StandardEvent uiEvent("UpdateInventoryUI");
                    uiEvent.values["itemName"] = ElementType{itemName};
                    uiEvent.values["quantity"] = ElementType{quantity};
                    sys->sendEvent(uiEvent);
                }
                else if (event.name == "RemoveItem")
                {
                    std::string itemName = event.values.at("itemName").get<std::string>();
                    int quantity = event.values.at("quantity").get<int>();

                    LOG_INFO("Inventory", "Removing " << quantity << "x " << itemName);
                }
                else if (event.name == "UseItem")
                {
                    std::string itemName = event.values.at("itemName").get<std::string>();
                    LOG_INFO("Inventory", "Using item: " << itemName);

                    // Send item effect event
                    sys->sendEvent("ItemUsed", "itemName", ElementType{itemName});
                }
            })
            .onSave([](StandardSystemHandle* sys, std::unordered_map<std::string, ElementType>& saveData) {
                // Save inventory state
                // You'd serialize your InventoryData here
                LOG_INFO("Inventory", "Saving inventory...");
            })
            .onLoad([](StandardSystemHandle* sys, const std::unordered_map<std::string, ElementType>& loadData) {
                // Load inventory state
                LOG_INFO("Inventory", "Loading inventory...");
            })
            .useStoragePolicy() // Event-driven, no per-frame execution needed
            .build();
    }

    /**
     * Example 5: How to register these systems with EntitySystem
     */
    void setupGameSystems(EntitySystem* ecs)
    {
        // Create and register systems
        auto playerMovement = createPlayerMovementSystem();
        auto healthRegen = createHealthRegenerationSystem();
        auto gameProgress = createGameProgressSystem();
        auto inventory = createInventorySystem();

        // Register with ECS
        ecs->registerSystem(playerMovement);
        ecs->registerSystem(healthRegen);
        ecs->registerSystem(gameProgress);
        ecs->registerSystem(inventory);

        LOG_INFO("Example", "All game systems registered");

        // Now you can send events and the systems will respond
        // Example: Trigger a player jump
        StandardEvent jumpEvent("PlayerJump");
        jumpEvent.values["jumpPower"] = ElementType{5.0f};
        ecs->sendEvent(jumpEvent);

        // Example: Add item to inventory
        StandardEvent addItemEvent("AddItem");
        addItemEvent.values["itemName"] = ElementType{std::string("Health Potion")};
        addItemEvent.values["quantity"] = ElementType{3};
        ecs->sendEvent(addItemEvent);
    }
}

/**
 * Benefits of this approach:
 *
 * 1. Fast compilation:
 *    - Users don't include system.h (heavy template header)
 *    - Only include standardsystem.h (forward declarations + simple interface)
 *
 * 2. Simple API:
 *    - No template syntax required
 *    - Fluent builder pattern
 *    - Lambda-based callbacks
 *
 * 3. Complete ECS integration:
 *    - Automatic event registration
 *    - Access to EntitySystem via handle
 *    - Can send events to other systems
 *
 * 4. Flexible:
 *    - Storage policy (event-only)
 *    - Sequential execution (with execute())
 *    - Save/load support
 *    - All ECS features available
 *
 * 5. Type-safe and dynamic:
 *    - StandardEvent uses name + map<string, ElementType>
 *    - StandardComponent uses name + map<string, ElementType>
 *    - Runtime flexibility with compile-time safety
 */
