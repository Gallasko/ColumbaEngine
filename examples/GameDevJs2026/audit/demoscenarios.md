# Audit: DemoScenarios

**Files**: `/UI/demoscenarios.h` (170 lines, header only)
**Total**: ~170 lines

## What's Good

- Clean data-driven approach: scenarios are pure data, separate from simulation logic.
- Structs are simple and focused (`DemoTileDef`, `DemoItemSpawn`, `DemoScenario`).
- `inline` functions avoid ODR violations — correct for header-only design.
- Comments above each scenario describe the layout clearly.

## What's Bad / Code Smells

### 1. Magic tile IDs everywhere
`4` (belt), `5` (furnace), `6` (assembler), `7` (miner), `8` (inserter) appear as bare literals.

### 2. Magic item IDs
`1` (iron ore), `5` (iron plate) — no named constants.

### 3. Magic direction values
`0` = right, etc. A `Direction` enum exists in the project but is not referenced here.

### 4. Heap allocation for compile-time data
`std::vector` members in `DemoScenario` mean every `create*Demo()` call allocates. For scenarios that are essentially constants, `std::array` or `std::initializer_list` would be more efficient.

## Could Live in the Engine

A generic `DemoScenario` + runner system could be engine-level if the engine had a tutorial/demo framework.

## Refactoring Suggestions

1. Replace all magic IDs with named constants from `TileIds.h` / `ItemIds.h`.
2. Replace direction `uint8_t` values with the project's `Direction` enum.
3. Consider `std::array` for fixed-size scenarios.

## Summary Rating

**Solid data definition, but magic numbers make it fragile and hard to maintain.**
