# Automatic C++ Event to StandardEvent Conversion

## Overview

PgEngine now supports automatic conversion of typed C++ events to `StandardEvent`s, allowing **standard systems** (script-based, dynamically-typed systems) to listen to and respond to **C++ events** without requiring manual bridge code.

This feature is **opt-in at compile time** via the `PG_AUTO_CONVERT_EVENTS_TO_STANDARD` preprocessor macro, ensuring zero overhead when disabled.

---

## Quick Start

### 1. Enable the Feature

Add the following compiler flag to enable automatic event conversion:

```cmake
# CMakeLists.txt
add_definitions(-DPG_AUTO_CONVERT_EVENTS_TO_STANDARD)
```

Or in your compiler command:
```bash
g++ -DPG_AUTO_CONVERT_EVENTS_TO_STANDARD ...
```

### 2. Use Existing Convertible Events

Many common events are already convertible:
- `OnMouseClick` → `StandardEvent("OnMouseClick")`
- `OnMouseRelease` → `StandardEvent("OnMouseRelease")`
- `OnMouseMove` → `StandardEvent("OnMouseMove")`
- `OnSDLScanCode` → `StandardEvent("OnSDLScanCode")`
- `OnSDLTextInput` → `StandardEvent("OnSDLTextInput")`
- And more...

### 3. Listen in Standard System

```cpp
auto inputHandler = createStandardSystem("InputHandler")
    .listenToEvents({"OnMouseClick", "OnSDLScanCode"})  // Listen to C++ events!
    .onEvent([](StandardSystemHandle* sys, const StandardEvent& event) {
        if (event.name == "OnMouseClick") {
            float x = event.get<float>("x");
            float y = event.get<float>("y");
            int button = event.get<int>("button");

            LOG_INFO("InputHandler", "Mouse clicked at (" << x << ", " << y << ")");
        }
        else if (event.name == "OnSDLScanCode") {
            int key = event.get<int>("key");
            LOG_INFO("InputHandler", "Key pressed: " << key);
        }
    })
    .build();
```

### 4. C++ Code Works Unchanged

```cpp
// Existing C++ code - no changes needed!
ecsRef->sendEvent(OnMouseClick{{100.0f, 200.0f}, SDL_BUTTON_LEFT});

// Now ALSO triggers StandardEvent("OnMouseClick") automatically
// IF PG_AUTO_CONVERT_EVENTS_TO_STANDARD is defined
```

---

## How It Works

### Architecture

1. **Type Trait Detection**: The `has_to_standard_event_v<Event>` trait checks if an event type has a `toStandardEvent()` method.

2. **Compile-Time Guard**: The `#ifdef PG_AUTO_CONVERT_EVENTS_TO_STANDARD` ensures zero overhead when disabled.

3. **Dual Dispatch**: When enabled, `sendEvent()` dispatches:
   - First: The typed C++ event (to C++ listeners)
   - Then: The converted StandardEvent (to standard listeners)

### Event Flow Diagram

```
C++ Code: ecsRef->sendEvent(OnMouseClick{{x, y}, button})
    ↓
EntitySystem::sendEvent<OnMouseClick>(event)
    ↓
    ├─→ [1] registry.processEvent(event)  // C++ Listeners
    |       ↓
    |   C++ System: Listener<OnMouseClick>::onEvent(event)
    |
    └─→ [2] if constexpr (has_to_standard_event_v<OnMouseClick>)
            StandardEvent stdEvent = event.toStandardEvent()
            registry.processEvent(stdEvent)  // Standard Listeners
                ↓
            Standard System: onEvent(StandardEvent("OnMouseClick"))
```

---

## Making Your Events Convertible

### Step 1: Mark Event as Convertible

In your event header file:

```cpp
#include "ECS/standardevent.h"

struct MyCustomEvent {
    int value;
    std::string message;

    // Add this macro to enable conversion
    STANDARD_EVENT_CONVERTIBLE(MyCustomEvent)
};
```

### Step 2: Implement Conversion

In your event source file (`.cpp`):

```cpp
#include "mycustomevent.h"

STANDARD_EVENT_CONVERSION_IMPL(MyCustomEvent)
{
    StandardEvent event("MyCustomEvent");
    event.values["value"] = ElementType{value};
    event.values["message"] = ElementType{message};
    return event;
}
```

### Step 3: Use It

```cpp
// C++ system sending event
ecsRef->sendEvent(MyCustomEvent{42, "Hello"});

// Standard system listening
auto listener = createStandardSystem("MyListener")
    .listenToEvents({"MyCustomEvent"})
    .onEvent([](StandardSystemHandle* sys, const StandardEvent& event) {
        int value = event.get<int>("value");
        std::string message = event.get<std::string>("message");
        LOG_INFO("MyListener", "Received: " << message << " = " << value);
    })
    .build();
```

---

## Helper Macros Reference

### `STANDARD_EVENT_CONVERTIBLE(EventType)`

Declares that an event can be converted to `StandardEvent`. Use in the event struct declaration.

**Example:**
```cpp
struct PlayerDiedEvent {
    _unique_id playerId;
    int score;

    STANDARD_EVENT_CONVERTIBLE(PlayerDiedEvent)
};
```

**Expands to:**
```cpp
StandardEvent toStandardEvent() const;
```

### `STANDARD_EVENT_CONVERSION_IMPL(EventType)`

Implements the conversion function. Use in the event source file.

**Example:**
```cpp
STANDARD_EVENT_CONVERSION_IMPL(PlayerDiedEvent)
{
    StandardEvent event("PlayerDied");
    event.values["playerId"] = ElementType{playerId};
    event.values["score"] = ElementType{score};
    return event;
}
```

**Expands to:**
```cpp
StandardEvent PlayerDiedEvent::toStandardEvent() const
```

---

## Conversion Best Practices

### 1. Event Naming Convention

Use clear, descriptive names for StandardEvent conversion:

```cpp
// ✅ GOOD: Clear and consistent
StandardEvent event("PlayerDied");
StandardEvent event("LevelComplete");
StandardEvent event("OnMouseClick");

// ❌ BAD: Unclear or inconsistent
StandardEvent event("evt");
StandardEvent event("player_died");  // Inconsistent casing
```

### 2. Field Naming

Use descriptive field names that match the original event fields:

```cpp
struct DamageEvent {
    _unique_id targetId;
    float damageAmount;
    DamageType type;

    STANDARD_EVENT_CONVERTIBLE(DamageEvent)
};

STANDARD_EVENT_CONVERSION_IMPL(DamageEvent)
{
    StandardEvent event("DamageEvent");
    // ✅ GOOD: Field names match original
    event.values["targetId"] = ElementType{targetId};
    event.values["damageAmount"] = ElementType{damageAmount};
    event.values["type"] = ElementType{static_cast<int>(type)};
    return event;
}
```

### 3. Handle Complex Types

For enums and complex types, convert to primitive types:

```cpp
enum class ConnectionState { Connecting, Connected, Disconnected };

struct NetworkEvent {
    ConnectionState state;
    std::string serverAddress;

    STANDARD_EVENT_CONVERTIBLE(NetworkEvent)
};

STANDARD_EVENT_CONVERSION_IMPL(NetworkEvent)
{
    StandardEvent event("NetworkEvent");
    // Convert enum to int
    event.values["state"] = ElementType{static_cast<int>(state)};
    event.values["serverAddress"] = ElementType{serverAddress};

    // Optional: Add human-readable state name
    event.values["stateName"] = ElementType{
        state == ConnectionState::Connecting ? "Connecting" :
        state == ConnectionState::Connected ? "Connected" : "Disconnected"
    };

    return event;
}
```

### 4. Optional Fields

Use `has()` to check for optional fields in standard systems:

```cpp
STANDARD_EVENT_CONVERSION_IMPL(EntityEvent)
{
    StandardEvent event("EntityEvent");
    event.values["entityId"] = ElementType{entityId};

    // Optional fields
    if (hasPosition) {
        event.values["x"] = ElementType{position.x};
        event.values["y"] = ElementType{position.y};
    }

    return event;
}

// Standard system handling
.onEvent([](StandardSystemHandle* sys, const StandardEvent& event) {
    size_t entityId = event.get<size_t>("entityId");

    // Check if position data exists
    if (event.has("x") && event.has("y")) {
        float x = event.get<float>("x");
        float y = event.get<float>("y");
        // Use position...
    }
})
```

---

## Performance Considerations

### Overhead When Enabled

- **Small**: One additional function call per convertible event
- **Negligible**: Most events are infrequent (user input, lifecycle events)
- **Optimized**: Only events with `toStandardEvent()` are converted (opt-in per event type)

### Overhead When Disabled

- **Zero**: The entire conversion code is removed at compile time via `#ifdef`
- **No Runtime Cost**: Type trait is resolved at compile time

### Benchmark Results

---

## Compile-Time Configuration

### Enable Globally

In your main `CMakeLists.txt`:

```cmake
# Enable for all targets
add_definitions(-DPG_AUTO_CONVERT_EVENTS_TO_STANDARD)
```

### Enable Per Target

```cmake
# Enable only for specific target
target_compile_definitions(MyGame PRIVATE PG_AUTO_CONVERT_EVENTS_TO_STANDARD)
```

### Enable for Debug Only

```cmake
# Only convert in debug builds
if(CMAKE_BUILD_TYPE MATCHES Debug)
    add_definitions(-DPG_AUTO_CONVERT_EVENTS_TO_STANDARD)
endif()
```

### Conditional Compilation Example

```cpp
#ifdef PG_AUTO_CONVERT_EVENTS_TO_STANDARD
    LOG_INFO("Engine", "Event auto-conversion is ENABLED");
#else
    LOG_INFO("Engine", "Event auto-conversion is DISABLED");
#endif
```

---

## Troubleshooting

### Problem: "toStandardEvent is not a member"

**Cause:** Event is marked with `STANDARD_EVENT_CONVERTIBLE` but implementation is missing.

**Solution:** Add the implementation in the `.cpp` file:

```cpp
STANDARD_EVENT_CONVERSION_IMPL(MyEvent)
{
    StandardEvent event("MyEvent");
    // ... populate fields
    return event;
}
```

### Problem: Standard system not receiving events

**Checks:**
1. Is `PG_AUTO_CONVERT_EVENTS_TO_STANDARD` defined? Check with:
   ```cpp
   #ifdef PG_AUTO_CONVERT_EVENTS_TO_STANDARD
       #pragma message("Auto-conversion enabled")
   #endif
   ```

2. Does the event have `STANDARD_EVENT_CONVERTIBLE` macro?
3. Is the standard system listening to the correct event name?
   ```cpp
   .listenToEvents({"OnMouseClick"})  // Must match toStandardEvent() name
   ```

4. Is the standard system registered in the ECS?
   ```cpp
   ecs.registerSystem(myStandardSystem);
   ```

### Problem: Linker error "undefined reference to toStandardEvent"

**Cause:** Declaration exists but implementation is missing.

**Solution:** Implement the conversion in the `.cpp` file using `STANDARD_EVENT_CONVERSION_IMPL`.

---

## Migration Guide

### From Manual Bridge Code

**Before (Manual Bridge):**

```cpp
struct EventBridge : public System<Listener<OnMouseClick>> {
    void onEvent(const OnMouseClick& event) override {
        StandardEvent stdEvent("OnMouseClick");
        stdEvent.values["x"] = ElementType{event.pos.x};
        stdEvent.values["y"] = ElementType{event.pos.y};
        ecsRef->sendEvent(stdEvent);  // Manual dispatch
    }
};
```

**After (Automatic):**

```cpp
// 1. Mark event as convertible (in header)
struct OnMouseClick {
    Point2D pos;
    MouseButton button;

    STANDARD_EVENT_CONVERTIBLE(OnMouseClick)  // Add this
};

// 2. Implement conversion (in .cpp)
STANDARD_EVENT_CONVERSION_IMPL(OnMouseClick)
{
    StandardEvent event("OnMouseClick");
    event.values["x"] = ElementType{pos.x};
    event.values["y"] = ElementType{pos.y};
    return event;
}

// 3. Remove manual bridge system - conversion happens automatically!
// 4. Enable flag: -DPG_AUTO_CONVERT_EVENTS_TO_STANDARD
```

---

## Examples

### Example 1: Input Handler

```cpp
// Standard system handling all input events
auto inputSystem = createStandardSystem("InputSystem")
    .listenToEvents({
        "OnMouseClick",
        "OnMouseRelease",
        "OnSDLScanCode",
        "OnSDLScanCodeReleased"
    })
    .onEvent([](StandardSystemHandle* sys, const StandardEvent& event) {
        if (event.name == "OnMouseClick") {
            LOG_INFO("Input", "Click at " << event.get<float>("x")
                     << ", " << event.get<float>("y"));
        }
        else if (event.name == "OnSDLScanCode") {
            int key = event.get<int>("key");
            // Handle key press
        }
    })
    .build();
```

### Example 2: Game State Logger

```cpp
auto logger = createStandardSystem("EventLogger")
    .listenToEvents({
        "PlayerDied",
        "LevelComplete",
        "EnemySpawned",
        "ItemCollected"
    })
    .onEvent([](StandardSystemHandle* sys, const StandardEvent& event) {
        // Log all game events to file
        std::ofstream logFile("game_events.log", std::ios::app);
        logFile << "[" << event.name << "] ";

        for (const auto& [key, value] : event.values) {
            logFile << key << "=" << value.toString() << " ";
        }

        logFile << std::endl;
    })
    .build();
```

### Example 3: Achievement System

```cpp
auto achievements = createStandardSystem("AchievementTracker")
    .listenToEvents({"EnemyKilled", "LevelComplete", "ItemCollected"})
    .onInit([](StandardSystemHandle* sys) {
        auto data = sys->getData();
        (*data)["enemiesKilled"] = ElementType{0};
        (*data)["levelsCompleted"] = ElementType{0};
    })
    .onEvent([](StandardSystemHandle* sys, const StandardEvent& event) {
        auto data = sys->getData();

        if (event.name == "EnemyKilled") {
            int count = (*data)["enemiesKilled"].get<int>() + 1;
            (*data)["enemiesKilled"] = ElementType{count};

            if (count >= 100) {
                sys->sendEvent(StandardEvent("AchievementUnlocked",
                    "name", ElementType{"Centurion"}));
            }
        }
    })
    .build();
```

---

## API Reference

### Type Traits

```cpp
// Check if event can be converted
template <typename Event>
inline constexpr bool has_to_standard_event_v = /* ... */;

// Usage
static_assert(has_to_standard_event_v<OnMouseClick>, "Event must be convertible");
```

### Macros

```cpp
// Mark event as convertible (in header)
STANDARD_EVENT_CONVERTIBLE(EventType)

// Implement conversion (in source)
STANDARD_EVENT_CONVERSION_IMPL(EventType) { /* return StandardEvent */ }
```

### StandardEvent Fields

| C++ Event | StandardEvent Name | Fields |
|-----------|-------------------|--------|
| `OnMouseClick` | `"OnMouseClick"` | `x`, `y`, `button` |
| `OnMouseRelease` | `"OnMouseRelease"` | `x`, `y`, `button` |
| `OnMouseMove` | `"OnMouseMove"` | `x`, `y` |
| `OnSDLScanCode` | `"OnSDLScanCode"` | `key`, `mod` |
| `OnSDLScanCodeReleased` | `"OnSDLScanCodeReleased"` | `key`, `mod` |
| `OnSDLTextInput` | `"OnSDLTextInput"` | `text` |
| `OnSDLMouseWheel` | `"OnSDLMouseWheel"` | `x`, `y` |

---

## Conclusion

Automatic event conversion provides a seamless bridge between typed C++ systems and dynamic standard systems, enabling:

✅ **Standard systems can listen to C++ events**
✅ **No manual bridge code required**
✅ **Zero overhead when disabled**
✅ **Opt-in per event type**
✅ **Compile-time safety**
✅ **Backward compatible**

For questions or issues, refer to the [standardevent.h](src/Engine/ECS/standardevent.h) header file.
