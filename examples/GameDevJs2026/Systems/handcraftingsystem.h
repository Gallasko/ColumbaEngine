#pragma once

#include "Systems/basicsystems.h"

#include "playerinventory.h"
#include "reciperegistry.h"
#include "worldfacts.h"

#include <string>

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
class HandCraftingSystem : public pg::System<pg::InitSys,
                                             pg::Listener<pg::TickEvent>,
                                             pg::Listener<HandCraftRequest>,
                                             pg::Listener<HandCraftCancel>>
{
public:
    static constexpr size_t TICK_RESOLUTION_MS = 100;

    HandCraftingSystem(ItemRegistry* itemRegistry,
                       RecipeRegistry* recipeRegistry)
        : itemRegistry(itemRegistry),
          recipeRegistry(recipeRegistry) {}

    virtual std::string getSystemName() const override { return "Hand Crafting System"; }

    void init() override {}

    virtual void onEvent(const pg::TickEvent& event) override
    {
        tickAccumulator += static_cast<size_t>(event.tick);
    }

    virtual void onEvent(const HandCraftRequest& event) override;

    virtual void onEvent(const HandCraftCancel&) override;

    void execute() override;

    // ===== Queries (used by the UI) =====

    // True when the player has every ingredient and the recipe is unlocked.
    bool canCraft(const Recipe& recipe) const;

    bool isUnlocked(const Recipe& recipe) const;

    bool isActive() const { return activeRecipe != nullptr; }

    const Recipe* getActiveRecipe() const { return activeRecipe; }

    size_t getActiveRecipeIndex() const { return activeRecipeIndex; }

    // 0.0 when idle or just started, 1.0 when complete.
    float getProgressRatio() const;

private:
    void completeCraft();

    ItemRegistry* itemRegistry = nullptr;
    RecipeRegistry* recipeRegistry = nullptr;

    const Recipe* activeRecipe = nullptr;
    size_t activeRecipeIndex = SIZE_MAX;
    size_t progressMs = 0;
    size_t tickAccumulator = 0;
};
