# Audit: WorldFacts

**Files**: `/Core/worldfacts.h` (161 lines), `/Core/worldfacts.cpp` (104 lines)
**Total**: ~265 lines

## What's Good

- **Event-driven architecture.** Facts are modified via `AddFact`, `RemoveFact`, `IncreaseFact` events. Decouples fact producers from the fact store.
- **Batched change notification.** Changes accumulate in `changedFacts` during the frame and are broadcast as a single `WorldFactsUpdate` event in `execute()`.
- **Template helpers** (`getFact<T>`, `setFact<T>`, `setFactIfNotExists<T>`) provide a type-safe, ergonomic API.
- **Serialization integrated** via `SaveSys`, making facts persist across saves.
- **`FactChecker`** provides declarative condition testing without procedural code.
- **Exception safety** — `IncreaseFact` handler catches exceptions from type mismatches.

## What's Bad / Code Smells

### 1. `printf` in save/load
Production code should use the engine's logging system, not raw `printf`.

### 2. `changedFacts` has no deduplication
If the same fact is set 100 times in one frame, 100 entries are pushed. An `unordered_set` would be cleaner.

### 3. Manual copy constructors that do nothing special
`IncreaseFact` and `FactChecker` manually define copy constructor/assignment that are identical to compiler-generated defaults. This suppresses move operations (Rule of Five violation). Delete them or use `= default`.

### 4. `factMap` and `factMetadata` are public
Any system can bypass the event pipeline and mutate them directly, breaking change-tracking invariants.

### 5. `WorldFactsUpdate` contains a raw pointer to `factMap`
If a listener stores this pointer past the frame, it becomes dangling if the map is reallocated.

### 6. `"Noop"` sentinel string in `IncreaseFact`
Undocumented and could collide with a real fact. Should be a named constant.

## Could Live in the Engine

**The entire WorldFacts system.** This is a generic key-value fact store with event-driven updates, serialization, and a declarative checker. Zero game-specific logic. Should be promoted to `pg::FactSystem`.

## Refactoring Suggestions

1. Replace `printf` with engine logging.
2. Deduplicate `changedFacts` with `unordered_set<string>`.
3. Delete manual copy constructors (use `= default` or omit).
4. Make `factMap` and `factMetadata` private.
5. Replace raw pointer in `WorldFactsUpdate` with const reference.

## Summary Rating

**Well-architected system that clearly belongs in the engine** — needs cleanup of boilerplate copy constructors, raw pointers, and printf logging.
