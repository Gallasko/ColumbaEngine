#include "hotbarsystem.h"

#include "2D/simple2dobject.h"
#include "2D/position.h"

#include <SDL2/SDL.h>

#include <cstdio>

void HotbarSystem::init()
{
    createHotbarUI();
}

void HotbarSystem::setHotbarVisible(bool vis)
{
    auto setVis = [this](uint64_t id, bool v) {
        if (id == 0) return;
        auto ent = ecsRef->getEntity(id);
        if (ent)
            ent->get<PositionComponent>()->setVisibility(v);
    };

    setVis(backdropEntityId, vis);
    setVis(highlightEntityId, vis);
    for (size_t i = 0; i < HOTBAR_SLOTS; ++i)
        setVis(slotEntityIds[i], vis);
}

void HotbarSystem::onEvent(const ResizeEvent& event)
{
    screenWidth = event.width;
    screenHeight = event.height;

    updateHighlight();
}

void HotbarSystem::onEvent(const OnSDLScanCode& event)
{
    // Number keys 1-9 select hotbar slots
    if (event.key >= SDL_SCANCODE_1 and event.key <= SDL_SCANCODE_9)
    {
        size_t index = event.key - SDL_SCANCODE_1;
        if (index < HOTBAR_SLOTS)
            selectSlot(index);
    }
}

void HotbarSystem::onEvent(const PlayerGainItemEvent& /*event*/)
{
    syncAllSlots();
}

void HotbarSystem::onEvent(const PlayerLoseItemEvent& /*event*/)
{
    syncAllSlots();
}

void HotbarSystem::onEvent(const InventoryOpenedEvent&)
{
    // Enable drag-drop on hotbar slots
    for (size_t i = 0; i < HOTBAR_SLOTS; ++i)
    {
        auto* sc = slotSystem->getSlotComponent(slotEntityIds[i]);
        if (sc)
            sc->clearFlag(SlotFlags::NoPickUp);
    }
}

void HotbarSystem::onEvent(const InventoryClosedEvent&)
{
    // Switch to selection-only mode and sync backing data
    for (size_t i = 0; i < HOTBAR_SLOTS; ++i)
    {
        auto* sc = slotSystem->getSlotComponent(slotEntityIds[i]);
        if (sc)
        {
            sc->setFlag(SlotFlags::NoPickUp);
            playerInv->getInventory().getSlot(PlayerInventorySystem::HOTBAR_START + i) = sc->stack;
        }
    }
}

void HotbarSystem::onProcessEvent(const SlotClickedEvent& event)
{
    for (size_t i = 0; i < HOTBAR_SLOTS; ++i)
    {
        if (event.entityId == slotEntityIds[i])
        {
            auto* sc = slotSystem->getSlotComponent(slotEntityIds[i]);

            if (sc and sc->isNoPickUp() and not slotSystem->hasHeldItem())
                selectSlot(i);

            return;
        }
    }
}

void HotbarSystem::onProcessEvent(const SlotPickedUpEvent& event)
{
    for (size_t i = 0; i < HOTBAR_SLOTS; ++i)
    {
        if (event.slotEntityId == slotEntityIds[i])
        {
            syncSlotToInventory(i);
            return;
        }
    }
}

void HotbarSystem::onProcessEvent(const SlotDroppedEvent& event)
{
    for (size_t i = 0; i < HOTBAR_SLOTS; ++i)
    {
        if (event.slotEntityId == slotEntityIds[i])
        {
            syncSlotToInventory(i);
            return;
        }
    }
}

ItemId HotbarSystem::itemAtPosition(float x, float y) const
{
    int idx = slotAtPosition(x, y);

    if (idx < 0)
        return ITEM_NONE;

    const auto& stack = playerInv->getInventory()
        .getSlot(PlayerInventorySystem::HOTBAR_START + static_cast<size_t>(idx));

    return stack.isEmpty() ? ITEM_NONE : stack.id;
}

void HotbarSystem::selectSlot(size_t index)
{
    if (index == selectedSlot)
        return;

    selectedSlot = index;
    updateHighlight();

    const auto& item = getSelectedItem();

    if (not item.isEmpty())
    {
        const auto& def = itemRegistry->get(item.id);
        printf("Hotbar: selected %s (x%d)\n", def.name.c_str(), item.count);
    }
    else
    {
        printf("Hotbar: selected empty slot %zu\n", index + 1);
    }
}

void HotbarSystem::consumeSelectedItem(uint16_t count)
{
    auto& slot = playerInv->getInventory().getSlot(PlayerInventorySystem::HOTBAR_START + selectedSlot);

    if (slot.isEmpty())
        return;

    if (slot.count <= count)
        slot.clear();
    else
        slot.count -= count;

    slotSystem->syncSlotVisual(slotEntityIds[selectedSlot], slot);
}

void HotbarSystem::createHotbarUI()
{
    auto windowEnt = ecsRef->getEntity("__MainWindow");
    auto windowId = windowEnt->id;

    // Backdrop — anchored to __MainWindow: fills width, sticks to bottom
    auto backdrop = makeSimple2DShape(ecsRef, Shape2D::Square, 0.0f, 0.0f,
        constant::Vector4D{30.0f, 30.0f, 40.0f, 200.0f});

    auto backdropPos = backdrop.get<PositionComponent>();
    backdropPos->setZ(90.f);
    backdropPos->setHeight(HOTBAR_HEIGHT);
    backdrop.get<ViewportComponent>()->setViewport(UI_VP);
    backdropEntityId = backdrop.entity->id;

    auto bdAnchor = ecsRef->attach<UiAnchor>(backdrop.entity);
    bdAnchor->setLeftAnchor(PosAnchor{windowId, AnchorType::Left});
    bdAnchor->setRightAnchor(PosAnchor{windowId, AnchorType::Right});
    bdAnchor->setBottomAnchor(PosAnchor{windowId, AnchorType::Bottom});

    // Invisible container — centered horizontally in backdrop, at top + padding
    float totalSlotsWidth = HOTBAR_SLOTS * SLOT_SIZE + (HOTBAR_SLOTS - 1) * SLOT_SPACING;
    auto container = ecsRef->createEntity();
    auto containerPos = ecsRef->attach<PositionComponent>(container);
    containerPos->setWidth(totalSlotsWidth);
    containerPos->setHeight(SLOT_SIZE);
    containerEntityId = container->id;

    auto cAnchor = ecsRef->attach<UiAnchor>(container);
    cAnchor->setHorizontalCenter(PosAnchor{backdropEntityId, AnchorType::HorizontalCenter});
    cAnchor->setTopAnchor(PosAnchor{backdropEntityId, AnchorType::Top});
    cAnchor->setTopMargin(SLOT_PADDING);

    // Slots via SlotSystem — NoPickUp by default (inventory starts closed)
    for (size_t i = 0; i < HOTBAR_SLOTS; ++i)
    {
        float slotLeftMargin = static_cast<float>(i) * (SLOT_SIZE + SLOT_SPACING);

        auto slotRef = slotSystem->createSlot(
            SlotCategory::Hotbar, static_cast<uint8_t>(i),
            SlotFlags::NoPickUp, SLOT_SIZE, ITEM_SIZE);
        slotEntityIds[i] = slotRef.id;

        slotRef.get<PositionComponent>()->setZ(95.f);

        auto anchor = slotRef.get<UiAnchor>();
        anchor->setLeftAnchor(PosAnchor{containerEntityId, AnchorType::Left});
        anchor->setLeftMargin(slotLeftMargin);
        anchor->setTopAnchor(PosAnchor{containerEntityId, AnchorType::Top});
    }

    // Selection highlight overlay
    auto highlight = makeSimple2DShape(ecsRef, Shape2D::Square, 0.0f, 0.0f,
        constant::Vector4D{255.0f, 255.0f, 255.0f, 60.0f});

    auto hlPos = highlight.get<PositionComponent>();
    hlPos->setZ(98.f);
    hlPos->setWidth(SLOT_SIZE + 4.0f);
    hlPos->setHeight(SLOT_SIZE + 4.0f);
    highlight.get<ViewportComponent>()->setViewport(UI_VP);
    highlightEntityId = highlight.entity->id;

    updateHighlight();
    syncAllSlots();
}

void HotbarSystem::updateHighlight()
{
    if (selectedSlot >= HOTBAR_SLOTS)
        return;

    auto hlEnt = ecsRef->getEntity(highlightEntityId);
    if (not hlEnt)
        return;

    // Anchor highlight to the selected slot prefab entity (auto-follows on resize)
    auto hlAnchor = hlEnt->get<UiAnchor>();
    if (not hlAnchor)
        hlAnchor = ecsRef->attach<UiAnchor>(hlEnt);

    hlAnchor->clearAnchors();
    hlAnchor->setLeftAnchor(PosAnchor{slotEntityIds[selectedSlot], AnchorType::Left});
    hlAnchor->setLeftMargin(-2.0f);
    hlAnchor->setTopAnchor(PosAnchor{slotEntityIds[selectedSlot], AnchorType::Top});
    hlAnchor->setTopMargin(-2.0f);
}

void HotbarSystem::refreshAllSlots()
{
    syncAllSlots();
}

void HotbarSystem::syncAllSlots()
{
    for (size_t i = 0; i < HOTBAR_SLOTS; ++i)
    {
        const auto& stack = playerInv->getInventory().getSlot(PlayerInventorySystem::HOTBAR_START + i);
        slotSystem->syncSlotVisual(slotEntityIds[i], stack);
    }
}

void HotbarSystem::syncSlotToInventory(size_t index)
{
    auto* sc = slotSystem->getSlotComponent(slotEntityIds[index]);
    if (sc)
        playerInv->getInventory().getSlot(PlayerInventorySystem::HOTBAR_START + index) = sc->stack;
}

int HotbarSystem::slotAtPosition(float x, float y) const
{
    if (y < screenHeight - HOTBAR_HEIGHT)
        return -1;

    float totalSlotsWidth = HOTBAR_SLOTS * SLOT_SIZE + (HOTBAR_SLOTS - 1) * SLOT_SPACING;
    float startX = (screenWidth - totalSlotsWidth) * 0.5f;
    float slotY = screenHeight - HOTBAR_HEIGHT + SLOT_PADDING;

    for (size_t i = 0; i < HOTBAR_SLOTS; ++i)
    {
        float slotX = startX + i * (SLOT_SIZE + SLOT_SPACING);

        if (x >= slotX and x <= slotX + SLOT_SIZE and
            y >= slotY and y <= slotY + SLOT_SIZE)
        {
            return static_cast<int>(i);
        }
    }

    return -1;
}
