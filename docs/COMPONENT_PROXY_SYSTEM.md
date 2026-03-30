# Zero-Copy Component Proxy System

**Status**: Design Phase
**Last Updated**: 2026-02-04
**Related**: [COMPONENT_GENERATOR.md](COMPONENT_GENERATOR.md), [vm_internals.rst](source/script/vm_internals.rst)

## Table of Contents
1. [Overview](#overview)
2. [Architecture](#architecture)
3. [How It Works](#how-it-works)
4. [Generated Code Structure](#generated-code-structure)
5. [Code Generation Templates](#code-generation-templates)
6. [VM Integration](#vm-integration)
7. [Usage Examples](#usage-examples)
8. [Benefits](#benefits)
9. [Implementation Checklist](#implementation-checklist)

---

## Overview

The Zero-Copy Component Proxy System provides **direct memory access** to ECS components from scripts without serialization overhead. Each component type gets its own proxy class that uses the VM's `__get` and `__set` metamethods to intercept property access.

### Key Features

- **Zero Serialization**: Direct pointer access via `createCustomPtr<T>()`
- **Per-Component Type Safety**: Each component has its own proxy class
- **Event Support**: Setters call component methods that fire change events
- **Custom Behavior**: Override specific property logic per component
- **Auto-Generated**: Component generator produces all proxy code
- **Backward Compatible**: Existing serialization code unchanged

### Problem Solved

**Before** (with serialization):
```javascript
// sys.getEntities() creates tables by serializing each component
// ~1000 components × ~50 properties = 50,000+ table field allocations
var entities = sys.getEntities("Position");
for (var i = 0; i < size(entities); i++) {
    var pos = entities[i].PositionComponent;
    pos.setX(pos.x + 10);  // Slow: reads from table, calls setter
}
```

**After** (with proxies):
```javascript
// sys.getEntities() creates proxies with component pointers
// ~1000 components × 1 pointer = 1,000 allocations
var entities = sys.getEntities("Position");
for (var i = 0; i < size(entities); i++) {
    var pos = entities[i].PositionComponent;  // PositionComponentProxy
    pos.x = pos.x + 10;  // Fast: direct memory access via metamethods
}
```

**Performance Gain**: ~50x fewer allocations, zero serialization overhead.

---

## Architecture

### Component Flow

```
┌─────────────────────────────────────────────────────────────────┐
│                     Component in C++ Memory                      │
│  ┌────────────────────────────────────────────────────────────┐ │
│  │ PositionComponent {                                         │ │
│  │   float x = 100.0f;    // Offset: 0                        │ │
│  │   float y = 200.0f;    // Offset: 4                        │ │
│  │   float z = 0.0f;      // Offset: 8                        │ │
│  │   ...                                                       │ │
│  │ }                                                           │ │
│  └────────────────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────────────┘
                             ▲
                             │ Pointer stored via createCustomPtr<T>()
                             │
┌─────────────────────────────────────────────────────────────────┐
│                      Proxy Instance (VM)                         │
│  ┌────────────────────────────────────────────────────────────┐ │
│  │ PositionComponentProxy instance {                          │ │
│  │   __componentPtr: CustomPtr<PositionComponent> ────────────┼─┘
│  │   class: PositionComponentProxy                            │
│  │ }                                                           │
│  └────────────────────────────────────────────────────────────┘ │
│                             │                                    │
│       ┌─────────────────────┼─────────────────────┐             │
│       ▼                     ▼                     ▼             │
│   pos.x              pos.y = 300           pos.setX(150)        │
│   (triggers          (triggers             (calls explicit      │
│    __get)            __set)                 method)             │
└─────────────────────────────────────────────────────────────────┘
```

### Class Hierarchy

```
Each Component Gets:
  1. ComponentProxy class (e.g., PositionComponentProxy)
     ├── Static registerWithVM(VM*) - Registers class with metamethods
     └── Static createProxy(VM*, Component*) - Creates proxy instance

  2. Native metamethods (__get, __set)
     ├── Extract component pointer from __componentPtr field
     ├── Cast to correct type using asCustomPtr<T>()
     └── Direct memory access or setter method call

  3. Optional explicit methods (getX, setX, etc.)
     └── Provide method call syntax as alternative
```

---

## How It Works

### Step 1: Proxy Class Registration (at VM startup)

Each proxy class is registered once with the VM:

```cpp
void PositionComponentProxy::registerWithVM(VM* vm) {
    // Create class
    Value klassValue = vm->createClass("PositionComponentProxy");
    Klass* klass = vm->asClass(klassValue);

    // Add __get metamethod
    vm->addNativeMethod(klass, "__get", [](VM* vm, int argCount, Value* args) -> Value {
        ObjInstance* self = vm->asInstance(args[0]);
        std::string propName = vm->asString(args[1]);

        // Get component pointer
        PositionComponent* comp = vm->asCustomPtr<PositionComponent>(
            self->fields["__componentPtr"]
        );

        // Direct memory access
        if (propName == "x") return makeDoubleValue(comp->x);
        if (propName == "y") return makeDoubleValue(comp->y);
        // ...
    });

    // Add __set metamethod
    vm->addNativeMethod(klass, "__set", [](VM* vm, int argCount, Value* args) -> Value {
        ObjInstance* self = vm->asInstance(args[0]);
        std::string propName = vm->asString(args[1]);
        Value newValue = args[2];

        PositionComponent* comp = vm->asCustomPtr<PositionComponent>(
            self->fields["__componentPtr"]
        );

        // Call setter methods (fire events)
        if (propName == "x") {
            float val = IS_DOUBLE(newValue) ? AS_DOUBLE(newValue) : AS_INT(newValue);
            comp->setX(val);  // Calls component's setter
            return newValue;
        }
        // ...
    });

    // Store class globally
    vm->globals["PositionComponentProxy"] = vm->retainValue(klassValue);
}
```

### Step 2: Proxy Factory Registration

Each component registers a factory function:

```cpp
// Auto-generated static initializer
namespace {
    struct PositionComponentProxyRegistrar {
        PositionComponentProxyRegistrar() {
            auto& reg = ComponentSerializerRegistry::instance()
                .getRegistration("PositionComponent");

            reg.proxyFactory = [](VM* vm, void* rawPtr) -> Value {
                PositionComponent* comp = static_cast<PositionComponent*>(rawPtr);
                return PositionComponentProxy::createProxy(vm, comp);
            };
        }
    };
    static PositionComponentProxyRegistrar s_positionProxyRegistrar;
}
```

### Step 3: Proxy Instance Creation

When `sys.getEntities()` is called:

```cpp
Value createProxy(VM* vm, PositionComponent* componentPtr) {
    // Get proxy class
    Klass* proxyClass = vm->asClass(vm->globals["PositionComponentProxy"]);

    // Create instance
    Value proxyInstance = vm->createInstance(proxyClass);
    ObjInstance* proxy = vm->asInstance(proxyInstance);

    // Store component pointer using existing infrastructure
    Value customPtr = vm->createCustomPtr<PositionComponent>(componentPtr);
    proxy->fields["__componentPtr"] = customPtr;

    return proxyInstance;
}
```

### Step 4: Property Access in Scripts

```javascript
var pos = entity.PositionComponent;  // PositionComponentProxy instance

// Reading: pos.x
// 1. VM sees GET_PROPERTY "x"
// 2. Checks instance fields - not found
// 3. Looks for __get metamethod in class
// 4. Calls __get(pos, "x")
// 5. __get extracts component pointer from __componentPtr
// 6. Returns makeDoubleValue(comp->x) - direct memory read!

// Writing: pos.x = 100
// 1. VM sees SET_PROPERTY "x"
// 2. Checks for __set metamethod
// 3. Calls __set(pos, "x", 100)
// 4. __set extracts component pointer
// 5. Calls comp->setX(100) - fires change events!
```

---

## Generated Code Structure

Each `.serialization.cpp` file contains:

```cpp
// PositionComponent.serialization.cpp (GENERATED)

namespace pg {

// ========== PROXY CLASS ==========
class PositionComponentProxy {
public:
    static void registerWithVM(VM* vm);
    static Value createProxy(VM* vm, PositionComponent* comp);
};

// ========== EXISTING SERIALIZER (backward compatibility) ==========
void serializePositionComponentWithSetters(VM* vm, ObjInstance* table,
                                           PositionComponent* component);
REGISTER_COMPONENT_SERIALIZER(PositionComponent, serializePositionComponentWithSetters);

// ========== EXISTING ATTACH HANDLER ==========
bool attachPositionComponent(VM* vm, EntitySystem* ecs, Entity* entity,
                              int argCount, Value* args);
REGISTER_COMPONENT_ATTACH_HANDLER(Position, attachPositionComponent);

// ========== PROXY REGISTRATION ==========
namespace {
    struct PositionComponentProxyRegistrar {
        PositionComponentProxyRegistrar();
    };
    static PositionComponentProxyRegistrar s_positionProxyRegistrar;
}

// ========== INITIALIZATION ==========
extern "C" void __init_PositionComponent_registration();

} // namespace pg
```

---

## Code Generation Templates

### Property Getter Template (for __get)

```cpp
// For each property, generate:

// Float property
if (propName == "{property_name}") return makeDoubleValue(comp->{property_name});

// Int property
if (propName == "{property_name}") return makeIntValue(comp->{property_name});

// Bool property
if (propName == "{property_name}") return makeBoolValue(comp->{property_name});

// String property
if (propName == "{property_name}") return vm->createString(comp->{property_name});
```

### Property Setter Template (for __set)

```cpp
// For each writable property, generate:

// Float property with setter
if (propName == "{property_name}") {
    float val = IS_DOUBLE(newValue) ? AS_DOUBLE(newValue) : AS_INT(newValue);
    comp->set{PropertyName}(val);  // Calls component's setter method
    return newValue;
}

// Int property with setter
if (propName == "{property_name}") {
    comp->set{PropertyName}(AS_INT(newValue));
    return newValue;
}

// Bool property with setter
if (propName == "{property_name}") {
    comp->set{PropertyName}(AS_BOOL(newValue));
    return newValue;
}

// String property with setter
if (propName == "{property_name}") {
    comp->set{PropertyName}(vm->asString(newValue));
    return newValue;
}
```

### Python Generator Code

```python
def generate_proxy_class(component_name, properties):
    """Generate complete proxy class for a component"""

    get_cases = []
    set_cases = []

    for prop in properties:
        # Generate __get case
        if prop['type'] == 'float':
            get_cases.append(f'if (propName == "{prop["name"]}") '
                           f'return makeDoubleValue(comp->{prop["name"]});')
        elif prop['type'] == 'int':
            get_cases.append(f'if (propName == "{prop["name"]}") '
                           f'return makeIntValue(comp->{prop["name"]});')
        elif prop['type'] == 'bool':
            get_cases.append(f'if (propName == "{prop["name"]}") '
                           f'return makeBoolValue(comp->{prop["name"]});')
        elif prop['type'] == 'string':
            get_cases.append(f'if (propName == "{prop["name"]}") '
                           f'return vm->createString(comp->{prop["name"]});')

        # Generate __set case (if writable)
        if prop.get('writable', True):
            setter_name = f"set{prop['name'].capitalize()}"

            if prop['type'] == 'float':
                set_cases.append(f'''if (propName == "{prop['name']}") {{
    float val = IS_DOUBLE(newValue) ? AS_DOUBLE(newValue) : AS_INT(newValue);
    comp->{setter_name}(val);
    return newValue;
}}''')
            elif prop['type'] == 'int':
                set_cases.append(f'''if (propName == "{prop['name']}") {{
    comp->{setter_name}(AS_INT(newValue));
    return newValue;
}}''')
            elif prop['type'] == 'bool':
                set_cases.append(f'''if (propName == "{prop['name']}") {{
    comp->{setter_name}(AS_BOOL(newValue));
    return newValue;
}}''')

    # Format the template
    return PROXY_CLASS_TEMPLATE.format(
        ComponentName=component_name,
        componentName=component_name.lower(),
        get_cases='\n            '.join(get_cases),
        set_cases='\n            '.join(set_cases)
    )

PROXY_CLASS_TEMPLATE = '''
// ========== PROXY CLASS (GENERATED) ==========

class {ComponentName}Proxy {{
public:
    static void registerWithVM(VM* vm) {{
        Value klassValue = vm->createClass("{ComponentName}Proxy");
        Klass* klass = vm->asClass(klassValue);

        // __get metamethod
        vm->addNativeMethod(klass, "__get", [](VM* vm, int argCount, Value* args) -> Value {{
            if (argCount < 2) throw std::runtime_error("__get requires 2 arguments");

            ObjInstance* self = vm->asInstance(args[0]);
            std::string propName = vm->asString(args[1]);

            {ComponentName}* comp = vm->asCustomPtr<{ComponentName}>(
                self->fields["__componentPtr"]
            );

            {get_cases}

            throw std::runtime_error("Unknown property: " + propName);
        }});

        // __set metamethod
        vm->addNativeMethod(klass, "__set", [](VM* vm, int argCount, Value* args) -> Value {{
            if (argCount < 3) throw std::runtime_error("__set requires 3 arguments");

            ObjInstance* self = vm->asInstance(args[0]);
            std::string propName = vm->asString(args[1]);
            Value newValue = args[2];

            {ComponentName}* comp = vm->asCustomPtr<{ComponentName}>(
                self->fields["__componentPtr"]
            );

            {set_cases}

            throw std::runtime_error("Unknown property: " + propName);
        }});

        vm->globals["{ComponentName}Proxy"] = vm->retainValue(klassValue);
        LOG_INFO("ComponentProxy", "Registered {ComponentName}Proxy class");
    }}

    static Value createProxy(VM* vm, {ComponentName}* componentPtr) {{
        auto it = vm->globals.find("{ComponentName}Proxy");
        if (it == vm->globals.end()) {{
            throw std::runtime_error("{ComponentName}Proxy not registered");
        }}

        Klass* proxyClass = vm->asClass(it->second);
        Value proxyInstance = vm->createInstance(proxyClass);
        ObjInstance* proxy = vm->asInstance(proxyInstance);

        Value customPtr = vm->createCustomPtr<{ComponentName}>(componentPtr);
        proxy->fields["__componentPtr"] = customPtr;

        return proxyInstance;
    }}
}};

// Register proxy factory
namespace {{
    struct {ComponentName}ProxyRegistrar {{
        {ComponentName}ProxyRegistrar() {{
            auto& reg = ComponentSerializerRegistry::instance()
                .getRegistration("{ComponentName}");

            reg.proxyFactory = [](VM* vm, void* rawPtr) -> Value {{
                {ComponentName}* comp = static_cast<{ComponentName}*>(rawPtr);
                return {ComponentName}Proxy::createProxy(vm, comp);
            }};
        }}
    }};
    static {ComponentName}ProxyRegistrar s_{componentName}ProxyRegistrar;
}}
'''
```

---

## VM Integration

### 1. Update ComponentSerializerRegistry

Add helper methods to [entitysystem.h](../src/Engine/ECS/entitysystem.h):

```cpp
class ComponentSerializerRegistry {
public:
    // ... existing methods ...

    /**
     * @brief Check if a component has a proxy factory registered
     */
    bool hasProxyFactory(const std::string& componentName) const {
        auto it = serializers_.find(componentName);
        return it != serializers_.end() && it->second.proxyFactory != nullptr;
    }

    /**
     * @brief Create a proxy instance for a component
     */
    Value createProxy(const std::string& componentName, VM* vm, void* componentPtr) const {
        auto it = serializers_.find(componentName);
        if (it != serializers_.end() && it->second.proxyFactory) {
            return it->second.proxyFactory(vm, componentPtr);
        }
        throw std::runtime_error("No proxy factory registered for: " + componentName);
    }

    /**
     * @brief Get a registration entry for modification (used during setup)
     */
    ComponentSerializerRegistration& getRegistration(const std::string& componentName) {
        return serializers_[componentName];
    }
};
```

### 2. Register Proxy Classes at VM Startup

In your ECS initialization:

```cpp
// In entitysystem.cpp or main.cpp

void EntitySystem::initializeScriptingVM(VM& vm) {
    // Register all component proxy classes
    PositionComponentProxy::registerWithVM(&vm);
    Texture2DComponentProxy::registerWithVM(&vm);
    VelocityComponentProxy::registerWithVM(&vm);
    // ... for each component (auto-generate this list)

    // Register ECS module
    vm.addNativeModule("ecs", EcsCompiledModule{this});

    LOG_INFO("EntitySystem", "All component proxies registered");
}
```

**Alternative: Macro-based registration**

```cpp
// In a generated header (e.g., AllComponentProxies.generated.h)
#define REGISTER_ALL_COMPONENT_PROXIES(vm) \
    do { \
        PositionComponentProxy::registerWithVM(vm); \
        Texture2DComponentProxy::registerWithVM(vm); \
        VelocityComponentProxy::registerWithVM(vm); \
    } while(0)

// In initialization
REGISTER_ALL_COMPONENT_PROXIES(&vm);
```

### 3. Update sys.getEntities() to Use Proxies

Modify your system module:

```cpp
// In sysmodule.h or wherever sys.getEntities() is defined

addNativeFunction("getEntities", [systemImplCopy, ecsRefCopy](VM* vm, int argCount, Value* args) -> Value {
    auto componentName = vm->asString(args[0]);
    auto* owner = systemImplCopy->getComponentOwner(componentName);

    Value vectorValue = vm->createVector();
    ObjVector* vector = vm->asVector(vectorValue);

    auto& registry = ComponentSerializerRegistry::instance();

    for (auto* component : owner->view()) {
        auto* entity = ecsRefCopy->getEntity(component->entityId);

        // Create entity wrapper table
        Value entityTable = vm->createInstance(vm->asClass(vm->globals["__Table"]));
        ObjInstance* entityInst = vm->asInstance(entityTable);

        entityInst->fields["__entityId"] = makeIntValue(entity->id);

        // For each component on the entity, create a proxy
        for (auto [compTypeId, compPtr] : entity->getAllComponents()) {
            std::string compTypeName = ecsRefCopy->getComponentRegistry()
                ->getComponentTypeName(compTypeId);

            // Use proxy if available, otherwise fall back to serialization
            if (registry.hasProxyFactory(compTypeName)) {
                Value proxyValue = registry.createProxy(compTypeName, vm, compPtr);
                entityInst->fields[compTypeName] = proxyValue;
            }
            else {
                // Fallback to old serialization method
                Value serializedComp = serializeComponentToTable(vm, ecsRefCopy, entity, compTypeId);
                entityInst->fields[compTypeName] = serializedComp;
            }
        }

        vector->fields.push_back(vm->retainValue(entityTable));
    }

    return vectorValue;
});
```

---

## Usage Examples

### Basic Property Access

```javascript
import "sys"

// Get entities with PositionComponent
var entities = sys.getEntities("Position");

for (var i = 0; i < size(entities); i++) {
    var entity = entities[i];
    var pos = entity.PositionComponent;  // PositionComponentProxy instance

    // Zero-copy read (direct memory access via __get)
    var x = pos.x;
    var y = pos.y;

    __dprint("Entity " + toString(entity.__entityId) + " at (" +
             toString(x) + ", " + toString(y) + ")");

    // Zero-copy write (calls setX() via __set, fires change events)
    pos.x = x + 10;
    pos.y = y + 5;
}
```

### Mixed Property Syntax

```javascript
var pos = entity.PositionComponent;

// All three approaches work:
pos.x = 100;              // Property syntax via __set (recommended)
pos.setX(100);            // Explicit method call
pos["x"] = 100;           // Index syntax via __set
```

### Batch Updates

```javascript
function moveAllEntitiesRight(distance) {
    var entities = sys.getEntities("Position");

    for (var i = 0; i < size(entities); i++) {
        var pos = entities[i].PositionComponent;
        pos.x = pos.x + distance;  // Zero-copy read + write
    }
}

moveAllEntitiesRight(50.0);
```

### Conditional Updates

```javascript
var entities = sys.getEntities("Position");

for (var i = 0; i < size(entities); i++) {
    var entity = entities[i];
    var pos = entity.PositionComponent;

    // Only update if off-screen
    if (pos.x < 0 or pos.x > 800) {
        pos.x = 400;  // Center horizontally
    }

    if (pos.y < 0 or pos.y > 600) {
        pos.y = 300;  // Center vertically
    }
}
```

### Reading Multiple Properties

```javascript
var pos = entity.PositionComponent;

// All zero-copy reads
var x = pos.x;
var y = pos.y;
var width = pos.width;
var height = pos.height;
var rotation = pos.rotation;

// Calculate bounding box (no serialization overhead!)
var left = x - width / 2;
var right = x + width / 2;
var top = y - height / 2;
var bottom = y + height / 2;
```

---

## Benefits

### Performance

| Metric | Before (Serialization) | After (Proxies) | Improvement |
|--------|----------------------|-----------------|-------------|
| Allocations | ~50,000 (1000 entities × 50 fields) | ~1,000 (1000 pointers) | **50x fewer** |
| Memory | ~400 KB (Value structs) | ~8 KB (pointers only) | **50x less** |
| Property Read | Table lookup + Value extraction | Direct memory dereference | **~10x faster** |
| Property Write | Table lookup + setter call | Direct setter call | **~5x faster** |
| GC Pressure | High (many retained Values) | Low (pointers only) | **Significant** |

### Code Quality

✅ **Type Safety**: Each component has its own proxy class
✅ **Static Analysis**: Each proxy can be analyzed separately
✅ **Custom Behavior**: Override specific properties per component
✅ **Event Support**: Setters call component methods that fire events
✅ **Auto-Generated**: No manual proxy code to maintain
✅ **Backward Compatible**: Existing serialization still works

### Developer Experience

✅ **Natural Syntax**: `pos.x = 100` works intuitively
✅ **No Script Changes**: Existing scripts continue to work
✅ **Mixed Approaches**: Can use proxies + serialization together
✅ **Debugging**: Proxy class names visible in logs/errors
✅ **Documentation**: Each proxy class can be documented

---

## Implementation Checklist

### Phase 1: Infrastructure (VM Changes)

- [ ] Add `hasProxyFactory()` to ComponentSerializerRegistry
- [ ] Add `createProxy()` to ComponentSerializerRegistry
- [ ] Add `getRegistration()` to ComponentSerializerRegistry
- [ ] Test existing `createCustomPtr<T>()` and `asCustomPtr<T>()`
- [ ] Verify `__get` and `__set` metamethods work correctly

### Phase 2: Code Generation (Generator Changes)

- [ ] Create proxy class generation template
- [ ] Generate `__get` metamethod with property cases
- [ ] Generate `__set` metamethod with setter calls
- [ ] Generate `registerWithVM()` static method
- [ ] Generate `createProxy()` static method
- [ ] Generate proxy factory registration code
- [ ] Add proxy generation to component generator

### Phase 3: VM Integration

- [ ] Create macro or function to register all proxy classes
- [ ] Call registration in VM initialization
- [ ] Update `sys.getEntities()` to use proxies
- [ ] Add fallback to serialization for components without proxies
- [ ] Test mixed proxy + serialization scenarios

### Phase 4: Testing

- [ ] Test basic property reads (pos.x)
- [ ] Test basic property writes (pos.x = 100)
- [ ] Test index syntax (pos["x"])
- [ ] Test explicit methods (pos.setX(100))
- [ ] Test event firing from setters
- [ ] Test with 1000+ entities
- [ ] Benchmark vs. old serialization approach
- [ ] Test memory usage and GC pressure

### Phase 5: Migration

- [ ] Generate proxies for all existing components
- [ ] Update documentation
- [ ] Create migration guide for custom components
- [ ] Deprecate old serialization approach (optional)

---

## Related Documentation

- [Component Generator System](COMPONENT_GENERATOR.md) - How components are generated
- [Component Schema Reference](COMPONENT_SCHEMA_REFERENCE.md) - `.pgcomp` file format
- [VM Internals](source/script/vm_internals.rst) - How the VM works internally
- [Metamethods Documentation](source/script/vm_internals.rst#metamethods) - `__get` and `__set` details

---

## Notes

### Field Name Convention

- **Internal fields**: Fields starting with `__` (double underscore) bypass metamethods
  - Example: `__componentPtr`, `__typeInfo`
  - Used to store proxy state without triggering recursion

### Pointer Lifetime

- Component pointers are **NOT owned** by proxies
- Proxies are **invalidated** when components are deleted
- Always ensure components exist before accessing via proxy
- Consider adding validation in `__get`/`__set` if needed

### Custom Behavior per Component

You can customize specific properties:

```cpp
// In __set metamethod for a specific component
if (propName == "health") {
    int newHealth = AS_INT(newValue);

    // Custom validation
    if (newHealth < 0) newHealth = 0;
    if (newHealth > comp->maxHealth) newHealth = comp->maxHealth;

    comp->setHealth(newHealth);  // Fires events
    return makeIntValue(newHealth);
}
```

### Readonly Properties

For readonly properties, simply don't add them to `__set`:

```cpp
// In __get (always included)
if (propName == "maxHealth") return makeIntValue(comp->maxHealth);

// NOT in __set (readonly)
// Attempting pos.maxHealth = 100 will throw error
```

---

**End of Document**
