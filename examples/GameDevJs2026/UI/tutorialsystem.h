#pragma once

#include "Systems/basicsystems.h"
#include "Input/inputcomponent.h"

#include "playerinventory.h"
#include "handcraftingsystem.h"
#include "gridsystem.h"
#include "worldfacts.h"
#include "inventoryui.h"
#include "missionui.h"
#include "hotbarsystem.h"
#include "hudbarsystem.h"
#include "reciperegistry.h"
#include "slotsystem.h"
#include "tutorialevents.h"

class SpotlightOverlaySystem;
namespace pg { class CameraSystem; }
using pg::CameraSystem;

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

class TutorialSystem : public pg::System<pg::Listener<PlayerGainItemEvent>,
                                      pg::Listener<InventoryOpenedEvent>,
                                      pg::Listener<InventoryClosedEvent>,
                                      pg::Listener<MissionUIOpenedEvent>,
                                      pg::Listener<MissionUIClosedEvent>,
                                      pg::Listener<HandCraftCompletedEvent>,
                                      pg::QueuedListener<SlotDroppedEvent>,
                                      pg::Listener<BuildingPlacedEvent>,
                                      pg::Listener<pg::AddFact>,
                                      pg::Listener<TutorialSkipRequested>>
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

    TutorialSystem(RecipeRegistry* recipeRegistry,
                   float screenWidth,
                   float screenHeight)
        : recipeRegistry(recipeRegistry),
          screenWidth(screenWidth),
          screenHeight(screenHeight) {}

    virtual std::string getSystemName() const override
        { return "Tutorial System"; }

    virtual void onEvent(const PlayerGainItemEvent& event) override;
    virtual void onEvent(const InventoryOpenedEvent& event) override;
    virtual void onEvent(const InventoryClosedEvent& event) override;
    virtual void onEvent(const MissionUIOpenedEvent& event) override;
    virtual void onEvent(const MissionUIClosedEvent& event) override;
    virtual void onEvent(const HandCraftCompletedEvent& event) override;
    virtual void onProcessEvent(const SlotDroppedEvent& event) override;
    virtual void onEvent(const BuildingPlacedEvent& event) override;
    virtual void onEvent(const pg::AddFact& event) override;
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
    void applyHudReveal();
    bool recipeOutputs(size_t recipeIndex, ItemId id) const;

    // Spawn a transient gold overlay over the inventory or hotbar slot that
    // currently holds `id`. Tweens its opacity to 0 and self-destroys.
    // No-op if the item isn't found in either inventory or hotbar slots.
    void pulseSlotForItem(ItemId id);

    // Scan the grid outward from the camera centre and return the closest
    // matching tile. `match` returns true for tiles that should be considered.
    // Falls back to (-1, -1) if nothing matches within the grid.
    template <typename Match>
    std::pair<int, int> findNearestTerrain(const Match& match) const;

    RecipeRegistry*         recipeRegistry = nullptr;

    float screenWidth  = 0.0f;
    float screenHeight = 0.0f;

    bool initialized    = false;
    int  currentStep    = 0;
    bool pendingAdvance = false;

    // Mirrors MissionUISystem visibility for the ValidateFirstSteps /
    // ValidateStoneMasonry sub-state machine. Updated from MissionUI*Event;
    // tutorial doesn't hold a pointer to MissionUISystem to keep dependencies
    // one-directional.
    bool missionUIOpen  = false;

    // Set the first time MissionUIOpened fires during the current step.
    // While true on a ValidateMission step the spotlight stays hidden even
    // after the player closes the panel — guidance shouldn't flap back on
    // before the player has had a chance to act in the mission UI. Reset
    // by advanceStep() on every transition.
    bool missionUiOpenedThisStep = false;
};
