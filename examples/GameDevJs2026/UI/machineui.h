#pragma once

#include "Systems/basicsystems.h"
#include "Input/inputcomponent.h"

#include "craftingsystem.h"
#include "inventoryui.h"
#include "slotsystem.h"
#include "playerinventory.h"

using namespace pg;

class CraftingUISystem;
class MachineDemoSystem;

// Side-panel UI for Furnace and Assembler.
// Shows input slot(s), output slot and a crafting progress bar.
class MachineUISystem : public System<Listener<ResizeEvent>,
                                      QueuedListener<OnSDLScanCode>,
                                      QueuedListener<TickEvent>,
                                      QueuedListener<SlotPickedUpEvent>,
                                      QueuedListener<SlotDroppedEvent>,
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
                    SlotSystem* slotSystem,
                    float screenWidth, float screenHeight)
        : craftingSystem(craftingSystem), itemRegistry(itemRegistry),
          playerInv(playerInv), inventoryUI(inventoryUI), slotSystem(slotSystem),
          screenWidth(screenWidth), screenHeight(screenHeight) {}

    virtual std::string getSystemName() const override { return "Machine UI System"; }

    bool isOpen() const { return visible; }
    const std::string& getOpenMachineName() const { return openMachineName; }

    void setCraftingUI(CraftingUISystem* ui) { craftingUI = ui; }
    void setMachineDemo(MachineDemoSystem* demo) { machineDemo = demo; }

    void open(int gridX, int gridY, const std::string& tileName);
    void close();

    virtual void onEvent(const ResizeEvent& event) override
    {
        screenWidth = event.width;
        screenHeight = event.height;
    }

    virtual void onProcessEvent(const OnSDLScanCode& event) override;
    virtual void onProcessEvent(const TickEvent&) override;
    virtual void onProcessEvent(const SlotPickedUpEvent& event) override;
    virtual void onProcessEvent(const SlotDroppedEvent& event) override;
    virtual void onProcessEvent(const OnMouseClick& event) override;
    virtual void onEvent(const InventoryClosedEvent&) override;

    // Called by the machine-feed callback when the player double-clicks a recipe.
    // Checks the player has all ingredients and moves them into the machine input slots.
    void feedMachineFromPlayer(const Recipe& recipe);

private:
    float getPanelWidth() const  { return 2.0f * SLOT_SIZE + ARROW_GAP + 2.0f * PANEL_PADDING; }
    float getPanelHeight() const { return PANEL_PADDING + TITLE_H + GAP_AFTER_TITLE
                                        + 2.0f * SLOT_SIZE + SLOT_SPACING + GAP_AFTER_SLOTS
                                        + PROGRESS_H + PANEL_PADDING; }

    void ensurePanelCreated();
    void setPanelVisibility(bool vis);
    void updateForMachineType();
    void createPanel();

    void syncAllSlots();
    void syncSlotToMachine(size_t slotIndex, bool isInput);
    void refreshProgressBar();

    void setEntityVisibility(uint64_t id, bool vis);

    // --- Members ---

    CraftingSystem* craftingSystem = nullptr;
    ItemRegistry* itemRegistry = nullptr;
    PlayerInventorySystem* playerInv = nullptr;
    InventoryUISystem* inventoryUI = nullptr;
    SlotSystem* slotSystem = nullptr;
    CraftingUISystem* craftingUI = nullptr;
    MachineDemoSystem* machineDemo = nullptr;
    float screenWidth = 0.0f;
    float screenHeight = 0.0f;

    bool visible = false;
    bool panelCreated = false;
    int openMachineX = -1;
    int openMachineY = -1;
    std::string openMachineName;

    // Entity IDs
    uint64_t backdropEntityId     = 0;
    uint64_t titleEntityId        = 0;
    uint64_t inputSlotEntityIds[2]  = {0, 0};
    uint64_t outputSlotEntityId   = 0;
    uint64_t progressBgEntityId   = 0;
    uint64_t progressFillEntityId = 0;
    uint64_t demoBtnBgEntityId    = 0;
    uint64_t demoBtnTextEntityId  = 0;

    float cachedBarMaxW = 0.0f;
};
