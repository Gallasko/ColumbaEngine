# Audit: MachineUI

**Files**: `/UI/machineui.h` (150 lines), `/UI/machineui.cpp` (497 lines)
**Total**: ~647 lines

## What's Good

- Cleanly handles both Furnace (1 input) and Assembler (2 inputs) with the same panel.
- `feedMachineFromPlayer()` validates all ingredients before consuming any — atomic transaction.
- Integrates with `CraftingUISystem` via callbacks — good loose coupling.
- Progress bar updates per-tick for smooth animation.
- "?" demo button integration with `MachineDemoSystem` is a nice touch.

## What's Bad / Code Smells

### 1. `using namespace pg;` in header
### 2. `setEntityVisibility` duplication
### 3. Dead parameters
`refreshItemSlotDisplay` has unused `slotX`, `slotY` parameters.

### 4. Magic number `6` for Assembler
`(openMachineType == 6) ? 2 : 1` appears multiple times.

### 5. "?" button size inconsistency
`20.0f` appears in both `createPanel()` and `onProcessEvent(OnMouseClick)` without a named constant. If one changes, hit testing breaks.

### 6. `getPanelX()` duplication
Same inventory width calculation as depotui, minerui, storageui.

## Could Live in the Engine

A `MachineSlotPanel` with configurable input/output slot counts and a progress bar is entirely generic.

## Refactoring Suggestions

1. Extract `DEMO_BTN_SIZE = 20.0f` as `static constexpr`.
2. Remove dead parameters.
3. Replace `openMachineType == 6` with named constant.
4. Share inventory width calculation.

## Summary Rating

**Functional and well-integrated, but accumulating copies of shared patterns.**
