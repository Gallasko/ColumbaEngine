# GameDevJs2026 Full Code Audit — Summary

**Total**: ~15,500 lines across 72 files (35 file pairs + 2 standalone)

---

## Files Audited (31 audit files)

| # | File | Lines | Rating |
|---|------|-------|--------|
| 1 | [tutorialsystem](tutorialsystem.md) | 278 | Solid, fragile magic numbers |
| 2 | [craftingui](craftingui.md) | 1,418 | Well-structured, UI boilerplate bloat |
| 3 | [itemregistry](itemregistry.md) | 166 | Clean API, fragile data registration |
| 4 | [buildingregistry](buildingregistry.md) | 149 | Small, has API trap (get by index vs tileId) |
| 5 | [inventory](inventory.md) | 125 | **Excellent — best file in the project** |
| 6 | [reciperegistry](reciperegistry.md) | 325 | Good design, unreadable recipes |
| 7 | [application](application.md) | 331 | Functional but monolithic |
| 8 | [main](main.md) | 13 | Clean entry point |
| 9 | [canvasgenerator](canvasgenerator.md) | 289 | One of the cleanest files |
| 10 | [grid](grid.md) | 85 | **Critical: no bounds checks** |
| 11 | [gridatlas](gridatlas.md) | 38 | Belongs in engine |
| 12 | [machinekey](machinekey.md) | 10 | Correct, needs decode function |
| 13 | [terrain](terrain.md) | 103 | Clean enum, magic item IDs |
| 14 | [worldfacts](worldfacts.md) | 265 | Well-architected, belongs in engine |
| 15 | [saveserialization](saveserialization.md) | 331 | Functional, needs versioning |
| 16 | [autosavesystem](autosavesystem.md) | 33 | Clean and simple |
| 17 | [camerasystem](camerasystem.md) | 273 | Good math, missing bounds |
| 18 | [craftingsystem](craftingsystem.md) | 404 | Good logic, magic numbers |
| 19 | [depotsystem](depotsystem.md) | 120 | Compact, duplicated pattern |
| 20 | [gamesystem](gamesystem.md) | 934 | Complex but well-organized |
| 21 | [gridsystem](gridsystem.md) | 1,073 | Most important system, DRY violation |
| 22 | [handcraftingsystem](handcraftingsystem.md) | 226 | Clean event-driven design |
| 23 | [insertersystem](insertersystem.md) | 587 | Polished, extensibility concern |
| 24 | [manualmining](manualmining.md) | 654 | Excellent UX polish |
| 25 | [minersystem](minersystem.md) | 299 | Clean producer |
| 26 | [missionsystem](missionsystem.md) | 336 | Good progression, hardcoded data |
| 27 | [playerinventory](playerinventory.md) | 114 | Simple, mutable accessor hole |
| 28 | [storagesystem](storagesystem.md) | 108 | Duplicate of depotsystem |
| 29 | [transportsystem](transportsystem.md) | 556 | Most algorithmically impressive |
| 30 | [demoscenarios](demoscenarios.md) | 170 | Good data, magic numbers |
| 31+ | [depotui](depotui.md), [hotbarsystem](hotbarsystem.md), [hudbarsystem](hudbarsystem.md), [inventoryui](inventoryui.md), [machinedemosystem](machinedemosystem.md), [machineui](machineui.md), [minerui](minerui.md), [missionui](missionui.md), [storageui](storageui.md), [toolbarsystem](toolbarsystem.md), [tooltipsystem](tooltipsystem.md) | ~5,000 | Various — see individual files |

---

## Top 10 Issues by Impact

| # | Issue | Severity | Scope |
|---|-------|----------|-------|
| 1 | **No bounds checks on `Grid::getCell`/`getLayer`** | Critical | Called from 9+ files, UB on bad input |
| 2 | **Magic item/tile IDs everywhere** | High | 20+ files, ~100+ occurrences |
| 3 | **`using namespace pg;` in headers** | High | All 26+ headers pollute every includer |
| 4 | **`setEntityVisibility` duplicated in 9 UI files** | High | ~45 lines of pure copy-paste |
| 5 | **UI entity creation boilerplate** | High | Every UI file spends 40%+ on entity setup |
| 6 | **`placeBuilding`/`restoreBuilding` 90% duplication** | Medium | gridsystem.cpp, ~160 lines duplicated |
| 7 | **Inserter type-switches on tileId** | Medium | insertersystem.cpp, extensibility blocker |
| 8 | **No save format versioning** | Medium | saveserialization.cpp, time bomb |
| 9 | **4 UI files broken on window resize** | Medium | hudbarsystem, toolbarsystem, machinedemosystem, missionui |
| 10 | **Snake_case conversion duplicated 3 times** | Low | craftingsystem, handcraftingsystem, playerinventory |

---

## What Should Move to the Engine

### High Priority (used by every game)
1. **`setEntityVisibility(ecs, entityId, bool)`** — trivial utility, eliminates 9 copies
2. **`GridAtlas` → `pg::GridAtlasLoader`** — generic sprite sheet chopper, zero game logic
3. **`WorldFacts` → `pg::FactSystem`** — generic key-value fact store with events, serialization, and FactChecker
4. **`XorShift32` → `pg::LocalRng`** — lightweight seedable local RNG
5. **`LambdaCallable` adapter** — bridges std::function to engine's AbstractCallable
6. **UI camera creation helper** — 5-line boilerplate every game with a HUD needs

### Medium Priority (common patterns)
7. **`Inventory` class** — zero game-specific dependencies, ready to lift as-is
8. **Autotile algorithm** — 9-tile/47-tile with fixpoint pass, common 2D game need
9. **World-space progress bar** — health bars, build progress, loading indicators
10. **Tooltip widget** — hover delay, structured content, edge-clamping, cursor-following
11. **Anchored panel builder** — fluent API for creating anchored UI rectangles/text
12. **Scrollable list widget** — row pooling, scroll offset, scrollbar drag

### Lower Priority (game-genre-specific)
13. **Belt/conveyor transport system** — merge resolution, cycle handling (factory games)
14. **Recipe/crafting framework** — inputs/outputs/time/unlock conditions
15. **`MachineSystemBase<T>` template** — register/unregister/save/load pattern for machines
16. **Step-based tutorial framework** — define steps as data, event-driven advancement, persist progress

---

## Recommended Fix Order

### Phase 1: Quick wins (< 1 day, high impact)
1. Create `itemids.h` + `tileids.h` with named constants — fixes magic numbers project-wide
2. Add `assert` to `Grid::getCell`/`getLayer` — prevents UB
3. Move `using namespace pg;` from all headers to .cpp files
4. Extract `setEntityVisibility` to a shared `UiHelpers.h`
5. Extract `toSnakeCase()` to a shared utility

### Phase 2: Structural refactoring (1-3 days)
6. Merge `placeBuilding`/`restoreBuilding` in gridsystem
7. Add `Listener<ResizeEvent>` to hudbar, toolbar, machinedemosystem, missionui
8. Add save format versioning to saveserialization
9. Break `application.cpp` setup into sub-functions
10. Decompose `transportTick()` into named submethods

### Phase 3: Engine promotion (ongoing)
11. Move `GridAtlas`, `WorldFacts`, `Inventory`, `XorShift32`, `LambdaCallable` to engine
12. Build `UiPanelBuilder` for anchored entity creation
13. Build `ScrollableListView` engine widget
14. Build `WorldProgressBar` engine component
15. Define `IItemSource`/`IItemSink` interfaces for inserter extensibility
