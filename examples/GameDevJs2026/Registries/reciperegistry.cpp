#include "reciperegistry.h"

std::vector<const Recipe*> RecipeRegistry::getRecipesForMachine(const std::string& machineName) const
{
    std::vector<const Recipe*> result;
    for (const auto& r : all())
        if (r.machineName == machineName and
            (r.category == RecipeCategory::Furnace or r.category == RecipeCategory::Assembler))
            result.push_back(&r);
    return result;
}

const Recipe* RecipeRegistry::findMatchingRecipe(const std::string& machineName,
                                                 const Inventory& inputSlots) const
{
    for (const auto& recipe : all())
    {
        if (recipe.machineName != machineName)
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

    // Furnace
    reg.addRecipe({
        "Smelt Iron Plate",
        "Furnace", {{1, 1}}, {{5, 1}}, 2000,
        RecipeCategory::Furnace, {}
    });
    reg.addRecipe({
        "Smelt Copper Plate",
        "Furnace", {{2, 1}}, {{6, 1}}, 2000,
        RecipeCategory::Furnace, {}
    });

    // Assembler
    reg.addRecipe({
        "Make Iron Gear",
        "Assembler", {{5, 2}}, {{7, 1}}, 1000,
        RecipeCategory::Assembler, {}
    });
    reg.addRecipe({
        "Make Copper Wire",
        "Assembler", {{6, 1}}, {{8, 2}}, 500,
        RecipeCategory::Assembler, {}
    });
    reg.addRecipe({
        "Make Circuit",
        "Assembler", {{5, 1}, {8, 3}}, {{9, 1}}, 3000,
        RecipeCategory::Assembler, {}
    });
    // Item IDs: 5=Iron Plate, 7=Iron Gear, 8=Copper Wire, 24=Conveyor Belt, 28=Inserter
    reg.addRecipe({
        "Make Conveyor Belt",
        "Assembler", {{5, 1}, {7, 1}}, {{24, 1}}, 500,
        RecipeCategory::Assembler, {}
    });
    reg.addRecipe({
        "Make Inserter",
        "Assembler", {{5, 1}, {8, 1}}, {{28, 1}}, 1000,
        RecipeCategory::Assembler, {}
    });
    // Item IDs: 5=Iron Plate, 7=Iron Gear, 27=Miner
    reg.addRecipe({
        "Make Miner",
        "Assembler", {{5, 3}, {7, 2}}, {{27, 1}}, 2000,
        RecipeCategory::Assembler, {}
    });

    // Item IDs: 7=Iron Gear, 8=Copper Wire, 32=Motor
    reg.addRecipe({
        "Make Motor",
        "Assembler", {{7, 2}, {8, 2}}, {{32, 1}}, 3000,
        RecipeCategory::Assembler, {}
    });

    // Item IDs: 9=Circuit, 5=Iron Plate, 21=Processor
    reg.addRecipe({
        "Make Processor",
        "Assembler", {{9, 2}, {5, 1}}, {{21, 1}}, 5000,
        RecipeCategory::Assembler, {}
    });

    // Item IDs: 21=Processor, 32=Motor, 9=Circuit, 33=Robot Core
    reg.addRecipe({
        "Make Robot Core",
        "Assembler", {{21, 1}, {32, 2}}, {{33, 1}}, 8000,
        RecipeCategory::Assembler, {}
    });

    // ===== Hand-craft recipes =====
    //
    // Unlocked-by-default recipes let the player bootstrap production by
    // hand; fact-gated recipes appear as the player discovers / progresses.

    // Iron Gear (hand): unlocks once the player has obtained Iron Plate.
    reg.addRecipe({
        "Craft Iron Gear",
        "", {{5, 2}}, {{7, 1}}, 2500,
        RecipeCategory::HandCraft,
        {
            pg::FactChecker{std::string("discovered_iron_plate"), true, pg::FactCheckEquality::Equal}
        }
    });

    // Copper Wire (hand): unlocks once the player has obtained Copper Plate.
    reg.addRecipe({
        "Craft Copper Wire",
        "", {{6, 1}}, {{8, 2}}, 2000,
        RecipeCategory::HandCraft,
        {
            pg::FactChecker{std::string("discovered_copper_plate"), true, pg::FactCheckEquality::Equal}
        }
    });

    // Hand-smelt Iron Plate: unlocks once the player has mined Coal at
    // least once. Slower and costlier than the Furnace version.
    reg.addRecipe({
        "Hand-Smelt Iron Plate",
        "", {{1, 1}, {3, 1}}, {{5, 1}}, 6000,
        RecipeCategory::HandCraft,
        {
            pg::FactChecker{std::string("discovered_coal"), true, pg::FactCheckEquality::Equal}
        }
    });

    // Hand-smelt Copper Plate: same gate as iron.
    reg.addRecipe({
        "Hand-Smelt Copper Plate",
        "", {{2, 1}, {3, 1}}, {{6, 1}}, 6000,
        RecipeCategory::HandCraft,
        {
            pg::FactChecker{std::string("discovered_coal"), true, pg::FactCheckEquality::Equal}
        }
    });

    // Circuit (hand): unlocks once the player has hand-crafted at least one
    // Iron Gear. Longer craft time than the Assembler version.
    reg.addRecipe({
        "Craft Circuit",
        "", {{5, 1}, {8, 3}}, {{9, 1}}, 4500,
        RecipeCategory::HandCraft,
        {
            pg::FactChecker{std::string("crafted_iron_gear"), 1, pg::FactCheckEquality::GreaterEqual}
        }
    });

    // Stone Pickaxe (hand): 3 Stone + 2 Wood → 1 Stone Pickaxe. Always available.
    // Item IDs: 4=Stone, 15=Wood, 29=Stone Pickaxe
    reg.addRecipe({
        "Craft Stone Pickaxe",
        "", {{4, 3}, {15, 2}}, {{29, 1}}, 3000,
        RecipeCategory::HandCraft,
        {}
    });

    // Iron Pickaxe (hand): 3 Iron Plate + 2 Wood → 1 Iron Pickaxe.
    // Unlocks after crafting a Stone Pickaxe.
    // Item IDs: 5=Iron Plate, 15=Wood, 30=Iron Pickaxe
    reg.addRecipe({
        "Craft Iron Pickaxe",
        "", {{5, 3}, {15, 2}}, {{30, 1}}, 4000,
        RecipeCategory::HandCraft,
        {
            pg::FactChecker{std::string("crafted_stone_pickaxe"), 1, pg::FactCheckEquality::GreaterEqual}
        }
    });

    // Craft Conveyor Belt: 2 Iron Plate + 1 Iron Gear → 1 Conveyor Belt.
    // Slower than the assembler but available by hand.
    reg.addRecipe({
        "Craft Conveyor Belt",
        "", {{5, 2}, {7, 1}}, {{24, 1}}, 3000,
        RecipeCategory::HandCraft, {}
    });

    // Craft Inserter: 2 Iron Plate + 2 Copper Wire → 1 Inserter.
    // Gated behind crafting an Iron Gear (i.e. having iron production).
    reg.addRecipe({
        "Craft Inserter",
        "", {{5, 2}, {8, 2}}, {{28, 1}}, 4000,
        RecipeCategory::HandCraft,
        {
            pg::FactChecker{std::string("crafted_iron_gear"), 1, pg::FactCheckEquality::GreaterEqual}
        }
    });

    // Craft Miner: 4 Iron Plate + 3 Iron Gear → 1 Miner.
    // Gated behind crafting an Iron Gear.
    reg.addRecipe({
        "Craft Miner",
        "", {{5, 4}, {7, 3}}, {{27, 1}}, 6000,
        RecipeCategory::HandCraft,
        {
            pg::FactChecker{std::string("crafted_iron_gear"), 1, pg::FactCheckEquality::GreaterEqual}
        }
    });

    // Craft Furnace: 5 Stone → 1 Furnace (always available)
    // Item IDs: 4=Stone, 25=Furnace
    reg.addRecipe({
        "Craft Furnace",
        "", {{4, 5}}, {{25, 1}}, 5000,
        RecipeCategory::HandCraft, {}
    });

    // Craft Assembler: 5 Iron Plate + 3 Iron Gear → 1 Assembler (always available)
    // Item IDs: 5=Iron Plate, 7=Iron Gear, 26=Assembler
    reg.addRecipe({
        "Craft Assembler",
        "", {{5, 5}, {7, 3}}, {{26, 1}}, 10000,
        RecipeCategory::HandCraft, {}
    });

    // Craft Storage: 4 Wood → 1 Storage (always available)
    // Item IDs: 15=Wood, 31=Storage
    reg.addRecipe({
        "Craft Storage",
        "", {{15, 4}}, {{31, 1}}, 3000,
        RecipeCategory::HandCraft, {}
    });

    // Craft Depot: 5 Iron Plate + 3 Iron Gear + 1 Circuit → 1 Depot
    // Gated behind discovering Circuit.
    // Item IDs: 5=Iron Plate, 7=Iron Gear, 9=Circuit, 34=Depot
    reg.addRecipe({
        "Craft Depot",
        "", {{5, 5}, {7, 3}, {9, 1}}, {{34, 1}}, 8000,
        RecipeCategory::HandCraft,
        {
            pg::FactChecker{std::string("discovered_circuit"), true, pg::FactCheckEquality::Equal}
        }
    });

    return reg;
}
