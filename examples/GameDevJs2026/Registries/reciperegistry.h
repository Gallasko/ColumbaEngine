#pragma once

#include "inventory.h"
#include "worldfacts.h"

#include <string>
#include <vector>

#include "Helpers/registry.h"

struct RecipeIngredient
{
    ItemId   id;
    uint16_t count;
};

// Where a recipe is crafted. Each recipe belongs to exactly one category,
// which controls *who* can select it:
//  - HandCraft:  Player-driven, triggered from the inventory UI.
//  - Furnace:    Auto-matched by the Furnace machine (tileId 5).
//  - Assembler:  Auto-matched by the Assembler machine (tileId 6).
//  - AutoCrafter: Player-configurable auto machine (future).
enum class RecipeCategory : uint8_t
{
    HandCraft,
    Furnace,
    Assembler,
    AutoCrafter
};

struct Recipe
{
    std::string name;
    uint16_t    machineType = 0;    // tileId of the machine (5=Furnace, 6=Assembler). 0 for HandCraft/AutoCrafter.
    std::vector<RecipeIngredient> inputs;
    std::vector<RecipeIngredient> outputs;
    size_t      craftTimeMs = 2000;

    RecipeCategory category = RecipeCategory::HandCraft;

    // All checkers must pass against the current fact map for this recipe to
    // be visible/usable. Empty = always unlocked.
    std::vector<pg::FactChecker> unlockConditions;
};

struct RecipeRegistry : public pg::Registry<Recipe>
{
    void addRecipe(const Recipe& recipe) { add(recipe); }

    std::vector<const Recipe*> getRecipesForMachine(uint16_t tileId) const;

    // Find the first recipe whose inputs are satisfiable by the given inventory.
    // Only machine-category recipes are considered here — hand-craft recipes
    // are driven separately by HandCraftingSystem.
    const Recipe* findMatchingRecipe(uint16_t machineType,
                                     const Inventory& inputSlots) const;
};

RecipeRegistry createDefaultRecipeRegistry();
