#pragma once

#include "Systems/basicsystems.h"
#include "Input/inputcomponent.h"

#include "inventoryui.h"
#include "worldfacts.h"

using namespace pg;

class HudBarSystem : public System<InitSys,
                                    QueuedListener<OnMouseClick>,
                                    Listener<TickEvent>>
{
public:
    static constexpr size_t UI_VP       = 2;
    static constexpr float BUTTON_SIZE  = 32.0f;
    static constexpr float BUTTON_GAP   = 6.0f;
    static constexpr float ICON_SIZE    = 24.0f;
    static constexpr float MARGIN_RIGHT = 12.0f;
    static constexpr float MARGIN_TOP   = 12.0f;
    static constexpr size_t NUM_BUTTONS = 3;

    // Button indices
    static constexpr size_t BTN_INVENTORY = 0;
    static constexpr size_t BTN_MISSIONS  = 1;
    static constexpr size_t BTN_SETTINGS  = 2;

    HudBarSystem(InventoryUISystem* inventoryUI, WorldFacts* worldFacts,
                 float screenWidth, float screenHeight)
        : inventoryUI(inventoryUI), worldFacts(worldFacts),
          screenWidth(screenWidth), screenHeight(screenHeight) {}

    virtual std::string getSystemName() const override { return "HUD Bar System"; }

    void init() override;

    virtual void onEvent(const TickEvent&) override;
    virtual void onProcessEvent(const OnMouseClick& event) override;

    // Set by application after MissionUI is created (Phase 4)
    void setMissionUIToggle(std::function<void()> toggle) { missionToggle = toggle; }

private:
    void createButtons();
    void updateMissionButtonVisibility();

    bool isClickOnButton(size_t idx, float x, float y) const;

    InventoryUISystem* inventoryUI = nullptr;
    WorldFacts* worldFacts = nullptr;
    float screenWidth = 0.0f;
    float screenHeight = 0.0f;

    std::function<void()> missionToggle;

    bool buttonsCreated = false;
    bool missionButtonVisible = false;

    // Cached positions (top-left of each button)
    float buttonX[NUM_BUTTONS] = {};
    float buttonY[NUM_BUTTONS] = {};

    // Entity IDs
    uint64_t buttonBgId[NUM_BUTTONS]   = {};
    uint64_t buttonIconId[NUM_BUTTONS] = {};
};
