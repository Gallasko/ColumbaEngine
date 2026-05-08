#include "handcraftingsystem.h"

void HandCraftingSystem::onEvent(const HandCraftRequest& event)
{
    LOG_INFO("HandCrafting", "HandCraftRequest recipeIndex=" << event.recipeIndex);
    if (activeRecipe != nullptr)
    {
        LOG_INFO("HandCrafting", "HandCraftRequest ignored — already crafting recipeIndex=" << activeRecipeIndex);
        return;
    }

    if (event.recipeIndex >= recipeRegistry->count())
    {
        LOG_INFO("HandCrafting", "HandCraftRequest invalid recipeIndex=" << event.recipeIndex
                << " (count=" << recipeRegistry->count() << ")");
        return;
    }

    const Recipe& recipe = recipeRegistry->get(event.recipeIndex);

    if (not canCraft(recipe))
    {
        LOG_INFO("HandCrafting", "HandCraftRequest cannot craft recipeIndex=" << event.recipeIndex);
        return;
    }

    // Consume inputs from the player inventory.
    auto& inv = playerInv->getInventory();
    for (const auto& input : recipe.inputs)
        inv.remove(input.id, input.count);

    activeRecipe = &recipe;
    activeRecipeIndex = event.recipeIndex;
    progressMs = 0;
    LOG_INFO("HandCrafting", "started craft recipeIndex=" << activeRecipeIndex
            << " craftTimeMs=" << activeRecipe->craftTimeMs);
}

void HandCraftingSystem::onEvent(const HandCraftCancel&)
{
    LOG_INFO("HandCrafting", "HandCraftCancel received (active=" << (activeRecipe != nullptr) << ")");
    if (activeRecipe == nullptr)
        return;

    LOG_INFO("HandCrafting", "cancelling craft recipeIndex=" << activeRecipeIndex
            << " — refunding inputs");

    // Refund consumed inputs to the player.
    for (const auto& input : activeRecipe->inputs)
        ecsRef->sendEvent(PlayerGainItemEvent{input.id, input.count});

    activeRecipe = nullptr;
    activeRecipeIndex = SIZE_MAX;
    progressMs = 0;
}

void HandCraftingSystem::execute()
{
    if (activeRecipe == nullptr)
    {
        tickAccumulator = 0;
        return;
    }

    while (tickAccumulator >= TICK_RESOLUTION_MS)
    {
        tickAccumulator -= TICK_RESOLUTION_MS;
        progressMs += TICK_RESOLUTION_MS;

        if (progressMs >= activeRecipe->craftTimeMs)
        {
            completeCraft();
            break;
        }
    }
}

bool HandCraftingSystem::canCraft(const Recipe& recipe) const
{
    if (not isUnlocked(recipe))
        return false;

    const auto& inv = playerInv->getInventory();
    for (const auto& input : recipe.inputs)
    {
        if (not inv.hasAtLeast(input.id, input.count))
            return false;
    }
    return true;
}

bool HandCraftingSystem::isUnlocked(const Recipe& recipe) const
{
    if (recipe.unlockConditions.empty())
        return true;

    if (worldFacts == nullptr)
        return true;

    for (const auto& checker : recipe.unlockConditions)
    {
        if (not checker.check(worldFacts->factMap))
            return false;
    }
    return true;
}

float HandCraftingSystem::getProgressRatio() const
{
    if (activeRecipe == nullptr or activeRecipe->craftTimeMs == 0)
        return 0.0f;
    float ratio = static_cast<float>(progressMs)
                / static_cast<float>(activeRecipe->craftTimeMs);
    if (ratio < 0.0f) ratio = 0.0f;
    if (ratio > 1.0f) ratio = 1.0f;
    return ratio;
}

void HandCraftingSystem::completeCraft()
{
    LOG_INFO("HandCrafting", "completeCraft recipeIndex=" << activeRecipeIndex
            << " — delivering " << activeRecipe->outputs.size() << " output(s)");

    // Deliver outputs into the player inventory.
    for (const auto& output : activeRecipe->outputs)
        ecsRef->sendEvent(PlayerGainItemEvent{output.id, output.count});

    // Bump progression facts so other recipes can unlock.
    // General per-recipe counter:
    std::string perRecipeFact = "crafted_recipe_" + std::to_string(activeRecipeIndex);
    ecsRef->sendEvent(pg::IncreaseFact{perRecipeFact, 1});

    // Per-output-item counter (more convenient for unlock gates):
    for (const auto& output : activeRecipe->outputs)
    {
        const auto& def = itemRegistry->get(output.id);
        // Build "crafted_<lowercase-snake-name>" — use the raw item name
        // minus spaces so recipe authors can write readable fact names.
        std::string factName = "crafted_";
        for (char c : def.name)
        {
            if (c == ' ')
                factName.push_back('_');
            else if (c >= 'A' and c <= 'Z')
                factName.push_back(static_cast<char>(c - 'A' + 'a'));
            else
                factName.push_back(c);
        }
        ecsRef->sendEvent(pg::IncreaseFact{factName, static_cast<int>(output.count)});
    }

    ecsRef->sendEvent(HandCraftCompletedEvent{activeRecipeIndex});

    activeRecipe = nullptr;
    activeRecipeIndex = SIZE_MAX;
    progressMs = 0;
}
