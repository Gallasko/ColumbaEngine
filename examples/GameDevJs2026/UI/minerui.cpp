#include "minerui.h"

#include "2D/simple2dobject.h"
#include "2D/texture.h"
#include "UI/ttftext.h"

#include <SDL2/SDL.h>

void MinerUISystem::open(int gridX, int gridY)
{
    if (visible)
        close();

    openMinerX = gridX;
    openMinerY = gridY;
    visible = true;

    // Also open the player inventory beside us
    if (inventoryUI and not inventoryUI->isOpen())
        inventoryUI->openInventory();

    ensurePanelCreated();
    setPanelVisibility(true);
    lastDisplayedStack.clear();
    refreshSlot();
}

void MinerUISystem::close()
{
    if (not visible)
        return;

    // Cancel any held item that came from our slot
    if (inventoryUI and inventoryUI->hasHeldItem())
        inventoryUI->cancelHeld();

    // Hide item/text entities
    setEntityVisibility(itemEntityId, false);
    setEntityVisibility(countTextEntityId, false);

    // Hide the panel skeleton
    setPanelVisibility(false);
    visible = false;
    openMinerX = -1;
    openMinerY = -1;
    lastDisplayedStack.clear();

    // Close the companion inventory (cascades to crafting).
    if (inventoryUI and inventoryUI->isOpen())
        inventoryUI->closeInventory();
}

void MinerUISystem::onProcessEvent(const OnSDLScanCode& event)
{
    if (not visible)
        return;

    if (event.key == SDL_SCANCODE_ESCAPE)
        close();
}

void MinerUISystem::onEvent(const InventoryClosedEvent&)
{
    if (visible)
        close();
}

void MinerUISystem::onProcessEvent(const TickEvent&)
{
    if (not visible)
        return;

    MinerData* miner = minerSystem->getMiner(openMinerX, openMinerY);
    if (not miner)
    {
        close();
        return;
    }

    const auto& stack = miner->outputSlots.getSlot(0);
    if (stack.id != lastDisplayedStack.id or stack.count != lastDisplayedStack.count)
        refreshSlot();

    refreshProgressBar();
}

void MinerUISystem::onProcessEvent(const OnMouseClick& event)
{
    if (not visible or event.button != SDL_BUTTON_LEFT)
        return;

    if (not isClickOnSlot(event.pos.x, event.pos.y))
        return;

    MinerData* miner = minerSystem->getMiner(openMinerX, openMinerY);
    if (not miner)
        return;

    auto& slot = miner->outputSlots.getSlot(0);

    if (inventoryUI->hasHeldItem())
        inventoryUI->dropOnExternal(slot);
    else
        inventoryUI->pickUpFromExternal(slot);

    refreshSlot();
}

float MinerUISystem::getPanelX() const
{
    // Inventory panel dimensions (mirrored from InventoryUISystem constants)
    float invW = InventoryUISystem::COLS * InventoryUISystem::SLOT_SIZE
               + (InventoryUISystem::COLS - 1) * InventoryUISystem::SLOT_SPACING
               + 2 * InventoryUISystem::PANEL_PADDING;
    float invX = (screenWidth - invW) * 0.5f;

    float minerW = SLOT_SIZE + 2 * PANEL_PADDING;
    return invX - GAP_BETWEEN_PANELS - minerW;
}

float MinerUISystem::getPanelY() const
{
    float titleH = 20.0f;
    float gapAfterTitle = 6.0f;
    float gapAfterSlot = 8.0f;
    float contentH = titleH + gapAfterTitle + SLOT_SIZE + gapAfterSlot + PROGRESS_BAR_HEIGHT;
    float panelH = contentH + 2 * PANEL_PADDING;
    return (screenHeight - panelH) * 0.5f;
}

void MinerUISystem::ensurePanelCreated()
{
    if (panelCreated)
        return;
    createPanel();
    setPanelVisibility(false);
    panelCreated = true;
}

void MinerUISystem::setPanelVisibility(bool vis)
{
    setEntityVisibility(backdropEntityId, vis);
    setEntityVisibility(titleEntityId, vis);
    setEntityVisibility(slotBgEntityId, vis);
    setEntityVisibility(progressBgEntityId, vis);
    setEntityVisibility(progressFillEntityId, vis);
}

void MinerUISystem::createPanel()
{
    float titleH = 20.0f;
    float gapAfterTitle = 6.0f;
    float gapAfterSlot = 8.0f;

    float contentH = titleH + gapAfterTitle + SLOT_SIZE + gapAfterSlot + PROGRESS_BAR_HEIGHT;
    float panelW = SLOT_SIZE + 2 * PANEL_PADDING;
    float panelH = contentH + 2 * PANEL_PADDING;
    float panelX = getPanelX();
    float panelY = getPanelY();

    // Backdrop
    auto backdrop = makeSimple2DShape(ecsRef, Shape2D::Square, 0.0f, 0.0f,
        constant::Vector4D{20.0f, 20.0f, 30.0f, 220.0f});

    auto bdPos = backdrop.get<PositionComponent>();
    bdPos->setX(panelX);
    bdPos->setY(panelY);
    bdPos->setZ(97.0f);
    bdPos->setWidth(panelW);
    bdPos->setHeight(panelH);
    backdrop.get<ViewportComponent>()->setViewport(UI_VP);
    backdropEntityId = backdrop.entity->id;

    ecsRef->attach<MouseLeftClickComponent>(backdrop.entity,
        makeCallable<PanelWasClickedEvent>(), MouseStateTrigger::OnPress);

    // Title text "Miner" — left-aligned with padding
    auto title = makeTTFText(ecsRef,
        panelX + PANEL_PADDING, panelY + PANEL_PADDING + 4.0f, 100.0f,
        FONT_PATH, "Miner", TITLE_SCALE,
        {255.0f, 255.0f, 255.0f, 255.0f});
    title.get<ViewportComponent>()->setViewport(UI_VP);
    titleEntityId = title.entity->id;

    // Slot background
    float slotX = panelX + PANEL_PADDING;
    float slotY = panelY + PANEL_PADDING + titleH + gapAfterTitle;

    auto slot = makeSimple2DShape(ecsRef, Shape2D::Square, 0.0f, 0.0f,
        constant::Vector4D{50.0f, 50.0f, 60.0f, 200.0f});

    auto slotPos = slot.get<PositionComponent>();
    slotPos->setX(slotX);
    slotPos->setY(slotY);
    slotPos->setZ(98.0f);
    slotPos->setWidth(SLOT_SIZE);
    slotPos->setHeight(SLOT_SIZE);
    slot.get<ViewportComponent>()->setViewport(UI_VP);
    slotBgEntityId = slot.entity->id;

    // Progress bar background
    float barX = slotX;
    float barY = slotY + SLOT_SIZE + gapAfterSlot;

    auto barBg = makeSimple2DShape(ecsRef, Shape2D::Square, 0.0f, 0.0f,
        constant::Vector4D{40.0f, 40.0f, 50.0f, 200.0f});

    auto barBgPos = barBg.get<PositionComponent>();
    barBgPos->setX(barX);
    barBgPos->setY(barY);
    barBgPos->setZ(98.0f);
    barBgPos->setWidth(PROGRESS_BAR_WIDTH);
    barBgPos->setHeight(PROGRESS_BAR_HEIGHT);
    barBg.get<ViewportComponent>()->setViewport(UI_VP);
    progressBgEntityId = barBg.entity->id;

    // Progress bar fill
    auto barFill = makeSimple2DShape(ecsRef, Shape2D::Square, 0.0f, 0.0f,
        constant::Vector4D{80.0f, 200.0f, 80.0f, 255.0f});

    auto barFillPos = barFill.get<PositionComponent>();
    barFillPos->setX(barX);
    barFillPos->setY(barY);
    barFillPos->setZ(99.0f);
    barFillPos->setWidth(0.0f);
    barFillPos->setHeight(PROGRESS_BAR_HEIGHT);
    barFill.get<ViewportComponent>()->setViewport(UI_VP);
    progressFillEntityId = barFill.entity->id;

    // Item texture entity (hidden by default)
    float itemOffset = (SLOT_SIZE - ITEM_SIZE) * 0.5f;
    auto itemTex = make2DTexture(ecsRef, ITEM_SIZE, ITEM_SIZE, "NoneIcon");
    auto itemTexPos = itemTex.get<PositionComponent>();
    itemTexPos->setX(slotX + itemOffset);
    itemTexPos->setY(slotY + itemOffset);
    itemTexPos->setZ(99.0f);
    itemTexPos->setVisibility(false);
    itemTex.get<ViewportComponent>()->setViewport(UI_VP);
    itemEntityId = itemTex.entity->id;

    // Count text entity (hidden by default)
    auto countText = makeTTFText(ecsRef,
        slotX + SLOT_SIZE - 4.0f, slotY + SLOT_SIZE - 4.0f, 100.0f,
        FONT_PATH, "", TEXT_SCALE,
        {255.0f, 255.0f, 255.0f, 255.0f});
    countText.get<PositionComponent>()->setVisibility(false);
    countText.get<ViewportComponent>()->setViewport(UI_VP);
    countTextEntityId = countText.entity->id;

    // Store layout positions for refresh
    cachedSlotX = slotX;
    cachedSlotY = slotY;
}

void MinerUISystem::refreshSlot()
{
    MinerData* miner = minerSystem->getMiner(openMinerX, openMinerY);
    if (not miner)
        return;

    const auto& stack = miner->outputSlots.getSlot(0);
    lastDisplayedStack = stack;

    if (stack.isEmpty())
    {
        setEntityVisibility(itemEntityId, false);
        setEntityVisibility(countTextEntityId, false);
        return;
    }

    // Update item texture and show
    const auto& def = itemRegistry->get(stack.id);
    auto itemEnt = ecsRef->getEntity(itemEntityId);
    if (itemEnt)
    {
        itemEnt->get<Texture2DComponent>()->setTexture(def.textureName);
        itemEnt->get<PositionComponent>()->setVisibility(true);
    }

    // Update count text
    if (stack.count > 1)
    {
        auto textEnt = ecsRef->getEntity(countTextEntityId);
        if (textEnt)
        {
            textEnt->get<TTFText>()->setText(std::to_string(stack.count));
            textEnt->get<PositionComponent>()->setVisibility(true);
        }
    }
    else
    {
        setEntityVisibility(countTextEntityId, false);
    }
}

void MinerUISystem::refreshProgressBar()
{
    MinerData* miner = minerSystem->getMiner(openMinerX, openMinerY);
    if (not miner)
        return;

    float progress = 0.0f;
    if (miner->isMining and MinerSystem::MINE_TIME_MS > 0)
        progress = static_cast<float>(miner->mineProgress) / static_cast<float>(MinerSystem::MINE_TIME_MS);

    if (progress > 1.0f) progress = 1.0f;

    auto fillEnt = ecsRef->getEntity(progressFillEntityId);
    if (fillEnt)
    {
        auto pos = fillEnt->get<PositionComponent>();
        pos->setWidth(PROGRESS_BAR_WIDTH * progress);
    }
}

void MinerUISystem::setEntityVisibility(uint64_t id, bool vis)
{
    if (id == 0) return;
    auto ent = ecsRef->getEntity(id);
    if (ent)
        ent->get<PositionComponent>()->setVisibility(vis);
}
