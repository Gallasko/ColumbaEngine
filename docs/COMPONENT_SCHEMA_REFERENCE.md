# Component Schema Reference (.pgcomp)

**Version**: 1.0
**Last Updated**: 2026-01-20

Complete reference for the `.pgcomp` component definition format.

---

## File Structure

```yaml
component: string              # Component name (required)

metadata:                      # Component metadata (optional)
  category: string
  implements: string|array
  doc: string
  includes: array

fields: array                  # Component fields (required)

methods: array                 # Component methods (optional)

hooks: array                   # Lifecycle hooks (optional)

generation: object             # Generation options (optional)
```

---

## Top-Level Fields

### `component` (required)
**Type**: `string`

The name of the component struct.

**Example**:
```yaml
component: PositionComponent
```

**Generated**: `struct PositionComponent { ... }`

---

### `metadata` (optional)
**Type**: `object`

Component metadata and configuration.

#### `metadata.category`
**Type**: `string`
**Default**: `""`

Optional category for organizational purposes (not used in code generation).

**Example**:
```yaml
metadata:
  category: "2D"
```

#### `metadata.implements`
**Type**: `string | array`
**Default**: `none`
**Valid Values**: `none`, `Ctor`, `Dtor`, `Component`, or array of these

Specifies base class inheritance.

**Examples**:
```yaml
# Single inheritance
metadata:
  implements: Ctor

# Multiple inheritance
metadata:
  implements: [Ctor, Dtor]

# Component (implies Ctor)
metadata:
  implements: Component
```

**Generated**:
```cpp
// implements: Ctor
struct PositionComponent : public Ctor { ... }

// implements: [Ctor, Dtor]
struct PositionComponent : public Ctor, public Dtor { ... }

// implements: Component
struct PositionComponent : public Component { ... }
```

#### `metadata.doc`
**Type**: `string`
**Default**: `""`

Documentation comment for the component.

**Example**:
```yaml
metadata:
  doc: "Component for entity position, size, and visibility in 2D space"
```

**Generated**:
```cpp
/**
 * Component for entity position, size, and visibility in 2D space
 */
struct PositionComponent { ... }
```

#### `metadata.includes`
**Type**: `array`
**Default**: `[]`

Additional header files to include in the generated header.

**Example**:
```yaml
metadata:
  includes:
    - "string"
    - "pgconstant.h"
    - "vector"
```

**Generated**:
```cpp
#include "ECS/component.h"  // Always included
#include "string"
#include "pgconstant.h"
#include "vector"
```

---

## Fields

### Field Definition
**Type**: `array` of `object`

Each field represents a member variable in the component struct.

#### Field Properties

| Property | Type | Required | Default | Description |
|----------|------|----------|---------|-------------|
| `name` | `string` | ✅ | - | Field name |
| `type` | `string` | ✅ | - | C++ type |
| `default` | `string` | ❌ | `""` | Default value |
| `writable` | `boolean` | ❌ | `true` | Can scripts write to this field? |
| `readable` | `boolean` | ❌ | `true` | Can scripts read this field? |
| `setter` | `string` | ❌ | `none` | Setter level (see below) |
| `event` | `string` | ❌ | `""` | Event type to send (for event setters) |
| `custom_code` | `string` | ❌ | `""` | Custom code before assignment (custom_event) |
| `custom_impl` | `string` | ❌ | `""` | Full implementation (custom setter) |
| `doc` | `string` | ❌ | `""` | Field documentation |

---

### Field Examples

#### Basic Field
```yaml
- name: x
  type: float
  default: 0.0
  doc: "X coordinate in world space"
```

**Generated**:
```cpp
// X coordinate in world space
float x = 0.0f;
```

#### Field with Basic Setter
```yaml
- name: rotation
  type: float
  default: 0.0
  writable: true
  setter: basic
```

**Generated**:
```cpp
// In .h
float rotation = 0.0f;
void setRotation(float value);

// In .cpp
void ComponentName::setRotation(float value) {
    rotation = value;
}
```

#### Field with Event Setter
```yaml
- name: width
  type: float
  default: 0.0
  writable: true
  setter: event
  event: PositionComponentChangedEvent
```

**Generated**:
```cpp
// In .h
float width = 0.0f;
void setWidth(float value);

// In .cpp
void ComponentName::setWidth(float value) {
    if (this->width != value) {
        this->width = value;
        if (ecsRef) {
            ecsRef->sendEvent(PositionComponentChangedEvent{id});
        }
    }
}
```

#### Field with Custom Event Setter
```yaml
- name: visible
  type: bool
  default: true
  writable: true
  setter: custom_event
  event: PositionComponentChangedEvent
  custom_code: |
    LOG_MILE("Position", "Visibility changed for entity " + std::to_string(id));
```

**Generated**:
```cpp
void ComponentName::setVisible(bool value) {
    if (this->visible != value) {
        LOG_MILE("Position", "Visibility changed for entity " + std::to_string(id));
        this->visible = value;
        if (ecsRef) {
            ecsRef->sendEvent(PositionComponentChangedEvent{id});
        }
    }
}
```

#### Field with Fully Custom Setter
```yaml
- name: x
  type: float
  default: 0.0
  writable: true
  setter: custom
  custom_impl: |
    if (x != value) {
        x = value;
        updateAnchoredChildren();
        if (ecsRef) {
            ecsRef->sendEvent(PositionComponentChangedEvent{id});
        }
    }
```

**Generated**:
```cpp
// In .h
float x = 0.0f;
void setX(float value);

// In .cpp
void ComponentName::setX(float value) {
    if (x != value) {
        x = value;
        updateAnchoredChildren();
        if (ecsRef) {
            ecsRef->sendEvent(PositionComponentChangedEvent{id});
        }
    }
}
```

#### Internal Field (Not Exposed to Scripts)
```yaml
- name: id
  type: _unique_id
  default: 0
  readable: false
  writable: false
```

**Generated**:
```cpp
_unique_id id = 0;
// Not included in proxy schema
```

---

## Setter Levels Reference

| Level | Value | Description | Use Case |
|-------|-------|-------------|----------|
| **0** | `none` (or omit) | No setter, direct access only | Simple data fields |
| **1** | `basic` | Simple assignment | Basic wrapping |
| **2** | `event` | Assignment + event notification | Most common case |
| **3** | `custom_event` | Custom code + assignment + event | Logging, state updates |
| **4** | `custom` | Fully custom implementation | Complex logic |

---

## Methods

### Method Definition
**Type**: `array` of `object`

Methods expose functionality to both C++ and scripts.

#### Method Properties

| Property | Type | Required | Default | Description |
|----------|------|----------|---------|-------------|
| `name` | `string` | ✅ | - | Method name |
| `returns` | `string` | ✅ | - | Return type |
| `params` | `array` | ❌ | `[]` | Parameter list |
| `inline` | `string` | ❌ | `""` | Inline implementation (in header) |
| `impl` | `string` | ❌ | `""` | Full implementation (in cpp) |

**Note**: Must provide either `inline` OR `impl`, not both.

---

### Method Examples

#### Simple Inline Method
```yaml
- name: isVisible
  returns: bool
  inline: "return visible;"
```

**Generated**:
```cpp
// In .h
bool isVisible() const { return visible; }
```

#### Method with Parameters
```yaml
- name: setColor
  returns: void
  params: [constant::Vector4D newColor]
  impl: |
    if (color != newColor) {
        color = newColor;
        changed = true;
        if (ecsRef) {
            ecsRef->sendEvent(EntityChangedEvent{entityId});
        }
    }
```

**Generated**:
```cpp
// In .h
void setColor(constant::Vector4D newColor);

// In .cpp
void ComponentName::setColor(constant::Vector4D newColor) {
    if (color != newColor) {
        color = newColor;
        changed = true;
        if (ecsRef) {
            ecsRef->sendEvent(EntityChangedEvent{entityId});
        }
    }
}
```

#### Method with Multiple Parameters
```yaml
- name: setPosition
  returns: void
  params: [float x, float y, float z]
  impl: |
    this->x = x;
    this->y = y;
    this->z = z;
    if (ecsRef) {
        ecsRef->sendEvent(PositionComponentChangedEvent{id});
    }
```

---

## Hooks

### Hook Definition
**Type**: `array` of `object`

Lifecycle hooks for component creation, deletion, etc.

#### Hook Properties

| Property | Type | Required | Default | Description |
|----------|------|----------|---------|-------------|
| `name` | `string` | ✅ | - | Hook name (`onCreation`, `onDeletion`) |
| `params` | `array` | ✅ | - | Parameter list |
| `inline` | `string` | ❌ | `""` | Inline implementation (in header) |
| `impl` | `string` | ❌ | `""` | Full implementation (in cpp) |

---

### Hook Examples

#### Simple Inline Hook
```yaml
hooks:
  - name: onCreation
    params: [EntityRef entity]
    inline: |
      id = entity.id;
      ecsRef = entity.ecs;
```

**Generated**:
```cpp
// In .h
virtual void onCreation(EntityRef entity) override {
    id = entity.id;
    ecsRef = entity.ecs;
}
```

#### Complex Hook Implementation
```yaml
hooks:
  - name: onCreation
    params: [EntityRef entity]
    impl: |
      id = entity.id;
      ecsRef = entity.ecs;

      // Initialize texture
      loadTextureFromFile("default.png");

      // Register with renderer
      if (auto* renderer = entity.ecs->getSystem<RenderSystem>()) {
          renderer->registerRenderable(id);
      }
```

**Generated**:
```cpp
// In .h
virtual void onCreation(EntityRef entity) override;

// In .cpp
void ComponentName::onCreation(EntityRef entity) {
    id = entity.id;
    ecsRef = entity.ecs;

    // Initialize texture
    loadTextureFromFile("default.png");

    // Register with renderer
    if (auto* renderer = entity.ecs->getSystem<RenderSystem>()) {
        renderer->registerRenderable(id);
    }
}
```

---

## Generation Options

### Generation Object
**Type**: `object`

Controls which files are generated.

#### Generation Properties

| Property | Type | Default | Description |
|----------|------|---------|-------------|
| `header` | `boolean` | `true` | Generate `.generated.h` |
| `cpp` | `boolean` | `true` | Generate `.generated.cpp` |
| `proxy` | `boolean` | `false` | Generate `.proxy.generated.cpp` |
| `serialization` | `boolean` | `false` | Generate `.serialization.generated.cpp` |
| `attach_handler` | `boolean` | `false` | Generate `.attach.generated.cpp` |

---

### Generation Examples

#### Full Generation
```yaml
generation:
  header: true
  cpp: true
  proxy: true
  serialization: true
  attach_handler: true
```

#### Header Only
```yaml
generation:
  header: true
  cpp: false
  proxy: false
  serialization: false
  attach_handler: false
```

---

## Complete Examples

### Minimal Component
```yaml
component: Health

fields:
  - name: current
    type: int
    default: 100

  - name: maximum
    type: int
    default: 100
```

### Standard Component
```yaml
component: Velocity

metadata:
  category: "Physics"
  implements: Component

fields:
  - name: vx
    type: float
    default: 0.0
    writable: true
    setter: event
    event: VelocityChangedEvent

  - name: vy
    type: float
    default: 0.0
    writable: true
    setter: event
    event: VelocityChangedEvent

  - name: entityId
    type: _unique_id
    default: 0
    readable: false
    writable: false

  - name: ecsRef
    type: EntitySystem*
    default: nullptr
    readable: false
    writable: false

methods:
  - name: getSpeed
    returns: float
    inline: "return std::sqrt(vx * vx + vy * vy);"

generation:
  header: true
  cpp: true
  proxy: true
  serialization: true
  attach_handler: true
```

### Complex Component with Custom Logic
```yaml
component: Transform

metadata:
  category: "2D"
  implements: Ctor
  includes:
    - "glm/glm.hpp"

fields:
  - name: position
    type: glm::vec3
    default: "glm::vec3(0.0f)"
    writable: true
    setter: custom
    custom_impl: |
      if (position != value) {
          position = value;
          dirty = true;
          updateChildren();
          if (ecsRef) {
              ecsRef->sendEvent(TransformChangedEvent{id});
          }
      }

  - name: rotation
    type: glm::quat
    default: "glm::quat(1.0f, 0.0f, 0.0f, 0.0f)"
    writable: true
    setter: custom
    custom_impl: |
      if (rotation != value) {
          rotation = value;
          dirty = true;
          if (ecsRef) {
              ecsRef->sendEvent(TransformChangedEvent{id});
          }
      }

  - name: scale
    type: glm::vec3
    default: "glm::vec3(1.0f)"
    writable: true
    setter: event
    event: TransformChangedEvent

  - name: dirty
    type: bool
    default: true
    readable: false
    writable: false

  - name: id
    type: _unique_id
    default: 0
    readable: false
    writable: false

  - name: ecsRef
    type: EntitySystem*
    default: nullptr
    readable: false
    writable: false

methods:
  - name: getMatrix
    returns: glm::mat4
    impl: |
      glm::mat4 mat = glm::mat4(1.0f);
      mat = glm::translate(mat, position);
      mat = mat * glm::mat4_cast(rotation);
      mat = glm::scale(mat, scale);
      return mat;

  - name: isDirty
    returns: bool
    inline: "return dirty;"

  - name: markClean
    returns: void
    inline: "dirty = false;"

hooks:
  - name: onCreation
    params: [EntityRef entity]
    inline: |
      id = entity.id;
      ecsRef = entity.ecs;

generation:
  header: true
  cpp: true
  proxy: true
  serialization: true
  attach_handler: true
```

---

## Type Mapping Reference

Common C++ types and their default values:

| C++ Type | Example Default | Notes |
|----------|----------------|-------|
| `int` | `0` | |
| `float` | `0.0` or `0.0f` | |
| `double` | `0.0` | |
| `bool` | `true` or `false` | |
| `std::string` | `""` | Use quotes |
| `_unique_id` | `0` | Engine type |
| `EntitySystem*` | `nullptr` | Pointer type |
| `glm::vec3` | `glm::vec3(0.0f)` | Struct with constructor |
| `constant::Vector4D` | `{255, 255, 255, 255}` | Aggregate initialization |

---

## YAML Syntax Notes

### Multi-line Strings
Use `|` for multi-line strings (preserves newlines):

```yaml
custom_impl: |
  if (x != value) {
      x = value;
      updateChildren();
  }
```

### Comments
Use `#` for comments:

```yaml
# This is a comment
fields:
  - name: x  # Inline comment
    type: float
```

### Arrays
Two syntaxes supported:

```yaml
# Inline
includes: ["header1.h", "header2.h"]

# Multi-line
includes:
  - "header1.h"
  - "header2.h"
```

---

## Validation Rules

The generator validates:

1. **Required fields**: `component`, `fields` must be present
2. **Field requirements**: Each field must have `name` and `type`
3. **Setter requirements**:
   - `event` setter requires `event` property
   - `custom_event` setter requires both `event` and `custom_code`
   - `custom` setter requires `custom_impl`
4. **Method requirements**: Must have either `inline` OR `impl`
5. **Hook requirements**: Must have either `inline` OR `impl`
6. **Type validity**: Types should be valid C++ types (not strictly validated)

---

## Best Practices

1. **Use event setters by default** for scriptable fields
2. **Mark internal fields** as `readable: false, writable: false`
3. **Document complex fields** with `doc` property
4. **Prefer inline methods** for simple getters
5. **Use custom setters sparingly** - only when event setters can't handle the logic
6. **Group related fields** with comments for readability
7. **Include necessary headers** in `metadata.includes`

---

## See Also

- [COMPONENT_GENERATOR.md](COMPONENT_GENERATOR.md) - Implementation roadmap
- [src/Engine/ECS/component.h](../src/Engine/ECS/component.h) - Base component classes
- [docs/compiler/COMPILER_OVERVIEW.md](compiler/COMPILER_OVERVIEW.md) - PG language overview
