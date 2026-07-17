# Audit: TutorialSystem

**Files**: `/UI/tutorialsystem.h` (79 lines), `/UI/tutorialsystem.cpp` (199 lines)
**Total**: ~278 lines

## What's Good

- **Clean step-driven state machine** — `StepDef` array with title/body pairs is data-driven and easy to extend. Adding a step is a one-line addition to the table.
- **Event-driven progression** — each step listens for a specific event type to advance. This leverages the ECS event system cleanly and keeps the tutorial decoupled from the systems it monitors.
- **`pendingAdvance` pattern** — deferring mutation to `execute()` instead of inside event handlers avoids reentrancy issues. Good discipline.
- **Persistence via WorldFacts** — tutorial progress survives save/load with `worldFacts->getFact<int>("tutorial_step", 0)`. Simple and effective.
- **Lazy creation** (`ensureCreated`) — panel isn't built until the system runs, avoiding initialization-order bugs.
- **Good use of `constexpr`** — all layout constants are compile-time.
- **Clean separation** — the system only knows about events and WorldFacts/RecipeRegistry. It doesn't reach into game systems to query state.

## What's Bad / Code Smells

### 1. Magic item/tile IDs everywhere
```cpp
if (currentStep == 1 && (event.id == 15 || event.id == 4))   // wood? stone?
if (currentStep == 4 && (event.id == 1 || event.id == 3))    // coal? iron?
if (output.id == 29)                                           // Stone Pickaxe
if (event.tileId == 5)                                         // Furnace
```
These are completely opaque. If registries change order, this silently breaks. Should use named constants from the registries or string lookups.

### 2. Step-to-condition mapping is implicit
The connection between step index (0, 1, 2…) and which `onEvent` overload checks for it is scattered across 5 different handlers. If you reorder `STEPS[]`, you must hunt through all handlers to fix indices. A table-driven approach (step index → condition predicate) would be safer.

### 3. `TickEvent` listener is dead code
The class inherits `Listener<TickEvent>` and has an empty `onEvent(const TickEvent&)` with a comment about future use. This adds event dispatch overhead for nothing.

### 4. No bounds check on `STEPS[]` access
`STEPS[currentStep]` in `ensureCreated()` and `updateContent()` trusts that `currentStep < TOTAL_STEPS`. If `currentStep` were corrupted (e.g. bad save data), this is undefined behavior.

### 5. `TOTAL_STEPS = 9` is manually maintained
Must match the array size of `STEPS[]`. Should be derived:
```cpp
static constexpr int TOTAL_STEPS = std::size(STEPS);  // or sizeof/sizeof[0]
```
Note: `std::size` on a static member array of incomplete class requires C++17 and the array being `constexpr` or the size being computed outside the class. Alternatively, derive it at the usage site.

### 6. Hardcoded colors
`{255, 210, 80, 255}` for gold, `{210, 210, 210, 255}` for grey, `{15, 15, 25, 200}` for backdrop — should come from a theme or at least be named constants.

### 7. `using namespace pg;` in a header
Pollutes the namespace of every file that includes `tutorialsystem.h`. Should be removed from the header and added only in the `.cpp`.

## Could Live in the Engine

### 1. Generic notification/toast panel
The backdrop + title + body pattern (create, show/hide, update text) is a common UI element. The engine could provide an `InfoPanel` or `ToastNotification` widget that handles entity creation, positioning, visibility toggling, and text updates. The tutorial system would just call `panel->show("Title", "Body")`.

### 2. Step-based progression framework
The pattern of "define steps as data, listen for events to advance, persist progress" is common in tutorials, quests, and achievements. A base class or utility could handle step management, persistence, and panel rendering — leaving subclasses to define step data and completion predicates.

### 3. `setEntityVisibility` helper
This exact function (`lookup entity by ID, toggle PositionComponent visibility`) is duplicated in almost every UI file in the project. It belongs in a utility or as a method on the ECS/entity wrapper. See also: `/UI/craftingui.cpp`, `/UI/inventoryui.cpp`, `/UI/machineui.cpp`, etc.

## Refactoring Suggestions

1. **Replace magic IDs with named constants** — e.g. `ItemId::WOOD`, `ItemId::STONE_PICKAXE`, `TileId::FURNACE`. These could live in a shared header or be queryable from the registries.
2. **Remove the `Listener<TickEvent>` inheritance** — it's unused. Remove both the inheritance and the empty handler.
3. **Derive `TOTAL_STEPS` from array size** — eliminates a manual sync point.
4. **Move `using namespace pg;` to the `.cpp`** — keep headers clean.
5. **Extract `setEntityVisibility` to a shared utility** — it's duplicated project-wide.

## Summary Rating

**Solid for a jam game, fragile in maintenance.** The architecture and patterns are clean. The main risk is the magic IDs — this file will silently break if registries are edited. The `using namespace` in the header is a hygiene issue worth fixing regardless.
