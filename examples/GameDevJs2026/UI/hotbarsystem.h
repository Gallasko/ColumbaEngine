#pragma once

#include "Systems/basicsystems.h"
#include "Input/inputcomponent.h"
#include "Renderer/renderer.h"

#include "playerinventory.h"
#include "itemregistry.h"
#include "buildingregistry.h"

class InventoryUISystem;

using namespace pg;

inline constexpr size_t HOTBAR_SLOTS = 9;

class HotbarSystem : public System<InitSys,
                                    Listener<OnSDLScanCode>,
                                    Listener<PlayerGainItemEvent>,
                                    Listener<PlayerLoseItemEvent>,
                                    QueuedListener<OnMouseClick>,
                                    QueuedListener<OnSDLMouseMotion>>
{
public:
    static constexpr float HOTBAR_HEIGHT = 48.0f;
    static constexpr float SLOT_SIZE = 32.0f;
    static constexpr float SLOT_SPACING = 4.0f;
    static constexpr float SLOT_PADDING = 8.0f;
    static constexpr float ITEM_SIZE = 24.0f;
    static constexpr float TEXT_SCALE = 0.25f;
    static constexpr size_t UI_VP = 2;

    static constexpr const char* FONT_PATH = "res/font/Inter/static/Inter_28pt-Light.ttf";

    HotbarSystem(PlayerInventorySystem* playerInv, ItemRegistry* itemRegistry,
                 BuildingRegistry* buildingRegistry,
                 float screenWidth, float screenHeight)
        : playerInv(playerInv), itemRegistry(itemRegistry),
          buildingRegistry(buildingRegistry),
          screenWidth(screenWidth), screenHeight(screenHeight) {}

    virtual std::string getSystemName() const override { return "Hotbar System"; }

    void init() override;

    // Event handlers
    virtual void onEvent(const OnSDLScanCode& event) override;
    virtual void onEvent(const PlayerGainItemEvent& event) override;
    virtual void onEvent(const PlayerLoseItemEvent& event) override;
    virtual void onProcessEvent(const OnMouseClick& event) override;
    virtual void onProcessEvent(const OnSDLMouseMotion& event) override;

    // Queries
    size_t getSelectedSlot() const { return selectedSlot; }

    const ItemStack& getSelectedItem() const
    {
        return playerInv->getInventory().getSlot(PlayerInventorySystem::HOTBAR_START + selectedSlot);
    }

    // Returns the BuildingDef for the selected item, or nullptr if not a building
    const BuildingDef* getSelectedBuildingDef() const
    {
        const auto& item = getSelectedItem();
        if (item.isEmpty())
            return nullptr;
        const auto& def = itemRegistry->get(item.id);
        if (def.buildingTileId == 0)
            return nullptr;
        return buildingRegistry->findByTileId(def.buildingTileId);
    }

    bool isMouseOverHotbar(float mouseY) const
    {
        return mouseY > screenHeight - HOTBAR_HEIGHT;
    }

    void setInventoryUI(InventoryUISystem* inv) { inventoryUI = inv; }

    // Consume one item from the selected hotbar slot (after building placement)
    void consumeSelectedItem(uint16_t count = 1);

    // Refresh all slot visuals
    void refreshAllSlots();
    void refreshSlot(size_t index);

    // Drag-swap: called by InventoryUISystem for cross-panel transfers
    ItemStack& getHotbarSlot(size_t index)
    {
        return playerInv->getInventory().getSlot(PlayerInventorySystem::HOTBAR_START + index);
    }

private:
    void selectSlot(size_t index);
    void createHotbarUI();
    void updateHighlight();

    int slotAtPosition(float x, float y) const;

    // Members
    PlayerInventorySystem* playerInv = nullptr;
    ItemRegistry* itemRegistry = nullptr;
    BuildingRegistry* buildingRegistry = nullptr;
    InventoryUISystem* inventoryUI = nullptr;
    float screenWidth = 0.0f;
    float screenHeight = 0.0f;

    size_t selectedSlot = 0;

    struct SlotVisual
    {
        uint64_t bgEntityId = 0;
        uint64_t itemEntityId = 0;
        uint64_t textEntityId = 0;
        float itemBaseX = 0.0f;
        float itemBaseY = 0.0f;
    };
    std::vector<SlotVisual> slotVisuals;
    uint64_t highlightEntityId = 0;
    uint64_t backdropEntityId = 0;

    float lastMouseX = 0.0f;
    float lastMouseY = 0.0f;
};
