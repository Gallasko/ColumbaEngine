#pragma once

#include "Systems/basicsystems.h"
#include "Input/inputcomponent.h"

#include "depotsystem.h"
#include "inventoryui.h"
#include "playerinventory.h"
#include "missionsystem.h"

class CraftingUISystem;

using namespace pg;

// Side-panel UI for Depot (tileId 10).
// Shows a 4-slot grid (2 columns x 2 rows) for Robot Core input / reward output.
// Mirrors the StorageUISystem pattern.
class DepotUISystem : public System<Listener<ResizeEvent>,
                                     QueuedListener<OnSDLScanCode>,
                                     QueuedListener<TickEvent>,
                                     QueuedListener<OnMouseClick>,
                                     Listener<InventoryClosedEvent>>
{
public:
    static constexpr size_t UI_VP             = 2;
    static constexpr size_t NUM_SLOTS         = 4;  // Input slots
    static constexpr size_t NUM_OUTPUT_SLOTS  = 4;  // Output slots
    static constexpr size_t COLS              = 2;
    static constexpr size_t ROWS              = 2;
    static constexpr float SLOT_SIZE          = 40.0f;
    static constexpr float ITEM_SIZE          = 28.0f;
    static constexpr float PANEL_PADDING      = 12.0f;
    static constexpr float SLOT_SPACING       = 4.0f;
    static constexpr float GAP_AFTER_TITLE    = 6.0f;
    static constexpr float SECTION_GAP        = 8.0f;
    static constexpr float TITLE_H            = 20.0f;
    static constexpr float TEXT_SCALE         = 0.3f;
    static constexpr float TITLE_SCALE        = 0.4f;
    static constexpr float GAP_BETWEEN_PANELS = 8.0f;

    // Mission section layout
    static constexpr float MISSION_ROW_H   = 24.0f;
    static constexpr float MISSION_ROW_GAP = 3.0f;
    static constexpr size_t MAX_MISSION_ROWS = 5;
    static constexpr float BTN_W           = 50.0f;
    static constexpr float BTN_H           = 20.0f;
    static constexpr float BTN_TEXT_SCALE  = 0.25f;
    static constexpr float PROGRESS_H      = 8.0f;

    static constexpr const char* FONT_PATH = "res/font/Inter/static/Inter_28pt-Light.ttf";

    DepotUISystem(DepotSystem* depotSystem, ItemRegistry* itemRegistry,
                  PlayerInventorySystem* playerInv, InventoryUISystem* inventoryUI,
                  MissionSystem* missionSystem,
                  float screenWidth, float screenHeight)
        : depotSystem(depotSystem), itemRegistry(itemRegistry),
          playerInv(playerInv), inventoryUI(inventoryUI),
          missionSystem(missionSystem),
          screenWidth(screenWidth), screenHeight(screenHeight) {}

    virtual std::string getSystemName() const override { return "Depot UI System"; }

    void setCraftingUI(CraftingUISystem* ui) { craftingUI = ui; }

    bool isOpen() const { return visible; }

    void open(int gridX, int gridY);
    void close();

    virtual void onEvent(const ResizeEvent& event) override
    {
        screenWidth = event.width;
        screenHeight = event.height;
    }

    virtual void onProcessEvent(const OnSDLScanCode& event) override;
    virtual void onProcessEvent(const TickEvent&) override;
    virtual void onProcessEvent(const OnMouseClick& event) override;
    virtual void onEvent(const InventoryClosedEvent&) override;

private:
    float getPanelWidth() const  { return COLS * SLOT_SIZE + (COLS - 1) * SLOT_SPACING + 2.0f * PANEL_PADDING; }
    float getPanelHeight() const { return PANEL_PADDING + TITLE_H + GAP_AFTER_TITLE
                                        + ROWS * SLOT_SIZE + (ROWS - 1) * SLOT_SPACING
                                        + SECTION_GAP + TITLE_H + GAP_AFTER_TITLE
                                        + ROWS * SLOT_SIZE + (ROWS - 1) * SLOT_SPACING
                                        + PANEL_PADDING; }
    float getPanelX() const;
    float getPanelY() const;

    // Mission panel (RIGHT of inventory)
    float getMissionPanelWidth() const  { return getPanelWidth(); }
    float getMissionPanelHeight() const { return PANEL_PADDING + TITLE_H + GAP_AFTER_TITLE
                                              + MAX_MISSION_ROWS * (MISSION_ROW_H + MISSION_ROW_GAP)
                                              + PANEL_PADDING; }
    float getMissionPanelX() const;
    float getMissionPanelY() const;

    void ensurePanelCreated();
    void setPanelVisibility(bool vis);
    void createPanel();

    void refreshSlot(size_t slotIndex);
    void refreshAllSlots();

    bool isClickOnSlot(size_t i, float x, float y) const
    {
        return x >= cachedSlotX[i] and x <= cachedSlotX[i] + SLOT_SIZE
           and y >= cachedSlotY[i] and y <= cachedSlotY[i] + SLOT_SIZE;
    }
    bool isClickOnOutputSlot(size_t i, float x, float y) const
    {
        return x >= cachedOutputSlotX[i] and x <= cachedOutputSlotX[i] + SLOT_SIZE
           and y >= cachedOutputSlotY[i] and y <= cachedOutputSlotY[i] + SLOT_SIZE;
    }

    void setEntityVisibility(uint64_t id, bool vis);
    void setEntityText(uint64_t id, const std::string& text);
    void refreshItemSlotDisplay(uint64_t itemEntId, uint64_t countEntId,
                                const ItemStack& stack);

    // Mission section
    void createMissionSection();
    void refreshMissionSection();

    // --- Members ---

    DepotSystem* depotSystem = nullptr;
    ItemRegistry* itemRegistry = nullptr;
    PlayerInventorySystem* playerInv = nullptr;
    InventoryUISystem* inventoryUI = nullptr;
    MissionSystem* missionSystem = nullptr;
    CraftingUISystem* craftingUI = nullptr;
    float screenWidth = 0.0f;
    float screenHeight = 0.0f;

    bool visible = false;
    bool panelCreated = false;
    int openDepotX = -1;
    int openDepotY = -1;

    // Cached slot positions for hit testing
    float cachedSlotX[NUM_SLOTS] = {};
    float cachedSlotY[NUM_SLOTS] = {};
    float cachedOutputSlotX[NUM_OUTPUT_SLOTS] = {};
    float cachedOutputSlotY[NUM_OUTPUT_SLOTS] = {};

    // Entity IDs
    uint64_t backdropEntityId = 0;
    uint64_t titleEntityId    = 0;
    uint64_t slotBgEntityId[NUM_SLOTS]   = {};
    uint64_t slotItemEntityId[NUM_SLOTS] = {};
    uint64_t slotCountEntityId[NUM_SLOTS] = {};

    // Output section
    uint64_t outputTitleEntityId = 0;
    uint64_t outputSlotBgEntityId[NUM_OUTPUT_SLOTS]   = {};
    uint64_t outputSlotItemEntityId[NUM_OUTPUT_SLOTS] = {};
    uint64_t outputSlotCountEntityId[NUM_OUTPUT_SLOTS] = {};

    // Mission panel (right side)
    uint64_t missionPanelBackdropId = 0;
    uint64_t missionSectionTitleId = 0;

    // Active mission display (when depot has an active mission)
    uint64_t activeMissionNameId = 0;
    uint64_t activeMissionProgressBgId = 0;
    uint64_t activeMissionProgressFillId = 0;
    uint64_t activeMissionStatusId = 0;
    uint64_t activeMissionClaimBtnBgId = 0;
    uint64_t activeMissionClaimBtnTextId = 0;
    float claimBtnX = 0, claimBtnY = 0;

    // Available mission rows (when no active mission)
    struct MissionRow
    {
        uint64_t bgId = 0;
        uint64_t nameId = 0;
        uint64_t infoId = 0;
        uint64_t btnBgId = 0;
        uint64_t btnTextId = 0;
        float btnX = 0, btnY = 0;
        size_t defIndex = 0;
    };
    MissionRow missionRows[MAX_MISSION_ROWS] = {};
};
