#pragma once

#include "Systems/basicsystems.h"
#include "Input/inputcomponent.h"

#include "imachineui.h"
#include "minersystem.h"
#include "inventoryui.h"
#include "slotsystem.h"
#include "playerinventory.h"

using namespace pg;

class MinerUISystem : public System<Listener<ResizeEvent>,
                                     QueuedListener<OnSDLScanCode>,
                                     QueuedListener<TickEvent>,
                                     QueuedListener<SlotPickedUpEvent>,
                                     QueuedListener<SlotDroppedEvent>,
                                     Listener<InventoryClosedEvent>>,
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

    MinerUISystem(MinerSystem* minerSystem, ItemRegistry* itemRegistry,
                  PlayerInventorySystem* playerInv, InventoryUISystem* inventoryUI,
                  SlotSystem* slotSystem,
                  float screenWidth, float screenHeight)
        : minerSystem(minerSystem), itemRegistry(itemRegistry),
          playerInv(playerInv), inventoryUI(inventoryUI), slotSystem(slotSystem),
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

    virtual void onEvent(const ResizeEvent& event) override
    {
        screenWidth = event.width;
        screenHeight = event.height;
    }

    virtual void onProcessEvent(const OnSDLScanCode& event) override;
    virtual void onProcessEvent(const TickEvent&) override;
    virtual void onProcessEvent(const SlotPickedUpEvent& event) override;
    virtual void onProcessEvent(const SlotDroppedEvent& event) override;
    virtual void onEvent(const InventoryClosedEvent&) override;

private:
    void ensurePanelCreated();
    void setPanelVisibility(bool vis);
    void createPanel();
    void refreshProgressBar();

    void setEntityVisibility(uint64_t id, bool vis);

    // --- Members ---

    MinerSystem* minerSystem = nullptr;
    ItemRegistry* itemRegistry = nullptr;
    PlayerInventorySystem* playerInv = nullptr;
    InventoryUISystem* inventoryUI = nullptr;
    SlotSystem* slotSystem = nullptr;
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
