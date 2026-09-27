# How a theme toggle crashed the engine: `get<>` poisoning and event-listener timing

Notes from a real debugging session in ColumbaEngine, September 2026. Two engine
lessons came out of one crash: a subtle `unordered_map::operator[]` bug in
`Entity::get<Comp>()` that made entities lie about their components, and a
timing rule for choosing `Listener` vs `QueuedListener` when an event handler
reads entities created the same frame. Source material for a tutorial/article —
every snippet below is from the actual code.

---

## The symptom

Chronicle (the UI kit example) has a `PaintSystem`: every themable entity
carries a `PaintComponent{token}` naming a colour token ("ink", "vellum", …),
and on `ThemeChangedEvent` the system re-resolves each token against the new
theme and calls `setColors` on whatever paintable component the entity has
(`TTFText`, `Simple2DObject`, or `IconComponent`).

Pressing **T** (toggle theme) segfaulted, reliably, in a scene that contained
**no icons at all**:

```
Thread 8 "Chronicle" received signal SIGSEGV, Segmentation fault.
#0  pg::IconComponent::setColors(pg::constant::Vector4D const&)
#1  chronicle::PaintSystem::applyColour(pg::EntityRef, std::string const&)
#2  chronicle::PaintSystem::repaintAll()
```

The paint code looked textbook-defensive:

```cpp
if (ent->has<IconComponent>())
{
    ent->get<IconComponent>()->setColors(colour);   // <- crashed here
}
```

`has<>` guarded the `get<>`. How does that dereference null?

## The false lead: threading

The backtrace showed the crash on a taskflow worker thread, and the repaint ran
from `execute()` (deferred via a dirty flag set in `onEvent`). First hypothesis:
a data race — `execute()` runs concurrently with the systems that own
`TTFText`/`IconComponent`, so we're reading half-written component pools.

Moving the repaint into the synchronous event dispatch didn't fix it. A
headless reproduction (fakeStart + layouts + repaint) passed. The threading
story was plausible and wrong — worth remembering that a crash on a worker
thread doesn't mean the *cause* is concurrency.

## The smoking gun

Instrumenting the paint pass printed this for **every** painted entity:

```
applyColour: entity=219 token=ink-muted has[ttf=1 simple2d=0 icon=1]
entity 219 has<IconComponent> but get<IconComponent> is empty
```

Every text entity, every shape, all of them: `has<IconComponent>() == true`,
`get<IconComponent>()` empty. The entities were lying, uniformly. That pattern
— a false positive on *every* entity for a component *nothing* in the scene
uses — points at shared state, not scene state.

## Root cause: `operator[]` in the `get<>` fast path

`Entity` keeps two structures:

```cpp
std::unordered_set<_unique_id> componentList;                 // flushed components
std::unordered_map<_unique_id, void*> pendingComponents;      // attached-while-running + cache
```

and `get<Comp>()` had this fast path:

```cpp
// Fast path: entity is fully initialized, return the cached pointer directly
if (initialized)
    return CompRef<Comp>(static_cast<Comp*>(pendingComponents[componentId]), id, ecsRef, true);
```

`pendingComponents[componentId]` is `std::unordered_map::operator[]`. On a
miss, **`operator[]` default-constructs and inserts the value** — here a null
`void*` — and returns it. So calling `get<IconComponent>()` on an entity that
has no icon:

1. returns a `CompRef` wrapping `nullptr` (bad but survivable), **and**
2. permanently inserts `{iconTypeId, nullptr}` into that entity's
   `pendingComponents` (the real damage).

Step 2 matters because `has<>` consults the same map:

```cpp
inline bool has(const _unique_id& otherId) const noexcept
{
    return componentList.find(otherId) != componentList.end()
        or pendingComponents.find(otherId) != pendingComponents.end();  // <- entry exists, value ignored
}
```

`find` sees the null entry and reports the component present. From that moment
the entity claims `has<IconComponent>() == true` forever, with a null component
behind it.

The poisoning was invisible in normal operation because the engine's idiom is
`if (auto* c = ent->get<Comp>())` — a probe that null-checks the result. Some
per-frame render code probes every UI entity for `IconComponent` that way. The
probe *worked* (null returned, branch skipped) while silently poisoning every
entity it touched. Then `PaintSystem` came along using the *other* idiom —
`has<>` guard, then unchecked `get<>` — and dereferenced the lie.

**One bad map access, two idioms that were each fine alone, and the failure
appears in a third system far from the cause.**

### The fix

Never `operator[]` on a lookup. `find`, skip null entries, fall through to the
flushed-pool path:

```cpp
// Fast path: the pointer is cached in pendingComponents. Use find, never operator[]:
// operator[] inserts a null entry on a miss, which makes this return a null component
// AND makes has<Comp>() report a false positive forever after (has() consults
// pendingComponents too). A miss here must fall through to the flushed-pool lookup.
const auto pending = pendingComponents.find(componentId);
if (pending != pendingComponents.end() and pending->second)
    return CompRef<Comp>(static_cast<Comp*>(pending->second), id, ecsRef, initialized);

// Normal path: component is flushed and lives in the sparse set pool
const auto& it = componentList.find(componentId);
if (it != componentList.end())
    return CompRef<Comp>(ecsRef->registry.retrieve<Comp>()->getComponent(id), id, ecsRef, initialized);
```

## The fix broke something else: the shape of "not found"

After the fix, the app crashed **at scene load** instead. New lesson: the old
fast path returned `CompRef(nullptr, id, ecsRef, initialized=true)` on a miss.
The fixed code returned a default-constructed `CompRef()` — null `component`,
null `ecsRef`, `initialized=false`. Same "empty" meaning, different shape.

`CompRef`'s accessors lazily re-fetch when not initialized:

```cpp
Comp* CompRef<Comp>::operator->()
{
    if (initialized)
        return component;
    // lazy re-fetch:
    auto comp = ecsRef->getComponent<Comp>(entityId);   // <- ecsRef is null. Boom.
    ...
}
```

Every probing caller (`if (auto* c = ent->get<Comp>())`) used to receive
`initialized=true` + null component → `operator Comp*` returned null → the
`if` handled it. Now they received `initialized=false` + null `ecsRef` → the
lazy re-fetch dereferenced null before the caller ever saw the pointer.

Fix: an empty ref must be safe to probe —

```cpp
// An empty ref (no ECS behind it) has nothing to re-fetch from; probing it
// must yield null, not a null-ecsRef dereference.
if (not ecsRef)
    return component;
```

**When you fix an error path, you inherit its callers' expectations about what
"error" looks like.** The old bug had shaped a de-facto contract.

## The log-level footnote

The miss path logged `LOG_ERROR("Entity doesn't have component: 70")`. Once
misses actually reached that line (the old fast path returned before it),
per-frame probes flooded the console. Probing is legitimate; a miss is not an
error. Downgraded to mile level and added the type name (an id alone is
useless — ids are assigned in registration order and vary run to run):

```cpp
LOG_MILE("Entity", "Entity " << id << " doesn't have component: " << componentId
    << " (" << ecsRef->registry.getComponentTypeName(componentId) << ")");
```

## Part two: `Listener` vs `QueuedListener` for the repaint

Independent of the `get<>` bug, the repaint handler had a timing question worth
its own section.

The ECS offers two ways for a system to receive an event:

- **`Listener<E>`** — `onEvent(const E&)` is invoked *synchronously inside
  `sendEvent`*, on whatever thread sent the event, at whatever moment mid-frame
  the sender happened to be.
- **`QueuedListener<E>`** — `onEvent` (generated) pushes the event on the
  system's queue; `onProcessEvent(const E&)` runs when the queue drains inside
  the system's own `execute()` slot in the taskflow — i.e. at a deterministic
  point of the frame, after the command dispatcher has flushed the previous
  frame's entity/component creations.

The repaint walks *other systems'* components (`TTFText`, `Simple2DObject`,
`IconComponent`) for every painted entity. Two reasons it belongs in
`onProcessEvent`:

1. **Creation timing.** An entity created the same frame the theme changes
   (scene load + immediate `ThemeChangedEvent`, or a label spawned in the same
   handler that toggles the theme) may not be committed yet when a synchronous
   `Listener` fires. Its components are still in flight through the command
   dispatcher. A `QueuedListener` handler runs a frame later, after the flush —
   everything it reads is real.
2. **Execution context.** `onProcessEvent` runs in the system's own task slot,
   so the ECS scheduler orders it against other systems like any `execute()`.
   A synchronous `Listener` runs in the *sender's* stack — for an input-driven
   event that's the input thread, mid-something-else.

The final system:

```cpp
struct PaintSystem : public pg::System<pg::Own<PaintComponent>,
                                       pg::QueuedListener<ThemeChangedEvent>>
{
    void onProcessEvent(const ThemeChangedEvent&) { repaintAll(); }

    void repaintAll()
    {
        // We own PaintComponent, so a view over it is the whole working set.
        for (auto* paint : view<PaintComponent>())
        {
            auto entity = ecsRef->getEntity(paint->entityId);
            if (entity)
                applyColour(entity, paint->token);
        }
    }
    ...
};
```

Note the second simplification: the first version registered a group
(`registerGroup<PaintComponent>` + `addOnGroup`/`removeOfGroup` callbacks
mirroring ids into a `std::set`). Groups are for joining components across
owners. A system that **owns** the component already has the authoritative set —
`view<PaintComponent>()` — with no bookkeeping to keep consistent.

## Regression tests

Three contracts, pinned in `test/ecssystem.cc` (pre-start and running, since
the crash fired from a running system):

```cpp
auto entity = ecs.createEntity();
ecs.attachGeneric<A>(entity, 1, 2);

// 1. A failed lookup returns empty...
EXPECT_TRUE(entity->get<B>().empty());

// 2. ...and must not have created a phantom B on the entity.
EXPECT_FALSE(entity->has<B>());

// 3. Probing the empty ref must yield null, not crash — callers write
//    `if (auto* b = ent->get<B>())` and rely on a plain null coming back.
B* raw = entity->get<B>();
EXPECT_EQ(raw, nullptr);
EXPECT_EQ(entity->get<B>().operator->(), nullptr);

// The component the entity does have is still reachable after the miss.
auto a = entity->get<A>();
ASSERT_FALSE(a.empty());
EXPECT_EQ(a->value, 3);
```

## Takeaways

1. **Query paths must be read-only.** A `get`/`has`/lookup that mutates state
   (even "just a cache") turns every observer into a mutator. The C++-specific
   trap: `map::operator[]` inserts; on any lookup path, write `find`.
2. **Two shared structures answering one question will disagree.** `has<>` and
   `get<>` both consulted `pendingComponents` but interpreted a null entry
   differently (present vs empty). If a null entry can exist, every reader
   needs the same convention — better, make the entry impossible.
3. **A guarded-then-unchecked idiom (`has` then `get`) is only as sound as the
   consistency between the two calls.** Either API contract works — but
   document which one holds.
4. **Fixing an error path changes its shape.** Callers had adapted to the buggy
   return (`initialized=true`, null pointer, no log). Audit what "not found"
   returns *and* what every accessor of that return does — the lazy re-fetch
   dereferenced a null `ecsRef` that could never be null before.
5. **Don't log errors on legitimate probes in hot paths**, and log type names,
   not just runtime-assigned ids.
6. **Event handlers that read other systems' entities want `QueuedListener`.**
   Synchronous `Listener` runs mid-frame on the sender's thread, possibly
   before same-frame entities are committed; `onProcessEvent` runs at the
   system's scheduled slot after the flush.
7. **If you own the component, iterate your view.** Group callbacks mirroring
   ids into a set are redundant bookkeeping for an `Own`ed type.
8. **Debugging note:** the uniform false positive ("every entity has an icon")
   was the pivot from "scene bug" to "engine bug". When instrumentation shows
   the impossible happening *uniformly*, suspect the query mechanism itself.

## Timeline of fixes (commits on `Chronicle`)

- `86a43a4d` — engine: `find` instead of `operator[]` in `Entity::get<>`,
  null-safe empty `CompRef`, mile-level miss log, regression tests.
- `5c74a0a5` — Chronicle: `PaintSystem` → `QueuedListener` over its owned view
  (plus an unrelated Label wrap-width fix from the same session).
