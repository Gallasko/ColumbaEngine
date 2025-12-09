# StandardSystem Quick Reference Card

## Creating a System

```cpp
auto* system = pg::createStandardSystem("SystemName")
    .ownComponents({"CompType1", "CompType2"})
    .onInit(callback)
    .onEvent("EventName", callback)
    .onExecute(callback)
    .onDelta(callback)
    .build();
    
entitySystem->registerSystem(system);
```

## Available Callbacks

| Callback | Signature | Called When |
|----------|-----------|------------|
| `onInit` | `(StandardSystemHandle*)` | System registered |
| `onExecute` | `(StandardSystemHandle*)` | Every frame |
| `onEvent` | `(StandardSystemHandle*, const StandardEvent&)` | Event fired |
| `onDelta` | `(StandardSystemHandle*, float deltaSeconds)` | Every frame with delta |
| `onSave` | `(StandardSystemHandle*, ElementMap&)` | Game saving |
| `onLoad` | `(StandardSystemHandle*, const ElementMap&)` | Game loading |
| `onFirstLoad` | `(StandardSystemHandle*)` | First time (no save exists) |

## StandardSystemHandle Methods

```cpp
// Components
StandardComponent* createComponent(size_t entityId, const std::string& type);
StandardComponent* getComponent(size_t entityId, const std::string& type);
void removeComponent(size_t entityId, const std::string& type);

// Events
void sendEvent(const StandardEvent& event);
void sendEvent(const std::string& eventName);
void sendEvent(const std::string& eventName, const std::string& key, const ElementType& value);

// System Data
ElementMap* getData();
ElementType getData(const std::string& name);
void setData(const std::string& name, const ElementType& value);

// ECS World
EntitySystem* getWorld() const;
```

## ElementType - Universal Container

```cpp
ElementType x = 42;              // int
ElementType y = 3.14f;           // float
ElementType s = "hello";         // string
ElementType b = true;            // bool

int i = x.get<int>();
float f = y.get<float>();
std::string str = s.get<std::string>();

// Type checking
if (x.isNumber()) { }
if (s.isLiteral()) { }
```

## StandardEvent Usage

```cpp
// Create event
StandardEvent evt("EventName");
evt.values["key1"] = ElementType{value1};
evt.values["key2"] = ElementType{value2};

// Send event
sys->sendEvent(evt);

// Receive in callback
.onEvent("EventName", [](StandardSystemHandle* sys, const StandardEvent& evt) {
    int val = evt.get<int>("key1");
    bool has = evt.has("key2");
})
```

## StandardComponent Usage

```cpp
// Create with default values
.ownComponent("Health", "current", 100, "max", 100)

// Create and use
StandardComponent* health = sys->createComponent(entityId, "Health");
health->set("current", 90);
int hp = health->get<int>("current");

// Property change event (fires "ChangedHealth" event)
health->setWithEvent("current", 80);

// Check property existence
if (health->has("current")) { }
```

## System Data Storage

```cpp
// Initialize in onInit
sys->setData("frameCount", ElementType{0});

// Access in callbacks
auto frameCount = sys->getData("frameCount").get<int>();
sys->setData("frameCount", ElementType{frameCount + 1});

// Iterate all data
for (const auto& [key, value] : *sys->getData()) {
    // Use key and value
}
```

## Available Events (Input)

### Keyboard
- **OnSDLScanCode** - Key pressed (SDL_Scancode key, Uint16 mod)
- **OnSDLScanCodeReleased** - Key released
- **OnSDLTextInput** - Text input (string text)

### Mouse
- **OnMouseClick** - Click event (Point2D pos, MouseButton button)
- **OnMouseRelease** - Release event
- **OnMouseMove** - Move event (Point2D pos)
- **OnSDLMouseWheel** - Wheel scroll (Sint32 x, Sint32 y)

### Gamepad
- **OnSDLGamepadPressed** - Button pressed (int id, unsigned int button)
- **OnSDLGamepadReleased** - Button released
- **OnSDLGamepadAxisChanged** - Axis moved (int id, unsigned int axis, int value)

### Core
- **TickEvent** - Frame tick (float tick in milliseconds)

## Execution Policies

```cpp
.useSequentialPolicy()    // Default: execute() every frame in sequence
.useStoragePolicy()       // Event-driven only, no execute()
.useParallelPolicy()      // Can run in parallel with other systems
.useManualPolicy()        // Manual execution control
.useIndependentPolicy()   // System runs independently
```

## Save/Load

```cpp
.enableSaveLoad()
.onSave([](StandardSystemHandle* sys, ElementMap& data) {
    data["key"] = ElementType{value};
})
.onLoad([](StandardSystemHandle* sys, const ElementMap& data) {
    auto val = data.at("key");
})
.onFirstLoad([](StandardSystemHandle* sys) {
    // Initialize defaults if no save exists
})
```

## Common Patterns

### Frame Counter
```cpp
.onInit([](StandardSystemHandle* sys) {
    sys->setData("frames", ElementType{0});
})
.onExecute([](StandardSystemHandle* sys) {
    auto f = sys->getData("frames").get<int>();
    sys->setData("frames", ElementType{f + 1});
})
```

### Handle Keyboard Input
```cpp
.onEvent("OnSDLScanCode", [](StandardSystemHandle* sys, const StandardEvent& evt) {
    auto key = evt.get<int>("key");
    auto mod = evt.get<int>("mod");
    // Handle key
})
```

### Handle Mouse Click
```cpp
.onEvent("OnMouseClick", [](StandardSystemHandle* sys, const StandardEvent& evt) {
    // Mouse event data available
})
```

### Time-Based Physics
```cpp
.onDelta([](StandardSystemHandle* sys, float dt) {
    // dt is in seconds
    // Update positions: position += velocity * dt
})
```

### Component Management
```cpp
auto comp = sys->createComponent(entityId, "Health");
if (comp) {
    comp->set("current", 100);
    comp->set("max", 100);
}

// Later...
auto health = sys->getComponent(entityId, "Health");
if (health && health->has("current")) {
    int hp = health->get<int>("current");
}

// Remove
sys->removeComponent(entityId, "Health");
```

## Important Notes

1. **Component registration**: Components must be owned (registered with `.ownComponent()`) to be created/deleted
2. **Event listening**: Only explicitly listened events are received (via `.onEvent()`)
3. **Delta time**: In milliseconds from TickEvent, convert to seconds: `deltaSeconds = tickEvent / 1000.0f`
4. **System data**: Not automatically saved - must explicitly copy to save data in `.onSave()`
5. **Entity iteration**: Not exposed in StandardSystemHandle - use traditional System for iteration-heavy work
6. **Thread safety**: Sequential policy is single-threaded; Parallel/Independent policies may need synchronization

## Files

- **Header**: `/src/Engine/ECS/standardsystem.h`
- **Implementation**: `/src/Engine/ECS/standardsystem.cpp`
- **Events**: `/src/Engine/ECS/standardevent.h`
- **Components**: `/src/Engine/ECS/component.h`
- **Input Events**: `/src/Engine/Input/inputcomponent.h`

