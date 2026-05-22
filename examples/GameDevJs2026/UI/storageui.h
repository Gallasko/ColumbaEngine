#pragma once

#include "Systems/basicsystems.h"
#include "Input/inputcomponent.h"

#include "imachineui.h"
#include "storagesystem.h"
#include "inventoryui.h"
#include "slotsystem.h"
#include "playerinventory.h"

using namespace pg;

// Side-panel UI for Storage (tileId 9).
// Shows an 8-slot grid (2 columns x 4 rows) for drag-and-drop item management.
class StorageUISystem : public System<Listener<ResizeEvent>,
                                      QueuedListener<OnSDLScanCode>,
                                      QueuedListener<TickEvent>,
                                      Listener<InventoryClosedEvent>>,
                        public IMachineUI
{
public:
    static constexpr size_t UI_VP             = 2;
    static constexpr size_t NUM_SLOTS         = 8;
    static constexpr size_t COLS              = 2;
    static constexpr size_t ROWS              = 4;
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

    StorageUISystem(ItemRegistry* itemRegistry,
                    float screenWidth, float screenHeight)
        : itemRegistry(itemRegistry),
          screenWidth(screenWidth), screenHeight(screenHeight) {}

    virtual std::string getSystemName() const override { return "Storage UI System"; }

    // ---- IMachineUI ----
    MachineUIDescriptor descriptor() const override
    {
        return {true /*requiresInventory*/, false, false};
    }
    bool isOpen() const override { return visible; }
    std::string getOpenMachineName() const override { return visible ? "Storage" : std::string{}; }
    void open(int gridX, int gridY, const std::string& /*machineName*/) override
    {
        open(gridX, gridY);
    }
    void close() override;

    void open(int gridX, int gridY);

    virtual void onEvent(const ResizeEvent& event) override
    {
        screenWidth = event.width;
        screenHeight = event.height;
    }

    virtual void onProcessEvent(const OnSDLScanCode& event) override;
    virtual void onProcessEvent(const TickEvent&) override;
    virtual void onEvent(const InventoryClosedEvent&) override;

private:
    float getPanelWidth() const  { return COLS * SLOT_SIZE + (COLS - 1) * SLOT_SPACING + 2.0f * PANEL_PADDING; }
    float getPanelHeight() const { return PANEL_PADDING + TITLE_H + GAP_AFTER_TITLE
                                        + ROWS * SLOT_SIZE + (ROWS - 1) * SLOT_SPACING + PANEL_PADDING; }

    void ensurePanelCreated();
    void setPanelVisibility(bool vis);
    void createPanel();

    void syncAllSlots();

    void setEntityVisibility(uint64_t id, bool vis);

    // --- Members ---

    ItemRegistry* itemRegistry = nullptr;
    float screenWidth = 0.0f;
    float screenHeight = 0.0f;

    bool visible = false;
    bool panelCreated = false;
    int openStorageX = -1;
    int openStorageY = -1;

    // Entity IDs
    uint64_t backdropEntityId = 0;
    uint64_t titleEntityId    = 0;
    uint64_t slotEntityIds[NUM_SLOTS] = {};
};
