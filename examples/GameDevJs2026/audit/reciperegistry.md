# Audit: RecipeRegistry

**Files**: `/Registries/reciperegistry.h` (59 lines), `/Registries/reciperegistry.cpp` (266 lines)
**Total**: ~325 lines

## What's Good

- **`RecipeCategory` enum** — clean separation of where recipes are crafted. The comment on the enum explains the design well.
- **`unlockConditions` using `FactChecker`** — flexible progression gating. Recipes can require any combination of world facts (discovered items, crafting milestones, etc.). This is a good data-driven design.
- **`findMatchingRecipe`** — correctly checks all inputs against inventory and filters by machine type and category. The logic is straightforward and correct.
- **Well-commented recipe definitions** — each recipe in `createDefaultRecipeRegistry()` has comments explaining item IDs and unlock conditions. The progression chain is documented inline.
- **`craftTimeMs`** — having craft time as data (not hardcoded per-machine) allows the same recipe to have different times for hand-craft vs. machine.
- **Progression design is thoughtful** — hand-craft versions are slower/costlier than machine versions, creating a natural incentive to automate. Unlock gates are sensible (need coal to smelt, need iron gear to craft circuits, etc.).

## What's Bad / Code Smells

### 1. Magic item IDs in every recipe definition
```cpp
reg.addRecipe({
    "Smelt Iron Plate",
    5, {{1, 1}}, {{5, 1}}, 2000,
    RecipeCategory::Furnace, {}
});
```
`5` is machineType (Furnace), `{{1, 1}}` is "1 Iron Ore", `{{5, 1}}` is "1 Iron Plate". This is nearly unreadable without the comments. With 25+ recipes, this is a significant maintenance burden. Named constants would transform this:
```cpp
reg.addRecipe({
    "Smelt Iron Plate",
    TileId::FURNACE, {{Items::IRON_ORE, 1}}, {{Items::IRON_PLATE, 1}}, 2000,
    RecipeCategory::Furnace, {}
});
```

### 2. `getRecipesForMachine` hardcodes categories
```cpp
if (r.machineType == tileId and
    (r.category == RecipeCategory::Furnace or r.category == RecipeCategory::Assembler))
```
This won't work for future machine types (the `AutoCrafter` category already exists in the enum) without editing this function. Should just check `r.machineType == tileId && r.category != RecipeCategory::HandCraft`, or better, derive the category from the machine type.

### 3. `findMatchingRecipe` returns the FIRST match — order-dependent
If two recipes with different outputs share the same inputs and machine type, whichever is registered first wins. This is a subtle design constraint that isn't documented. Currently not a problem (all recipes have unique input sets per machine), but it's fragile.

### 4. `recipes` is a public `std::vector` — no encapsulation
Any system can directly iterate, mutate, or resize the recipe list. `CraftingUISystem`, `HandCraftingSystem`, and others all access `recipeRegistry->recipes[i]` directly. This means the registry can't add indexing, caching, or validation without touching every consumer.

### 5. Positional aggregate init for `Recipe`
```cpp
{"Craft Iron Gear", 0, {{5, 2}}, {{7, 1}}, 2500, RecipeCategory::HandCraft, { ... }}
```
7 positional fields, two of which are nested initializer lists. Adding a field to `Recipe` would break every recipe definition.

### 6. `RecipeIngredient` uses bare `ItemId` and `uint16_t`
```cpp
struct RecipeIngredient { ItemId id; uint16_t count; };
```
This is actually fine structurally, but the fact that `id` values are always raw numbers (not named constants) makes recipe definitions opaque.

### 7. FactChecker construction is verbose
```cpp
pg::FactChecker{std::string("discovered_iron_plate"), true, pg::FactCheckEquality::Equal}
```
The `std::string(...)` wrapping is needed because the fact value is a variant/any type, but it's noisy. A helper like `pg::factEquals("discovered_iron_plate", true)` would clean this up.

## Could Live in the Engine

### 1. Recipe/crafting framework
The `Recipe` struct (inputs, outputs, craft time, category, unlock conditions) is generic enough for any crafting game. Combined with `Inventory` and `FactChecker`, this could be an engine-level crafting module.

### 2. `findMatchingRecipe` as a utility
"Given a set of available items, find which recipe can be crafted" is a reusable algorithm.

### 3. `RecipeCategory` concept
The idea of categorizing recipes by *where* they're crafted is universal.

## Refactoring Suggestions

1. **Use named item/tile constants** — this is the highest-impact change. Every recipe would go from `{{1, 1}}` to `{{Items::IRON_ORE, 1}}`.
2. **Fix `getRecipesForMachine`** to not hardcode specific categories. Either filter by `machineType != 0` or check `category != HandCraft`.
3. **Make `recipes` private** with a const accessor:
   ```cpp
   const std::vector<Recipe>& all() const { return recipes; }
   ```
4. **Add a `FactChecker` helper** to reduce construction verbosity.
5. **Consider designated initializers** for `Recipe` construction.
6. **Document the first-match behavior** of `findMatchingRecipe`, or add priority/specificity logic.

## Summary Rating

**Good game design, poor readability.** The progression system is well thought out — unlock gates, hand-craft vs. machine tradeoffs, and fact-based conditions are all solid. But the recipe definitions are nearly unreadable due to raw numeric IDs. This file benefits the most from the project-wide "named constants" fix. The `getRecipesForMachine` hardcoded categories are a minor extensibility issue.
