# Audit: HudBarSystem

**Files**: `/UI/hudbarsystem.h` (67 lines), `/UI/hudbarsystem.cpp` (130 lines)
**Total**: ~197 lines

## What's Good

- Small and focused — just 3 buttons.
- Named button indices (`BTN_INVENTORY`, `BTN_MISSIONS`, `BTN_SETTINGS`).
- Mission button visibility driven by `WorldFacts` — clean data-driven approach.
- `updateMissionButtonVisibility()` has a guard against redundant updates.

## What's Bad / Code Smells

### 1. `using namespace pg;` in header
### 2. No resize handling — BROKEN ON RESIZE
`screenWidth`/`screenHeight` are set in constructor but never updated. Does not listen to `ResizeEvent`. Button positions are computed once in `createButtons()` and cached forever. Window resize → buttons at wrong position.

### 3. Buttons are not anchored
Unlike the hotbar, positions are absolute. Inconsistent with the rest of the UI.

### 4. `setVis` lambda — yet another copy

## Could Live in the Engine

A generic `IconButtonBar` widget (horizontal row of icon buttons with callbacks).

## Refactoring Suggestions

1. **Add `Listener<ResizeEvent>`** and update positions (or anchor them).
2. Remove `using namespace pg;` from header.
3. Consider anchoring buttons to `__MainWindow` top-right corner.

## Summary Rating

**Simple and correct for a static window, but broken on window resize.**
