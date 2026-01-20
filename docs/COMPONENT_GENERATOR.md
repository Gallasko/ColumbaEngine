# Component Generator System

**Status**: Design Phase
**Last Updated**: 2026-01-20

## Table of Contents
1. [Overview](#overview)
2. [Component Definition Schema](#component-definition-schema)
3. [Setter Levels](#setter-levels)
4. [Generated Files](#generated-files)
5. [Missing VM Functions](#missing-vm-functions)
6. [Implementation Roadmap](#implementation-roadmap)

---

## Overview

The Component Generator System automates the creation of ECS components from declarative YAML definitions (`.pgcomp` files). This eliminates boilerplate and ensures consistency across:

- Component struct definitions
- Setters with event notifications
- VM proxy schemas (for script access)
- Archive serialization
- Script attach handlers

**Key Principle**: Everything is defined in the `.pgcomp` file. All generated code can be safely regenerated at any time.

---

## Component Definition Schema

### File Format: `.pgcomp` (YAML)

```yaml
component: ComponentName

# Metadata
metadata:
  category: "string"           # Optional: Component category (e.g., "2D", "UI", "Game")
  implements: Ctor|Dtor|Component|[Ctor, Dtor]|none  # Base class inheritance
  doc: "string"                # Optional: Component documentation
  includes:                    # Optional: Additional headers to include
    - "header1.h"
    - "header2.h"

# Fields
fields:
  - name: fieldName
    type: cppType              # C++ type (float, int, bool, std::string, etc.)
    default: defaultValue      # Default value (use quotes for strings)
    writable: true|false       # Can scripts write to this field?
    readable: true|false       # Can scripts read this field? (default: true)
    setter: none|basic|event|custom_event|custom  # Setter level (see below)
    event: EventTypeName       # Event to send (for event/custom_event setters)
    custom_code: |             # Additional code before assignment (custom_event)
      // C++ code here
    custom_impl: |             # Full implementation (custom setter)
      // C++ code here
    doc: "string"              # Optional: Field documentation

# Methods
methods:
  - name: methodName
    returns: returnType        # Return type
    params: [type1 param1, type2 param2]  # Optional: Parameters
    inline: "code"             # For simple one-liners (goes in header)
    impl: |                    # For complex methods (goes in cpp)
      // C++ code here

# Lifecycle Hooks
hooks:
  - name: onCreation|onDeletion
    params: [EntityRef entity]
    inline: |                  # Inline implementation (short)
      // C++ code
    impl: |                    # Full implementation (complex logic)
      // C++ code

# Code Generation Options
generation:
  header: true|false           # Generate .generated.h
  cpp: true|false              # Generate .generated.cpp
  proxy: true|false            # Generate proxy schema for VM
  serialization: true|false    # Generate Archive serialization
  attach_handler: true|false   # Generate script attach handler
```

---

## Setter Levels

### Level 0: No Setter (Direct Access)
```yaml
- name: x
  type: float
  default: 0.0
  writable: true
  # No setter field
```

**Generated**: Field only, scripts access directly via proxy.

---

### Level 1: Basic Setter
```yaml
- name: rotation
  type: float
  default: 0.0
  writable: true
  setter: basic
```

**Generated**:
```cpp
void setRotation(float value) {
    rotation = value;
}
```

---

### Level 2: Event Setter
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
void setWidth(float value) {
    if (this->width != value) {
        this->width = value;
        if (ecsRef) {
            ecsRef->sendEvent(PositionComponentChangedEvent{id});
        }
    }
}
```

**Requirements**: Component must have `ecsRef` and `id` fields.

---

### Level 3: Custom Event Setter
```yaml
- name: wrap
  type: bool
  default: false
  writable: true
  setter: custom_event
  event: EntityChangedEvent
  custom_code: |
    LOG_THIS("TTF Text");
    changed = true;
```

**Generated**:
```cpp
void setWrap(bool value) {
    if (this->wrap != value) {
        LOG_THIS("TTF Text");
        changed = true;
        this->wrap = value;
        if (ecsRef) {
            ecsRef->sendEvent(EntityChangedEvent{entityId});
        }
    }
}
```

**Use Case**: When you need to execute additional code (logging, state updates) before sending the event.

---

### Level 4: Fully Custom Setter
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
void setX(float value);

// In .cpp
void PositionComponent::setX(float value) {
    if (x != value) {
        x = value;
        updateAnchoredChildren();
        if (ecsRef) {
            ecsRef->sendEvent(PositionComponentChangedEvent{id});
        }
    }
}
```

**Use Case**: Complex logic that doesn't fit the event setter pattern.

---

## Generated Files

For each component, the generator produces up to 5 files:

```
Generated/
├── ComponentName.generated.h          # Header with declarations
├── ComponentName.generated.cpp        # All implementations
├── ComponentName.proxy.generated.cpp  # VM proxy schema
├── ComponentName.serialization.generated.cpp  # Archive serialization
└── ComponentName.attach.generated.cpp # Script attach handler
```

All files are marked with `// AUTO-GENERATED - DO NOT EDIT` and can be safely regenerated.

---

## Missing VM Functions

To implement the generator in PG script, the following VM functions need to be added:

### File Module (`import "file"`)

| Function | Signature | Description |
|----------|-----------|-------------|
| `writeFile` | `writeFile(filename: string, content: string) -> bool` | Write content to file, returns success |
| `fileExists` | `fileExists(filename: string) -> bool` | Check if file exists |
| `readFile` | (existing) | Read entire file as string |

**Implementation Location**: [src/Engine/Files/filemodule.h](../src/Engine/Files/filemodule.h)

---

### String Module (`import "string"`)

| Function | Signature | Description |
|----------|-----------|-------------|
| `startsWith` | `startsWith(str: string, prefix: string) -> bool` | Check if string starts with prefix |
| `endsWith` | `endsWith(str: string, suffix: string) -> bool` | Check if string ends with suffix |
| `contains` | `contains(str: string, substring: string) -> bool` | Check if string contains substring |
| `charAt` | `charAt(str: string, index: int) -> string` | Get character at index |
| `substring` | `substring(str: string, start: int, end: int) -> string` | Extract substring |
| `replace` | `replace(str: string, from: string, to: string) -> string` | Replace all occurrences |
| `join` | `join(array: vector, separator: string) -> string` | Join array elements with separator |
| `capitalize` | `capitalize(str: string) -> string` | Capitalize first letter |
| `toLowerCase` | `toLowerCase(str: string) -> string` | Convert to lowercase |
| `toUpperCase` | `toUpperCase(str: string) -> string` | Convert to uppercase |
| `trim` | `trim(str: string) -> string` | Remove leading/trailing whitespace |
| `indexOf` | `indexOf(str: string, substring: string) -> int` | Find first occurrence (-1 if not found) |
| `toString` | (existing) | Convert value to string |
| `strlen` | (existing) | Get string length |
| `split` | (existing) | Split string by delimiter |
| `splitLines` | (existing) | Split string by newlines |

**Implementation Location**: [src/Engine/Helpers/stringmodule.h](../src/Engine/Helpers/stringmodule.h)

---

### Args Module (`import "args"` or add to `sys`)

| Function | Signature | Description |
|----------|-----------|-------------|
| `getArg` | `getArg(index: int) -> string` | Get command-line argument by index |
| `getArgCount` | `getArgCount() -> int` | Get number of command-line arguments |

**Implementation Location**: New module [src/Engine/Helpers/argsmodule.h](../src/Engine/Helpers/argsmodule.h) or add to existing `sysmodule.h`

---

### Vector/Array Helper Functions

| Function | Signature | Description |
|----------|-----------|-------------|
| `push` | `array.push(value)` | Add element to end (existing via vector methods?) |
| `length` or `size` | `length(array) -> int` | Get array size |

**Note**: Check if these already exist via vector operations.

---

## Implementation Roadmap

### Phase 1: Add Missing VM Functions (Estimated: 2-3 hours)
- [ ] Implement string functions in `stringmodule.h`
- [ ] Implement file write functions in `filemodule.h`
- [ ] Implement args module
- [ ] Test each function with simple PG scripts

### Phase 2: Write YAML Parser in PG (Estimated: 2-3 hours)
- [ ] Create `tools/pgcompgen/yaml_parser.pg`
- [ ] Support key-value pairs
- [ ] Support lists (lines starting with `-`)
- [ ] Support nested structures (indentation)
- [ ] Support multi-line strings (`|`)
- [ ] Support comments (`#`)
- [ ] Test with example `.pgcomp` files

### Phase 3: Write Code Generators (Estimated: 4-6 hours)
- [ ] Create `tools/pgcompgen/generators/header_gen.pg`
- [ ] Create `tools/pgcompgen/generators/cpp_gen.pg`
  - [ ] Level 1: Basic setter
  - [ ] Level 2: Event setter
  - [ ] Level 3: Custom event setter
  - [ ] Level 4: Fully custom setter
- [ ] Create `tools/pgcompgen/generators/proxy_gen.pg`
- [ ] Create `tools/pgcompgen/generators/serialization_gen.pg`
- [ ] Create `tools/pgcompgen/generators/attach_gen.pg`

### Phase 4: Main Generator Script (Estimated: 1-2 hours)
- [ ] Create `tools/pgcompgen/pgcompgen.pg`
- [ ] Parse command-line arguments
- [ ] Load and parse `.pgcomp` file
- [ ] Call all generators
- [ ] Write output files
- [ ] Add error handling and validation

### Phase 5: CMake Integration (Estimated: 1 hour)
- [ ] Add custom command to run generator
- [ ] Scan for `.pgcomp` files
- [ ] Add generated files to build
- [ ] Ensure proper dependency tracking

### Phase 6: Create Base Infrastructure (Estimated: 3-4 hours)
- [ ] Create `src/Engine/Compiler/component_proxy.h`
- [ ] Implement `ComponentProxyMetadata` struct
- [ ] Implement `ComponentProxyRegistry`
- [ ] Implement `ObjComponentProxy` object type
- [ ] Add `OP_Get_Proxy_Property_Fast` opcode
- [ ] Add `OP_Set_Proxy_Property_Fast` opcode

### Phase 7: Iterator-Based getEntities (Estimated: 2-3 hours)
- [ ] Modify `getEntities()` to return lazy iterator
- [ ] Implement iterator protocol in VM
- [ ] Generate proxies on-demand during iteration

### Phase 8: Test & Migrate (Estimated: 2-3 hours)
- [ ] Create simple test component (Health)
- [ ] Migrate PositionComponent to `.pgcomp`
- [ ] Migrate TTFText to `.pgcomp`
- [ ] Test performance improvements
- [ ] Profile asteroid update script

---

## Example Component Definitions

### Simple Component: Health.pgcomp
```yaml
component: Health

metadata:
  category: "Game"
  implements: Component

fields:
  - name: current
    type: int
    default: 100
    writable: true
    setter: event
    event: HealthChangedEvent

  - name: maximum
    type: int
    default: 100
    writable: true
    setter: event
    event: HealthChangedEvent

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
  - name: isDead
    returns: bool
    inline: "return current <= 0;"

  - name: isFullHealth
    returns: bool
    inline: "return current >= maximum;"

  - name: getHealthPercent
    returns: float
    inline: "return maximum > 0 ? (float)current / maximum : 0.0f;"

generation:
  header: true
  cpp: true
  proxy: true
  serialization: true
  attach_handler: true
```

### Complex Component: PositionComponent.pgcomp
See [Component Definition Schema](#component-definition-schema) section for full example.

---

## Notes

- The generator is written in PG script itself (`pgcompgen.pg`), demonstrating the language's capability for metaprogramming
- All generated files include a header: `// AUTO-GENERATED - DO NOT EDIT`
- The `.pgcomp` file is the single source of truth
- Custom setter implementations (`custom_impl`) are embedded directly in the `.pgcomp` file
- The system supports incremental migration: existing components can be migrated one at a time

---

## References

- [Component Definition Schema](#component-definition-schema)
- [Setter Levels](#setter-levels)
- [src/Engine/ECS/component.h](../src/Engine/ECS/component.h) - Base component classes
- [src/Engine/Compiler/ecsserialization.h](../src/Engine/Compiler/ecsserialization.h) - Current manual serialization
- [docs/compiler/COMPILER_OVERVIEW.md](compiler/COMPILER_OVERVIEW.md) - PG language overview
