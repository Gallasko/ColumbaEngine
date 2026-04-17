#pragma once

#include "Systems/basicsystems.h"
#include "Input/inputcomponent.h"

#include "playerinventory.h"
#include "handcraftingsystem.h"
#include "gridsystem.h"
#include "worldfacts.h"
#include "inventoryui.h"
#include "reciperegistry.h"

using namespace pg;

class TutorialSystem : public System<Listener<PlayerGainItemEvent>,
                                      Listener<InventoryOpenedEvent>,
                                      Listener<HandCraftCompletedEvent>,
                                      Listener<BuildingPlacedEvent>,
                                      Listener<TickEvent>>
{
public:
    static constexpr size_t UI_VP       = 2;
    static constexpr float  PANEL_W     = 260.0f;
    static constexpr float  PANEL_H     = 60.0f;
    static constexpr float  PADDING     = 10.0f;
    static constexpr float  TITLE_SCALE = 0.4f;
    static constexpr float  BODY_SCALE  = 0.30f;
    static constexpr float  PANEL_X     = 10.0f;
    static constexpr float  PANEL_Y     = 10.0f;
    static constexpr int    TOTAL_STEPS = 7;

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
    virtual void onEvent(const BuildingPlacedEvent& event) override;
    virtual void onEvent(const TickEvent& event) override;

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
