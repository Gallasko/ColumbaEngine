#pragma once

#include "Systems/basicsystems.h"
#include "Input/inputcomponent.h"

#include "playerinventory.h"
#include "handcraftingsystem.h"
#include "gridsystem.h"
#include "worldfacts.h"
#include "inventoryui.h"
#include "reciperegistry.h"
#include "slotsystem.h"
#include "tutorialevents.h"

using namespace pg;

class SpotlightOverlaySystem;
class CameraSystem;

// Forced early-game tutorial. State persists in WorldFacts as `tutorial_step`.
// Each state advances on a specific event. The two ValidateMission* steps gate
// access to crafts whose recipes are locked behind mission completion facts.
//
// MineFirstResource     -> PlayerGainItemEvent (Wood or Stone)
// OpenInventory         -> InventoryOpenedEvent
// ValidateFirstSteps    -> AddFact (name == "mission_tools", value == true)
// CraftPickaxe          -> HandCraftCompletedEvent (recipe outputs Stone Pickaxe)
// MovePickaxeToHotbar   -> SlotDroppedEvent (Hotbar, item.id == Stone Pickaxe)
// MineOre               -> PlayerGainItemEvent (Iron Ore, Copper Ore, or Coal)
// ValidateStoneMasonry  -> AddFact (name == "mission_furnace", value == true)
// CraftFurnace          -> HandCraftCompletedEvent (recipe outputs Furnace)
// PlaceFurnaceInHotbar  -> SlotDroppedEvent (Hotbar, item.id == Furnace)
// PlaceFurnaceOnGrid    -> BuildingPlacedEvent (tileName == "Furnace")
// Complete              -> tutorial done; panel hidden
enum class TutorialStep : int
{
    MineFirstResource    = 0,
    OpenInventory        = 1,
    ValidateFirstSteps   = 2,
    CraftPickaxe         = 3,
    MovePickaxeToHotbar  = 4,
    MineOre              = 5,
    ValidateStoneMasonry = 6,
    CraftFurnace         = 7,
    PlaceFurnaceInHotbar = 8,
    PlaceFurnaceOnGrid   = 9,
    Complete             = 10,
};

class TutorialSystem : public System<Listener<PlayerGainItemEvent>,
                                      Listener<InventoryOpenedEvent>,
                                      Listener<HandCraftCompletedEvent>,
                                      QueuedListener<SlotDroppedEvent>,
                                      Listener<BuildingPlacedEvent>,
                                      Listener<AddFact>,
                                      Listener<TutorialSkipRequested>>
{
public:
    static constexpr int    TOTAL_STEPS = static_cast<int>(TutorialStep::Complete);

    // Item ids referenced by the advance conditions. Must match itemregistry.cpp.
    static constexpr ItemId WOOD_ID          = 15;
    static constexpr ItemId STONE_ID         = 4;
    static constexpr ItemId IRON_ORE_ID      = 1;
    static constexpr ItemId COPPER_ORE_ID    = 2;
    static constexpr ItemId COAL_ID          = 3;
    static constexpr ItemId STONE_PICKAXE_ID = 29;
    static constexpr ItemId FURNACE_ID       = 25;

    TutorialSystem(WorldFacts* worldFacts,
                   RecipeRegistry* recipeRegistry,
                   SpotlightOverlaySystem* spotlight,
                   CameraSystem* cameraSystem,
                   GridSystem* gridSystem,
                   float screenWidth,
                   float screenHeight)
        : worldFacts(worldFacts),
          recipeRegistry(recipeRegistry),
          spotlight(spotlight),
          cameraSystem(cameraSystem),
          gridSystem(gridSystem),
          screenWidth(screenWidth),
          screenHeight(screenHeight) {}

    virtual std::string getSystemName() const override
        { return "Tutorial System"; }

    virtual void onEvent(const PlayerGainItemEvent& event) override;
    virtual void onEvent(const InventoryOpenedEvent& event) override;
    virtual void onEvent(const HandCraftCompletedEvent& event) override;
    virtual void onProcessEvent(const SlotDroppedEvent& event) override;
    virtual void onEvent(const BuildingPlacedEvent& event) override;
    virtual void onEvent(const AddFact& event) override;
    virtual void onEvent(const TutorialSkipRequested& event) override;

    void execute() override;

private:
    struct StepDef
    {
        const char* title;
        const char* body;
    };

    static const StepDef STEPS[];

    void initOnFirstTick();
    void advanceStep();
    void presentCurrentStep();
    bool recipeOutputs(size_t recipeIndex, ItemId id) const;

    // Scan the grid outward from the camera centre and return the closest
    // matching tile. `match` returns true for tiles that should be considered.
    // Falls back to (-1, -1) if nothing matches within the grid.
    template <typename Match>
    std::pair<int, int> findNearestTerrain(const Match& match) const;

    WorldFacts*             worldFacts     = nullptr;
    RecipeRegistry*         recipeRegistry = nullptr;
    SpotlightOverlaySystem* spotlight      = nullptr;
    CameraSystem*           cameraSystem   = nullptr;
    GridSystem*             gridSystem     = nullptr;

    float screenWidth  = 0.0f;
    float screenHeight = 0.0f;

    bool initialized    = false;
    int  currentStep    = 0;
    bool pendingAdvance = false;
};
