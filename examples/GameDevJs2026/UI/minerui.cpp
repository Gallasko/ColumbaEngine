#include "minerui.h"

#include "2D/simple2dobject.h"
#include "UI/ttftext.h"

#include <SDL2/SDL.h>

void MinerUISystem::open(int gridX, int gridY)
{
    if (visible)
        close();

    openMinerX = gridX;
    openMinerY = gridY;
    visible = true;

    if (inventoryUI and not inventoryUI->isOpen())
        inventoryUI->openInventory();

    ensurePanelCreated();
    setPanelVisibility(true);

    MinerData* miner = minerSystem->getMiner(openMinerX, openMinerY);
    if (miner)
        slotSystem->syncSlotVisual(slotEntityId, miner->outputSlots.getSlot(0));
}

void MinerUISystem::close()
{
    if (not visible)
        return;

    if (slotSystem->hasHeldItem())
        slotSystem->cancelHeld();

    // Sync slot back to miner
    MinerData* miner = minerSystem->getMiner(openMinerX, openMinerY);
    if (miner)
    {
        auto* sc = slotSystem->getSlotComponent(slotEntityId);
        if (sc)
            miner->outputSlots.getSlot(0) = sc->stack;
    }

    setPanelVisibility(false);
    visible = false;
    openMinerX = -1;
    openMinerY = -1;

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

    slotSystem->syncSlotVisual(slotEntityId, miner->outputSlots.getSlot(0));
    refreshProgressBar();
}

void MinerUISystem::onProcessEvent(const SlotPickedUpEvent& event)
{
    if (not visible or event.slotEntityId != slotEntityId)
        return;

    MinerData* miner = minerSystem->getMiner(openMinerX, openMinerY);
    if (miner)
    {
        auto* sc = slotSystem->getSlotComponent(slotEntityId);
        if (sc)
            miner->outputSlots.getSlot(0) = sc->stack;
    }
}

void MinerUISystem::onProcessEvent(const SlotDroppedEvent& event)
{
    if (not visible or event.slotEntityId != slotEntityId)
        return;

    MinerData* miner = minerSystem->getMiner(openMinerX, openMinerY);
    if (miner)
    {
        auto* sc = slotSystem->getSlotComponent(slotEntityId);
        if (sc)
            miner->outputSlots.getSlot(0) = sc->stack;
    }
}

float MinerUISystem::getPanelX() const
{
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
    setEntityVisibility(progressBgEntityId, vis);
    setEntityVisibility(progressFillEntityId, vis);

    auto ent = ecsRef->getEntity(slotEntityId);
    if (ent)
        ent->get<PositionComponent>()->setVisibility(vis);
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

    // Backdrop & title via engine factories
    auto* factory = ecsRef->getSystem<PrefabFactoryRegistry>();
    auto panelEnt = factory->build("Panel", PrefabParams{
        {"width", panelW}, {"height", panelH},
        {"r", 20.0f}, {"g", 20.0f}, {"b", 30.0f}, {"a", 220.0f},
        {"z", 97.0f},
        {"viewport", static_cast<int>(UI_VP)},
    });
    {
        auto bdPos = panelEnt->get<PositionComponent>();
        bdPos->setX(panelX);
        bdPos->setY(panelY);
    }
    backdropEntityId = panelEnt->id;

    if (auto bgEnt = panelEnt->get<Prefab>()->getEntity("bg"))
        ecsRef->attach<MouseLeftClickComponent>(bgEnt,
            makeCallable<PanelWasClickedEvent>(), MouseStateTrigger::OnPress);

    auto titleEnt = factory->build("Text", PrefabParams{
        {"x",        panelX + PANEL_PADDING},
        {"y",        panelY + PANEL_PADDING + 4.0f},
        {"z",        100.0f},
        {"font",     std::string(FONT_PATH)},
        {"text",     std::string("Miner")},
        {"scale",    TITLE_SCALE},
        {"viewport", static_cast<int>(UI_VP)},
    });
    titleEntityId = titleEnt->id;

    // Output slot via SlotSystem
    float slotX = panelX + PANEL_PADDING;
    float slotY = panelY + PANEL_PADDING + titleH + gapAfterTitle;

    auto slotRef = slotSystem->createSlot(SlotCategory::Output, 0);
    slotEntityId = slotRef.id;

    auto pos = slotRef.get<PositionComponent>();
    pos->setX(slotX);
    pos->setY(slotY);

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
}

void MinerUISystem::refreshProgressBar()
{
    MinerData* miner = minerSystem->getMiner(openMinerX, openMinerY);
    if (not miner)
        return;

    float progress = 0.0f;
    if (miner->isMining and MinerSystem::MINE_TIME_MS > 0)
        progress = static_cast<float>(miner->mineProgress) / static_cast<float>(MinerSystem::MINE_TIME_MS);

    if (progress > 1.0f)
        progress = 1.0f;

    auto fillEnt = ecsRef->getEntity(progressFillEntityId);
    if (fillEnt)
        fillEnt->get<PositionComponent>()->setWidth(PROGRESS_BAR_WIDTH * progress);
}

void MinerUISystem::setEntityVisibility(uint64_t id, bool vis)
{
    if (id == 0)
        return;

    auto ent = ecsRef->getEntity(id);
    if (ent)
        ent->get<PositionComponent>()->setVisibility(vis);
}
