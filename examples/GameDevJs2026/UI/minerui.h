#pragma once

#include "Systems/basicsystems.h"
#include "Input/inputcomponent.h"

#include "imachineui.h"
#include "minersystem.h"
#include "inventoryui.h"
#include "slotsystem.h"
#include "playerinventory.h"

class MinerUISystem : public pg::System<pg::Listener<pg::ResizeEvent>,
                                     pg::QueuedListener<pg::OnSDLScanCode>,
                                     pg::QueuedListener<pg::TickEvent>,
                                     pg::Listener<InventoryClosedEvent>>,
                      public IMachineUI
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

    MinerUISystem(ItemRegistry* itemRegistry,
                  float screenWidth, float screenHeight)
        : itemRegistry(itemRegistry),
          screenWidth(screenWidth), screenHeight(screenHeight) {}

    virtual std::string getSystemName() const override { return "Miner UI System"; }

    // ---- IMachineUI ----
    MachineUIDescriptor descriptor() const override
    {
        return {true /*requiresInventory*/, false, false};
    }
    bool isOpen() const override { return visible; }
    std::string getOpenMachineName() const override { return visible ? "Miner" : std::string{}; }
    void open(int gridX, int gridY, const std::string& /*machineName*/) override
    {
        open(gridX, gridY);
    }
    void close() override;

    void open(int gridX, int gridY);

    virtual void onEvent(const pg::ResizeEvent& event) override
    {
        screenWidth = event.width;
        screenHeight = event.height;
    }

    virtual void onProcessEvent(const pg::OnSDLScanCode& event) override;
    virtual void onProcessEvent(const pg::TickEvent&) override;
    virtual void onEvent(const InventoryClosedEvent&) override;

private:
    void ensurePanelCreated();
    void setPanelVisibility(bool vis);
    void createPanel();
    void refreshProgressBar();

    void setEntityVisibility(uint64_t id, bool vis);

    // --- Members ---

    ItemRegistry* itemRegistry = nullptr;
    float screenWidth = 0.0f;
    float screenHeight = 0.0f;

    bool visible = false;
    bool panelCreated = false;
    int openMinerX = -1;
    int openMinerY = -1;

    // Entity IDs
    uint64_t backdropEntityId = 0;
    uint64_t titleEntityId = 0;
    uint64_t slotEntityId = 0;
    uint64_t progressBgEntityId = 0;
    uint64_t progressFillEntityId = 0;
};
