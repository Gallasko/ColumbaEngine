# StandardSystem Implementation - Complete Capabilities Reference

## Overview

The StandardSystem is a simplified wrapper around the ECS that provides a clean, non-template-based interface for creating game systems. It uses `StandardEvent` and `StandardComponent` for dynamic, runtime-configured systems while maintaining full ECS integration.

---

## 1. Available Callbacks

The StandardSystemBuilder supports the following lifecycle callbacks:

### 1.1 Initialization Callbacks

#### `.onInit(callback)` - C++ Callback
```cpp
.onInit([](StandardSystemHandle* sys) {
    // Called once when the system is registered
    // Use for setup, logging initialization, etc.
    LOG_INFO("MySystem", "Initialized!");
})
```

#### `.onInit(scriptPath)` - Script File
```cpp
.onInit("scripts/init.pgs")
```

#### `.onFirstLoad(callback)` - C++ Callback (Save/Load)
```cpp
.onFirstLoad([](StandardSystemHandle* sys) {
    // Called only the first time if no save data exists
    // Useful for setting default values
})
```
**Note**: Only works when `.enableSaveLoad()` is called.

---

### 1.2 Execution Callbacks

#### `.onExecute(callback)` - Per-Frame Update
```cpp
.onExecute([](StandardSystemHandle* sys) {
    // Called every frame (default Sequential policy)
    // Use for game logic updates
    auto data = sys->getData();
    // Update game state
})
```

#### `.onExecute(scriptPath)` - Script File
```cpp
.onExecute("scripts/update.pgs")
```

**Execution Policies** (determine if/how `execute()` is called):
- **Sequential** (default): `execute()` called every frame in sequence
- **Storage**: No automatic `execute()` calls - event-driven only
- **Parallel**: `execute()` called in parallel with other systems
- **Manual**: Manual execution control
- **Independent**: System runs independently

---

### 1.3 Event Callbacks

#### `.onEvent(eventName, callback)` - Specific Event
```cpp
.onEvent("PlayerJump", [](StandardSystemHandle* sys, const StandardEvent& event) {
    float power = event.get<float>("jumpPower");
    // Handle jump event
})
```

Multiple event handlers can be registered:
```cpp
.onEvent("PlayerJump", jumpHandler)
.onEvent("PlayerLand", landHandler)
.onEvent("GameOver", gameOverHandler)
```

#### `.onEvent(eventName, scriptPath)` - Script Handler
```cpp
.onEvent("PlayerDamage", "scripts/damage.pgs")
```

**Event Listening**:
- All `.onEvent()` registrations are automatically added to the system's listened events
- System will only receive events it explicitly listens to
- Multiple handlers can be registered for the same event

---

### 1.4 Delta Time Callback

#### `.onDelta(callback)` - Per-Frame Delta Time
```cpp
.onDelta([](StandardSystemHandle* sys, float deltaTime) {
    // Called every frame with delta time in seconds
    // Use for time-based updates
    auto pos = sys->getData("position");
    // deltaTime is in seconds (TickEvent.tick is in milliseconds, automatically converted)
})
```

#### `.onDelta(scriptPath)` - Script File
```cpp
.onDelta("scripts/physics.pgs")
```

**Triggering**: Delta callback is only called if:
1. A delta callback is registered (via C++ or script)
2. The system executes (calls `execute()`)
3. There is a non-zero delta time available

---

### 1.5 Save/Load Callbacks

#### `.enableSaveLoad()` - Enable Persistence
Must be called to enable save/load functionality.

#### `.onSave(callback)` - Save State
```cpp
.enableSaveLoad()
.onSave([](StandardSystemHandle* sys, ElementMap& saveData) {
    // Called when the system needs to save state
    // Populate saveData with key-value pairs
    saveData["playerHealth"] = ElementType{100};
    saveData["playerLevel"] = ElementType{5};
})
```

#### `.onLoad(callback)` - Load State
```cpp
.onLoad([](StandardSystemHandle* sys, const ElementMap& loadData) {
    // Called when loading a saved game
    // loadData contains previously saved data
    auto health = loadData.at("playerHealth").get<int>();
    sys->setData("health", ElementType{health});
})
```

**Callback Execution Order**:
1. System is registered → `onRegisterFinished()` (internal)
2. `onInit` callback fires
3. If save file exists: `onLoad` called
4. If save file doesn't exist: `onFirstLoad` called

---

## 2. StandardSystemHandle - Entity & Component Interaction

The `StandardSystemHandle` is passed to all callbacks and provides access to the ECS and system state.

### 2.1 Component Operations

#### Create Component
```cpp
StandardComponent* comp = sys->createComponent(entityId, "Health");
if (comp) {
    comp->set("current", 100);
    comp->set("max", 100);
}
```

#### Get Component
```cpp
StandardComponent* health = sys->getComponent(entityId, "Health");
if (health && health->has("current")) {
    int hp = health->get<int>("current");
}
```

#### Remove Component
```cpp
sys->removeComponent(entityId, "Health");
```

**Component Type Management**:
- Components must be registered with `.ownComponents()` or `.ownComponent()`
- Only owned components can be created/removed by the system
- Components are automatically registered with default values

---

### 2.2 Event Sending

#### Send Event (Standard Name Only)
```cpp
sys->sendEvent("PlayerJump");
```

#### Send Event with Data
```cpp
StandardEvent event("PlayerDamaged");
event.values["amount"] = ElementType{25};
event.values["source"] = ElementType{"Enemy"};
sys->sendEvent(event);
```

#### Send Event (Constructor Overload)
```cpp
sys->sendEvent("PlayerMoved", "x", 10.5f);  // Creates event with one key-value
sys->sendEvent("Collision", "type", "obstacle", "damage", 5);  // Multiple values
```

---

### 2.3 System Data Storage

System-wide persistent storage (shared across all callbacks):

#### Get All Data
```cpp
ElementMap* allData = sys->getData();
for (const auto& [key, value] : *allData) {
    LOG_INFO("System", key << " = " << value.toString());
}
```

#### Get Single Value
```cpp
ElementType score = sys->getData("playerScore");
int scoreInt = score.get<int>();
```

#### Set Value
```cpp
sys->setData("playerScore", ElementType{1000});
sys->setData("gameState", ElementType{"playing"});
sys->setData("isGameOver", ElementType{false});
```

**Storage Features**:
- Persists across all callbacks in a frame
- Not automatically saved (use `.onSave()` for persistence)
- Type-safe with `ElementType` (supports int, float, double, bool, string, size_t)

---

### 2.4 ECS World Access

#### Get EntitySystem
```cpp
EntitySystem* world = sys->getWorld();
if (world) {
    Entity* entity = world->getEntity(entityId);
}
```

---

## 3. StandardEvent Structure

Dynamic event with string name and value map:

```cpp
struct StandardEvent {
    std::string name;                              // Event name
    ElementMap values;                             // Key-value pairs of data
    
    // Helper methods
    bool has(const std::string& key) const;        // Check if key exists
    template<typename T> T get(const std::string& key) const;  // Get typed value
    ElementType getElement(const std::string& key) const;      // Get raw ElementType
};
```

### Creating Events

```cpp
// Constructor variants
StandardEvent e1("EventName");

StandardEvent e2("EventName", "key1", value1);

StandardEvent e3("EventName", "key1", value1, "key2", value2);

// Manual construction
StandardEvent e4("EventName");
e4.values["damage"] = ElementType{50};
e4.values["source"] = ElementType{"Player"};
```

---

## 4. StandardComponent Structure

Dynamic component with type name and property map:

```cpp
struct StandardComponent : public Component {
    std::string typeName;                          // Component type name
    ElementMap properties;                         // Key-value properties
    
    // Helper methods
    template<typename T> void set(const std::string& key, const T& value);
    template<typename T> T get(const std::string& key) const;
    bool has(const std::string& key) const;
    
    // Change event notification
    template<typename T> void setWithEvent(const std::string& key, const T& value);
};
```

### Component Default Values

Register components with default values:

```cpp
StandardSystemBuilder("MySystem")
    .ownComponent("Health", "current", 100, "max", 100)
    .ownComponent("Position", "x", 0.0f, "y", 0.0f, "z", 0.0f)
```

When a component is created, these default values are automatically populated.

### Component Change Events

When a property changes, a `"Changed<ComponentTypeName>"` event is fired:

```cpp
comp->setWithEvent("health", 90);
// Fires: StandardEvent("ChangedHealth") with:
//   - "propertyName" = "health"
//   - "oldValue" = <previous value>
//   - "newValue" = 90
//   - "entityId" = <entity id>
```

---

## 5. ElementType - Type-Safe Storage

Universal type container supporting: `float`, `int`, `double`, `bool`, `string`, `size_t`

```cpp
ElementType value = 42;                    // Automatically typed as int
ElementType fval = 3.14f;                  // Automatically typed as float
ElementType str = "hello";                 // Automatically typed as string

// Type checking
bool isNum = value.isNumber();
bool isBool = value.isBool();
bool isStr = value.isLiteral();

// Conversion
int i = value.get<int>();
float f = fval.get<float>();
std::string s = str.get<std::string>();
```

---

## 6. System Data Storage

### Storage Mechanism

Each StandardSystem has an associated `ElementMap` accessible via `sys->getData()`:

```cpp
// In onInit
sys->setData("frameCount", ElementType{0});

// In onExecute
auto frameCount = sys->getData("frameCount").get<int>();
sys->setData("frameCount", ElementType{frameCount + 1});
```

### Use Cases

- Track frame count or time elapsed
- Store temporary game state
- Cache calculations between callbacks
- Maintain per-system configuration

### Persistence

System data is NOT automatically saved:
```cpp
.enableSaveLoad()
.onSave([](StandardSystemHandle* sys, ElementMap& saveData) {
    auto storedData = sys->getData();
    for (const auto& [key, value] : *storedData) {
        saveData[key] = value;  // Manually save what you need
    }
})
```

---

## 7. Entity Iteration & Filtering

### Template-Based Systems (for reference)

In traditional systems, you can iterate using the template system:

```cpp
// In a System<> based class:
auto healthList = view<HealthComponent>();
for (const auto& health : healthList) {
    health->current -= 5;  // Modify component
}
```

### StandardSystem Limitations

StandardSystem doesn't provide template-based iteration. To iterate entities:

```cpp
.onExecute([](StandardSystemHandle* sys) {
    EntitySystem* world = sys->getWorld();
    // Manual iteration needed - not currently exposed in StandardSystemHandle
    // This is a limitation of the current implementation
})
```

### Workaround: Use Traditional Systems

For complex entity queries, consider:
1. Creating a traditional `System<>` for iteration-heavy work
2. Using components with callbacks (`OnEventComponent`)
3. Filtering via component presence checks

---

## 8. Available Events

### Input Events

#### Keyboard Events
```cpp
// Key Press
struct OnSDLScanCode {
    SDL_Scancode key;       // Which key was pressed
    Uint16 mod;             // Modifier keys (Shift, Ctrl, etc.)
    STANDARD_EVENT_CONVERTIBLE(OnSDLScanCode)
};

// Listen in StandardSystem
.onEvent("OnSDLScanCode", [](StandardSystemHandle* sys, const StandardEvent& event) {
    auto key = event.get<int>("key");  // Actually SDL_Scancode
    auto mod = event.get<int>("mod");
})
```

#### Key Release
```cpp
struct OnSDLScanCodeReleased {
    SDL_Scancode key;
    Uint16 mod;
    STANDARD_EVENT_CONVERTIBLE(OnSDLScanCodeReleased)
};
```

#### Text Input
```cpp
struct OnSDLTextInput {
    std::string text;       // Actual text character(s) typed
    STANDARD_EVENT_CONVERTIBLE(OnSDLTextInput)
};
```

#### Mouse Events
```cpp
struct OnMouseMove {
    Point2D pos;
    Input *inputHandler;
    STANDARD_EVENT_CONVERTIBLE(OnMouseMove)
};

struct OnMouseClick {
    Point2D pos;
    MouseButton button;     // Left, Right, Middle
    STANDARD_EVENT_CONVERTIBLE(OnMouseClick)
};

struct OnMouseRelease {
    Point2D pos;
    MouseButton button;
    STANDARD_EVENT_CONVERTIBLE(OnMouseRelease)
};

struct OnSDLMouseWheel {
    Sint32 x;              // Horizontal scroll
    Sint32 y;              // Vertical scroll
    STANDARD_EVENT_CONVERTIBLE(OnSDLMouseWheel)
};
```

#### Gamepad Events
```cpp
struct OnSDLGamepadPressed {
    int id;                // Gamepad ID
    unsigned int button;   // Which button
};

struct OnSDLGamepadReleased {
    int id;
    unsigned int button;
};

struct OnSDLGamepadAxisChanged {
    int id;
    unsigned int axis;     // Which axis (stick, trigger)
    int value;             // Axis value
};
```

### Core Engine Events

#### Tick Event
```cpp
struct TickEvent {
    float tick;            // Time delta in milliseconds
    // Sent by TickingSystem at fixed intervals
};
```

**Usage**: Listen for frame updates with consistent timing
```cpp
.onEvent("TickEvent", [](StandardSystemHandle* sys, const StandardEvent& event) {
    float delta = event.get<float>("tick");  // milliseconds
})
```

### Custom Events

You can send arbitrary StandardEvents:

```cpp
// Sending
StandardEvent custom("PlayerLevelUp");
custom.values["level"] = ElementType{5};
sys->sendEvent(custom);

// Receiving
.onEvent("PlayerLevelUp", [](StandardSystemHandle* sys, const StandardEvent& event) {
    int newLevel = event.get<int>("level");
})
```

---

## 9. Entity Lifecycle Hooks

### Component Creation Hook

Components can implement `onCreation`:

```cpp
struct Ctor {
    virtual void onCreation(EntityRef entity) = 0;
};

// StandardComponent inherits from Component which inherits from Ctor
```

When a component is added to an entity:
```cpp
auto comp = sys->createComponent(entityId, "Health");
// Component's onCreation(entity) is called automatically
```

### Component Deletion Hook

Components can implement `onDeletion`:

```cpp
struct Dtor {
    virtual void onDeletion(EntityRef entity) = 0;
};
```

When a component is removed:
```cpp
sys->removeComponent(entityId, "Health");
// Component's onDeletion(entity) is called automatically
```

### System Initialization Hook

```cpp
.onInit([](StandardSystemHandle* sys) {
    // Called when system is registered to the ECS
    // Happens before any events are processed
})
```

### Entity Lifecycle Components

Two special components for entity-level hooks:

#### OnEventComponent
```cpp
struct OnEventComponent : public Ctor, public Dtor {
    // Triggers callback when typed event is sent
    // Attached to entities to handle events
};
```

#### OnStandardEventComponent
```cpp
struct OnStandardEventComponent : public Ctor, public Dtor {
    std::string eventName;
    std::function<void(const StandardEvent&)> callback;
    // Triggers callback when named StandardEvent is sent
};
```

---

## 10. Execution Policies

Control when and how the system's `execute()` method is called:

### Sequential (Default)
```cpp
// execute() called every frame in sequence
.useSequentialPolicy()  // or omit, it's default
```

### Storage
```cpp
// No automatic execute() - event-driven only
.useStoragePolicy()
```

**Best for**: Event-driven systems that don't need per-frame updates

### Parallel
```cpp
// execute() can run in parallel with other systems
.useParallelPolicy()
```

**Best for**: CPU-intensive systems that don't access shared data

### Manual
```cpp
// Manual execute() calls only
.useManualPolicy()
```

**Best for**: Systems controlled externally

### Independent
```cpp
// System runs independently (rarely used)
.useIndependentPolicy()
```

---

## 11. Building and Registering

### Create and Register

```cpp
// Create the system
auto* system = pg::createStandardSystem("GameLogic")
    .ownComponents({"Health", "Position"})
    .listenToEvents({"PlayerJump", "PlayerDamage"})
    .onInit([](StandardSystemHandle* sys) {
        LOG_INFO("GameLogic", "System initialized");
    })
    .onEvent("PlayerDamage", [](StandardSystemHandle* sys, const StandardEvent& event) {
        int damage = event.get<int>("amount");
        // Handle damage
    })
    .onExecute([](StandardSystemHandle* sys) {
        // Per-frame logic
    })
    .build();

// Register with ECS
entitySystem->registerSystem(system);
```

### Return Type

`.build()` returns `StandardSystemImpl*` which is a pointer to the actual system object that implements `AbstractSystem`.

---

## 12. Complete Example

```cpp
#include "ECS/standardsystem.h"
#include "ECS/entitysystem.h"

auto playerSystem = pg::createStandardSystem("PlayerController")
    .ownComponents({"Position", "Velocity", "Health"})
    .onInit([](pg::StandardSystemHandle* sys) {
        LOG_INFO("PlayerController", "Player system initialized");
        sys->setData("playerScore", pg::ElementType{0});
    })
    .onEvent("OnSDLScanCode", [](pg::StandardSystemHandle* sys, const pg::StandardEvent& event) {
        auto key = event.get<int>("key");
        // Handle keyboard input
    })
    .onEvent("OnMouseClick", [](pg::StandardSystemHandle* sys, const pg::StandardEvent& event) {
        // Handle mouse click
    })
    .onEvent("TickEvent", [](pg::StandardSystemHandle* sys, const pg::StandardEvent& event) {
        float delta = event.get<float>("tick") / 1000.0f;  // Convert ms to seconds
        // Update game physics
    })
    .onExecute([](pg::StandardSystemHandle* sys) {
        auto score = sys->getData("playerScore").get<int>();
        // Update UI or game state each frame
    })
    .onDelta([](pg::StandardSystemHandle* sys, float deltaSeconds) {
        // Time-based updates
    })
    .enableSaveLoad()
    .onSave([](pg::StandardSystemHandle* sys, pg::ElementMap& saveData) {
        saveData["playerScore"] = sys->getData("playerScore");
    })
    .onLoad([](pg::StandardSystemHandle* sys, const pg::ElementMap& saveData) {
        sys->setData("playerScore", saveData.at("playerScore"));
    })
    .useSequentialPolicy()
    .build();

entitySystem->registerSystem(playerSystem);
```

---

## Summary of Key Capabilities

| Capability | Available | Method |
|-----------|-----------|--------|
| **Initialization** | Yes | `.onInit()` |
| **Per-Frame Updates** | Yes | `.onExecute()`, `.onDelta()` |
| **Event Handling** | Yes | `.onEvent()` - multiple handlers |
| **Component Management** | Yes | `create/get/removeComponent()` |
| **System Data Storage** | Yes | `getData()`, `setData()` |
| **Event Sending** | Yes | `sendEvent()` - multiple variants |
| **Entity Iteration** | Limited | Requires traditional System or manual iteration |
| **Component Default Values** | Yes | `.ownComponent()` with default pairs |
| **Save/Load** | Yes | `.onSave()`, `.onLoad()`, `.onFirstLoad()` |
| **Keyboard Input** | Yes | `OnSDLScanCode`, `OnSDLScanCodeReleased` |
| **Mouse Input** | Yes | `OnMouseClick`, `OnMouseMove`, `OnSDLMouseWheel` |
| **Gamepad Input** | Yes | `OnSDLGamepadPressed/Released/AxisChanged` |
| **Delta Time** | Yes | `.onDelta()` callback |
| **Execution Policies** | Yes | Sequential, Parallel, Manual, Storage, Independent |
| **Script Integration** | Yes | `.onInit("script.pgs")`, `.onExecute("script.pgs")`, etc. |

