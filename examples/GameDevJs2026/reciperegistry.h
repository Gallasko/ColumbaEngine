#pragma once

#include "inventory.h"

#include <vector>
#include <string>

struct RecipeIngredient
{
    ItemId   id;
    uint16_t count;
};

struct Recipe
{
    std::string name;
    uint16_t    machineType;                    // tileId of the machine (5=Furnace, 6=Assembler)
    std::vector<RecipeIngredient> inputs;
    std::vector<RecipeIngredient> outputs;
    size_t      craftTimeMs = 2000;
};

struct RecipeRegistry
{
    std::vector<Recipe> recipes;

    void addRecipe(const Recipe& recipe)
    {
        recipes.push_back(recipe);
    }

    std::vector<const Recipe*> getRecipesForMachine(uint16_t tileId) const
    {
        std::vector<const Recipe*> result;
        for (const auto& r : recipes)
            if (r.machineType == tileId)
                result.push_back(&r);
        return result;
    }

    // Find the first recipe whose inputs are satisfiable by the given inventory
    const Recipe* findMatchingRecipe(uint16_t machineType,
                                     const Inventory& inputSlots) const
    {
        for (const auto& recipe : recipes)
        {
            if (recipe.machineType != machineType)
                continue;

            bool allSatisfied = true;
            for (const auto& input : recipe.inputs)
            {
                if (not inputSlots.hasAtLeast(input.id, input.count))
                {
                    allSatisfied = false;
                    break;
                }
            }
            if (allSatisfied)
                return &recipe;
        }
        return nullptr;
    }
};

inline RecipeRegistry createDefaultRecipeRegistry()
{
    RecipeRegistry reg;

    // Item IDs from createDefaultItemRegistry():
    // 1=Iron Ore, 2=Copper Ore, 3=Coal, 4=Stone
    // 5=Iron Plate, 6=Copper Plate, 7=Iron Gear, 8=Copper Wire, 9=Circuit

    // Furnace recipes (machineType = 5)
    reg.addRecipe({"Smelt Iron Plate", 5, {{1, 1}}, {{5, 1}}, 2000});
    reg.addRecipe({"Smelt Copper Plate", 5, {{2, 1}}, {{6, 1}}, 2000});

    // Assembler recipes (machineType = 6)
    reg.addRecipe({"Make Iron Gear", 6, {{5, 2}}, {{7, 1}}, 1000});
    reg.addRecipe({"Make Copper Wire", 6, {{6, 1}}, {{8, 2}}, 500});
    reg.addRecipe({"Make Circuit", 6, {{5, 1}, {8, 3}}, {{9, 1}}, 3000});

    return reg;
}
