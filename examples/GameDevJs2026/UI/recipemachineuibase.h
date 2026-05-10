#pragma once

#include "Systems/basicsystems.h"
#include "Input/inputcomponent.h"

#include "imachineui.h"

#include "craftingsystem.h"
#include "inventoryui.h"
#include "slotsystem.h"
#include "playerinventory.h"

using namespace pg;

class CraftingUISystem;
class MachineDemoSystem;

// Shared implementation for machine UIs that follow the
// "input slots + output slot + progress bar + right-side recipe panel"
// shape (currently Furnace and Assembler).
//
// Concrete subclasses (FurnaceUI, AssemblerUI) supply numInputs and the
// machine display name in their constructor. The base owns the panel
// layout, slot wiring, recipe-panel callback hookup and the per-tick
// machine sync; subclasses may override individual methods if their
// behaviour diverges later.
class RecipeMachineUIBase
    : public System<Listener<ResizeEvent>,
                    QueuedListener<OnSDLScanCode>,
                    QueuedListener<TickEvent>,
                    QueuedListener<SlotPickedUpEvent>,
                    QueuedListener<SlotDroppedEvent>,
                    QueuedListener<OnMouseClick>,
                    Listener<InventoryClosedEvent>>,
      public IMachineUI
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

    RecipeMachineUIBase(int numInputs, std::string machineNameLabel,
                        CraftingSystem* craftingSystem, ItemRegistry* itemRegistry,
                        PlayerInventorySystem* playerInv, InventoryUISystem* inventoryUI,
                        SlotSystem* slotSystem,
                        float screenWidth, float screenHeight)
        : numInputs(numInputs), machineNameLabel(std::move(machineNameLabel)),
          craftingSystem(craftingSystem), itemRegistry(itemRegistry),
          playerInv(playerInv), inventoryUI(inventoryUI), slotSystem(slotSystem),
          screenWidth(screenWidth), screenHeight(screenHeight) {}

    void setCraftingUI(CraftingUISystem* ui) { craftingUI = ui; }
    void setMachineDemo(MachineDemoSystem* demo) { machineDemo = demo; }

    // ---- IMachineUI ----
    MachineUIDescriptor descriptor() const override
    {
        return {true /*requiresInventory*/, true /*wantsRecipePanel*/, false};
    }
    void open(int gridX, int gridY, const std::string& machineName) override;
    void close() override;
    bool isOpen() const override { return visible; }
    std::string getOpenMachineName() const override { return openMachineName; }

    // ---- Event listeners ----
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

    // Called by the recipe-panel double-click. Pulls ingredients from the
    // player into the open machine's input slots.
    void feedMachineFromPlayer(const Recipe& recipe);

protected:
    void ensurePanelCreated();
    void setPanelVisibility(bool vis);
    void updateForMachineType();
    void createPanel();

    void syncAllSlots();
    void syncSlotToMachine(size_t slotIndex, bool isInput);
    void refreshProgressBar();

    float getPanelWidth() const  { return 2.0f * SLOT_SIZE + ARROW_GAP + 2.0f * PANEL_PADDING; }
    // Panel height covers the worst case (maxInputs = 2). updateForMachineType
    // shrinks the bg for 1-input UIs.
    float getPanelHeight() const { return PANEL_PADDING + TITLE_H + GAP_AFTER_TITLE
                                        + 2.0f * SLOT_SIZE + SLOT_SPACING + GAP_AFTER_SLOTS
                                        + PROGRESS_H + PANEL_PADDING; }

    // Configuration (set once in constructor)
    int numInputs;
    std::string machineNameLabel;

    // Dependencies
    CraftingSystem* craftingSystem = nullptr;
    ItemRegistry* itemRegistry = nullptr;
    PlayerInventorySystem* playerInv = nullptr;
    InventoryUISystem* inventoryUI = nullptr;
    SlotSystem* slotSystem = nullptr;
    CraftingUISystem* craftingUI = nullptr;
    MachineDemoSystem* machineDemo = nullptr;
    float screenWidth = 0.0f;
    float screenHeight = 0.0f;

    // Per-open state
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
