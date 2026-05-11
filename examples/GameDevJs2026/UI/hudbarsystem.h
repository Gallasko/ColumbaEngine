#pragma once

#include "Systems/basicsystems.h"
#include "Input/inputcomponent.h"

#include "worldfacts.h"
#include "playerinventory.h"
#include "missionui.h"

#include <functional>

using namespace pg;

class MissionSystem;

// Fired synchronously by the MouseLeftClickComponent attached to each HUD
// button. HudBarSystem listens, marks the click as a panel click (so
// GameSystem won't close panels via outside-click) and dispatches to the
// stored toggle callback.
struct HudInventoryButtonClicked {};
struct HudMissionButtonClicked   {};

class HudBarSystem : public System<InitSys,
                                    Listener<TickEvent>,
                                    Listener<MissionUIOpenedEvent>,
                                    Listener<MissionUIClosedEvent>,
                                    Listener<HudInventoryButtonClicked>,
                                    Listener<HudMissionButtonClicked>>
{
public:
    static constexpr size_t UI_VP       = 2;
    static constexpr float BUTTON_SIZE  = 48.0f;
    static constexpr float BUTTON_GAP   = 6.0f;
    static constexpr float ICON_SIZE    = 40.0f;
    static constexpr float MARGIN_RIGHT = 12.0f;
    static constexpr float MARGIN_TOP   = 12.0f;
    static constexpr size_t NUM_BUTTONS = 3;

    // Button indices
    static constexpr size_t BTN_INVENTORY = 0;
    static constexpr size_t BTN_MISSIONS  = 1;
    static constexpr size_t BTN_SETTINGS  = 2;

    static constexpr float TEXT_SCALE = 0.30f;
    static constexpr const char* FONT_PATH = "res/font/Inter/static/Inter_28pt-Light.ttf";
    static constexpr ItemId TICKET_ID = 35;

    HudBarSystem(float screenWidth, float screenHeight)
        : screenWidth(screenWidth), screenHeight(screenHeight) {}

    virtual std::string getSystemName() const override { return "HUD Bar System"; }

    void init() override;

    virtual void onEvent(const TickEvent&) override;
    virtual void onEvent(const MissionUIOpenedEvent&) override;
    virtual void onEvent(const MissionUIClosedEvent&) override;
    virtual void onEvent(const HudInventoryButtonClicked&) override;
    virtual void onEvent(const HudMissionButtonClicked&) override;

    // Set by application after GameSystem is created. These route through
    // GameSystem so it can enforce UI group exclusivity (close mission when
    // inventory opens, and vice versa).
    void setInventoryToggle(std::function<void()> toggle) { inventoryToggle = std::move(toggle); }
    void setMissionUIToggle(std::function<void()> toggle) { missionToggle = std::move(toggle); }

    // Show or hide the mission HUD button (entities + click handling). Used
    // by the tutorial system for progressive HUD reveal so the button only
    // appears at the step that introduces it.
    void setMissionButtonVisible(bool vis);

    // Entity for the ticket-currency pill, used by other UIs that want to
    // anchor themselves directly below the currency display.
    uint64_t getTicketDisplayEntityId() const { return ticketBgId; }

private:
    void createButtons();
    void updateMissionButtonVisibility();

    float screenWidth = 0.0f;
    float screenHeight = 0.0f;

    std::function<void()> inventoryToggle;
    std::function<void()> missionToggle;

    bool buttonsCreated = false;
    bool missionButtonVisible = true;
    bool ticketDisplayVisible = false;

    // Entity IDs
    uint64_t buttonBgId[NUM_BUTTONS]   = {};
    uint64_t buttonIconId[NUM_BUTTONS] = {};

    // Ticket currency display
    uint64_t ticketBgId = 0;
    uint64_t ticketIconId = 0;
    uint64_t ticketTextId = 0;
    uint16_t lastTicketCount = 0;

    void createTicketDisplay();
    void onTicketsChanged(uint32_t newCount);

    // Mission notification badge — lights up when a mission becomes
    // unlocked or a main mission's materials become satisfied. Cleared
    // when the player opens the mission tab.
    static constexpr float MISSION_BADGE_SIZE = 10.0f;
    uint64_t missionBadgeId = 0;
    int prevUnlockCount = -1;     // sentinel: first tick seeds without arming
    int prevCompletableCount = -1;
    bool missionTabOpen = false;

    void createMissionBadge();
    void updateMissionBadge();
    int countUnlockedMissions() const;
    int countCompletableMainMissions() const;
    void setEntityVisibility(uint64_t id, bool vis);
};
