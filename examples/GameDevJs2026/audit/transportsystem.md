# Audit: TransportSystem

**Files**: `/Systems/transportsystem.h` (103 lines), `/Systems/transportsystem.cpp` (453 lines)
**Total**: ~556 lines

## What's Good

- **Sophisticated transport algorithm**: cycle detection, round-robin for contested merge cells, chain propagation for `canMove`, and tail-first application.
- `BeltItemGrid` is a fixed-size 2D array for O(1) spatial lookups — correct for a grid-based system.
- Round-robin `lastServedDir` prevents starvation at merge points.
- Visual position sync on every tick handles externally-placed items (inserters).
- Save/load only persists non-empty cells — efficient.
- `BuildingRemovedEvent` handler returns belt items to player.
- Clean serialization specializations for `SavedBeltItem`.

## What's Bad / Code Smells

### 1. `transportTick()` is 270+ lines
Single monolithic method. Should be decomposed: `syncVisualPositions()`, `collectMoves()`, `detectCycles()`, `resolveContested()`, `propagateCanMove()`, `applyMoves()`.

### 2. `using namespace pg;` in header

### 3. O(n²) revisit detection in `canMove` propagation
`for (size_t idx : chain) if (idx == cur)` is a linear scan. For long belt networks, this matters. An `unordered_set` would be O(1).

### 4. Magic tile ID `4` for belt
Appears ~5 times.

### 5. Repeated item visual sizing
`0.6f * TILE_SIZE` and `ITEM_Y_OFFSET = -2.0f` appear in 4 places — should be named constants or a helper.

### 6. `BeltItemGrid` memory footprint
`64*64 * (sizeof(ItemId) + sizeof(uint64_t))` ≈ 40KB. Fine for 64x64 but a concern for larger grids.

## Could Live in the Engine

The belt/conveyor transport system with merge resolution and cycle handling is a high-value engine feature for any factory/logistics game.

## Refactoring Suggestions

1. Break `transportTick()` into 5-6 named submethods.
2. Use `unordered_set` for cycle detection.
3. Define `BELT_TILE_ID = 4`.
4. Extract item visual sizing into named constants.

## Summary Rating

**The most algorithmically impressive system — correct, handles edge cases, but the monolithic tick method needs structural decomposition.**
