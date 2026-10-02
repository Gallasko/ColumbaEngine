# Component Proxy System

How a PgScript script reads and writes C++ ECS components. This describes the code as it is in
`src/Engine/Compiler/`, not a design.

## Table of Contents

- [Overview](#overview)
- [Source Files](#source-files)
- [What a Script Sees](#what-a-script-sees)
- [How a Property Access Runs](#how-a-property-access-runs)
- [Metadata](#metadata)
- [Creating Proxies](#creating-proxies)
- [Attaching Components from a Script](#attaching-components-from-a-script)
- [Table Copies](#table-copies)
- [Limits](#limits)
- [Tests and Benchmarks](#tests-and-benchmarks)

## Overview

A script never gets a copy of a component. It gets a **proxy**: a VM instance that holds a pointer to the
live C++ component and a pointer to the **metadata** of its type. Every property access on the proxy is
forwarded to the component:

- A **read** calls the property getter and returns the current C++ value.
- A **write** calls the component setter, so change events fire exactly as they do from C++.

Nothing is cached on the script side, so a proxy always shows the current state of the component.

## Source Files

| File | Content |
|---|---|
| `src/Engine/Compiler/componentproxy.h` | `PropertyMetadata`, `ComponentProxyMetadata`, `ComponentProxyRegistry`, `ComponentProxy` |
| `src/Engine/Compiler/componentattach.h` | `ComponentAttachRegistry`, `REGISTER_COMPONENT_ATTACH_HANDLER`, the `attachComp` native |
| `src/Engine/Compiler/tableserialization.h` | Copies between C++ objects and script tables: `serializeToTable`, `deserializeTo` |
| `src/Engine/Compiler/ecsserialization.h` | Entity tables: `serializeEntityToTable`, `serializeEntityViewToTable`, `deserializeEntityFromTable` |
| `src/Engine/Compiler/vm_struct_op.cpp` | The property opcodes, with the proxy fast path |
| `tools/component_generator.pg` | Emits the metadata of every `.pgcomp` component |

`ecsserialization.h` includes the three other headers, so including it is enough.

## What a Script Sees

An entity is a table. Each component is stored under its type name:

```javascript
var entity = ecs.getEntity(id)

var pos = entity["PositionComponent"]

// Read: calls PositionComponent::getX()
var x = pos.x

// Write: calls PositionComponent::setX(), which sends PositionComponentChangedEvent
pos.x = x + 10

// Natives bound to the entity
if (entity.has("Velocity"))
{
    entity.attachComp("Collision", "layerId", 1)
}
```

The entity table holds:

| Field | Value |
|---|---|
| `__entityId` | Id of the entity |
| `attachComp` | Native that attaches a component to this entity |
| `has` | Native that tells if this entity has a component |
| One field per component | A proxy, or a table copy if the type has no metadata |

Rules of a proxy:

- Assignment is the only way to write: there are no `setX()` methods.
- An **unknown property** reads as `-1`, and a write to it is ignored.
- A write to a **read-only** property stops the script with a runtime error.
- `proxy.__className` gives the component type name.
- A script defined component (a `StandardComponent`) accepts any property name: its properties are
  looked up in the component at access time.

## How a Property Access Runs

A proxy is an `ObjInstance` of the `ComponentProxy` class with no field. It uses two members of
`ObjInstance` (`src/Engine/Compiler/object.h`):

- `proxyTarget`: the C++ component, as a `void*`.
- `proxyMeta`: the `ComponentProxyMetadata` of its type. It is null on every instance that is not a proxy.

`op_get_property_decoded` and `op_set_property_decoded` test `proxyMeta` first:

```
pos.x
  -> op_get_property_decoded
       instance->proxyMeta != nullptr
  -> ComponentProxy::getProperty(vm, instance, "x")
       metadata->findProperty("x")
  -> property->getter(instance->proxyTarget, vm)
```

There is no field lookup, no metamethod lookup, no string created for the property name and no native
call on that path. `ComponentProxyMetadata::findProperty` is the only step that depends on the property
name.

The `ComponentProxy` class still has `__get` and `__set` native methods. They serve the accesses that do
not go through the property opcodes, such as `proxy["x"]`, and they call the same
`ComponentProxy::getProperty` and `ComponentProxy::setProperty`.

A name that starts with `__` is never forwarded to the component: it addresses the instance itself.

## Metadata

`ComponentProxyMetadata` describes one component type:

| Member | Role |
|---|---|
| `componentTypeName` | Key in the registry |
| `properties` | One `PropertyMetadata` per property, in declaration order |
| `retriever` | Fetches the component of an entity: `void* (*)(EntitySystem*, _unique_id)` |
| `dynamicGetter`, `dynamicSetter` | Fallbacks for properties only known at runtime (`StandardComponent`) |

`PropertyMetadata` describes one property:

| Member | Role |
|---|---|
| `name`, `type`, `writable` | `type` is only used by the editor inspector to pick a widget |
| `getter`, `setter` | Script access, `Value` in and out |
| `sGetter`, `sSetter` | String access, used by the editor inspector |

All accessors are plain function pointers. The metadata is registered once, at static initialization
time, in `ComponentProxyRegistry`. A registered entry is never moved, so proxies keep a pointer to it.

For a `.pgcomp` component, `tools/component_generator.pg` writes the registration in
`<Name>.serialization.cpp`. Do not write it by hand for a generated component. For a hand-written
component the registration looks like this:

```cpp
struct HealthProxyMetadataRegistrar
{
    HealthProxyMetadataRegistrar()
    {
        ComponentProxyMetadata metadata;

        metadata.componentTypeName = "Health";

        metadata.retriever = [](EntitySystem* ecs, _unique_id entityId) -> void* {
            return ecs->getComponent<Health>(entityId);
        };

        metadata.addProperty(PropertyMetadata{"hp", PropertyType::Int, true,
            [](void* component, VM*) -> Value {
                return makeIntValue(static_cast<Health*>(component)->hp);
            },
            [](void* component, VM*, Value value) {
                static_cast<Health*>(component)->setHp(static_cast<int>(AS_INT(value)));
            },
            nullptr,
            nullptr});

        ComponentProxyRegistry::instance().registerMetadata(metadata);
    }
};

static HealthProxyMetadataRegistrar healthProxyMetadataRegistrar;
```

`ComponentProxy::registerWithVM` registers the `StandardComponent` entry, which has no property list
and uses the dynamic accessors.

## Creating Proxies

`serializeEntityToTable(vm, ecs, entity)` builds the entity table. For each component of the entity:

1. The type name is read from the `ComponentRegistry`.
2. The metadata is looked up by name. This is the only lookup by type name in the life of the proxy.
3. The component pointer comes from `metadata->retriever`, or from the `StandardComponent` storage of
   that name.
4. `ComponentProxy::createProxy(vm, metadata, component)` creates the instance and sets its two pointers.

If the type has no metadata, the script gets a table copy instead (see [Table Copies](#table-copies)).

The other entry points build the same proxies:

- `serializeEntityToTable(vm, ecs, compList)` takes a `CompList` and only exposes its components. The
  module factories (`createTexture`, ...) use it.
- `serializeEntityViewToTable(vm, ecs, entityId, names)` only exposes the named components and adds no
  native. The entity loop lowering pass uses it through `__ecsEntityView`.
- `serializeComponentToTable(vm, ecs, entity, componentId)` gives the proxy of one component.

## Attaching Components from a Script

`attachComp(name, key, value, ...)` looks the name up in `ComponentAttachRegistry`:

- With a handler, the component is built with its real C++ type. Generated components register one with
  `REGISTER_COMPONENT_ATTACH_HANDLER`, under the short name of the component (`Position` for
  `PositionComponent`).
- Without a handler, a `StandardComponent` of that name is attached, with the key / value pairs as
  properties.

## Table Copies

`serializeToTable(vm, object)` copies any object that has a `serialize()` function into a new table, and
`deserializeTo<Type>(vm, table)` builds an object back from a table. A copy is a snapshot: writing to
it does not change the C++ object. System events are passed to scripts this way
(`src/Engine/ECS/system.cpp`).

In a copy, a vector becomes a table indexed by `"0"`, `"1"`, ..., a map becomes a table indexed by its
keys, and an `ElementType` is flattened to the value it holds.

## Limits

- **Lifetime**: a proxy does not own its component and does not know when it is destroyed. A script
  must not keep a proxy across the removal of its entity or component.
- **Lookup by name**: `findProperty` compares the property name against each property of the type. A
  decoded instruction has no room for an inline cache (`DecodedInstruction` is fixed at 24 bytes).
- **Entity tables are eager**: every component of the entity gets its proxy when the table is built, and
  the `attachComp` and `has` natives are created per entity. Use `serializeEntityViewToTable` when only
  some components are needed.

## Tests and Benchmarks

- `test/componentproxy.cc` (target `t1`): proxy reads and writes, read-only and unknown properties,
  index access, entity tables, entity views and table copies.
- `benchmark/componentproxy.cc` (target `bench`): script read, script write, proxy creation and entity
  table creation, each next to a loop baseline.

```bash
make t1
./t1 --gtest_filter=component_proxy_test.*:table_serialization_test.*

make bench
./bench --gtest_filter=component_proxy_benchmark.*
```
