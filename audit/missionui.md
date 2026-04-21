# Audit: MissionUI

**Files**: `/UI/missionui.h` (135 lines), `/UI/missionui.cpp` (562 lines)
**Total**: ~697 lines

## What's Good

- The most UI-complex panel: available missions, active missions with progress bars, and a shop section.
- `refresh()` called every frame keeps display up to date with mission progress.
- Button state changes (GO / FULL / LOCKED) with color changes provide good UX feedback.
- `isClickInRect()` is a clean generic helper.
- `setEntityText()` is a useful abstraction not seen in other files.
- Close-on-click-outside behavior is properly implemented.
- `PendingStart` struct for deferred mission start is clean.

## What's Bad / Code Smells

### 1. `using namespace pg;` in header
### 2. `setEntityVisibility` duplication
### 3. Magic item ID `35` for Tickets
### 4. Panel height computed once — never updated
If mission defs change after creation, the panel height is stale.

### 5. Fixed caps with no scroll
`MAX_DEF_ROWS = 5` and `MAX_ACTIVE_ROWS = 3` — excess missions are silently truncated.

### 6. `refresh()` updates ALL entities every frame
Generates unnecessary `setText()` calls for text that hasn't changed. Dirty-flag optimization needed.

### 7. `setPanelVisibility()` enumerates every entity ID
Very verbose and error-prone if elements are added. Should use a container.

### 8. No resize handling

## Could Live in the Engine

- `setEntityText()` helper.
- `isClickInRect()` utility.
- A scrollable list panel with dynamic rows.

## Refactoring Suggestions

1. Store entity IDs in a container so `setPanelVisibility()` can iterate.
2. Add dirty-flag optimization to `refresh()`.
3. Replace magic item ID `35`.
4. Add `Listener<ResizeEvent>`.
5. Add scroll support for excess missions.

## Summary Rating

**Feature-rich and mostly correct, but the most maintenance-heavy file due to verbose entity management and missing resize support.**
