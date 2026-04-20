#include "hotbarsystem.h"
#include "inventoryui.h"

#include "2D/simple2dobject.h"
#include "2D/texture.h"
#include "2D/position.h"
#include "UI/ttftext.h"

#include <SDL2/SDL.h>

#include <cstdio>

void HotbarSystem::init()
{
    createHotbarUI();
}

void HotbarSystem::onEvent(const ResizeEvent& event)
{
    screenWidth = event.width;
    screenHeight = event.height;

    // Slot bgs are anchored and auto-reposition; refresh items/texts to match
    refreshAllSlots();
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
    refreshAllSlots();
}

void HotbarSystem::onEvent(const PlayerLoseItemEvent& /*event*/)
{
    refreshAllSlots();
}

void HotbarSystem::onProcessEvent(const OnMouseClick& event)
{
    if (event.button != SDL_BUTTON_LEFT)
        return;

    int slot = slotAtPosition(event.pos.x, event.pos.y);
    if (slot < 0)
        return;

    // When inventory is open, handle item transfers
    if (inventoryUI and inventoryUI->isOpen())
    {
        auto& hotbarSlot = getHotbarSlot(static_cast<size_t>(slot));

        if (inventoryUI->hasHeldItem())
            inventoryUI->dropOnExternal(hotbarSlot);
        else
            inventoryUI->pickUpFromExternal(hotbarSlot);

        refreshSlot(static_cast<size_t>(slot));
        return;
    }

    selectSlot(static_cast<size_t>(slot));
}

void HotbarSystem::onProcessEvent(const OnSDLMouseMotion& event)
{
    lastMouseX = static_cast<float>(event.x);
    lastMouseY = static_cast<float>(event.y);
}

ItemId HotbarSystem::itemAtPosition(float x, float y) const
{
    int idx = slotAtPosition(x, y);
    if (idx < 0) return ITEM_NONE;
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

    refreshSlot(selectedSlot);
}

void HotbarSystem::createHotbarUI()
{
    auto windowEnt = ecsRef->getEntity("__MainWindow");
    auto windowId = windowEnt->id;

    // Backdrop — anchored to __MainWindow: fills width, sticks to bottom
    auto backdrop = makeSimple2DShape(ecsRef, Shape2D::Square, 0.0f, 0.0f,
        constant::Vector4D{30.0f, 30.0f, 40.0f, 200.0f});

    auto backdropPos = backdrop.get<PositionComponent>();
    backdropPos->setZ(0.9f);
    backdropPos->setHeight(HOTBAR_HEIGHT);
    backdrop.get<Simple2DObject>()->setViewport(UI_VP);
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

    // Slots — anchored to container
    slotVisuals.resize(HOTBAR_SLOTS);

    for (size_t i = 0; i < HOTBAR_SLOTS; ++i)
    {
        float slotLeftMargin = static_cast<float>(i) * (SLOT_SIZE + SLOT_SPACING);

        // Slot background
        auto slotBg = makeSimple2DShape(ecsRef, Shape2D::Square, 0.0f, 0.0f,
            constant::Vector4D{50.0f, 50.0f, 60.0f, 200.0f});

        auto bgPos = slotBg.get<PositionComponent>();
        bgPos->setZ(0.95f);
        bgPos->setWidth(SLOT_SIZE);
        bgPos->setHeight(SLOT_SIZE);
        slotBg.get<Simple2DObject>()->setViewport(UI_VP);
        slotVisuals[i].bgEntityId = slotBg.entity->id;

        auto slotAnchor = ecsRef->attach<UiAnchor>(slotBg.entity);
        slotAnchor->setLeftAnchor(PosAnchor{containerEntityId, AnchorType::Left});
        slotAnchor->setLeftMargin(slotLeftMargin);
        slotAnchor->setTopAnchor(PosAnchor{containerEntityId, AnchorType::Top});

        // Item texture entity (hidden; positioned in refreshSlot)
        auto tex = make2DTexture(ecsRef, ITEM_SIZE, ITEM_SIZE, "NoneIcon");
        auto itemPos = tex.get<PositionComponent>();
        itemPos->setZ(0.96f);
        itemPos->setVisibility(false);
        tex.get<Texture2DComponent>()->setViewport(UI_VP);
        slotVisuals[i].itemEntityId = tex.entity->id;

        // Count text entity (hidden; positioned in refreshSlot)
        auto text = makeTTFText(ecsRef,
            0.0f, 0.0f, 0.97f,
            FONT_PATH, "", TEXT_SCALE,
            {255.0f, 255.0f, 255.0f, 255.0f});
        text.get<PositionComponent>()->setVisibility(false);
        text.get<TTFText>()->setViewport(UI_VP);
        slotVisuals[i].textEntityId = text.entity->id;
    }

    // Selection highlight overlay
    auto highlight = makeSimple2DShape(ecsRef, Shape2D::Square, 0.0f, 0.0f,
        constant::Vector4D{255.0f, 255.0f, 255.0f, 60.0f});

    auto hlPos = highlight.get<PositionComponent>();
    hlPos->setZ(0.98f);
    hlPos->setWidth(SLOT_SIZE + 4.0f);
    hlPos->setHeight(SLOT_SIZE + 4.0f);
    highlight.get<Simple2DObject>()->setViewport(UI_VP);
    highlightEntityId = highlight.entity->id;

    updateHighlight();
    refreshAllSlots();
}

void HotbarSystem::updateHighlight()
{
    if (selectedSlot >= slotVisuals.size())
        return;

    auto hlEnt = ecsRef->getEntity(highlightEntityId);
    if (not hlEnt)
        return;

    auto slotBgId = slotVisuals[selectedSlot].bgEntityId;

    // Anchor highlight to the selected slot bg (auto-follows on resize)
    auto hlAnchor = hlEnt->get<UiAnchor>();
    if (not hlAnchor)
        hlAnchor = ecsRef->attach<UiAnchor>(hlEnt);

    hlAnchor->clearAnchors();
    hlAnchor->setLeftAnchor(PosAnchor{slotBgId, AnchorType::Left});
    hlAnchor->setLeftMargin(-2.0f);
    hlAnchor->setTopAnchor(PosAnchor{slotBgId, AnchorType::Top});
    hlAnchor->setTopMargin(-2.0f);
}

void HotbarSystem::refreshAllSlots()
{
    for (size_t i = 0; i < HOTBAR_SLOTS; ++i)
        refreshSlot(i);
}

void HotbarSystem::refreshSlot(size_t index)
{
    if (index >= slotVisuals.size())
        return;

    auto& sv = slotVisuals[index];
    const auto& stack = playerInv->getInventory().getSlot(PlayerInventorySystem::HOTBAR_START + index);

    auto setVis = [this](uint64_t id, bool vis) {
        if (id == 0) return;
        auto ent = ecsRef->getEntity(id);
        if (ent)
            ent->get<PositionComponent>()->setVisibility(vis);
    };

    if (stack.isEmpty())
    {
        setVis(sv.itemEntityId, false);
        setVis(sv.textEntityId, false);
        return;
    }

    // Read slot bg position (auto-updated by anchoring)
    auto slotEnt = ecsRef->getEntity(sv.bgEntityId);
    if (not slotEnt)
        return;
    auto slotPos = slotEnt->get<PositionComponent>();
    float slotX = slotPos->getX();
    float slotY = slotPos->getY();
    float itemOffset = (SLOT_SIZE - ITEM_SIZE) * 0.5f;

    // Update item texture and show
    const auto& def = itemRegistry->get(stack.id);
    auto itemEnt = ecsRef->getEntity(sv.itemEntityId);
    if (itemEnt)
    {
        itemEnt->get<Texture2DComponent>()->setTexture(def.textureName);
        auto pos = itemEnt->get<PositionComponent>();
        float iconW = ITEM_SIZE * def.iconWidthRatio;
        pos->setWidth(iconW);
        pos->setHeight(ITEM_SIZE);
        pos->setX(slotX + itemOffset + (ITEM_SIZE - iconW) * 0.5f);
        pos->setY(slotY + itemOffset);
        pos->setVisibility(true);
    }

    // Update count text position and show
    if (stack.count > 1)
    {
        auto textEnt = ecsRef->getEntity(sv.textEntityId);
        if (textEnt)
        {
            textEnt->get<TTFText>()->setText(std::to_string(stack.count));
            auto pos = textEnt->get<PositionComponent>();
            pos->setX(slotX + SLOT_SIZE - 4.0f);
            pos->setY(slotY + SLOT_SIZE - 4.0f);
            pos->setVisibility(true);
        }
    }
    else
    {
        setVis(sv.textEntityId, false);
    }
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
