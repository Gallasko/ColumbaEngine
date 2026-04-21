# Audit: HandCraftingSystem

**Files**: `/Systems/handcraftingsystem.h` (89 lines), `/Systems/handcraftingsystem.cpp` (137 lines)
**Total**: ~226 lines

## What's Good

- Clean event-driven design: `HandCraftRequest` starts, `HandCraftCancel` refunds, `HandCraftCompletedEvent` notifies.
- Input consumption is immediate on start, with refund on cancel — correct Factorio-style pattern.
- `canCraft` checks both unlock conditions and ingredient availability.
- `isUnlocked` gracefully handles null `worldFacts`.
- Progress ratio clamped to [0, 1].
- `TICK_RESOLUTION_MS` named constant.

## What's Bad / Code Smells

### 1. `using namespace pg;` in header
### 2. Duplicated snake_case conversion
Lines 119-128 — identical to `craftingsystem.cpp` and `playerinventory.cpp`.

### 3. Raw pointer to recipe
`activeRecipe` is a raw pointer into `recipeRegistry->recipes` vector. If the registry is ever resorted or reallocated, this dangles. Storing the index would be safer.

### 4. Empty `init()` override
Not harmful but slightly noisy.

## Could Live in the Engine

The "start craft → tick timer → complete → emit event" loop is generic enough for any crafting/research system.

## Refactoring Suggestions

1. Use recipe index instead of raw pointer.
2. Extract the snake_case utility.

## Summary Rating

**Clean, well-designed event-driven crafting system. Minor pointer safety concern.**
