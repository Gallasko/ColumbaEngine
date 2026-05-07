#pragma once

#include "Systems/basicsystems.h"
#include "Input/inputcomponent.h"
#include "Renderer/renderer.h"

#include "playerinventory.h"
#include "itemregistry.h"
#include "buildingregistry.h"
#include "inventoryui.h"
#include "slotsystem.h"

using namespace pg;

inline constexpr size_t HOTBAR_SLOTS = 9;

class HotbarSystem : public System<InitSys,
                                    Listener<OnSDLScanCode>,
                                    Listener<ResizeEvent>,
                                    Listener<PlayerGainItemEvent>,
                                    Listener<PlayerLoseItemEvent>,
                                    Listener<InventoryOpenedEvent>,
                                    Listener<InventoryClosedEvent>,
                                    QueuedListener<SlotClickedEvent>,
                                    QueuedListener<SlotPickedUpEvent>,
                                    QueuedListener<SlotDroppedEvent>>
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
                 BuildingRegistry* buildingRegistry, SlotSystem* slotSystem,
                 float screenWidth, float screenHeight)
        : playerInv(playerInv), itemRegistry(itemRegistry),
          buildingRegistry(buildingRegistry), slotSystem(slotSystem),
          screenWidth(screenWidth), screenHeight(screenHeight) {}

    virtual std::string getSystemName() const override { return "Hotbar System"; }

    void init() override;

    // Event handlers
    virtual void onEvent(const OnSDLScanCode& event) override;
    virtual void onEvent(const ResizeEvent& event) override;
    virtual void onEvent(const PlayerGainItemEvent& event) override;
    virtual void onEvent(const PlayerLoseItemEvent& event) override;
    virtual void onEvent(const InventoryOpenedEvent&) override;
    virtual void onEvent(const InventoryClosedEvent&) override;
    virtual void onProcessEvent(const SlotClickedEvent& event) override;
    virtual void onProcessEvent(const SlotPickedUpEvent& event) override;
    virtual void onProcessEvent(const SlotDroppedEvent& event) override;

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

        if (def.buildingName.empty())
            return nullptr;

        return buildingRegistry->findByName(def.buildingName);
    }

    bool isMouseOverHotbar(float mouseY) const
    {
        return mouseY > screenHeight - HOTBAR_HEIGHT;
    }

    // Returns the slot prefab entity for the given hotbar slot index
    // (0..HOTBAR_SLOTS-1). Used by the tutorial system for pulse-on-craft.
    uint64_t getSlotEntityId(size_t i) const
    {
        return (i < HOTBAR_SLOTS) ? slotEntityIds[i] : 0;
    }

    // Returns the ItemId at screen-space (x, y), or ITEM_NONE if not over a slot.
    ItemId itemAtPosition(float x, float y) const;

    // Consume one item from the selected hotbar slot (after building placement)
    void consumeSelectedItem(uint16_t count = 1);

    // Refresh all slot visuals from player inventory
    void refreshAllSlots();

    // Drag-swap: called by external code for cross-panel transfers
    ItemStack& getHotbarSlot(size_t index)
    {
        return playerInv->getInventory().getSlot(PlayerInventorySystem::HOTBAR_START + index);
    }

private:
    void selectSlot(size_t index);
    void createHotbarUI();
    void updateHighlight();
    void syncAllSlots();
    void syncSlotToInventory(size_t index);

    int slotAtPosition(float x, float y) const;

    // Members
    PlayerInventorySystem* playerInv = nullptr;
    ItemRegistry* itemRegistry = nullptr;
    BuildingRegistry* buildingRegistry = nullptr;
    SlotSystem* slotSystem = nullptr;
    float screenWidth = 0.0f;
    float screenHeight = 0.0f;

    size_t selectedSlot = 0;

    uint64_t slotEntityIds[HOTBAR_SLOTS] = {};
    uint64_t highlightEntityId = 0;
    uint64_t backdropEntityId = 0;
    uint64_t containerEntityId = 0;
};
