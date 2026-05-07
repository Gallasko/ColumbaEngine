#pragma once

#include "Systems/basicsystems.h"
#include "ECS/entitysystem_fwd.h"
#include "Input/inputcomponent.h"

#include "playerinventory.h"
#include "itemregistry.h"
#include "slotsystem.h"

using namespace pg;

struct InventoryOpenedEvent {};
struct InventoryClosedEvent {};
struct PanelWasClickedEvent {};

class InventoryUISystem : public System<InitSys,
                                        Listener<ResizeEvent>,
                                        QueuedListener<OnSDLScanCode>,
                                        QueuedListener<SlotPickedUpEvent>,
                                        QueuedListener<SlotDroppedEvent>>
{
public:
    static constexpr size_t INV_UI_VIEWPORT = 2;
    static constexpr size_t COLS = 5;
    static constexpr size_t ROWS = 4;
    static constexpr size_t NUM_SLOTS = COLS * ROWS;
    static constexpr float SLOT_SIZE = 40.0f;
    static constexpr float SLOT_SPACING = 4.0f;
    static constexpr float PANEL_PADDING = 12.0f;
    static constexpr float ITEM_SIZE = 28.0f;
    static constexpr float TEXT_SCALE = 0.3f;

    static constexpr const char* FONT_PATH = "res/font/Inter/static/Inter_28pt-Light.ttf";

    InventoryUISystem(PlayerInventorySystem* playerInv, ItemRegistry* itemRegistry,
                      SlotSystem* slotSystem,
                      float screenWidth, float screenHeight)
        : playerInv(playerInv), itemRegistry(itemRegistry), slotSystem(slotSystem),
          screenWidth(screenWidth), screenHeight(screenHeight) {}

    virtual std::string getSystemName() const override { return "Inventory UI System"; }

    void init() override;

    bool isOpen() const { return visible; }
    uint64_t getBackdropEntityId() const { return backdropEntityId; }

    // Returns the slot prefab entity for the given main-inventory slot index
    // (0..NUM_SLOTS-1). Used by the tutorial system for pulse-on-craft visuals.
    uint64_t getSlotEntityId(size_t i) const
    {
        return (i < NUM_SLOTS) ? slotEntityIds[i] : 0;
    }
    static constexpr size_t getNumSlots() { return NUM_SLOTS; }

    // Returns the ItemId under screen-space (x,y), or ITEM_NONE if panel is
    // closed or the position is not over a non-empty slot.
    ItemId itemAtPosition(float x, float y) const;

    // --- Held Item Support (delegates to SlotSystem) ---

    bool hasHeldItem() const { return slotSystem->hasHeldItem(); }
    void cancelHeld();

    // --- Event Handlers ---

    virtual void onEvent(const ResizeEvent& event) override
    {
        screenWidth = event.width;
        screenHeight = event.height;

        if (visible)
            refreshAllSlots();
    }

    virtual void onProcessEvent(const OnSDLScanCode& event) override;
    virtual void onProcessEvent(const SlotPickedUpEvent& event) override;
    virtual void onProcessEvent(const SlotDroppedEvent& event) override;

    // --- Open / Close ---

    void openInventory();
    void closeInventory();

    // Refresh all slot visuals to match the current player inventory state.
    // Safe to call from external systems (e.g. HandCraftingSystem after a
    // craft completes). Does nothing if the panel hasn't been built yet.
    void refreshAllSlots();

private:
    // --- Panel Creation / Visibility ---

    void ensurePanelCreated();
    void setPanelVisibility(bool vis);
    void createPanel();

    // --- Sync ---

    void syncAllSlots();
    void syncSlotToInventory(size_t index);

    // --- Helpers ---

    void setEntityVisibility(uint64_t id, bool vis);

    // --- Members ---

    PlayerInventorySystem* playerInv = nullptr;
    ItemRegistry* itemRegistry = nullptr;
    SlotSystem* slotSystem = nullptr;
    float screenWidth = 0.0f;
    float screenHeight = 0.0f;

    bool visible = false;
    bool panelCreated = false;

    uint64_t backdropEntityId = 0;
    uint64_t slotEntityIds[NUM_SLOTS] = {};
};
