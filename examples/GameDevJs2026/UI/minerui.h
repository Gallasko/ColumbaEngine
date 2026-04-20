#pragma once

#include "Systems/basicsystems.h"
#include "Input/inputcomponent.h"

#include "minersystem.h"
#include "inventoryui.h"
#include "playerinventory.h"

using namespace pg;

class MinerUISystem : public System<Listener<ResizeEvent>,
                                     QueuedListener<OnSDLScanCode>,
                                     QueuedListener<TickEvent>,
                                     QueuedListener<OnMouseClick>,
                                     Listener<InventoryClosedEvent>>
{
public:
    static constexpr size_t UI_VP = 2;
    static constexpr float SLOT_SIZE = 40.0f;
    static constexpr float ITEM_SIZE = 28.0f;
    static constexpr float PANEL_PADDING = 12.0f;
    static constexpr float TEXT_SCALE = 0.3f;
    static constexpr float TITLE_SCALE = 0.4f;
    static constexpr float PROGRESS_BAR_HEIGHT = 8.0f;
    static constexpr float PROGRESS_BAR_WIDTH = SLOT_SIZE;
    static constexpr float GAP_BETWEEN_PANELS = 8.0f;

    static constexpr const char* FONT_PATH = "res/font/Inter/static/Inter_28pt-Light.ttf";

    MinerUISystem(MinerSystem* minerSystem, ItemRegistry* itemRegistry,
                  PlayerInventorySystem* playerInv, InventoryUISystem* inventoryUI,
                  float screenWidth, float screenHeight)
        : minerSystem(minerSystem), itemRegistry(itemRegistry),
          playerInv(playerInv), inventoryUI(inventoryUI),
          screenWidth(screenWidth), screenHeight(screenHeight) {}

    virtual std::string getSystemName() const override { return "Miner UI System"; }

    bool isOpen() const { return visible; }

    bool isClickOnPanel(float x, float y) const;

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
    // Compute panel position: to the left of the inventory panel
    float getPanelX() const;
    float getPanelY() const;

    void ensurePanelCreated();
    void setPanelVisibility(bool vis);
    void createPanel();
    void refreshSlot();
    void refreshProgressBar();

    // --- Click Handling ---

    bool isClickOnSlot(float x, float y) const
    {
        return x >= cachedSlotX and x <= cachedSlotX + SLOT_SIZE
           and y >= cachedSlotY and y <= cachedSlotY + SLOT_SIZE;
    }

    // --- Helpers ---

    void setEntityVisibility(uint64_t id, bool vis);

    // --- Members ---

    MinerSystem* minerSystem = nullptr;
    ItemRegistry* itemRegistry = nullptr;
    PlayerInventorySystem* playerInv = nullptr;
    InventoryUISystem* inventoryUI = nullptr;
    float screenWidth = 0.0f;
    float screenHeight = 0.0f;

    bool visible = false;
    bool panelCreated = false;
    int openMinerX = -1;
    int openMinerY = -1;

    ItemStack lastDisplayedStack;

    // Cached layout positions
    float cachedSlotX = 0.0f;
    float cachedSlotY = 0.0f;

    // Entity IDs
    uint64_t backdropEntityId = 0;
    uint64_t titleEntityId = 0;
    uint64_t slotBgEntityId = 0;
    uint64_t itemEntityId = 0;
    uint64_t countTextEntityId = 0;
    uint64_t progressBgEntityId = 0;
    uint64_t progressFillEntityId = 0;
};
