#pragma once

#include "Systems/basicsystems.h"
#include "Input/inputcomponent.h"

#include "depotsystem.h"
#include "inventoryui.h"
#include "playerinventory.h"

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
    static constexpr size_t NUM_SLOTS         = 4;
    static constexpr size_t COLS              = 2;
    static constexpr size_t ROWS              = 2;
    static constexpr float SLOT_SIZE          = 40.0f;
    static constexpr float ITEM_SIZE          = 28.0f;
    static constexpr float PANEL_PADDING      = 12.0f;
    static constexpr float SLOT_SPACING       = 4.0f;
    static constexpr float GAP_AFTER_TITLE    = 6.0f;
    static constexpr float TITLE_H            = 20.0f;
    static constexpr float TEXT_SCALE         = 0.3f;
    static constexpr float TITLE_SCALE        = 0.4f;
    static constexpr float GAP_BETWEEN_PANELS = 8.0f;

    static constexpr const char* FONT_PATH = "res/font/Inter/static/Inter_28pt-Light.ttf";

    DepotUISystem(DepotSystem* depotSystem, ItemRegistry* itemRegistry,
                  PlayerInventorySystem* playerInv, InventoryUISystem* inventoryUI,
                  float screenWidth, float screenHeight)
        : depotSystem(depotSystem), itemRegistry(itemRegistry),
          playerInv(playerInv), inventoryUI(inventoryUI),
          screenWidth(screenWidth), screenHeight(screenHeight) {}

    virtual std::string getSystemName() const override { return "Depot UI System"; }

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
                                        + ROWS * SLOT_SIZE + (ROWS - 1) * SLOT_SPACING + PANEL_PADDING; }
    float getPanelX() const;
    float getPanelY() const;

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

    void setEntityVisibility(uint64_t id, bool vis);
    void refreshItemSlotDisplay(uint64_t itemEntId, uint64_t countEntId,
                                const ItemStack& stack);

    // --- Members ---

    DepotSystem* depotSystem = nullptr;
    ItemRegistry* itemRegistry = nullptr;
    PlayerInventorySystem* playerInv = nullptr;
    InventoryUISystem* inventoryUI = nullptr;
    float screenWidth = 0.0f;
    float screenHeight = 0.0f;

    bool visible = false;
    bool panelCreated = false;
    int openDepotX = -1;
    int openDepotY = -1;

    // Cached slot positions for hit testing
    float cachedSlotX[NUM_SLOTS] = {};
    float cachedSlotY[NUM_SLOTS] = {};

    // Entity IDs
    uint64_t backdropEntityId = 0;
    uint64_t titleEntityId    = 0;
    uint64_t slotBgEntityId[NUM_SLOTS]   = {};
    uint64_t slotItemEntityId[NUM_SLOTS] = {};
    uint64_t slotCountEntityId[NUM_SLOTS] = {};
};
