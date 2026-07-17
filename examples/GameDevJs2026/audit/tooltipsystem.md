# Audit: TooltipSystem

**Files**: `/UI/tooltipsystem.h` (137 lines), `/UI/tooltipsystem.cpp` (429 lines)
**Total**: ~566 lines

## What's Good

- 200ms hover delay prevents tooltip flicker — good UX.
- `buildContent()` is a pure function that computes tooltip text — clean separation of data from presentation.
- Smart recipe search: prefers currently open machine's recipes, falls back to any recipe.
- `positionTooltip()` handles all 4 edge-clamping cases (right, top, bottom, flip-below-cursor).
- `setLine()` helper combines text, color, and visibility in one call.
- `categoryName()` and `machineLabel()` are clean static helpers.
- `worldSourceTier` handling provides useful context for non-craftable items.

## What's Bad / Code Smells

### 1. `using namespace pg;` in header
### 2. `setEntityVisibility` duplication
### 3. Magic machine type IDs
`5` (Furnace), `6` (Assembler) in `machineLabel()`.

### 4. Undocumented magic expression
`NAME_SCALE * 48.0f` — what is `48.0f`? Presumably the font's nominal pixel height, but entirely opaque.

### 5. Dead parameter
`placeAt()` takes `numBodyLines` but ignores it (`(void)numBodyLines;`).

### 6. Layout gaps from hidden lines
Hidden lines still occupy vertical space. Tooltip appears taller than necessary.

### 7. `snprintf` with small fixed buffers
`char buf[64]`, `char ibuf[48]` — sufficient now but fragile if names get longer. `std::string` would be safer.

### 8. No left-edge clamping
If cursor is near left edge and tooltip flips, `tx` could go negative.

## Could Live in the Engine

The entire `TooltipSystem` is a generic UI pattern. An engine-level tooltip widget that takes structured content and positions itself near the cursor would serve any game.

## Refactoring Suggestions

1. Document or name the `48.0f` font height constant.
2. Remove dead `numBodyLines` parameter.
3. Compact body line layout to skip hidden lines.
4. Add left-edge clamping.
5. Replace `snprintf` with `std::string` formatting.
6. Replace magic machine type IDs.

## Summary Rating

**Well-engineered tooltip with smart content generation, but has layout gaps and a few magic constants.**
