# Audit: MinerSystem

**Files**: `/Systems/minersystem.h` (86 lines), `/Systems/minersystem.cpp` (213 lines)
**Total**: ~299 lines

## What's Good

- `MINER_TILE_ID = 7` and all timing constants are named.
- `resolveOreUnder` uses majority-vote across the miner's footprint — clever and user-friendly.
- Clean idle/mining state transitions with proper texture switching.
- Items returned to player on removal.
- Save/load correctly restores produced item from terrain.

## What's Bad / Code Smells

### 1. `using namespace pg;` in header
### 2. Hardcoded miner footprint
`resolveOreUnder` hardcodes `W = 2, H = 3` — should reference `BuildingDef::gridW/gridH`.

### 3. C-style arrays for ore vote counting
`candidates[4]` and `counts[4]` — `std::array` or `std::unordered_map` would be cleaner and extensible.

### 4. `producedItem` is runtime-only state without annotation
Re-resolved from terrain on load, but not marked as transient.

## Could Live in the Engine

The "produce item on timer if output has space" pattern is a generic producer that could be parameterized.

## Refactoring Suggestions

1. Get miner footprint from `BuildingDef`.
2. Use `std::array` for ore vote counting.

## Summary Rating

**Clean and simple producer system. Minor hardcoded footprint concern.**
