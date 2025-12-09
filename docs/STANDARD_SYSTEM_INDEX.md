# StandardSystem Implementation - Complete Documentation Index

## Quick Start

**New to StandardSystem?** Start here:
1. Read: **STANDARD_SYSTEM_QUICK_REFERENCE.md** (5-10 min read)
2. Review the example at the end of this document
3. Check specific features in **STANDARD_SYSTEM_CAPABILITIES.md**

## Documentation Files

### 1. STANDARD_SYSTEM_QUICK_REFERENCE.md
**Best for**: Developers who want a cheat sheet

- Single-page reference of all methods and callbacks
- Common code patterns
- Input event listings
- Key limitations and notes

### 2. STANDARD_SYSTEM_CAPABILITIES.md (Comprehensive Guide)
**Best for**: Understanding all features in detail

**Sections**:
- Section 1: All Available Callbacks (7 types)
- Section 2: StandardSystemHandle Methods (4 categories)
- Section 3: StandardEvent Structure
- Section 4: StandardComponent Structure  
- Section 5: ElementType Type System
- Section 6: System Data Storage
- Section 7: Entity Iteration & Filtering
- Section 8: Available Events (Input & Core)
- Section 9: Entity Lifecycle Hooks
- Section 10: Execution Policies (5 types)
- Section 11: Building and Registering
- Section 12: Complete Working Example

### 3. Source Files (Reference)

| File | Location | Purpose |
|------|----------|---------|
| **standardsystem.h** | `/src/Engine/ECS/standardsystem.h` | Builder interface definition |
| **standardsystem.cpp** | `/src/Engine/ECS/standardsystem.cpp` | Implementation |
| **standardevent.h** | `/src/Engine/ECS/standardevent.h` | StandardEvent struct |
| **component.h** | `/src/Engine/ECS/component.h` | StandardComponent struct |
| **inputcomponent.h** | `/src/Engine/Input/inputcomponent.h` | Input event definitions |
| **coresystems.h** | `/src/Engine/Systems/coresystems.h` | TickEvent definition |

## Features Summary

### Callbacks (7 types)

1. **onInit** - System initialization
2. **onExecute** - Per-frame updates
3. **onEvent** - Event handling
4. **onDelta** - Delta time updates
5. **onSave** - Serialization
6. **onLoad** - Deserialization
7. **onFirstLoad** - Initial defaults

### Entity & Component Interaction

- Create/Get/Remove components dynamically
- Property-based components with defaults
- Automatic change event notifications
- Type-safe property access

### Events

**Input Events (10 types)**:
- Keyboard: OnSDLScanCode, OnSDLScanCodeReleased, OnSDLTextInput
- Mouse: OnMouseClick, OnMouseRelease, OnMouseMove, OnSDLMouseWheel
- Gamepad: OnSDLGamepadPressed, OnSDLGamepadReleased, OnSDLGamepadAxisChanged

**Core Events**:
- TickEvent (per-frame timing)
- Custom StandardEvents

### System Features

- **System Data Storage**: Key-value persistent storage
- **Execution Policies**: 5 modes (Sequential, Storage, Parallel, Manual, Independent)
- **Save/Load Support**: Optional persistence
- **Script Integration**: Callbacks via .pgs script files
- **Event Dispatching**: Multi-handler support per event

## Common Use Cases

### 1. Simple Event-Driven System
```cpp
auto* sys = pg::createStandardSystem("MySystem")
    .onEvent("MyEvent", [](StandardSystemHandle* h, const StandardEvent& e) {
        // Handle event
    })
    .useStoragePolicy()  // No per-frame overhead
    .build();
entitySystem->registerSystem(sys);
```

### 2. Game Logic with Updates
```cpp
auto* sys = pg::createStandardSystem("GameLogic")
    .onInit([](StandardSystemHandle* h) {
        h->setData("score", ElementType{0});
    })
    .onExecute([](StandardSystemHandle* h) {
        auto score = h->getData("score").get<int>();
        // Update game
    })
    .onEvent("ScoreChanged", [](StandardSystemHandle* h, const StandardEvent& e) {
        int newScore = e.get<int>("points");
        h->setData("score", ElementType{newScore});
    })
    .useSequentialPolicy()  // Default - run every frame
    .build();
entitySystem->registerSystem(sys);
```

### 3. Input Handler
```cpp
auto* sys = pg::createStandardSystem("Input")
    .onEvent("OnSDLScanCode", [](StandardSystemHandle* h, const StandardEvent& e) {
        auto key = e.get<int>("key");
        auto mod = e.get<int>("mod");
        // Handle input
    })
    .onEvent("OnMouseClick", [](StandardSystemHandle* h, const StandardEvent& e) {
        // Handle click
    })
    .useStoragePolicy()
    .build();
entitySystem->registerSystem(sys);
```

### 4. Physics with Delta Time
```cpp
auto* sys = pg::createStandardSystem("Physics")
    .ownComponents({"Position", "Velocity"})
    .onDelta([](StandardSystemHandle* h, float dt) {
        // dt is in seconds
        // Update positions based on velocity * dt
    })
    .build();
entitySystem->registerSystem(sys);
```

### 5. Persistent Game State
```cpp
auto* sys = pg::createStandardSystem("GameState")
    .onInit([](StandardSystemHandle* h) {
        h->setData("playerHealth", ElementType{100});
    })
    .enableSaveLoad()
    .onSave([](StandardSystemHandle* h, ElementMap& data) {
        data["health"] = h->getData("playerHealth");
    })
    .onLoad([](StandardSystemHandle* h, const ElementMap& data) {
        h->setData("playerHealth", data.at("health"));
    })
    .onFirstLoad([](StandardSystemHandle* h) {
        h->setData("playerHealth", ElementType{100});
    })
    .build();
entitySystem->registerSystem(sys);
```

## Key Concepts

### StandardEvent
Dynamic event with string name and property map:
```cpp
StandardEvent evt("PlayerDamage", "amount", 25, "source", "Enemy");
sys->sendEvent(evt);
```

### StandardComponent
Dynamic component with string type and property map:
```cpp
auto comp = sys->createComponent(entityId, "Health");
comp->set("current", 100);
comp->set("max", 100);
```

### ElementType
Universal type container for int, float, double, bool, string, size_t:
```cpp
ElementType val = 42;          // Auto-typed as int
int i = val.get<int>();
```

### System Data
Per-system persistent storage:
```cpp
sys->setData("key", ElementType{value});
auto val = sys->getData("key");
```

## Limitations & Workarounds

| Limitation | Workaround |
|-----------|-----------|
| Can't iterate entities in StandardSystemHandle | Create a traditional System<> for iteration-heavy work |
| No template-based type safety for components | Use string type names with convention (e.g., "Health", "Position") |
| Component iteration not exposed | Store entity IDs in system data and access individually |

## Performance Characteristics

- **Memory**: Each system: handle (pointer) + callback functions + system data map
- **CPU**: Event dispatch is O(1) per listener; system data lookup is O(1) average
- **Execution**: Sequential policy runs every frame; Storage policy is event-driven
- **Thread Safety**: Sequential/Storage policies are safe; Parallel policy needs synchronization

## Integration Notes

- StandardSystem works alongside traditional System<> based systems
- Events are dispatched to all listening systems
- Component changes trigger "Changed<Type>" events automatically
- System data is NOT saved automatically (must handle in onSave)

## Example Architecture

A typical game might use:
1. **Input System** (StandardSystem, Storage policy) - Handles keyboard/mouse/gamepad
2. **Game Logic System** (StandardSystem, Sequential policy) - Game rules and state
3. **Physics System** (StandardSystem with onDelta) - Movement and collision
4. **UI System** (Traditional System<>) - Complex iteration over UI elements
5. **Save System** (StandardSystem) - Coordinates save/load across all systems

## Next Steps

1. **Quick Integration**: Use STANDARD_SYSTEM_QUICK_REFERENCE.md to create your first system
2. **Deep Dive**: Read relevant sections of STANDARD_SYSTEM_CAPABILITIES.md
3. **Patterns**: Review "Common Use Cases" section above
4. **Debug**: Check implementation in /src/Engine/ECS/standardsystem.cpp

## Questions?

Refer to specific sections:
- "How do I handle events?" - Section 8 in CAPABILITIES.md
- "How do I create/modify components?" - Section 4 in CAPABILITIES.md  
- "How do I save/load?" - Section 1.5 in CAPABILITIES.md
- "What events are available?" - Section 8 in CAPABILITIES.md
- "How do I access the ECS world?" - Section 2.4 in CAPABILITIES.md

