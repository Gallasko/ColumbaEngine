#pragma once

#include "Systems/basicsystems.h"
#include "Input/inputcomponent.h"

#include "playerinventory.h"
#include "itemregistry.h"

#include <functional>

using namespace pg;

struct InventoryOpenedEvent {};
struct InventoryClosedEvent {};

class InventoryUISystem : public System<InitSys,
                                        QueuedListener<OnSDLScanCode>,
                                        QueuedListener<OnMouseClick>,
                                        QueuedListener<OnSDLMouseMotion>>
{
public:
    static constexpr size_t INV_UI_VIEWPORT = 2;
    static constexpr size_t COLS = 5;
    static constexpr size_t ROWS = 4;
    static constexpr float SLOT_SIZE = 40.0f;
    static constexpr float SLOT_SPACING = 4.0f;
    static constexpr float PANEL_PADDING = 12.0f;
    static constexpr float ITEM_SIZE = 28.0f; // Item square inside the slot
    static constexpr float TEXT_SCALE = 0.3f;

    static constexpr const char* FONT_PATH = "res/font/Inter/static/Inter_28pt-Light.ttf";

    InventoryUISystem(PlayerInventorySystem* playerInv, ItemRegistry* itemRegistry,
                      float screenWidth, float screenHeight)
        : playerInv(playerInv), itemRegistry(itemRegistry),
          screenWidth(screenWidth), screenHeight(screenHeight) {}

    virtual std::string getSystemName() const override { return "Inventory UI System"; }

    void init() override;

    bool isOpen() const { return visible; }

    bool isClickOnPanel(float x, float y) const;

    // --- External Slot Support (for miner UI, etc.) ---

    bool hasHeldItem() const { return not heldItem.isEmpty(); }

    void setExternalClickCheck(std::function<bool(float, float)> check)
    {
        externalClickCheck = std::move(check);
    }

    void pickUpFromExternal(ItemStack& slot);
    void dropOnExternal(ItemStack& slot);

    // --- Event Handlers ---

    virtual void onProcessEvent(const OnSDLScanCode& event) override;
    virtual void onProcessEvent(const OnMouseClick& event) override;
    virtual void onProcessEvent(const OnSDLMouseMotion& event) override;

    // --- Open / Close ---

    void openInventory();
    void closeInventory();
    void cancelHeld();

    // Refresh all slot visuals to match the current player inventory state.
    // Safe to call from external systems (e.g. HandCraftingSystem after a
    // craft completes). Does nothing if the panel hasn't been built yet.
    void refreshAllSlots();

    // Returns the held item to its source without any visual refresh.
    // Use this when the panel is about to be hidden (avoids creating
    // deferred entities that would immediately leak on panel hide).
    void cancelHeldDataOnly();

private:
    // --- Panel Creation / Visibility ---

    void ensurePanelCreated();
    void setPanelVisibility(bool vis);
    void createPanel();

    // --- Slot Refresh ---

    void refreshSlot(size_t index);

    // --- Drag & Drop ---

    void pickUpFromSlot(size_t slotIndex);
    void dropOnSlot(size_t slotIndex);
    void clearHeld();

    // --- Held Item Visual ---

    void showHeldVisual();
    void hideHeldVisual();
    void updateHeldPosition();

    // --- Hit Testing ---

    int slotAtPosition(float x, float y) const;
    std::pair<float, float> slotScreenPos(size_t index) const;

    // --- Helpers ---

    void setEntityVisibility(uint64_t id, bool vis);

    // --- Members ---

    PlayerInventorySystem* playerInv = nullptr;
    ItemRegistry* itemRegistry = nullptr;
    float screenWidth = 0.0f;
    float screenHeight = 0.0f;

    bool visible = false;
    bool panelCreated = false;

    uint64_t backdropEntityId = 0;

    struct SlotVisual
    {
        uint64_t bgEntityId = 0;
        uint64_t itemEntityId = 0;
        uint64_t textEntityId = 0;
    };
    std::vector<SlotVisual> slotVisuals;

    ItemStack heldItem;
    int heldFromSlot = -1;
    ItemStack* externalSourceSlot = nullptr;
    uint64_t heldItemEntityId = 0;
    uint64_t heldTextEntityId = 0;

    std::function<bool(float, float)> externalClickCheck;

    float lastMouseX = 0.0f;
    float lastMouseY = 0.0f;
};
