#pragma once

#include "Systems/basicsystems.h"

#include "playerinventory.h"
#include "reciperegistry.h"
#include "worldfacts.h"

#include <string>

using namespace pg;

// Request sent by the crafting UI when the player clicks "Craft" on a recipe.
struct HandCraftRequest
{
    size_t recipeIndex; // Index into RecipeRegistry::recipes
};

// Cancel the current hand-craft. Consumed inputs are returned to the player.
struct HandCraftCancel {};

// Broadcast when a hand-craft finishes successfully.
struct HandCraftCompletedEvent
{
    size_t recipeIndex;
};

// Drives the player's single active hand-craft. Keeps a progress counter,
// consumes inputs immediately on start, and emits the outputs when the
// timer expires.
class HandCraftingSystem : public System<InitSys,
                                         Listener<TickEvent>,
                                         Listener<HandCraftRequest>,
                                         Listener<HandCraftCancel>>
{
public:
    static constexpr size_t TICK_RESOLUTION_MS = 100;

    HandCraftingSystem(PlayerInventorySystem* playerInv,
                       ItemRegistry* itemRegistry,
                       RecipeRegistry* recipeRegistry,
                       pg::WorldFacts* worldFacts)
        : playerInv(playerInv), itemRegistry(itemRegistry),
          recipeRegistry(recipeRegistry), worldFacts(worldFacts) {}

    virtual std::string getSystemName() const override { return "Hand Crafting System"; }

    void init() override {}

    virtual void onEvent(const TickEvent& event) override
    {
        tickAccumulator += static_cast<size_t>(event.tick);
    }

    virtual void onEvent(const HandCraftRequest& event) override
    {
        if (activeRecipe != nullptr)
            return; // Already crafting

        if (event.recipeIndex >= recipeRegistry->recipes.size())
            return;

        const Recipe& recipe = recipeRegistry->recipes[event.recipeIndex];

        if (not canCraft(recipe))
            return;

        // Consume inputs from the player inventory.
        auto& inv = playerInv->getInventory();
        for (const auto& input : recipe.inputs)
            inv.remove(input.id, input.count);

        activeRecipe = &recipe;
        activeRecipeIndex = event.recipeIndex;
        progressMs = 0;
    }

    virtual void onEvent(const HandCraftCancel&) override
    {
        if (activeRecipe == nullptr)
            return;

        // Refund consumed inputs to the player.
        for (const auto& input : activeRecipe->inputs)
            ecsRef->sendEvent(PlayerGainItemEvent{input.id, input.count});

        activeRecipe = nullptr;
        activeRecipeIndex = SIZE_MAX;
        progressMs = 0;
    }

    void execute() override
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

    // ===== Queries (used by the UI) =====

    // True when the player has every ingredient and the recipe is unlocked.
    bool canCraft(const Recipe& recipe) const
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

    bool isUnlocked(const Recipe& recipe) const
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

    bool isActive() const { return activeRecipe != nullptr; }

    const Recipe* getActiveRecipe() const { return activeRecipe; }

    size_t getActiveRecipeIndex() const { return activeRecipeIndex; }

    // 0.0 when idle or just started, 1.0 when complete.
    float getProgressRatio() const
    {
        if (activeRecipe == nullptr or activeRecipe->craftTimeMs == 0)
            return 0.0f;
        float ratio = static_cast<float>(progressMs)
                    / static_cast<float>(activeRecipe->craftTimeMs);
        if (ratio < 0.0f) ratio = 0.0f;
        if (ratio > 1.0f) ratio = 1.0f;
        return ratio;
    }

private:
    void completeCraft()
    {
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

    PlayerInventorySystem* playerInv = nullptr;
    ItemRegistry* itemRegistry = nullptr;
    RecipeRegistry* recipeRegistry = nullptr;
    pg::WorldFacts* worldFacts = nullptr;

    const Recipe* activeRecipe = nullptr;
    size_t activeRecipeIndex = SIZE_MAX;
    size_t progressMs = 0;
    size_t tickAccumulator = 0;
};
