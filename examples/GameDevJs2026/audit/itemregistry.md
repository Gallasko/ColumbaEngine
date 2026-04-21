# Audit: ItemRegistry

**Files**: `/Registries/itemregistry.h` (61 lines), `/Registries/itemregistry.cpp` (105 lines)
**Total**: ~166 lines

## What's Good

- **`ItemId` typedef + `ITEM_NONE` constant** — good foundation. Having a named type for item IDs makes the code self-documenting.
- **`ItemDef` is well-designed** — rich but not bloated. Fields like `worldSourceTier`, `iconWidthRatio`, `miningSpeedMult`, and `description` show thoughtful game design needs baked into the data.
- **`ItemStack`** is clean with `isEmpty()` and `clear()` helpers. Simple value type, easy to reason about.
- **`nameToId` map** — O(1) lookup by name is the right call for a registry.
- **Auto-assigned IDs** — `addItem()` assigns sequential IDs, preventing manual ID management errors. The caller passes `id=0` and gets the real ID back.
- **`findByName` returns pointer** — nullable return for "not found" is appropriate.
- **`ItemCategory` enum** — clean categorization.

## What's Bad / Code Smells

### 1. `get(ItemId id)` has no bounds check
```cpp
const ItemDef& get(ItemId id) const { return items[id]; }
```
If `id >= items.size()`, this is undefined behavior. Every caller trusts that IDs are valid. A debug assert at minimum would catch bad IDs from corrupt saves or recipe typos.

### 2. Null item bypasses `addItem()`
```cpp
// Index 0 is the null item
reg.items.push_back({ITEM_NONE, "None", "", ItemCategory::Resource, 0});
```
This is pushed directly into `items`, bypassing `addItem()`. So `"None"` isn't in the `nameToId` map. Inconsistent — should go through `addItem()` or have a dedicated init path.

### 3. Positional aggregate initialization is extremely fragile
```cpp
reg.addItem({0, "Iron Ore", "Items.8", ItemCategory::Resource, 50, 0, 0, 1.0f, 1.0f, "A raw ore...", 1});
```
`ItemDef` has 11 fields. If a field is added or reordered, every one of these 35+ lines breaks silently (wrong values in wrong fields) or at compile time with confusing errors. C++20 designated initializers or a builder pattern would be much safer:
```cpp
reg.addItem({.name = "Iron Ore", .textureName = "Items.8", .category = ItemCategory::Resource, ...});
```

### 4. No named item ID constants — root cause of project-wide magic numbers
Comments like `// IDs 1-4`, `// IDs 5-8` document the IDs but don't create usable constants. This is the source of every `event.id == 15` and `output.id == 29` scattered across the codebase. A simple addition would fix this:
```cpp
namespace Items {
    inline constexpr ItemId IRON_ORE = 1;
    inline constexpr ItemId WOOD = 15;
    inline constexpr ItemId STONE_PICKAXE = 29;
    // ...
}
```

### 5. `findByBuildingTileId` is O(n)
Linear scan over all items for every lookup. With 35 items this is fine, but a `std::unordered_map<uint16_t, ItemId>` would be trivial to maintain and correct.

### 6. Items are not grouped logically in registration order
IDs 1-4 are raw resources, 5-8 intermediates, 9 is a product, 10-14 are liquids, 15-17 are *more* raw resources, 18-20 are *more* intermediates. The registration order is clearly the order features were added during development, not a deliberate design. This makes the ID comments confusing.

## Could Live in the Engine

### 1. Generic typed registry
The pattern of `vector<DefType>` + auto-ID + `nameToId` map + `get(id)` + `findByName()` is reusable. A `Registry<DefType, IdType>` template in the engine would eliminate boilerplate for items, buildings, recipes, etc.

### 2. `ItemStack` as a reusable primitive
`ItemStack` (id + count + isEmpty/clear) is a universal game concept. Could be templated on the ID type.

## Refactoring Suggestions

1. **Add a shared `itemids.h`** with named constants for every item, generated from or kept in sync with `createDefaultItemRegistry()`. This single change eliminates magic numbers project-wide.
2. **Use C++20 designated initializers** (or a builder) for `ItemDef` construction to prevent positional init bugs.
3. **Add a debug bounds check** to `get()` — at least an `assert(id < items.size())`.
4. **Route the null item through `addItem()`** or explicitly document why it's special.
5. **Add a `tileIdToItem` map** for O(1) `findByBuildingTileId`.

## Summary Rating

**Clean API design, fragile data registration.** The `ItemRegistry` and `ItemDef` types are well-thought-out. The main problems are the fragile positional initialization and the lack of named ID constants — the latter being the single biggest maintainability issue in the entire project.
