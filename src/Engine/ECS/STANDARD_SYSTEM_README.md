# StandardSystem - Simplified ECS System Creation

## Overview

The `StandardSystem` provides a simplified wrapper for creating ECS systems that work with `StandardEvent` and `StandardComponent` (name + map of values). It hides all the template complexity and boilerplate from `system.h`, resulting in faster compilation and a cleaner API.

## Key Benefits

1. **Fast Compilation**: Doesn't include heavy template headers like `system.h`
2. **Simple API**: Fluent builder pattern with lambda callbacks
3. **No Templates**: No need to understand complex template syntax
4. **Full ECS Integration**: Complete access to EntitySystem and all ECS features
5. **Flexible**: Supports all execution policies, save/load, and event listening

## Basic Usage

```cpp
#include "ECS/standardsystem.h"

// Create a simple event-based system
auto mySystem = pg::createStandardSystem("MySystem")
    .listenToEvents({"EventA", "EventB"})
    .onInit([](pg::StandardSystemHandle* sys) {
        // Initialize
    })
    .onEvent([](pg::StandardSystemHandle* sys, const pg::StandardEvent& event) {
        if (event.name == "EventA") {
            // Handle event
        }
    })
    .build();

// Register with ECS
entitySystem->registerSystem(mySystem);
```

## StandardEvent Structure

`StandardEvent` is a flexible event type that uses:
- `name`: String identifier for the event type
- `values`: Map of key-value pairs (std::unordered_map<std::string, ElementType>)

```cpp
// Creating events
pg::StandardEvent event("PlayerDamage");
event.values["amount"] = pg::ElementType{10};
event.values["source"] = pg::ElementType{std::string("Enemy")};

// Sending events
systemHandle->sendEvent(event);

// Or shorthand
systemHandle->sendEvent("EventName");
systemHandle->sendEvent("EventName", "key", pg::ElementType{value});
```

## StandardComponent Structure

`StandardComponent` is a flexible component type that uses:
- `typeName`: String identifier for the component type
- `properties`: Map of key-value pairs (std::unordered_map<std::string, ElementType>)

```cpp
pg::StandardComponent health;
health.typeName = "Health";
health.set<int>("current", 100);
health.set<int>("max", 100);
health.set<float>("regenRate", 1.5f);

// Accessing properties
int current = health.get<int>("current");
bool hasMax = health.has("max");
```

## Builder API

### Listening to Events

```cpp
// Listen to single event
.listenToEvent("EventName")

// Listen to multiple events
.listenToEvents({"Event1", "Event2", "Event3"})
```

### Execution Policies

```cpp
// Sequential (default) - execute() called every frame
.useSequentialPolicy()

// Storage - no automatic execute(), event-only
.useStoragePolicy()

// Manual - manual execution control
.useManualPolicy()

// Parallel - parallel execution (requires parallelExecute implementation)
.useParallelPolicy()
```

### Callbacks

```cpp
// Initialization callback
.onInit([](StandardSystemHandle* sys) {
    // Called once when system is registered
})

// Event callback
.onEvent([](StandardSystemHandle* sys, const StandardEvent& event) {
    // Called for each event the system listens to
})

// Execute callback (called every frame if not using StoragePolicy)
.onExecute([](StandardSystemHandle* sys) {
    // Called every frame
})
```

### Save/Load Support

```cpp
.enableSaveLoad()
.onSave([](StandardSystemHandle* sys, std::unordered_map<std::string, ElementType>& saveData) {
    // Save your data
    saveData["level"] = ElementType{5};
    saveData["score"] = ElementType{1000.0f};
})
.onLoad([](StandardSystemHandle* sys, const std::unordered_map<std::string, ElementType>& loadData) {
    // Load your data
    int level = loadData.at("level").get<int>();
})
.onFirstLoad([](StandardSystemHandle* sys) {
    // Called when no save file exists
})
```

## Complete Examples

### Example 1: Simple Event-Driven System

```cpp
auto playerSystem = pg::createStandardSystem("PlayerController")
    .listenToEvents({"KeyPress", "KeyRelease", "MouseClick"})
    .onInit([](pg::StandardSystemHandle* sys) {
        LOG_INFO("Player", "Player controller initialized");
    })
    .onEvent([](pg::StandardSystemHandle* sys, const pg::StandardEvent& event) {
        if (event.name == "KeyPress") {
            std::string key = event.values.at("key").get<std::string>();
            LOG_INFO("Player", "Key pressed: " << key);

            if (key == "Space") {
                sys->sendEvent("PlayerJump", "power", pg::ElementType{5.0f});
            }
        }
    })
    .useStoragePolicy()
    .build();
```

### Example 2: Frame-Based System

```cpp
auto particleSystem = pg::createStandardSystem("ParticleSystem")
    .listenToEvents({"SpawnParticle", "ClearParticles"})
    .onInit([](pg::StandardSystemHandle* sys) {
        // Initialize particle pools
    })
    .onEvent([](pg::StandardSystemHandle* sys, const pg::StandardEvent& event) {
        if (event.name == "SpawnParticle") {
            float x = event.values.at("x").get<float>();
            float y = event.values.at("y").get<float>();
            // Spawn particle at (x, y)
        }
    })
    .onExecute([](pg::StandardSystemHandle* sys) {
        // Update all particles every frame
    })
    .useSequentialPolicy()
    .build();
```

### Example 3: Persistent System with Save/Load

```cpp
auto questSystem = pg::createStandardSystem("QuestSystem")
    .listenToEvents({"QuestStart", "QuestComplete", "QuestFail"})
    .enableSaveLoad()
    .onInit([](pg::StandardSystemHandle* sys) {
        LOG_INFO("Quest", "Quest system ready");
    })
    .onEvent([](pg::StandardSystemHandle* sys, const pg::StandardEvent& event) {
        if (event.name == "QuestComplete") {
            int questId = event.values.at("questId").get<int>();
            LOG_INFO("Quest", "Quest " << questId << " completed!");

            // Trigger rewards
            sys->sendEvent("GiveReward", "gold", pg::ElementType{100});
        }
    })
    .onSave([](pg::StandardSystemHandle* sys, std::unordered_map<std::string, ElementType>& data) {
        // Save active quests
        data["activeQuestCount"] = pg::ElementType{3};
    })
    .onLoad([](pg::StandardSystemHandle* sys, const std::unordered_map<std::string, ElementType>& data) {
        // Load active quests
        int count = data.at("activeQuestCount").get<int>();
    })
    .build();
```

## StandardSystemHandle API

The handle provides access to ECS functionality:

```cpp
// Get the EntitySystem
EntitySystem* world = handle->getWorld();

// Send events
handle->sendEvent(StandardEvent("EventName"));
handle->sendEvent("EventName");
handle->sendEvent("EventName", "key", ElementType{value});

// Component management (TODO: implement based on your needs)
StandardComponent* comp = handle->createComponent(entityId, "ComponentType");
handle->removeComponent(entityId, "ComponentType");
StandardComponent* comp = handle->getComponent(entityId, "ComponentType");
```

## Comparison: Traditional vs Standard System

### Traditional Approach (system.h)

```cpp
#include "ECS/system.h"  // Heavy template header

struct MySystem : public System<
    Own<PositionComponent>,
    Ref<VelocityComponent>,
    Listener<TickEvent>,
    Listener<CollisionEvent>,
    InitSys
>
{
    virtual void init() override {
        registerGroup<PositionComponent, VelocityComponent>();
    }

    virtual void onEvent(const TickEvent& event) override {
        // Handle tick
    }

    virtual void onEvent(const CollisionEvent& event) override {
        // Handle collision
    }

    virtual void execute() override {
        // Update logic
    }

    virtual std::string getSystemName() const override {
        return "My System";
    }
};
```

### Standard System Approach

```cpp
#include "ECS/standardsystem.h"  // Lightweight header

auto mySystem = pg::createStandardSystem("MySystem")
    .listenToEvents({"TickEvent", "CollisionEvent"})
    .onInit([](pg::StandardSystemHandle* sys) {
        // Initialize
    })
    .onEvent([](pg::StandardSystemHandle* sys, const pg::StandardEvent& event) {
        if (event.name == "TickEvent") {
            // Handle tick
        }
        else if (event.name == "CollisionEvent") {
            // Handle collision
        }
    })
    .onExecute([](pg::StandardSystemHandle* sys) {
        // Update logic
    })
    .build();
```

## When to Use StandardSystem

**Use StandardSystem when:**
- You want fast compilation times
- You're prototyping or iterating quickly
- You prefer a functional/lambda-based style
- You're working with dynamic event/component types
- You want to avoid template complexity

**Use traditional System when:**
- You need type-safe components at compile time
- You're building high-performance systems
- You need advanced ECS features (groups with multiple components, etc.)
- You want compile-time guarantees about component ownership

## Implementation Details

The StandardSystem wrapper internally creates specialized System<> templates based on your configuration:

- **StandardSystemImpl**: Basic system with event listening and execution
- **StandardSystemImplSaveable**: System with save/load support
- **StandardSystemImplStorage**: System with StoragePolicy (event-only)

All the template complexity is hidden in `standardsystem.cpp`, keeping your code clean and compilation fast.

## Future Enhancements

Potential improvements to consider:

1. **Component Support**: Full integration with StandardComponent creation/management
2. **Group Support**: Register and iterate over component groups dynamically
3. **Query Builder**: Fluent API for entity queries
4. **Hot Reload**: Support for reloading system logic at runtime
5. **Scripting Integration**: Expose StandardSystem to scripting languages

## See Also

- `exemples/standard_system_example.cpp` - Complete working examples
- `src/Engine/ECS/system.h` - Traditional system implementation
- `src/Engine/ECS/eventlistener.h` - Event listener documentation
