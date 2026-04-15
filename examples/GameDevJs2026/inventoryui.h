#pragma once

#include "Systems/basicsystems.h"
#include "2D/simple2dobject.h"
#include "UI/ttftext.h"
#include "Input/inputcomponent.h"

#include "playerinventory.h"
#include "itemregistry.h"

using namespace pg;

// Item color lookup — items don't have textures yet, so use colored squares
inline constant::Vector4D getItemColor(ItemId id)
{
    static const constant::Vector4D colors[] = {
        {  0.0f,   0.0f,   0.0f,   0.0f},  // 0: None
        {160.0f, 160.0f, 180.0f, 255.0f},  // 1: Iron Ore
        {200.0f, 120.0f,  60.0f, 255.0f},  // 2: Copper Ore
        { 40.0f,  40.0f,  40.0f, 255.0f},  // 3: Coal
        {180.0f, 180.0f, 160.0f, 255.0f},  // 4: Stone
        {200.0f, 200.0f, 220.0f, 255.0f},  // 5: Iron Plate
        {220.0f, 140.0f,  80.0f, 255.0f},  // 6: Copper Plate
        {140.0f, 140.0f, 160.0f, 255.0f},  // 7: Iron Gear
        {240.0f, 160.0f,  40.0f, 255.0f},  // 8: Copper Wire
        { 60.0f, 180.0f,  60.0f, 255.0f},  // 9: Circuit
    };

    constexpr size_t count = sizeof(colors) / sizeof(colors[0]);
    if (id >= count)
        return {200.0f, 200.0f, 60.0f, 255.0f}; // fallback yellow
    return colors[id];
}

class InventoryUISystem : public System<Listener<OnSDLScanCode>,
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

    bool isOpen() const { return visible; }

    // --- Event Handlers ---

    virtual void onEvent(const OnSDLScanCode& event) override
    {
        if (event.key == SDL_SCANCODE_TAB)
        {
            if (visible)
                closeInventory();
            else
                openInventory();
        }
    }

    virtual void onProcessEvent(const OnMouseClick& event) override
    {
        if (not visible or event.button != SDL_BUTTON_LEFT)
            return;

        int slot = slotAtPosition(event.pos.x, event.pos.y);
        if (slot < 0)
        {
            // Click outside inventory — cancel held item
            if (not heldItem.isEmpty())
                cancelHeld();
            return;
        }

        if (heldItem.isEmpty())
            pickUpFromSlot(static_cast<size_t>(slot));
        else
            dropOnSlot(static_cast<size_t>(slot));
    }

    virtual void onProcessEvent(const OnSDLMouseMotion& event) override
    {
        lastMouseX = static_cast<float>(event.x);
        lastMouseY = static_cast<float>(event.y);

        if (visible and not heldItem.isEmpty())
            updateHeldPosition();
    }

private:
    // --- Open / Close ---

    void openInventory()
    {
        visible = true;
        createPanel();
        refreshAllSlots();
    }

    void closeInventory()
    {
        if (not heldItem.isEmpty())
            cancelHeld();

        destroyPanel();
        visible = false;
    }

    // --- Panel Creation / Destruction ---

    void createPanel()
    {
        float panelW = COLS * SLOT_SIZE + (COLS - 1) * SLOT_SPACING + 2 * PANEL_PADDING;
        float panelH = ROWS * SLOT_SIZE + (ROWS - 1) * SLOT_SPACING + 2 * PANEL_PADDING;
        float panelX = (screenWidth - panelW) * 0.5f;
        float panelY = (screenHeight - panelH) * 0.5f;

        // Backdrop
        auto backdrop = makeSimple2DShape(ecsRef, Shape2D::Square, 0.0f, 0.0f,
            constant::Vector4D{20.0f, 20.0f, 30.0f, 220.0f});

        auto bdPos = backdrop.get<PositionComponent>();
        bdPos->setX(panelX);
        bdPos->setY(panelY);
        bdPos->setZ(97.0f);
        bdPos->setWidth(panelW);
        bdPos->setHeight(panelH);
        backdrop.get<Simple2DObject>()->setViewport(INV_UI_VIEWPORT);
        backdropEntityId = backdrop.entity->id;

        // Slot backgrounds
        slotVisuals.resize(PlayerInventorySystem::NUM_SLOTS);
        for (size_t i = 0; i < PlayerInventorySystem::NUM_SLOTS; ++i)
        {
            auto [sx, sy] = slotScreenPos(i);

            auto slot = makeSimple2DShape(ecsRef, Shape2D::Square, 0.0f, 0.0f,
                constant::Vector4D{50.0f, 50.0f, 60.0f, 200.0f});

            auto pos = slot.get<PositionComponent>();
            pos->setX(sx);
            pos->setY(sy);
            pos->setZ(98.0f);
            pos->setWidth(SLOT_SIZE);
            pos->setHeight(SLOT_SIZE);
            slot.get<Simple2DObject>()->setViewport(INV_UI_VIEWPORT);

            slotVisuals[i].bgEntityId = slot.entity->id;
            slotVisuals[i].itemEntityId = 0;
            slotVisuals[i].textEntityId = 0;
        }
    }

    void destroyPanel()
    {
        // Destroy held visuals
        destroyHeldVisual();

        // Destroy slot visuals
        for (auto& sv : slotVisuals)
        {
            destroyEntity(sv.bgEntityId);
            destroyEntity(sv.itemEntityId);
            destroyEntity(sv.textEntityId);
        }
        slotVisuals.clear();

        // Destroy backdrop
        destroyEntity(backdropEntityId);
        backdropEntityId = 0;
    }

    // --- Slot Refresh ---

    void refreshAllSlots()
    {
        for (size_t i = 0; i < PlayerInventorySystem::NUM_SLOTS; ++i)
            refreshSlot(i);
    }

    void refreshSlot(size_t index)
    {
        if (index >= slotVisuals.size())
            return;

        auto& sv = slotVisuals[index];

        // Destroy old item visual and text
        destroyEntity(sv.itemEntityId);
        sv.itemEntityId = 0;
        destroyEntity(sv.textEntityId);
        sv.textEntityId = 0;

        const auto& stack = playerInv->getInventory().getSlot(index);
        if (stack.isEmpty())
            return;

        auto [sx, sy] = slotScreenPos(index);

        // Item colored square, centered inside the slot
        float itemOffset = (SLOT_SIZE - ITEM_SIZE) * 0.5f;
        auto item = makeSimple2DShape(ecsRef, Shape2D::Square, 0.0f, 0.0f, getItemColor(stack.id));

        auto itemPos = item.get<PositionComponent>();
        itemPos->setX(sx + itemOffset);
        itemPos->setY(sy + itemOffset);
        itemPos->setZ(99.0f);
        itemPos->setWidth(ITEM_SIZE);
        itemPos->setHeight(ITEM_SIZE);
        item.get<Simple2DObject>()->setViewport(INV_UI_VIEWPORT);
        sv.itemEntityId = item.entity->id;

        // Stack count text at bottom-right of slot
        if (stack.count > 1)
        {
            std::string countStr = std::to_string(stack.count);
            auto text = makeTTFText(ecsRef,
                sx + SLOT_SIZE - 4.0f, sy + SLOT_SIZE - 4.0f, 100.0f,
                FONT_PATH, countStr, TEXT_SCALE,
                {255.0f, 255.0f, 255.0f, 255.0f});

            text.get<TTFText>()->setViewport(INV_UI_VIEWPORT);
            sv.textEntityId = text.entity->id;
        }
    }

    // --- Drag & Drop ---

    void pickUpFromSlot(size_t slotIndex)
    {
        auto& inv = playerInv->getInventory();
        auto& slot = inv.getSlot(slotIndex);
        if (slot.isEmpty())
            return;

        heldItem = slot;
        heldFromSlot = static_cast<int>(slotIndex);
        slot.clear();

        refreshSlot(slotIndex);
        createHeldVisual();
        updateHeldPosition();
    }

    void dropOnSlot(size_t slotIndex)
    {
        auto& inv = playerInv->getInventory();
        auto& slot = inv.getSlot(slotIndex);
        int oldHeldFrom = heldFromSlot;

        if (slot.isEmpty())
        {
            // Place into empty slot
            slot = heldItem;
            heldItem.clear();
            heldFromSlot = -1;
            destroyHeldVisual();
        }
        else if (slot.id == heldItem.id)
        {
            // Stack merge
            uint16_t maxStack = itemRegistry->get(slot.id).maxStack;
            uint16_t space = maxStack - slot.count;
            uint16_t toAdd = std::min(space, heldItem.count);
            slot.count += toAdd;
            heldItem.count -= toAdd;

            if (heldItem.count == 0)
            {
                heldItem.clear();
                heldFromSlot = -1;
                destroyHeldVisual();
            }
            else
            {
                // Update held visual text
                destroyHeldVisual();
                createHeldVisual();
            }
        }
        else
        {
            // Swap
            ItemStack temp = slot;
            slot = heldItem;
            heldItem = temp;
            heldFromSlot = static_cast<int>(slotIndex);
            destroyHeldVisual();
            createHeldVisual();
        }

        refreshSlot(slotIndex);

        // Also refresh the old source slot to clear any visual artifacts
        if (oldHeldFrom >= 0 and oldHeldFrom != static_cast<int>(slotIndex))
            refreshSlot(static_cast<size_t>(oldHeldFrom));
    }

    void cancelHeld()
    {
        if (heldItem.isEmpty())
            return;

        if (heldFromSlot >= 0 and heldFromSlot < static_cast<int>(PlayerInventorySystem::NUM_SLOTS))
        {
            auto& slot = playerInv->getInventory().getSlot(static_cast<size_t>(heldFromSlot));
            if (slot.isEmpty())
            {
                slot = heldItem;
            }
            else
            {
                // Source slot filled meanwhile — find first available
                playerInv->getInventory().insert(heldItem.id, heldItem.count, *itemRegistry);
            }
            refreshSlot(static_cast<size_t>(heldFromSlot));
        }
        else
        {
            playerInv->getInventory().insert(heldItem.id, heldItem.count, *itemRegistry);
        }

        heldItem.clear();
        heldFromSlot = -1;
        destroyHeldVisual();
        refreshAllSlots();
    }

    // --- Held Item Visual ---

    void createHeldVisual()
    {
        if (heldItem.isEmpty())
            return;

        float offsetX = 8.0f;
        float offsetY = 8.0f;

        auto item = makeSimple2DShape(ecsRef, Shape2D::Square, 0.0f, 0.0f, getItemColor(heldItem.id));
        auto itemPos = item.get<PositionComponent>();
        itemPos->setX(lastMouseX + offsetX);
        itemPos->setY(lastMouseY + offsetY);
        itemPos->setZ(101.0f);
        itemPos->setWidth(ITEM_SIZE);
        itemPos->setHeight(ITEM_SIZE);
        item.get<Simple2DObject>()->setViewport(INV_UI_VIEWPORT);
        heldItemEntityId = item.entity->id;

        if (heldItem.count > 1)
        {
            std::string countStr = std::to_string(heldItem.count);
            auto text = makeTTFText(ecsRef,
                lastMouseX + offsetX + ITEM_SIZE - 4.0f,
                lastMouseY + offsetY + ITEM_SIZE - 4.0f,
                102.0f,
                FONT_PATH, countStr, TEXT_SCALE,
                {255.0f, 255.0f, 255.0f, 255.0f});
            text.get<TTFText>()->setViewport(INV_UI_VIEWPORT);
            heldTextEntityId = text.entity->id;
        }
    }

    void destroyHeldVisual()
    {
        destroyEntity(heldItemEntityId);
        heldItemEntityId = 0;
        destroyEntity(heldTextEntityId);
        heldTextEntityId = 0;
    }

    void updateHeldPosition()
    {
        if (heldItemEntityId == 0)
            return;

        float offsetX = 8.0f;
        float offsetY = 8.0f;

        auto ent = ecsRef->getEntity(heldItemEntityId);
        if (ent)
        {
            auto pos = ent->get<PositionComponent>();
            pos->setX(lastMouseX + offsetX);
            pos->setY(lastMouseY + offsetY);
        }

        if (heldTextEntityId != 0)
        {
            auto textEnt = ecsRef->getEntity(heldTextEntityId);
            if (textEnt)
            {
                auto pos = textEnt->get<PositionComponent>();
                pos->setX(lastMouseX + offsetX + ITEM_SIZE - 4.0f);
                pos->setY(lastMouseY + offsetY + ITEM_SIZE - 4.0f);
            }
        }
    }

    // --- Hit Testing ---

    int slotAtPosition(float x, float y) const
    {
        float panelW = COLS * SLOT_SIZE + (COLS - 1) * SLOT_SPACING + 2 * PANEL_PADDING;
        float panelH = ROWS * SLOT_SIZE + (ROWS - 1) * SLOT_SPACING + 2 * PANEL_PADDING;
        float panelX = (screenWidth - panelW) * 0.5f;
        float panelY = (screenHeight - panelH) * 0.5f;

        for (size_t i = 0; i < PlayerInventorySystem::NUM_SLOTS; ++i)
        {
            auto [sx, sy] = slotScreenPos(i);
            if (x >= sx and x <= sx + SLOT_SIZE and
                y >= sy and y <= sy + SLOT_SIZE)
            {
                return static_cast<int>(i);
            }
        }
        return -1;
    }

    std::pair<float, float> slotScreenPos(size_t index) const
    {
        float panelW = COLS * SLOT_SIZE + (COLS - 1) * SLOT_SPACING + 2 * PANEL_PADDING;
        float panelH = ROWS * SLOT_SIZE + (ROWS - 1) * SLOT_SPACING + 2 * PANEL_PADDING;
        float panelX = (screenWidth - panelW) * 0.5f;
        float panelY = (screenHeight - panelH) * 0.5f;

        size_t col = index % COLS;
        size_t row = index / COLS;
        float x = panelX + PANEL_PADDING + col * (SLOT_SIZE + SLOT_SPACING);
        float y = panelY + PANEL_PADDING + row * (SLOT_SIZE + SLOT_SPACING);
        return {x, y};
    }

    // --- Helpers ---

    void destroyEntity(uint64_t& id)
    {
        if (id != 0)
        {
            auto ent = ecsRef->getEntity(id);
            if (ent)
                ecsRef->removeEntity(id);
            id = 0;
        }
    }

    // --- Members ---

    PlayerInventorySystem* playerInv = nullptr;
    ItemRegistry* itemRegistry = nullptr;
    float screenWidth = 0.0f;
    float screenHeight = 0.0f;

    bool visible = false;

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
    uint64_t heldItemEntityId = 0;
    uint64_t heldTextEntityId = 0;

    float lastMouseX = 0.0f;
    float lastMouseY = 0.0f;
};
