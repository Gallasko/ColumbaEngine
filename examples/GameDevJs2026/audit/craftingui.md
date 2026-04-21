# Audit: CraftingUISystem

**Files**: `/UI/craftingui.h` (276 lines), `/UI/craftingui.cpp` (1,142 lines)
**Total**: ~1,418 lines — **the largest UI file in the project**

## What's Good

### Architecture
- **Dual-mode design** (hand-craft vs. machine) is handled cleanly with `activeMachineType`. The same panel, rows, and scrolling work for both modes with minimal branching.
- **Anchoring system usage** — the panel is anchored to the inventory backdrop and the window, so it repositions correctly on resize. This is the engine's anchor system used well.
- **Lazy panel creation** (`ensurePanelCreated`) — entities aren't created until the panel is first opened. Good for startup performance.
- **Callback-based machine integration** — `machineFeedCallback` and `machineSelectCallback` decouple the crafting UI from the machine system. Clean inversion of control.
- **Tab system** — `CraftTab` enum with `classifyRecipe()` is a clean way to filter recipes. Tab highlighting and switching is straightforward.
- **Row pooling** — only `VISIBLE_ROWS` (6) row entities exist; data is swapped in via `refreshRows()`. This is the right approach for a scrollable list with potentially many items.
- **`stripCraftingPrefix`** — thoughtful UX detail, stripping "Hand-Craft ", "Smelt ", etc. from displayed names to save space.
- **Double-click detection** for machine feeding is correctly implemented with timestamp + row tracking.
- **Scrollbar dragging** — full implementation with track click, thumb dragging, and proportional thumb sizing.

### Code Quality
- **All layout constants are `constexpr`** — easy to tweak and no magic numbers for dimensions.
- **`RowTint` enum** — named tint states instead of inline color values in the tinting logic.
- **Good use of `QueuedListener`** for input events — prevents input processing from happening mid-frame.

## What's Bad / Code Smells

### 1. Massive `createPanel()` — 280 lines of entity creation (lines 399–678)
This is the core problem. `createPanel()` creates ~30+ entities (backdrop, title, 4 tabs, 6 rows × 7 entities each, scrollbar track+thumb, progress bar bg+fill, 2 buttons × 2 entities each). Each entity requires 4-6 lines of boilerplate:
```cpp
auto bg = makeSimple2DShape(ecsRef, Shape2D::Square, 0.0f, 0.0f, color);
auto bgPos = bg.get<PositionComponent>();
bgPos->setZ(98.0f);
bgPos->setWidth(w);
bgPos->setHeight(h);
bg.get<Simple2DObject>()->setViewport(UI_VP);
someId = bg.entity->id;
auto anchor = ecsRef->attach<UiAnchor>(bg.entity);
anchor->setLeftAnchor(...);
anchor->setTopAnchor(...);
```
This pattern repeats ~15 times. A builder/factory for "anchored UI rectangle" or "anchored text label" would cut this in half.

### 2. `refreshRows()` is 170 lines and does too much (lines 750–922)
It rebuilds visible recipes, positions icons, sets text, computes tints, shows/hides demo buttons, and refreshes the scrollbar. This should be split into smaller functions.

### 3. Magic tile IDs for machine types
```cpp
(activeMachineType == 5) ? "Furnace" : (activeMachineType == 6) ? "Assembler" : "Craft"
```
Same problem as the tutorial system. `5` and `6` are opaque. The building registry should provide display names.

### 4. `using namespace pg;` in header
Same issue as tutorialsystem.h — pollutes every includer's namespace.

### 5. Heavy constructor — 7 parameters
```cpp
CraftingUISystem(HandCraftingSystem*, RecipeRegistry*, ItemRegistry*,
                 PlayerInventorySystem*, WorldFacts*, InventoryUISystem*,
                 float screenWidth, float screenHeight)
```
This is a lot of dependencies. Consider whether some could be looked up from the ECS or passed via a config struct.

### 6. `isClickOnPanel` queries entity positions at runtime
```cpp
auto bdPos = bdEnt->get<PositionComponent>();
float px = bdPos->getX();
...
return x >= px and x <= px + pw and y >= py and y <= py + ph;
```
This is repeated in `rowAtPosition`, `tabAtPosition`, `getScrollTrackRect`, and the demo button hit test. There's no shared hit-testing utility.

### 7. Colors still hardcoded in `createPanel()`
Backdrop: `{20, 20, 30, 220}`, row bg: `{50, 50, 60, 200}`, tab bg: `{50, 50, 60, 200}`, craft button: `{70, 130, 80, 240}`, cancel button: `{130, 70, 70, 240}`, etc. These should come from a theme or palette.

### 8. `setEntityVisibility` duplicated from tutorial system
Exact same function body. Should be shared.

### 9. `init()` is empty
```cpp
void init() override {}
```
If it does nothing, don't override it (unless the base class requires it — in which case the engine should provide a default).

### 10. `onProcessEvent(const OnMouseRelease&)` ignores its parameter
```cpp
void CraftingUISystem::onProcessEvent(const OnMouseRelease& event)
{
    draggingScrollbar = false;
}
```
Minor, but `event` is unused. Could use `const OnMouseRelease&` without naming it.

## Could Live in the Engine

### 1. Scrollable list widget
The row pooling + scroll offset + scrollbar track/thumb + mouse wheel + drag-to-scroll pattern is generic. The engine could provide a `ScrollableListView` that:
- Manages a pool of N visible row entities
- Handles scroll offset, mouse wheel, scrollbar drag
- Calls a user-provided callback to populate each row
- Handles show/hide

This would eliminate ~300 lines from this file alone, and the same pattern appears in `/UI/inventoryui.cpp`, `/UI/storageui.cpp`, `/UI/depotui.cpp`.

### 2. Anchored panel builder
A fluent API for creating anchored UI elements:
```cpp
auto panel = UiPanelBuilder(ecsRef)
    .size(width, height)
    .color({20, 20, 30, 220})
    .anchorLeftTo(otherId, AnchorType::Right, margin)
    .verticalCenterIn(windowId)
    .viewport(UI_VP)
    .build();
```
Would replace the 8-line boilerplate pattern that repeats in every UI file.

### 3. Hit-test utility
`isPointInRect` and the pattern of "get entity position, check if point is inside" is used in every UI system. The engine should provide `bool hitTest(EntityRef, float x, float y)`.

### 4. `setEntityVisibility` as engine utility
Already mentioned in the tutorial audit. This is duplicated in nearly every UI file.

### 5. Tab bar widget
The tab creation + highlight switching + hit testing pattern could be a reusable `TabBar` component.

## Refactoring Suggestions

1. **Extract `createPanel()` into smaller functions**: `createBackdrop()`, `createTabs()`, `createRowSkeletons()`, `createScrollbar()`, `createProgressBar()`, `createButtons()`. Each would be 30-50 lines instead of one 280-line wall.

2. **Extract row refresh logic**: Split `refreshRows()` into `refreshRowContent(rowIdx, recipe)`, `refreshRowIngredients(rowIdx, recipe)`, `refreshRowDemoButton(rowIdx, recipe)`, and `refreshRowTint(rowIdx, recipe)`.

3. **Replace magic tile IDs** with named constants or registry lookups for display names.

4. **Move `using namespace pg;`** to the `.cpp` file.

5. **Introduce a shared `UiHelper` namespace/class** with:
   - `setEntityVisibility(EntitySystem*, uint64_t, bool)`
   - `hitTestEntity(EntitySystem*, uint64_t, float x, float y)`
   - `makeAnchoredRect(...)` / `makeAnchoredText(...)`

6. **Consider a config struct** for the constructor instead of 7 parameters:
   ```cpp
   struct CraftingUIConfig {
       HandCraftingSystem* handCrafting;
       RecipeRegistry* recipeRegistry;
       // ...
   };
   ```

## Summary Rating

**Functionally complete and well-structured at the system level, but suffering from UI boilerplate bloat.** The dual-mode design, callback decoupling, row pooling, and scroll implementation are all good. The main issue is that ~40% of the code is repetitive entity creation and positioning that should be abstracted. The `createPanel()` and `refreshRows()` methods are too long. This file is the strongest candidate for engine-level UI abstractions (scrollable list, panel builder, hit testing).
