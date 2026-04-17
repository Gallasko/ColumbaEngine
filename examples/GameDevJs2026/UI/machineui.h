#pragma once

#include "Systems/basicsystems.h"
#include "Input/inputcomponent.h"

#include "craftingsystem.h"
#include "inventoryui.h"
#include "playerinventory.h"

using namespace pg;

class CraftingUISystem;

// Side-panel UI for Furnace (tileId 5) and Assembler (tileId 6).
// Shows input slot(s), output slot and a crafting progress bar.
// Mirrors the MinerUISystem pattern exactly.
class MachineUISystem : public System<QueuedListener<OnSDLScanCode>,
                                      QueuedListener<TickEvent>,
                                      QueuedListener<OnMouseClick>,
                                      Listener<InventoryClosedEvent>>
{
public:
    static constexpr size_t UI_VP             = 2;
    static constexpr float SLOT_SIZE          = 40.0f;
    static constexpr float ITEM_SIZE          = 28.0f;
    static constexpr float PANEL_PADDING      = 12.0f;
    static constexpr float SLOT_SPACING       = 4.0f;
    static constexpr float ARROW_GAP          = 20.0f;
    static constexpr float PROGRESS_H         = 8.0f;
    static constexpr float GAP_AFTER_TITLE    = 6.0f;
    static constexpr float GAP_AFTER_SLOTS    = 8.0f;
    static constexpr float TITLE_H            = 20.0f;
    static constexpr float TEXT_SCALE         = 0.3f;
    static constexpr float TITLE_SCALE        = 0.4f;
    static constexpr float GAP_BETWEEN_PANELS = 8.0f;

    static constexpr const char* FONT_PATH = "res/font/Inter/static/Inter_28pt-Light.ttf";

    MachineUISystem(CraftingSystem* craftingSystem, ItemRegistry* itemRegistry,
                    PlayerInventorySystem* playerInv, InventoryUISystem* inventoryUI,
                    float screenWidth, float screenHeight)
        : craftingSystem(craftingSystem), itemRegistry(itemRegistry),
          playerInv(playerInv), inventoryUI(inventoryUI),
          screenWidth(screenWidth), screenHeight(screenHeight) {}

    virtual std::string getSystemName() const override { return "Machine UI System"; }

    bool isOpen() const { return visible; }
    uint16_t getOpenMachineType() const { return openMachineType; }

    bool isClickOnPanel(float x, float y) const;

    void setCraftingUI(CraftingUISystem* ui) { craftingUI = ui; }

    void open(int gridX, int gridY, uint16_t tileId);
    void close();

    virtual void onProcessEvent(const OnSDLScanCode& event) override;
    virtual void onProcessEvent(const TickEvent&) override;
    virtual void onProcessEvent(const OnMouseClick& event) override;
    virtual void onEvent(const InventoryClosedEvent&) override;

    // Called by the machine-feed callback when the player double-clicks a recipe.
    // Checks the player has all ingredients and moves them into the machine input slots.
    void feedMachineFromPlayer(const Recipe& recipe);

private:
    // Panel width  = 2*SLOT_SIZE + ARROW_GAP + 2*PANEL_PADDING = 124px
    // Panel height = PANEL_PADDING + TITLE_H + GAP_AFTER_TITLE
    //              + 2*SLOT_SIZE + SLOT_SPACING + GAP_AFTER_SLOTS
    //              + PROGRESS_H + PANEL_PADDING = 150px (max, assembler)
    float getPanelWidth() const  { return 2.0f * SLOT_SIZE + ARROW_GAP + 2.0f * PANEL_PADDING; }
    float getPanelHeight() const { return PANEL_PADDING + TITLE_H + GAP_AFTER_TITLE
                                        + 2.0f * SLOT_SIZE + SLOT_SPACING + GAP_AFTER_SLOTS
                                        + PROGRESS_H + PANEL_PADDING; }
    float getPanelX() const;
    float getPanelY() const;

    void ensurePanelCreated();
    void setPanelVisibility(bool vis);
    void updateForMachineType();
    void createPanel();

    void refreshSlot(size_t slotIndex, bool isInput);
    void refreshAllSlots();
    void refreshProgressBar();

    bool isClickOnInputSlot(size_t i, float x, float y) const
    {
        return x >= cachedInputSlotX[i] and x <= cachedInputSlotX[i] + SLOT_SIZE
           and y >= cachedInputSlotY[i] and y <= cachedInputSlotY[i] + SLOT_SIZE;
    }
    bool isClickOnOutputSlot(float x, float y) const
    {
        return x >= cachedOutputSlotX and x <= cachedOutputSlotX + SLOT_SIZE
           and y >= cachedOutputSlotY and y <= cachedOutputSlotY + SLOT_SIZE;
    }

    void setEntityVisibility(uint64_t id, bool vis);
    void refreshItemSlotDisplay(uint64_t itemEntId, uint64_t countEntId,
                                float slotX, float slotY, const ItemStack& stack);

    // --- Members ---

    CraftingSystem* craftingSystem = nullptr;
    ItemRegistry* itemRegistry = nullptr;
    PlayerInventorySystem* playerInv = nullptr;
    InventoryUISystem* inventoryUI = nullptr;
    CraftingUISystem* craftingUI = nullptr;
    float screenWidth = 0.0f;
    float screenHeight = 0.0f;

    bool visible = false;
    bool panelCreated = false;
    int openMachineX = -1;
    int openMachineY = -1;
    uint16_t openMachineType = 0; // 5=Furnace, 6=Assembler

    // Cached slot positions for hit testing
    float cachedInputSlotX[2]  = {0.0f, 0.0f};
    float cachedInputSlotY[2]  = {0.0f, 0.0f};
    float cachedOutputSlotX = 0.0f;
    float cachedOutputSlotY = 0.0f;
    float cachedBarX = 0.0f;
    float cachedBarMaxW = 0.0f;

    // Entity IDs
    uint64_t backdropEntityId     = 0;
    uint64_t titleEntityId        = 0;
    uint64_t inputSlotBgEntityId[2]  = {0, 0};
    uint64_t inputItemEntityId[2]    = {0, 0};
    uint64_t inputCountEntityId[2]   = {0, 0};
    uint64_t outputSlotBgEntityId = 0;
    uint64_t outputItemEntityId   = 0;
    uint64_t outputCountEntityId  = 0;
    uint64_t progressBgEntityId   = 0;
    uint64_t progressFillEntityId = 0;
};
