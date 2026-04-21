# Audit: Application

**Files**: `/application.h` (24 lines), `/application.cpp` (307 lines)
**Total**: ~331 lines

## What's Good

- **Clean separation of concerns.** `GameApp` owns the engine and the three registries, then delegates everything to a setup lambda. The header is minimal — five members, three methods.
- **Explicit system creation order.** Comments explain why camera must precede grid, why WorldFacts must precede HandCraftingSystem, etc. This makes the initialization dependency graph readable.
- **UI camera created inline** with a named scope block limiting the lifetime of the temporary pointer.

## What's Bad / Code Smells

### 1. God function — the setup lambda is ~250 lines
Creates every system in the game with no intermediate grouping (`initAssets()`, `initSystems()`, `initUI()`). A single typo or reorder breaks the entire game.

### 2. `GameSystem` constructor takes 14 parameters
This is a code smell for a "god system." Any system that needs references to the grid, camera, hotbar, three different UIs, the registries, and the transport layer is doing too much or is a thin dispatch layer that could be replaced by events.

### 3. Hardcoded resource paths
Strings like `"res/ext/Automation Components/Conveyor_Belt.json"` are scattered through the lambda. If the asset directory moves or an asset is renamed, every path must be hand-edited. A data-driven manifest or `constexpr` path constants would help.

### 4. No error handling on asset loads
If `loadAnim()` or `registerAtlasTexture()` fails, the setup lambda keeps running. There are no null checks, no try/catch, no early-abort.

### 5. Magic atlas dimensions
Values like `384, 48, 48, 48, 8, 8` are inline with only a comment. These should be defined alongside the atlas or extracted from the Aseprite JSON automatically.

### 6. Empty destructor
`GameApp::~GameApp()` is empty — either remove the explicit definition (Rule of Zero) or document why it exists.

## Could Live in the Engine

- **Atlas registration boilerplate.** Every `registerAtlasTexture` call follows the same pattern. A helper like `window.masterRenderer->registerGridAtlas("Crate", "res/.../Crate.png", 16, 16, 1, 1)` would cut every call to one line.
- **UI camera creation.** The 5-line block to create a UI camera entity, attach `BaseCamera2D`, set dimensions, and register it is generic boilerplate every game with a HUD needs.

## Refactoring Suggestions

1. **Extract sub-functions**: `loadAtlases(ecs, window)`, `createCoreSystems(ecs, ...)`, `createUISystems(ecs, ...)`.
2. **Data-driven asset manifest** — replace the 20+ `registerAtlasTexture` calls with a loop over a static table or JSON.
3. **Reduce `GameSystem` parameter count** — use a config struct or have it listen to engine events.
4. **Delete the empty destructor.**

## Summary Rating

**Functional bootstrap that does its job but is a single monolithic blob** — splitting asset loading from system wiring would make it significantly more maintainable.
