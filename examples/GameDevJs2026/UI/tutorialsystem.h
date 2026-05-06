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

using namespace pg;

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
                                      Listener<AddFact>>
{
public:
    static constexpr size_t UI_VP       = 2;
    static constexpr float  PANEL_W     = 280.0f;
    static constexpr float  PANEL_H     = 70.0f;
    static constexpr float  PADDING     = 10.0f;
    static constexpr float  TITLE_SCALE = 0.4f;
    static constexpr float  BODY_SCALE  = 0.30f;
    static constexpr float  PANEL_X     = 10.0f;
    static constexpr float  PANEL_Y     = 10.0f;

    static constexpr int    TOTAL_STEPS = static_cast<int>(TutorialStep::Complete);

    // Item ids referenced by the advance conditions. Must match itemregistry.cpp.
    static constexpr ItemId WOOD_ID          = 15;
    static constexpr ItemId STONE_ID         = 4;
    static constexpr ItemId IRON_ORE_ID      = 1;
    static constexpr ItemId COPPER_ORE_ID    = 2;
    static constexpr ItemId COAL_ID          = 3;
    static constexpr ItemId STONE_PICKAXE_ID = 29;
    static constexpr ItemId FURNACE_ID       = 25;

    static constexpr const char* FONT_PATH =
        "res/font/Inter/static/Inter_28pt-Light.ttf";

    TutorialSystem(WorldFacts* worldFacts,
                   RecipeRegistry* recipeRegistry)
        : worldFacts(worldFacts),
          recipeRegistry(recipeRegistry) {}

    virtual std::string getSystemName() const override
        { return "Tutorial System"; }

    virtual void onEvent(const PlayerGainItemEvent& event) override;
    virtual void onEvent(const InventoryOpenedEvent& event) override;
    virtual void onEvent(const HandCraftCompletedEvent& event) override;
    virtual void onProcessEvent(const SlotDroppedEvent& event) override;
    virtual void onEvent(const BuildingPlacedEvent& event) override;
    virtual void onEvent(const AddFact& event) override;

    void execute() override;

private:
    struct StepDef
    {
        const char* title;
        const char* body;
    };

    static const StepDef STEPS[];

    void ensureCreated();
    void advanceStep();
    void updateContent();
    void hidePanel();
    void setEntityVisibility(uint64_t id, bool vis);

    bool recipeOutputs(size_t recipeIndex, ItemId id) const;

    WorldFacts*     worldFacts     = nullptr;
    RecipeRegistry* recipeRegistry = nullptr;

    bool created        = false;
    bool visible        = false;
    int  currentStep    = 0;
    bool pendingAdvance = false;

    uint64_t backdropId = 0;
    uint64_t titleId    = 0;
    uint64_t bodyId     = 0;
};
