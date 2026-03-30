# ECS Serialization Example

This example demonstrates how to pass ECS entities and components to PgCompiler scripts as global variables (like system modules).

## Files

- **`ecsserialization.h`** - Helper functions for serializing/deserializing ECS entities and components to/from VM tables
- **`ecs_serialization_example.cpp`** - C++ example that creates an entity and passes it to a script
- **`entity_test.pg`** - Script that reads and modifies entity data

## What This Example Does

1. **C++ Side:**
   - Creates an `EntitySystem` and registers `PositionComponentSystem`
   - Creates an entity with a `PositionComponent`
   - Serializes the entity to a VM table using `serializeEntityToTable()`
   - Passes the table as a global variable `playerEntity` to the script
   - Runs the script
   - Reads back modified values using `deserializeTo<>()`

2. **Script Side:**
   - Accesses the `playerEntity` global variable (provided by C++)
   - Reads the `__entityId` field
   - Accesses the `PositionComponent` nested table
   - Reads component properties (x, y, z, width, height, rotation, visible)
   - Calculates derived values (center position)
   - Modifies the position values
   - The modified values are reflected back in the C++ table

## Building and Running

```bash
# Build
cd build
cmake ..
make EcsSerializationExample

# Run the example
./EcsSerializationExample
```

## Expected Output

```
[Example] === ECS Entity as Script Global Example ===

[Example] Creating entity in C++...
[Example] Entity ID: 1
[Example] Position: (100, 200, 0)
[Example] Size: 64 x 64

[Example] Serializing entity to VM table...
[Example] Adding entity as global 'playerEntity' to script...

[Example] Running script...

[Script] === Accessing Entity from Script ===

[Script] Entity ID: 1

[Script] PositionComponent properties:
[Script]   x: 100
[Script]   y: 200
[Script]   z: 0
[Script]   width: 64
[Script]   height: 64
[Script]   rotation: 0
[Script]   visible: true

[Script] Calculating center position...
[Script]   Center: (132, 232)

[Script] Modifying position...
[Script] New position:
[Script]   x: 150
[Script]   y: 225

[Script] === Script Complete ===

[Example] === Reading modified values back to C++ ===
[Example] Modified values from script:
[Example]   x: 150 (was 100)
[Example]   y: 225 (was 200)

[Example] Changes applied to entity!

[Example] === Example Complete ===
```

## How It Works

### Entity Table Structure

When an entity is serialized to a VM table, it has this structure:

```javascript
{
  "__entityId": 1,
  "PositionComponent": {
    "__className": "PositionComponent",
    "x": 100,
    "y": 200,
    "z": 0,
    "width": 64,
    "height": 64,
    "rotation": 0,
    "visible": true,
    "observable": true
  }
}
```

### Accessing in Script

```javascript
// Access entity ID
var entityId = playerEntity["__entityId"];

// Access component
var position = playerEntity["PositionComponent"];

// Access component properties
var x = position["x"];
var y = position["y"];

// Modify values (changes are reflected in the C++ table)
position["x"] = 150;
position["y"] = 225;
```

## API Reference

### Serialization Functions

#### `serializeEntityToTable(vm, ecs, entity)`
Serializes an entire entity with all its components to a VM table.

**Parameters:**
- `vm` - Pointer to the VM
- `ecs` - Pointer to the EntitySystem
- `entity` - Pointer to the Entity to serialize

**Returns:** VM `Value` containing the entity table

---

#### `serializeComponentToTable(vm, ecs, entity, componentId)`
Serializes a single component to a VM table.

**Parameters:**
- `vm` - Pointer to the VM
- `ecs` - Pointer to the EntitySystem
- `entity` - Pointer to the Entity owning the component
- `componentId` - The component type ID

**Returns:** VM `Value` containing the component table

---

#### `serializeToTable<Type>(vm, component)`
Serializes a component object directly to a VM table (template version).

**Parameters:**
- `vm` - Pointer to the VM
- `component` - The component instance to serialize

**Returns:** VM `Value` containing the component table

**Example:**
```cpp
PositionComponent pos;
pos.x = 100;
pos.y = 200;
Value table = serializeToTable<PositionComponent>(vm, pos);
```

---

### Deserialization Functions

#### `deserializeEntityFromTable(vm, ecs, entityTable, createNew)`
Deserializes a VM table to an entity.

**Parameters:**
- `vm` - Pointer to the VM
- `ecs` - Pointer to the EntitySystem
- `entityTable` - VM `Value` containing the entity table
- `createNew` - If true, always creates a new entity

**Returns:** `EntityRef` to the created/updated entity

---

#### `deserializeComponentFromTable(vm, ecs, entity, componentTable, typeName)`
Deserializes a VM table and attaches the component to an entity.

**Parameters:**
- `vm` - Pointer to the VM
- `ecs` - Pointer to the EntitySystem
- `entity` - EntityRef to attach component to
- `componentTable` - VM `Value` containing the component table
- `typeName` - Optional component type name

**Returns:** `bool` indicating success

---

#### `deserializeTo<Type>(vm, table)`
Deserializes a VM table directly to a component object (template version).

**Parameters:**
- `vm` - Pointer to the VM
- `table` - VM `Value` containing the component table

**Returns:** Instance of the component type

**Example:**
```cpp
Value table = ...; // VM table with PositionComponent data
PositionComponent pos = deserializeTo<PositionComponent>(vm, table);
std::cout << "Position: " << pos.x << ", " << pos.y << std::endl;
```

---

### Batch Operations

#### `serializeEntitiesToTable(vm, ecs, entities)`
Serializes multiple entities to a VM table array.

#### `deserializeEntitiesFromTable(vm, ecs, entitiesTable, createNew)`
Deserializes multiple entities from a VM table array.

## Use Cases

1. **Script-Driven Gameplay:**
   - Pass entities to scripts for AI logic
   - Scripts can read and modify entity properties
   - Changes are reflected back in the ECS

2. **Data Inspection:**
   - Debug entities by passing them to scripts
   - Print component values
   - Validate entity state

3. **Configuration:**
   - Load entity configurations from scripts
   - Scripts can create entity tables that get deserialized to C++

4. **Serialization/Save Systems:**
   - Export entities to tables for saving
   - Load entities from saved table data

## Notes

- Tables are passed by reference - modifications in scripts affect the C++ table
- Use `deserializeTo<>()` to read values back to C++ objects
- Component types must have `serialize()` and `deserialize()` functions defined
- The `__className` field identifies component types during deserialization
