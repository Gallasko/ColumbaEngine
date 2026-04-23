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
                                                 const Inventory& inputSlots,
                                                 const std::unordered_map<std::string, pg::ElementType>* facts) const
{
    for (const auto& recipe : all())
    {
        if (recipe.machineName != machineName)
            continue;
        if (recipe.category == RecipeCategory::HandCraft or
            recipe.category == RecipeCategory::AutoCrafter)
            continue;

        // Check unlock conditions if facts are provided
        if (facts and not recipe.unlockConditions.empty())
        {
            bool unlocked = true;
            for (const auto& cond : recipe.unlockConditions)
            {
                if (not cond.check(*facts))
                {
                    unlocked = false;
                    break;
                }
            }
            if (not unlocked)
                continue;
        }

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

    // Furnace (gated behind mission_smelting — deliver 5 Iron Ore first)
    reg.addRecipe({
        "Smelt Iron Plate",
        "Furnace", {{1, 1}}, {{5, 1}}, 2000,
        RecipeCategory::Furnace,
        {
            pg::FactChecker{std::string("mission_smelting"), true, pg::FactCheckEquality::Equal}
        }
    });
    reg.addRecipe({
        "Smelt Copper Plate",
        "Furnace", {{2, 1}}, {{6, 1}}, 2000,
        RecipeCategory::Furnace,
        {
            pg::FactChecker{std::string("mission_smelting"), true, pg::FactCheckEquality::Equal}
        }
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
    // All recipes are gated behind mission completion facts.
    // Missions drive the linear progression — no discovery-based unlocks.

    // Stone Pickaxe: unlocked by mission_tools (First Steps)
    // Item IDs: 4=Stone, 15=Wood, 29=Stone Pickaxe
    reg.addRecipe({
        "Craft Stone Pickaxe",
        "", {{4, 3}, {15, 2}}, {{29, 1}}, 3000,
        RecipeCategory::HandCraft,
        {
            pg::FactChecker{std::string("mission_tools"), true, pg::FactCheckEquality::Equal}
        }
    });

    // Furnace: unlocked by mission_furnace (Stone Masonry)
    // Item IDs: 4=Stone, 25=Furnace
    reg.addRecipe({
        "Craft Furnace",
        "", {{4, 5}}, {{25, 1}}, 5000,
        RecipeCategory::HandCraft,
        {
            pg::FactChecker{std::string("mission_furnace"), true, pg::FactCheckEquality::Equal}
        }
    });

    // Iron Gear (hand): unlocked by mission_metals (Metal Working)
    reg.addRecipe({
        "Craft Iron Gear",
        "", {{5, 2}}, {{7, 1}}, 2500,
        RecipeCategory::HandCraft,
        {
            pg::FactChecker{std::string("mission_metals"), true, pg::FactCheckEquality::Equal}
        }
    });

    // Copper Wire (hand): unlocked by mission_metals (Metal Working)
    reg.addRecipe({
        "Craft Copper Wire",
        "", {{6, 1}}, {{8, 2}}, 2000,
        RecipeCategory::HandCraft,
        {
            pg::FactChecker{std::string("mission_metals"), true, pg::FactCheckEquality::Equal}
        }
    });

    // Storage: unlocked by mission_metals (Metal Working)
    // Item IDs: 15=Wood, 31=Storage
    reg.addRecipe({
        "Craft Storage",
        "", {{15, 4}}, {{31, 1}}, 3000,
        RecipeCategory::HandCraft,
        {
            pg::FactChecker{std::string("mission_metals"), true, pg::FactCheckEquality::Equal}
        }
    });

    // Assembler: unlocked by mission_mechanical (Mechanical Parts)
    // Item IDs: 5=Iron Plate, 7=Iron Gear, 26=Assembler
    reg.addRecipe({
        "Craft Assembler",
        "", {{5, 5}, {7, 3}}, {{26, 1}}, 10000,
        RecipeCategory::HandCraft,
        {
            pg::FactChecker{std::string("mission_mechanical"), true, pg::FactCheckEquality::Equal}
        }
    });

    // Conveyor Belt: unlocked by mission_mechanical (Mechanical Parts)
    reg.addRecipe({
        "Craft Conveyor Belt",
        "", {{5, 2}, {7, 1}}, {{24, 1}}, 3000,
        RecipeCategory::HandCraft,
        {
            pg::FactChecker{std::string("mission_mechanical"), true, pg::FactCheckEquality::Equal}
        }
    });

    // Iron Pickaxe: unlocked by mission_mechanical (Mechanical Parts)
    // Item IDs: 5=Iron Plate, 15=Wood, 30=Iron Pickaxe
    reg.addRecipe({
        "Craft Iron Pickaxe",
        "", {{5, 3}, {15, 2}}, {{30, 1}}, 4000,
        RecipeCategory::HandCraft,
        {
            pg::FactChecker{std::string("mission_mechanical"), true, pg::FactCheckEquality::Equal}
        }
    });

    // Miner: unlocked by mission_automation (Scaling Up)
    // Item IDs: 5=Iron Plate, 7=Iron Gear, 27=Miner
    reg.addRecipe({
        "Craft Miner",
        "", {{5, 4}, {7, 3}}, {{27, 1}}, 6000,
        RecipeCategory::HandCraft,
        {
            pg::FactChecker{std::string("mission_automation"), true, pg::FactCheckEquality::Equal}
        }
    });

    // Inserter: unlocked by mission_automation (Scaling Up)
    reg.addRecipe({
        "Craft Inserter",
        "", {{5, 2}, {8, 2}}, {{28, 1}}, 4000,
        RecipeCategory::HandCraft,
        {
            pg::FactChecker{std::string("mission_automation"), true, pg::FactCheckEquality::Equal}
        }
    });

    // Circuit (hand): unlocked by mission_electronics (Electronics)
    reg.addRecipe({
        "Craft Circuit",
        "", {{5, 1}, {8, 3}}, {{9, 1}}, 4500,
        RecipeCategory::HandCraft,
        {
            pg::FactChecker{std::string("mission_electronics"), true, pg::FactCheckEquality::Equal}
        }
    });

    // Depot: unlocked by mission_electronics (Electronics)
    // Item IDs: 5=Iron Plate, 7=Iron Gear, 9=Circuit, 34=Depot
    reg.addRecipe({
        "Craft Depot",
        "", {{5, 5}, {7, 3}, {9, 1}}, {{34, 1}}, 8000,
        RecipeCategory::HandCraft,
        {
            pg::FactChecker{std::string("mission_electronics"), true, pg::FactCheckEquality::Equal}
        }
    });

    return reg;
}
