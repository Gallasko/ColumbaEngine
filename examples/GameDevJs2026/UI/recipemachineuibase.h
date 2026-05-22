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
class RecipeMachineUIBase : public System<Listener<ResizeEvent>,
                    QueuedListener<OnSDLScanCode>,
                    QueuedListener<TickEvent>,
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
                        ItemRegistry* itemRegistry,
                        float screenWidth, float screenHeight)
        : numInputs(numInputs), machineNameLabel(std::move(machineNameLabel)),
          itemRegistry(itemRegistry),
          screenWidth(screenWidth), screenHeight(screenHeight) {}

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
    void refreshProgressBar();

    float getPanelWidth() const  { return 2.0f * SLOT_SIZE + ARROW_GAP + 2.0f * PANEL_PADDING; }
    // Panel height covers the worst case (maxInputs = 2). updateForMachineType
    // shrinks the bg for 1-input UIs.
    float getPanelHeight() const { return PANEL_PADDING + TITLE_H + GAP_AFTER_TITLE +
                                          2.0f * SLOT_SIZE + SLOT_SPACING + GAP_AFTER_SLOTS +
                                          PROGRESS_H + PANEL_PADDING; }

    // Configuration (set once in constructor)
    int numInputs;
    std::string machineNameLabel;

    // Dependencies
    ItemRegistry* itemRegistry = nullptr;
    float screenWidth = 0.0f;
    float screenHeight = 0.0f;

    // Per-open state
    bool visible = false;
    bool panelCreated = false;
    int openMachineX = -1;
    int openMachineY = -1;
    std::string openMachineName;

    // Entity handles populated post-build by walking the prefab tree. Saves a per-access
    // hash-map lookup compared to storing _unique_id and re-resolving via ecs->getEntity.
    EntityRef backdrop;          // outer Prefab wrap returned by buildNode (carries UiAnchor)
    EntityRef bgLeaf;            // inner Shape2D backdrop — what other entities anchor to
    EntityRef title;
    EntityRef inputSlots[2];
    EntityRef outputSlot;
    EntityRef progressBg;
    EntityRef progressFill;
    EntityRef demoBtnBg;
    EntityRef demoBtnText;

    float cachedBarMaxW = 0.0f;
};
