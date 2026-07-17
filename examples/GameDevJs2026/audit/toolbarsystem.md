# Audit: ToolbarSystem

**Files**: `/UI/toolbarsystem.h` (55 lines), `/UI/toolbarsystem.cpp` (164 lines)
**Total**: ~219 lines

## What's Good

- Creates its own UI camera — important bootstrap that other systems depend on.
- Clean slot selection with keyboard (1-9) and mouse click.
- Textured vs colored square slot creation for the building registry.
- Selection highlight positioning is clean.

## What's Bad / Code Smells

### 1. `using namespace pg;` in header
### 2. `UI_VIEWPORT = 2` is a global-scope `inline constexpr`
Every other file defines its own `UI_VP = 2` as a class constant. This one escapes to global scope and pollutes all includers.

### 3. No resize handling
`screenWidth`/`screenHeight` set in constructor, toolbar positioned absolutely. Window resize → toolbar at wrong position.

### 4. `printf` debug logging
### 5. `ecsRef->_attach<BaseCamera2D>` uses underscore API
Looks like a private API — stands out.

## Could Live in the Engine

- UI camera creation (`createUICamera`) is a one-time bootstrap every game with 2D UI needs. Should be engine-level.
- `UI_VIEWPORT = 2` should be a single engine-level constant.

## Refactoring Suggestions

1. Move `UI_VIEWPORT` into a shared constants header or engine config.
2. Add `Listener<ResizeEvent>` and anchor the backdrop.
3. Use engine logging instead of `printf`.

## Summary Rating

**Works as bootstrap UI but is the most outdated file — missing resize support and using global-scope constants.**
