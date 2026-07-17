# Audit: BuildingRegistry

**Files**: `/Registries/buildingregistry.h` (42 lines), `/Registries/buildingregistry.cpp` (107 lines)
**Total**: ~149 lines

## What's Good

- **`BuildingDef` is compact and well-documented** — each field has a clear purpose. `gridW/gridH` for footprint, `PlacementMode` for input behavior, `hasDirection` for rotation, `isAnimated` for the render system.
- **`PlacementMode` enum** — clean distinction between `ClickToPlace` and `LineDrag`. Good design for extensibility (could add `AreaDrag`, etc.).
- **`findByTileId` returns nullable pointer** — correct API for optional lookup.
- **Inline comments on every registration** — each building's fields are annotated, making the positional init readable despite the fragility.

## What's Bad / Code Smells

### 1. `get(size_t index)` is by vector index, NOT by tileId
```cpp
const BuildingDef& get(size_t index) const { return buildings[index]; }
```
This is confusing. The conveyor has `tileId = 4` but is at `buildings[0]`. Callers need to know the difference between "slot index" and "tile ID", and the API doesn't make this clear. Contrast with `ItemRegistry::get(ItemId id)` where the ID *is* the index — here they diverge.

### 2. No bounds check on `get()`
Same issue as `ItemRegistry::get()`. UB if index is out of range.

### 3. `findByTileId` is O(n) linear scan
Should use a `std::unordered_map<uint16_t, size_t>` for tileId → index mapping.

### 4. tileIds start at 4 — undocumented gap
tileIds are 4, 5, 6, 7, 8, 9, 10. The range 0-3 is presumably reserved for terrain tiles, but this isn't documented anywhere in this file. A comment or a constant like `FIRST_BUILDING_TILE_ID = 4` would help.

### 5. Inconsistent factory function name
```cpp
BuildingRegistry createDefaultRegistry();     // Missing "Building"
ItemRegistry createDefaultItemRegistry();      // Has "Item"
RecipeRegistry createDefaultRecipeRegistry();  // Has "Recipe"
```
Should be `createDefaultBuildingRegistry()` for consistency.

### 6. Positional aggregate init for 9 fields
```cpp
reg.addBuilding({
    4,                                             // tileId
    "Conveyor",                                    // name
    "Conveyor_Belt",                               // textureName
    {100.0f, 160.0f, 220.0f, 255.0f},             // color
    1, 1,                                          // 1x1
    PlacementMode::LineDrag,                       // drag to draw path
    true,                                          // hasDirection
    true                                           // isAnimated
});
```
The inline comments make this readable, but it's still fragile — adding a field to `BuildingDef` requires updating every entry.

### 7. No named tileId constants
Same problem as item IDs. `tileId == 5` appears across the codebase meaning "Furnace" but there's no `TileId::FURNACE = 5` constant.

### 8. `addBuilding` doesn't validate uniqueness
Two buildings can be registered with the same `tileId`, and `findByTileId` would silently return the first one. No duplicate detection.

## Could Live in the Engine

### 1. Generic typed registry (same as ItemRegistry)
The `vector<Def> + get() + findBy*()` pattern is identical. A single `Registry<T, IdType>` template covers both.

### 2. Placement mode enum
`PlacementMode` is generic enough for any tile/grid-based game.

## Refactoring Suggestions

1. **Rename to `createDefaultBuildingRegistry()`** for consistency.
2. **Add named tileId constants** in a shared header:
   ```cpp
   namespace TileId {
       inline constexpr uint16_t CONVEYOR = 4;
       inline constexpr uint16_t FURNACE = 5;
       // ...
   }
   ```
3. **Add a tileId → index map** for O(1) lookups.
4. **Make `get()` take a tileId** (not a vector index), or rename it to `getByIndex()` to avoid confusion.
5. **Add debug bounds checking** to `get()`.
6. **Document the tileId gap** (0-3 = terrain).

## Summary Rating

**Small, functional, but has an API trap.** The `get()` by index vs lookup by tileId distinction is a foot-gun. The file is small enough that the fragile init isn't a big problem, but the missing tileId constants are felt across the project.
