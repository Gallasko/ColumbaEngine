#include "reciperegistry.h"

void RecipeRegistry::addRecipe(const Recipe& recipe)
{
    recipes.push_back(recipe);
}

std::vector<const Recipe*> RecipeRegistry::getRecipesForMachine(uint16_t tileId) const
{
    std::vector<const Recipe*> result;
    for (const auto& r : recipes)
        if (r.machineType == tileId and
            (r.category == RecipeCategory::Furnace or r.category == RecipeCategory::Assembler))
            result.push_back(&r);
    return result;
}

const Recipe* RecipeRegistry::findMatchingRecipe(uint16_t machineType,
                                                 const Inventory& inputSlots) const
{
    for (const auto& recipe : recipes)
    {
        if (recipe.machineType != machineType)
            continue;
        if (recipe.category == RecipeCategory::HandCraft or
            recipe.category == RecipeCategory::AutoCrafter)
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

RecipeRegistry createDefaultRecipeRegistry()
{
    RecipeRegistry reg;

    // Item IDs from createDefaultItemRegistry():
    //   1=Iron Ore, 2=Copper Ore, 3=Coal, 4=Stone
    //   5=Iron Plate, 6=Copper Plate, 7=Iron Gear, 8=Copper Wire, 9=Circuit

    // ===== Machine recipes =====

    // Furnace (machineType = 5)
    reg.addRecipe({
        "Smelt Iron Plate",
        5, {{1, 1}}, {{5, 1}}, 2000,
        RecipeCategory::Furnace, {}
    });
    reg.addRecipe({
        "Smelt Copper Plate",
        5, {{2, 1}}, {{6, 1}}, 2000,
        RecipeCategory::Furnace, {}
    });

    // Assembler (machineType = 6)
    reg.addRecipe({
        "Make Iron Gear",
        6, {{5, 2}}, {{7, 1}}, 1000,
        RecipeCategory::Assembler, {}
    });
    reg.addRecipe({
        "Make Copper Wire",
        6, {{6, 1}}, {{8, 2}}, 500,
        RecipeCategory::Assembler, {}
    });
    reg.addRecipe({
        "Make Circuit",
        6, {{5, 1}, {8, 3}}, {{9, 1}}, 3000,
        RecipeCategory::Assembler, {}
    });

    // ===== Hand-craft recipes =====
    //
    // Unlocked-by-default recipes let the player bootstrap production by
    // hand; fact-gated recipes appear as the player discovers / progresses.

    // Iron Gear (hand): basic progression item, always available.
    reg.addRecipe({
        "Craft Iron Gear",
        0, {{5, 2}}, {{7, 1}}, 2500,
        RecipeCategory::HandCraft,
        {}
    });

    // Copper Wire (hand): always available.
    reg.addRecipe({
        "Craft Copper Wire",
        0, {{6, 1}}, {{8, 2}}, 2000,
        RecipeCategory::HandCraft,
        {}
    });

    // Hand-smelt Iron Plate: unlocks once the player has mined Coal at
    // least once. Slower and costlier than the Furnace version.
    reg.addRecipe({
        "Hand-Smelt Iron Plate",
        0, {{1, 1}, {3, 1}}, {{5, 1}}, 6000,
        RecipeCategory::HandCraft,
        {
            pg::FactChecker{std::string("discovered_coal"), true, pg::FactCheckEquality::Equal}
        }
    });

    // Hand-smelt Copper Plate: same gate as iron.
    reg.addRecipe({
        "Hand-Smelt Copper Plate",
        0, {{2, 1}, {3, 1}}, {{6, 1}}, 6000,
        RecipeCategory::HandCraft,
        {
            pg::FactChecker{std::string("discovered_coal"), true, pg::FactCheckEquality::Equal}
        }
    });

    // Circuit (hand): unlocks once the player has hand-crafted at least one
    // Iron Gear. Longer craft time than the Assembler version.
    reg.addRecipe({
        "Craft Circuit",
        0, {{5, 1}, {8, 3}}, {{9, 1}}, 4500,
        RecipeCategory::HandCraft,
        {
            pg::FactChecker{std::string("crafted_iron_gear"), 1, pg::FactCheckEquality::GreaterEqual}
        }
    });

    return reg;
}
