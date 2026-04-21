# Audit: CraftingSystem

**Files**: `/Systems/craftingsystem.h` (88 lines), `/Systems/craftingsystem.cpp` (316 lines)
**Total**: ~404 lines

## What's Good

- Named constants for tick/anim durations.
- Clean state machine: pull inputs → start craft → advance timer → check output → deposit or stall.
- Proper return of items to player on building removal.
- Emits `crafted_` and `discovered_` facts for mission/recipe unlock gating — well thought out.
- Supports locked recipes via `recipeRegistry->findMatchingRecipe()`.

## What's Bad / Code Smells

### 1. Magic tile IDs
`5` (Furnace) and `6` (Assembler) scattered throughout (~10 occurrences). Should be named constants.

### 2. `using namespace pg;` in header

### 3. Duplicated snake_case conversion
Lines 231-238: exact same name-to-snake-case loop appears in `handcraftingsystem.cpp` and `playerinventory.cpp`. Should be a shared utility.

### 4. Hardcoded texture names
`"Stone_Furnace_Active"`, `"Assembler_Machine_1_Running"` — should come from a machine definition, not be inline strings.

### 5. Hardcoded sprite sizes
Furnace active sprite offset `-16.0f` and height `64.0f` vs idle `48.0f`. If art changes, code must change in multiple places. A `MachineTypeDef` struct with anim frames, textures, and sprite dimensions would be cleaner.

## Could Live in the Engine

The tick-accumulator + craft-timer pattern is generic. An `AccumulatedTickSystem` base class would deduplicate this across all systems.

## Refactoring Suggestions

1. Extract `FURNACE_TILE_ID` and `ASSEMBLER_TILE_ID` as constants.
2. Create a `MachineTypeDef` holding anim properties.
3. Extract `toSnakeCase()` into a shared utility.
4. Move sprite size changes into a helper method.

## Summary Rating

**Functional and well-sequenced logic, but heavily marred by magic numbers and duplicated string logic.**
