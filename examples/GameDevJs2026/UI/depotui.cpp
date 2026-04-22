#include "depotui.h"

#include "2D/simple2dobject.h"
#include "2D/texture.h"
#include "UI/ttftext.h"

#include <SDL2/SDL.h>

// ---------------------------------------------------------------------------
// open / close
// ---------------------------------------------------------------------------

void DepotUISystem::open(int gridX, int gridY)
{
    if (visible)
        close();

    openDepotX = gridX;
    openDepotY = gridY;
    visible = true;

    if (inventoryUI and not inventoryUI->isOpen())
        inventoryUI->openInventory();

    ensurePanelCreated();
    setPanelVisibility(true);
    refreshAllSlots();
}

void DepotUISystem::close()
{
    if (inventoryUI and inventoryUI->hasHeldItem())
        inventoryUI->cancelHeld();

    // Hide item/count entities
    for (size_t i = 0; i < NUM_SLOTS; ++i)
    {
        setEntityVisibility(slotItemEntityId[i],  false);
        setEntityVisibility(slotCountEntityId[i], false);
    }
    for (size_t i = 0; i < NUM_OUTPUT_SLOTS; ++i)
    {
        setEntityVisibility(outputSlotItemEntityId[i],  false);
        setEntityVisibility(outputSlotCountEntityId[i], false);
    }

    setPanelVisibility(false);
    visible = false;
    openDepotX = -1;
    openDepotY = -1;
}

// ---------------------------------------------------------------------------
// Events
// ---------------------------------------------------------------------------

void DepotUISystem::onProcessEvent(const OnSDLScanCode& event)
{
    if (not visible) return;
    if (event.key == SDL_SCANCODE_ESCAPE)
        close();
}

void DepotUISystem::onEvent(const InventoryClosedEvent&)
{
    if (visible)
        close();
}

void DepotUISystem::onProcessEvent(const TickEvent&)
{
    if (not visible) return;

    DepotData* depot = depotSystem->getDepot(openDepotX, openDepotY);
    if (not depot)
    {
        close();
        return;
    }

    refreshAllSlots();
}

void DepotUISystem::onProcessEvent(const OnMouseClick& event)
{
    if (not visible or event.button != SDL_BUTTON_LEFT)
        return;

    DepotData* depot = depotSystem->getDepot(openDepotX, openDepotY);
    if (not depot)
        return;

    for (size_t i = 0; i < NUM_SLOTS; ++i)
    {
        if (isClickOnSlot(i, event.pos.x, event.pos.y))
        {
            auto& slot = depot->inventory.getSlot(i);
            if (inventoryUI->hasHeldItem())
                inventoryUI->dropOnExternal(slot);
            else
                inventoryUI->pickUpFromExternal(slot);
            refreshAllSlots();
            return;
        }
    }

    // Output slots — pick up only (no dropping)
    for (size_t i = 0; i < NUM_OUTPUT_SLOTS; ++i)
    {
        if (isClickOnOutputSlot(i, event.pos.x, event.pos.y))
        {
            if (not inventoryUI->hasHeldItem())
            {
                auto& slot = depot->output.getSlot(i);
                inventoryUI->pickUpFromExternal(slot);
                refreshAllSlots();
            }
            return;
        }
    }
}

// ---------------------------------------------------------------------------
// Layout
// ---------------------------------------------------------------------------

float DepotUISystem::getPanelX() const
{
    float invW = InventoryUISystem::COLS * InventoryUISystem::SLOT_SIZE
               + (InventoryUISystem::COLS - 1) * InventoryUISystem::SLOT_SPACING
               + 2.0f * InventoryUISystem::PANEL_PADDING;
    float invX = (screenWidth - invW) * 0.5f;
    return invX - GAP_BETWEEN_PANELS - getPanelWidth();
}

float DepotUISystem::getPanelY() const
{
    return (screenHeight - getPanelHeight()) * 0.5f;
}

// ---------------------------------------------------------------------------
// Panel creation
// ---------------------------------------------------------------------------

void DepotUISystem::ensurePanelCreated()
{
    if (panelCreated) return;
    createPanel();
    setPanelVisibility(false);
    panelCreated = true;
}

void DepotUISystem::setPanelVisibility(bool vis)
{
    setEntityVisibility(backdropEntityId, vis);
    setEntityVisibility(titleEntityId,    vis);
    for (size_t i = 0; i < NUM_SLOTS; ++i)
        setEntityVisibility(slotBgEntityId[i], vis);
    setEntityVisibility(outputTitleEntityId, vis);
    for (size_t i = 0; i < NUM_OUTPUT_SLOTS; ++i)
        setEntityVisibility(outputSlotBgEntityId[i], vis);
}

void DepotUISystem::createPanel()
{
    float panelX = getPanelX();
    float panelY = getPanelY();
    float panelW = getPanelWidth();
    float panelH = getPanelHeight();

    float slotsStartY = panelY + PANEL_PADDING + TITLE_H + GAP_AFTER_TITLE;

    // Backdrop
    {
        auto bd = makeSimple2DShape(ecsRef, Shape2D::Square, 0.0f, 0.0f,
            constant::Vector4D{20.0f, 20.0f, 30.0f, 220.0f});
        auto pos = bd.get<PositionComponent>();
        pos->setX(panelX); pos->setY(panelY); pos->setZ(97.0f);
        pos->setWidth(panelW); pos->setHeight(panelH);
        bd.get<ViewportComponent>()->setViewport(UI_VP);
        backdropEntityId = bd.entity->id;

        ecsRef->attach<MouseLeftClickComponent>(bd.entity,
            makeCallable<PanelWasClickedEvent>(), MouseStateTrigger::OnPress);
    }

    // Title
    {
        auto t = makeTTFText(ecsRef,
            panelX + PANEL_PADDING, panelY + PANEL_PADDING + 4.0f, 100.0f,
            FONT_PATH, "Depot", TITLE_SCALE, {255.0f, 255.0f, 255.0f, 255.0f});
        t.get<ViewportComponent>()->setViewport(UI_VP);
        titleEntityId = t.entity->id;
    }

    // Input slot backgrounds + item/count entities
    for (size_t i = 0; i < NUM_SLOTS; ++i)
    {
        size_t col = i % COLS;
        size_t row = i / COLS;

        float sx = panelX + PANEL_PADDING + col * (SLOT_SIZE + SLOT_SPACING);
        float sy = slotsStartY + row * (SLOT_SIZE + SLOT_SPACING);

        cachedSlotX[i] = sx;
        cachedSlotY[i] = sy;

        // Slot background
        auto slotBg = makeSimple2DShape(ecsRef, Shape2D::Square, 0.0f, 0.0f,
            constant::Vector4D{50.0f, 50.0f, 60.0f, 200.0f});
        auto pos = slotBg.get<PositionComponent>();
        pos->setX(sx); pos->setY(sy); pos->setZ(98.0f);
        pos->setWidth(SLOT_SIZE); pos->setHeight(SLOT_SIZE);
        slotBg.get<ViewportComponent>()->setViewport(UI_VP);
        slotBgEntityId[i] = slotBg.entity->id;

        // Item texture
        float offset = (SLOT_SIZE - ITEM_SIZE) * 0.5f;
        auto itemTex = make2DTexture(ecsRef, ITEM_SIZE, ITEM_SIZE, "NoneIcon");
        auto iPos = itemTex.get<PositionComponent>();
        iPos->setX(sx + offset); iPos->setY(sy + offset); iPos->setZ(99.0f);
        iPos->setVisibility(false);
        itemTex.get<ViewportComponent>()->setViewport(UI_VP);
        slotItemEntityId[i] = itemTex.entity->id;

        // Count text
        auto countTxt = makeTTFText(ecsRef,
            sx + SLOT_SIZE - 4.0f, sy + SLOT_SIZE - 4.0f, 100.0f,
            FONT_PATH, "", TEXT_SCALE, {255.0f, 255.0f, 255.0f, 255.0f});
        countTxt.get<PositionComponent>()->setVisibility(false);
        countTxt.get<ViewportComponent>()->setViewport(UI_VP);
        slotCountEntityId[i] = countTxt.entity->id;
    }

    // --- Output section ---
    float outputStartY = slotsStartY + ROWS * (SLOT_SIZE + SLOT_SPACING) + SECTION_GAP;

    // Output title
    {
        auto t = makeTTFText(ecsRef,
            panelX + PANEL_PADDING, outputStartY + 4.0f, 100.0f,
            FONT_PATH, "Output", TITLE_SCALE, {180.0f, 180.0f, 200.0f, 255.0f});
        t.get<ViewportComponent>()->setViewport(UI_VP);
        outputTitleEntityId = t.entity->id;
    }

    float outputSlotsStartY = outputStartY + TITLE_H + GAP_AFTER_TITLE;

    for (size_t i = 0; i < NUM_OUTPUT_SLOTS; ++i)
    {
        size_t col = i % COLS;
        size_t row = i / COLS;

        float sx = panelX + PANEL_PADDING + col * (SLOT_SIZE + SLOT_SPACING);
        float sy = outputSlotsStartY + row * (SLOT_SIZE + SLOT_SPACING);

        cachedOutputSlotX[i] = sx;
        cachedOutputSlotY[i] = sy;

        // Slot background (slightly different color for output)
        auto slotBg = makeSimple2DShape(ecsRef, Shape2D::Square, 0.0f, 0.0f,
            constant::Vector4D{45.0f, 55.0f, 50.0f, 200.0f});
        auto pos = slotBg.get<PositionComponent>();
        pos->setX(sx); pos->setY(sy); pos->setZ(98.0f);
        pos->setWidth(SLOT_SIZE); pos->setHeight(SLOT_SIZE);
        slotBg.get<ViewportComponent>()->setViewport(UI_VP);
        outputSlotBgEntityId[i] = slotBg.entity->id;

        // Item texture
        float offset = (SLOT_SIZE - ITEM_SIZE) * 0.5f;
        auto itemTex = make2DTexture(ecsRef, ITEM_SIZE, ITEM_SIZE, "NoneIcon");
        auto iPos = itemTex.get<PositionComponent>();
        iPos->setX(sx + offset); iPos->setY(sy + offset); iPos->setZ(99.0f);
        iPos->setVisibility(false);
        itemTex.get<ViewportComponent>()->setViewport(UI_VP);
        outputSlotItemEntityId[i] = itemTex.entity->id;

        // Count text
        auto countTxt = makeTTFText(ecsRef,
            sx + SLOT_SIZE - 4.0f, sy + SLOT_SIZE - 4.0f, 100.0f,
            FONT_PATH, "", TEXT_SCALE, {255.0f, 255.0f, 255.0f, 255.0f});
        countTxt.get<PositionComponent>()->setVisibility(false);
        countTxt.get<ViewportComponent>()->setViewport(UI_VP);
        outputSlotCountEntityId[i] = countTxt.entity->id;
    }
}

// ---------------------------------------------------------------------------
// Refresh helpers
// ---------------------------------------------------------------------------

void DepotUISystem::refreshItemSlotDisplay(uint64_t itemEntId, uint64_t countEntId,
                                            const ItemStack& stack)
{
    if (stack.isEmpty())
    {
        setEntityVisibility(itemEntId,  false);
        setEntityVisibility(countEntId, false);
        return;
    }

    const auto& def = itemRegistry->get(stack.id);
    auto itemEnt = ecsRef->getEntity(itemEntId);
    if (itemEnt)
    {
        itemEnt->get<Texture2DComponent>()->setTexture(def.textureName);
        itemEnt->get<PositionComponent>()->setVisibility(true);
    }

    if (stack.count > 1)
    {
        auto countEnt = ecsRef->getEntity(countEntId);
        if (countEnt)
        {
            countEnt->get<TTFText>()->setText(std::to_string(stack.count));
            countEnt->get<PositionComponent>()->setVisibility(true);
        }
    }
    else
    {
        setEntityVisibility(countEntId, false);
    }
}

void DepotUISystem::refreshSlot(size_t slotIndex)
{
    DepotData* depot = depotSystem->getDepot(openDepotX, openDepotY);
    if (not depot) return;

    const auto& stack = depot->inventory.getSlot(slotIndex);
    refreshItemSlotDisplay(slotItemEntityId[slotIndex], slotCountEntityId[slotIndex], stack);
}

void DepotUISystem::refreshAllSlots()
{
    DepotData* depot = depotSystem->getDepot(openDepotX, openDepotY);
    if (not depot) return;

    for (size_t i = 0; i < NUM_SLOTS; ++i)
        refreshSlot(i);

    for (size_t i = 0; i < NUM_OUTPUT_SLOTS; ++i)
    {
        const auto& stack = depot->output.getSlot(i);
        refreshItemSlotDisplay(outputSlotItemEntityId[i], outputSlotCountEntityId[i], stack);
    }
}

// ---------------------------------------------------------------------------
// Visibility helper
// ---------------------------------------------------------------------------

void DepotUISystem::setEntityVisibility(uint64_t id, bool vis)
{
    if (id == 0) return;
    auto ent = ecsRef->getEntity(id);
    if (ent)
        ent->get<PositionComponent>()->setVisibility(vis);
}
