#include "inventoryui.h"

#include "2D/simple2dobject.h"
#include "2D/texture.h"
#include "UI/ttftext.h"

#include <SDL2/SDL.h>

#include <algorithm>

bool InventoryUISystem::isClickOnPanel(float x, float y) const
{
    if (not visible) return false;
    float panelW = COLS * SLOT_SIZE + (COLS - 1) * SLOT_SPACING + 2 * PANEL_PADDING;
    float panelH = ROWS * SLOT_SIZE + (ROWS - 1) * SLOT_SPACING + 2 * PANEL_PADDING;
    float panelX = (screenWidth - panelW) * 0.5f;
    float panelY = (screenHeight - panelH) * 0.5f;
    return x >= panelX and x <= panelX + panelW
       and y >= panelY and y <= panelY + panelH;
}

void InventoryUISystem::pickUpFromExternal(ItemStack& slot)
{
    if (slot.isEmpty() or not heldItem.isEmpty())
        return;

    heldItem = slot;
    heldFromSlot = -1;
    externalSourceSlot = &slot;
    slot.clear();

    showHeldVisual();
    updateHeldPosition();
}

void InventoryUISystem::dropOnExternal(ItemStack& slot)
{
    if (heldItem.isEmpty())
        return;

    if (slot.isEmpty())
    {
        slot = heldItem;
        clearHeld();
    }
    else if (slot.id == heldItem.id)
    {
        uint16_t maxStack = itemRegistry->get(slot.id).maxStack;
        uint16_t space = maxStack - slot.count;
        uint16_t toAdd = std::min(space, heldItem.count);
        slot.count += toAdd;
        heldItem.count -= toAdd;

        if (heldItem.count == 0)
            clearHeld();
        else
            showHeldVisual(); // Update count display
    }
    else
    {
        ItemStack temp = slot;
        slot = heldItem;
        heldItem = temp;
        externalSourceSlot = &slot;
        heldFromSlot = -1;
        showHeldVisual();
    }
}

void InventoryUISystem::onProcessEvent(const OnSDLScanCode& event)
{
    if (event.key == SDL_SCANCODE_TAB)
    {
        if (visible)
            closeInventory();
        else
            openInventory();
    }
}

void InventoryUISystem::onProcessEvent(const OnMouseClick& event)
{
    if (not visible or event.button != SDL_BUTTON_LEFT)
        return;

    int slot = slotAtPosition(event.pos.x, event.pos.y);
    if (slot < 0)
    {
        // Check if click is on an external panel (e.g., miner UI)
        if (externalClickCheck and externalClickCheck(event.pos.x, event.pos.y))
            return; // Let the external system handle it

        // Don't cancel held here — GameSystem handles click-outside-to-close
        // which calls closeInventory() → cancelHeld(). Doing it here too
        // would duplicate the item since both systems process the same event.
        return;
    }

    if (heldItem.isEmpty())
        pickUpFromSlot(static_cast<size_t>(slot));
    else
        dropOnSlot(static_cast<size_t>(slot));
}

void InventoryUISystem::onProcessEvent(const OnSDLMouseMotion& event)
{
    lastMouseX = static_cast<float>(event.x);
    lastMouseY = static_cast<float>(event.y);

    if (visible and not heldItem.isEmpty())
        updateHeldPosition();
}

void InventoryUISystem::openInventory()
{
    visible = true;
    ensurePanelCreated();
    setPanelVisibility(true);
    refreshAllSlots();
}

void InventoryUISystem::closeInventory()
{
    if (not heldItem.isEmpty())
        cancelHeldDataOnly();

    hideHeldVisual();

    // Hide all slot items
    for (size_t i = 0; i < slotVisuals.size(); ++i)
    {
        setEntityVisibility(slotVisuals[i].itemEntityId, false);
        setEntityVisibility(slotVisuals[i].textEntityId, false);
    }

    setPanelVisibility(false);
    visible = false;
}

void InventoryUISystem::cancelHeld()
{
    cancelHeldDataOnly();
    if (visible)
        refreshAllSlots();
}

void InventoryUISystem::refreshAllSlots()
{
    if (not panelCreated)
        return;
    for (size_t i = 0; i < PlayerInventorySystem::MAIN_SLOTS; ++i)
        refreshSlot(i);
}

void InventoryUISystem::cancelHeldDataOnly()
{
    if (heldItem.isEmpty())
        return;

    if (externalSourceSlot)
    {
        if (externalSourceSlot->isEmpty())
            *externalSourceSlot = heldItem;
        else
            playerInv->getInventory().insert(heldItem.id, heldItem.count, *itemRegistry);
    }
    else if (heldFromSlot >= 0 and heldFromSlot < static_cast<int>(PlayerInventorySystem::MAIN_SLOTS))
    {
        auto& slot = playerInv->getInventory().getSlot(static_cast<size_t>(heldFromSlot));
        if (slot.isEmpty())
            slot = heldItem;
        else
            playerInv->getInventory().insert(heldItem.id, heldItem.count, *itemRegistry);
    }
    else
    {
        playerInv->getInventory().insert(heldItem.id, heldItem.count, *itemRegistry);
    }

    clearHeld();
}

void InventoryUISystem::ensurePanelCreated()
{
    if (panelCreated)
        return;
    createPanel();
    setPanelVisibility(false);
    panelCreated = true;
}

void InventoryUISystem::setPanelVisibility(bool vis)
{
    setEntityVisibility(backdropEntityId, vis);
    for (auto& sv : slotVisuals)
        setEntityVisibility(sv.bgEntityId, vis);
}

void InventoryUISystem::createPanel()
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

    // Slot backgrounds + pre-created item + text entities
    slotVisuals.resize(PlayerInventorySystem::MAIN_SLOTS);
    for (size_t i = 0; i < PlayerInventorySystem::MAIN_SLOTS; ++i)
    {
        auto [sx, sy] = slotScreenPos(i);

        // Slot background
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

        // Item texture entity (hidden by default)
        float itemOffset = (SLOT_SIZE - ITEM_SIZE) * 0.5f;
        auto tex = make2DTexture(ecsRef, ITEM_SIZE, ITEM_SIZE, "NoneIcon");
        auto itemPos = tex.get<PositionComponent>();
        itemPos->setX(sx + itemOffset);
        itemPos->setY(sy + itemOffset);
        itemPos->setZ(99.0f);
        itemPos->setVisibility(false);
        tex.get<Texture2DComponent>()->setViewport(INV_UI_VIEWPORT);
        slotVisuals[i].itemEntityId = tex.entity->id;

        // Count text entity (hidden by default)
        auto text = makeTTFText(ecsRef,
            sx + SLOT_SIZE - 4.0f, sy + SLOT_SIZE - 4.0f, 100.0f,
            FONT_PATH, "", TEXT_SCALE,
            {255.0f, 255.0f, 255.0f, 255.0f});
        text.get<PositionComponent>()->setVisibility(false);
        text.get<TTFText>()->setViewport(INV_UI_VIEWPORT);
        slotVisuals[i].textEntityId = text.entity->id;
    }

    // Held item entity (hidden by default)
    {
        auto tex = make2DTexture(ecsRef, ITEM_SIZE, ITEM_SIZE, "NoneIcon");
        auto itemPos = tex.get<PositionComponent>();
        itemPos->setZ(101.0f);
        itemPos->setVisibility(false);
        tex.get<Texture2DComponent>()->setViewport(INV_UI_VIEWPORT);
        heldItemEntityId = tex.entity->id;

        auto text = makeTTFText(ecsRef,
            0.0f, 0.0f, 102.0f,
            FONT_PATH, "", TEXT_SCALE,
            {255.0f, 255.0f, 255.0f, 255.0f});
        text.get<PositionComponent>()->setVisibility(false);
        text.get<TTFText>()->setViewport(INV_UI_VIEWPORT);
        heldTextEntityId = text.entity->id;
    }
}

void InventoryUISystem::refreshSlot(size_t index)
{
    if (index >= slotVisuals.size())
        return;

    auto& sv = slotVisuals[index];
    const auto& stack = playerInv->getInventory().getSlot(index);

    if (stack.isEmpty())
    {
        setEntityVisibility(sv.itemEntityId, false);
        setEntityVisibility(sv.textEntityId, false);
        return;
    }

    // Update item texture and show
    const auto& def = itemRegistry->get(stack.id);
    auto itemEnt = ecsRef->getEntity(sv.itemEntityId);
    if (itemEnt)
    {
        itemEnt->get<Texture2DComponent>()->setTexture(def.textureName);
        itemEnt->get<PositionComponent>()->setVisibility(true);
    }

    // Update count text
    if (stack.count > 1)
    {
        auto textEnt = ecsRef->getEntity(sv.textEntityId);
        if (textEnt)
        {
            textEnt->get<TTFText>()->setText(std::to_string(stack.count));
            textEnt->get<PositionComponent>()->setVisibility(true);
        }
    }
    else
    {
        setEntityVisibility(sv.textEntityId, false);
    }
}

void InventoryUISystem::pickUpFromSlot(size_t slotIndex)
{
    auto& inv = playerInv->getInventory();
    auto& slot = inv.getSlot(slotIndex);
    if (slot.isEmpty())
        return;

    heldItem = slot;
    heldFromSlot = static_cast<int>(slotIndex);
    externalSourceSlot = nullptr;
    slot.clear();

    refreshSlot(slotIndex);
    showHeldVisual();
    updateHeldPosition();
}

void InventoryUISystem::dropOnSlot(size_t slotIndex)
{
    auto& inv = playerInv->getInventory();
    auto& slot = inv.getSlot(slotIndex);
    int oldHeldFrom = heldFromSlot;

    if (slot.isEmpty())
    {
        slot = heldItem;
        clearHeld();
    }
    else if (slot.id == heldItem.id)
    {
        uint16_t maxStack = itemRegistry->get(slot.id).maxStack;
        uint16_t space = maxStack - slot.count;
        uint16_t toAdd = std::min(space, heldItem.count);
        slot.count += toAdd;
        heldItem.count -= toAdd;

        if (heldItem.count == 0)
            clearHeld();
        else
            showHeldVisual(); // Update count display
    }
    else
    {
        ItemStack temp = slot;
        slot = heldItem;
        heldItem = temp;
        heldFromSlot = static_cast<int>(slotIndex);
        externalSourceSlot = nullptr;
        showHeldVisual();
    }

    refreshSlot(slotIndex);

    if (oldHeldFrom >= 0 and oldHeldFrom != static_cast<int>(slotIndex))
        refreshSlot(static_cast<size_t>(oldHeldFrom));
}

void InventoryUISystem::clearHeld()
{
    heldItem.clear();
    heldFromSlot = -1;
    externalSourceSlot = nullptr;
    hideHeldVisual();
}

void InventoryUISystem::showHeldVisual()
{
    if (heldItem.isEmpty())
        return;

    // Update held item texture and show
    const auto& def = itemRegistry->get(heldItem.id);
    auto itemEnt = ecsRef->getEntity(heldItemEntityId);
    if (itemEnt)
    {
        itemEnt->get<Texture2DComponent>()->setTexture(def.textureName);
        itemEnt->get<PositionComponent>()->setVisibility(true);
    }

    // Update held count text
    auto textEnt = ecsRef->getEntity(heldTextEntityId);
    if (textEnt)
    {
        if (heldItem.count > 1)
        {
            textEnt->get<TTFText>()->setText(std::to_string(heldItem.count));
            textEnt->get<PositionComponent>()->setVisibility(true);
        }
        else
        {
            textEnt->get<PositionComponent>()->setVisibility(false);
        }
    }

    updateHeldPosition();
}

void InventoryUISystem::hideHeldVisual()
{
    setEntityVisibility(heldItemEntityId, false);
    setEntityVisibility(heldTextEntityId, false);
}

void InventoryUISystem::updateHeldPosition()
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

int InventoryUISystem::slotAtPosition(float x, float y) const
{
    for (size_t i = 0; i < PlayerInventorySystem::MAIN_SLOTS; ++i)
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

std::pair<float, float> InventoryUISystem::slotScreenPos(size_t index) const
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

void InventoryUISystem::setEntityVisibility(uint64_t id, bool vis)
{
    if (id == 0) return;
    auto ent = ecsRef->getEntity(id);
    if (ent)
        ent->get<PositionComponent>()->setVisibility(vis);
}
