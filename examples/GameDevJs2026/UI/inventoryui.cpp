#include "inventoryui.h"

#include "2D/simple2dobject.h"
#include "UI/sizer.h"
#include "2D/texture.h"
#include "2D/position.h"
#include "UI/ttftext.h"

#include <SDL2/SDL.h>

#include <algorithm>

void InventoryUISystem::init()
{
    ensurePanelCreated();
}

ItemId InventoryUISystem::itemAtPosition(float x, float y) const
{
    if (not visible) return ITEM_NONE;
    int idx = slotAtPosition(x, y);
    if (idx < 0) return ITEM_NONE;
    const auto& stack = playerInv->getInventory().getSlot(static_cast<size_t>(idx));
    return stack.isEmpty() ? ITEM_NONE : stack.id;
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
    bool clickedOnPanel = panelClickedThisFrame;
    panelClickedThisFrame = false;

    if (not visible or event.button != SDL_BUTTON_LEFT)
        return;

    int slot = slotAtPosition(event.pos.x, event.pos.y);
    if (slot < 0)
    {
        // Click was on another panel (e.g., miner UI) — don't interfere
        if (clickedOnPanel)
            return;

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
    ecsRef->sendEvent(InventoryOpenedEvent{});
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
    ecsRef->sendEvent(InventoryClosedEvent{});
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
    auto windowEnt = ecsRef->getEntity("__MainWindow");
    auto windowAnchor = windowEnt->get<UiAnchor>();

    float panelW = COLS * SLOT_SIZE + (COLS - 1) * SLOT_SPACING + 2 * PANEL_PADDING;
    float panelH = ROWS * SLOT_SIZE + (ROWS - 1) * SLOT_SPACING + 2 * PANEL_PADDING;

    // Backdrop — centered in __MainWindow
    auto backdrop = makeSimple2DShape(ecsRef, Shape2D::Square, 0.0f, 0.0f,
        constant::Vector4D{20.0f, 20.0f, 30.0f, 220.0f});

    auto bdPos = backdrop.get<PositionComponent>();
    bdPos->setZ(97.0f);
    bdPos->setWidth(panelW);
    bdPos->setHeight(panelH);
    backdrop.get<ViewportComponent>()->setViewport(INV_UI_VIEWPORT);
    backdropEntityId = backdrop.entity->id;

    ecsRef->attach<MouseLeftClickComponent>(backdrop.entity,
        makeCallable<PanelWasClickedEvent>(), MouseStateTrigger::OnPress);

    auto bdAnchor = ecsRef->attach<UiAnchor>(backdrop.entity);
    bdAnchor->centeredIn(windowAnchor);

    // Horizontal layout for slot grid (fitToAxis wraps into 5-column rows)
    float contentW = COLS * SLOT_SIZE + (COLS - 1) * SLOT_SPACING;
    float contentH = ROWS * SLOT_SIZE + (ROWS - 1) * SLOT_SPACING;
    auto layout = makeHorizontalLayout(ecsRef, 0, 0, contentW, contentH, false);
    slotLayoutEntityId = layout.entity->id;
    auto hLayout = layout.get<HorizontalLayout>();
    hLayout->fitToAxis = true;
    hLayout->spacing = SLOT_SPACING;

    auto layoutAnchor = layout.get<UiAnchor>();
    layoutAnchor->setLeftAnchor(PosAnchor{backdropEntityId, AnchorType::Left});
    layoutAnchor->setLeftMargin(PANEL_PADDING);
    layoutAnchor->setTopAnchor(PosAnchor{backdropEntityId, AnchorType::Top});
    layoutAnchor->setTopMargin(PANEL_PADDING);

    // Slot backgrounds + pre-created item + text entities
    slotVisuals.resize(PlayerInventorySystem::MAIN_SLOTS);
    for (size_t i = 0; i < PlayerInventorySystem::MAIN_SLOTS; ++i)
    {
        // Slot background — positioned by layout
        auto slot = makeSimple2DShape(ecsRef, Shape2D::Square, 0.0f, 0.0f,
            constant::Vector4D{50.0f, 50.0f, 60.0f, 200.0f});

        auto pos = slot.get<PositionComponent>();
        pos->setZ(98.0f);
        pos->setWidth(SLOT_SIZE);
        pos->setHeight(SLOT_SIZE);
        slot.get<ViewportComponent>()->setViewport(INV_UI_VIEWPORT);
        slotVisuals[i].bgEntityId = slot.entity->id;

        hLayout->addEntity(slot.entity);

        // Item texture entity — centered in slot via anchor
        auto tex = make2DTexture(ecsRef, ITEM_SIZE, ITEM_SIZE, "NoneIcon");
        auto itemPos = tex.get<PositionComponent>();
        itemPos->setZ(99.0f);
        itemPos->setVisibility(false);
        tex.get<ViewportComponent>()->setViewport(INV_UI_VIEWPORT);
        slotVisuals[i].itemEntityId = tex.entity->id;

        auto itemAnchor = ecsRef->attach<UiAnchor>(tex.entity);
        itemAnchor->setVerticalCenter(PosAnchor{slotVisuals[i].bgEntityId, AnchorType::VerticalCenter});
        itemAnchor->setHorizontalCenter(PosAnchor{slotVisuals[i].bgEntityId, AnchorType::HorizontalCenter});

        // Count text entity — anchored to bottom-right of slot
        auto text = makeTTFText(ecsRef,
            0.0f, 0.0f, 100.0f,
            FONT_PATH, "", TEXT_SCALE,
            {255.0f, 255.0f, 255.0f, 255.0f});
        text.get<PositionComponent>()->setVisibility(false);
        text.get<ViewportComponent>()->setViewport(INV_UI_VIEWPORT);
        slotVisuals[i].textEntityId = text.entity->id;

        auto textAnchor = ecsRef->attach<UiAnchor>(text.entity);
        textAnchor->setLeftAnchor(PosAnchor{slotVisuals[i].bgEntityId, AnchorType::Left});
        textAnchor->setLeftMargin(SLOT_SIZE - 4.0f);
        textAnchor->setTopAnchor(PosAnchor{slotVisuals[i].bgEntityId, AnchorType::Top});
        textAnchor->setTopMargin(SLOT_SIZE - 4.0f);
    }

    // Held item entity (hidden by default)
    {
        auto tex = make2DTexture(ecsRef, ITEM_SIZE, ITEM_SIZE, "NoneIcon");
        auto itemPos = tex.get<PositionComponent>();
        itemPos->setZ(101.0f);
        itemPos->setVisibility(false);
        tex.get<ViewportComponent>()->setViewport(INV_UI_VIEWPORT);
        heldItemEntityId = tex.entity->id;

        auto text = makeTTFText(ecsRef,
            0.0f, 0.0f, 102.0f,
            FONT_PATH, "", TEXT_SCALE,
            {255.0f, 255.0f, 255.0f, 255.0f});
        text.get<PositionComponent>()->setVisibility(false);
        text.get<ViewportComponent>()->setViewport(INV_UI_VIEWPORT);
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

    // Update item texture and show (position handled by anchor)
    const auto& def = itemRegistry->get(stack.id);
    auto itemEnt = ecsRef->getEntity(sv.itemEntityId);
    if (itemEnt)
    {
        itemEnt->get<Texture2DComponent>()->setTexture(def.textureName);
        auto pos = itemEnt->get<PositionComponent>();
        float iconW = ITEM_SIZE * def.iconWidthRatio;
        pos->setWidth(iconW);
        pos->setHeight(ITEM_SIZE);
        pos->setVisibility(true);
    }

    // Update count text (position handled by anchor)
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
    for (size_t i = 0; i < slotVisuals.size(); ++i)
    {
        auto ent = ecsRef->getEntity(slotVisuals[i].bgEntityId);
        if (not ent) continue;
        auto pos = ent->get<PositionComponent>();
        if (x >= pos->x and x <= pos->x + SLOT_SIZE and
            y >= pos->y and y <= pos->y + SLOT_SIZE)
        {
            return static_cast<int>(i);
        }
    }
    return -1;
}

std::pair<float, float> InventoryUISystem::slotScreenPos(size_t index) const
{
    auto ent = ecsRef->getEntity(slotVisuals[index].bgEntityId);
    if (not ent) return {0.0f, 0.0f};
    auto pos = ent->get<PositionComponent>();
    return {pos->x, pos->y};
}

void InventoryUISystem::setEntityVisibility(uint64_t id, bool vis)
{
    if (id == 0) return;
    auto ent = ecsRef->getEntity(id);
    if (ent)
        ent->get<PositionComponent>()->setVisibility(vis);
}
