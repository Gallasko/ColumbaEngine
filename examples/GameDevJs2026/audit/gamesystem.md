# Audit: GameSystem

**Files**: `/Systems/gamesystem.h` (112 lines), `/Systems/gamesystem.cpp` (822 lines)
**Total**: ~934 lines

## What's Good

- Clean separation of concerns: cursor/ghost management, placement validation, line-drag state machine, and click routing.
- `cornerTileMap` and `DIRECTION_TILE_INDEX` are `constexpr` arrays — good compile-time data.
- Line-drag path supports loop truncation, gap-filling for fast mouse movement, and proper corner/variant resolution.
- Ghost preview correctly tints red for invalid placement and adjusts for multi-cell buildings.
- `commitDragPath` has a proper three-phase approach: place, resolve belt variants, update neighbors.
- Building removal returns items to player.

## What's Bad / Code Smells

### 1. Monster constructor — 14 parameters, all raw pointers
Worst constructor in the codebase. A config struct or dependency injection would help.

### 2. Massive "UI is open?" checks duplicated in 4 places
Lines 28-41, 71-103, 210-215, 234-239. Should be a single `isAnyUIOpen()` method.

### 3. `using namespace pg;` in header

### 4. Magic tile IDs
`5`, `6`, `4`, `8` scattered throughout (~10 occurrences).

### 5. `getMouseGridPos()` called multiple times per click
Each call re-invokes `screenToWorld`. Should compute once and pass down.

### 6. Linear scan for `findInPath`
O(n) per path element. An `unordered_set<uint32_t>` using `machineKey` encoding would be O(1).

### 7. `setX(-1000.0f)` as "hidden" sentinel
Fragile — if camera scrolls to -1000, the entity becomes visible. Should use proper visibility flag.

## Could Live in the Engine

- A `GhostPreview` system (semi-transparent placement preview with validity coloring) is common in builder/strategy games.
- A `LineDragPlacementTool` abstraction could be engine-level.

## Refactoring Suggestions

1. Extract `isAnyUIOpen()` and `closeAllUIs()` helpers.
2. Bundle constructor deps into a `GameSystemDeps` struct.
3. Compute `getMouseGridPos()` once per event.
4. Replace `findInPath` with `unordered_set`.
5. Replace `setX(-1000)` with proper visibility control.
6. Extract click-on-building-open-UI into a dispatch table.

## Summary Rating

**The brains of the game — complex but well-organized. Needs refactoring for the 14-param constructor and duplicated UI-open checks.**
