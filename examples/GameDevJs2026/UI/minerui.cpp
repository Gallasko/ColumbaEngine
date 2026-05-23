#include "minerui.h"

#include "2D/simple2dobject.h"
#include "2D/position.h"
#include "UI/ttftext.h"

#include <SDL2/SDL.h>

void MinerUISystem::open(int gridX, int gridY)
{
    if (visible)
        close();

    openMinerX = gridX;
    openMinerY = gridY;
    visible = true;

    auto* inventoryUI = ecsRef->getSystem<InventoryUISystem>();
    if (inventoryUI and not inventoryUI->isOpen())
        inventoryUI->openInventory();

    ensurePanelCreated();
    setPanelVisibility(true);

    MinerData* miner = ecsRef->getSystem<MinerSystem>()->getMiner(openMinerX, openMinerY);
    if (miner)
        ecsRef->getSystem<SlotSystem>()->syncSlotVisual(slotEntityId, miner->outputSlots.getSlot(0));
}

void MinerUISystem::close()
{
    if (not visible)
        return;

    auto* slotSystem = ecsRef->getSystem<SlotSystem>();
    if (slotSystem->hasHeldItem())
        slotSystem->cancelHeld();

    setPanelVisibility(false);
    visible = false;
    openMinerX = -1;
    openMinerY = -1;

    auto* inventoryUI = ecsRef->getSystem<InventoryUISystem>();
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

    MinerData* miner = ecsRef->getSystem<MinerSystem>()->getMiner(openMinerX, openMinerY);
    if (not miner)
    {
        close();
        return;
    }

    ecsRef->getSystem<SlotSystem>()->syncSlotVisual(slotEntityId, miner->outputSlots.getSlot(0));
    refreshProgressBar();
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
    const float titleH = 20.0f;
    const float gapAfterTitle = 6.0f;
    const float gapAfterSlot = 8.0f;

    const float contentH = titleH + gapAfterTitle + SLOT_SIZE + gapAfterSlot + PROGRESS_BAR_HEIGHT;
    const float panelW = SLOT_SIZE + 2 * PANEL_PADDING;
    const float panelH = contentH + 2 * PANEL_PADDING;

    // Anchor target: the miner panel sits left of the inventory panel, vertically
    // centred against it. Falls back to centring in __MainWindow if inventoryUI
    // hasn't created its panel yet.
    uint64_t anchorTargetId = 0;
    if (auto* inventoryUI = ecsRef->getSystem<InventoryUISystem>())
        anchorTargetId = inventoryUI->getBackdropEntityId();
    if (anchorTargetId == 0)
    {
        auto windowEnt = ecsRef->getEntity("__MainWindow");
        if (windowEnt) anchorTargetId = windowEnt->id;
    }

    auto* factory = ecsRef->getSystem<PrefabFactoryRegistry>();
    auto panelEnt = factory->build("Panel", PrefabParams{
        {"width", panelW}, {"height", panelH},
        {"r", 20.0f}, {"g", 20.0f}, {"b", 30.0f}, {"a", 220.0f},
        {"z", 97.0f},
        {"viewport", static_cast<int>(UI_VP)},
    });
    backdropEntityId = panelEnt->id;

    {
        auto a = panelEnt->get<UiAnchor>();
        a->setRightAnchor(PosAnchor{anchorTargetId, AnchorType::Left});
        a->setRightMargin(GAP_BETWEEN_PANELS);
        a->setVerticalCenter(PosAnchor{anchorTargetId, AnchorType::VerticalCenter});
    }

    if (auto bgEnt = panelEnt->get<Prefab>()->getEntity("bg"))
        ecsRef->attach<MouseLeftClickComponent>(bgEnt,
            makeCallable<PanelWasClickedEvent>(), MouseStateTrigger::OnPress);

    auto titleEnt = factory->build("Text", PrefabParams{
        {"x", 0.0f}, {"y", 0.0f},
        {"z",        100.0f},
        {"font",     std::string(FONT_PATH)},
        {"text",     std::string("Miner")},
        {"scale",    TITLE_SCALE},
        {"viewport", static_cast<int>(UI_VP)},
    });
    titleEntityId = titleEnt->id;
    {
        auto a = ecsRef->attach<UiAnchor>(titleEnt);
        a->setLeftAnchor(PosAnchor{backdropEntityId, AnchorType::Left});
        a->setTopAnchor(PosAnchor{backdropEntityId, AnchorType::Top});
        a->setLeftMargin(PANEL_PADDING);
        a->setTopMargin(PANEL_PADDING + 4.0f);
    }

    // Output slot via SlotSystem (UiAnchor auto-attached)
    auto* slotSystem = ecsRef->getSystem<SlotSystem>();
    auto slotRef = slotSystem->createSlot(SlotCategory::Output, 0);
    slotEntityId = slotRef.id;
    slotSystem->bindSlotChange(slotRef, [this](const ItemStack& s) {
        if (auto* m = ecsRef->getSystem<MinerSystem>()->getMiner(openMinerX, openMinerY))
            m->outputSlots.getSlot(0) = s;
    });
    {
        auto a = slotRef.get<UiAnchor>();
        a->setLeftAnchor(PosAnchor{backdropEntityId, AnchorType::Left});
        a->setTopAnchor(PosAnchor{backdropEntityId, AnchorType::Top});
        a->setLeftMargin(PANEL_PADDING);
        a->setTopMargin(PANEL_PADDING + titleH + gapAfterTitle);
    }

    // Progress bar background
    auto barBg = makeSimple2DShape(ecsRef, Shape2D::Square, 0.0f, 0.0f,
        constant::Vector4D{40.0f, 40.0f, 50.0f, 200.0f});
    {
        auto pos = barBg.get<PositionComponent>();
        pos->setZ(98.0f);
        pos->setWidth(PROGRESS_BAR_WIDTH);
        pos->setHeight(PROGRESS_BAR_HEIGHT);
        barBg.get<ViewportComponent>()->setViewport(UI_VP);
        progressBgEntityId = barBg.entity->id;

        auto a = ecsRef->attach<UiAnchor>(barBg.entity);
        a->setLeftAnchor(PosAnchor{backdropEntityId, AnchorType::Left});
        a->setTopAnchor(PosAnchor{backdropEntityId, AnchorType::Top});
        a->setLeftMargin(PANEL_PADDING);
        a->setTopMargin(PANEL_PADDING + titleH + gapAfterTitle + SLOT_SIZE + gapAfterSlot);
    }

    // Progress bar fill
    auto barFill = makeSimple2DShape(ecsRef, Shape2D::Square, 0.0f, 0.0f,
        constant::Vector4D{80.0f, 200.0f, 80.0f, 255.0f});
    {
        auto pos = barFill.get<PositionComponent>();
        pos->setZ(99.0f);
        pos->setWidth(0.0f);
        pos->setHeight(PROGRESS_BAR_HEIGHT);
        barFill.get<ViewportComponent>()->setViewport(UI_VP);
        progressFillEntityId = barFill.entity->id;

        auto a = ecsRef->attach<UiAnchor>(barFill.entity);
        a->setLeftAnchor(PosAnchor{backdropEntityId, AnchorType::Left});
        a->setTopAnchor(PosAnchor{backdropEntityId, AnchorType::Top});
        a->setLeftMargin(PANEL_PADDING);
        a->setTopMargin(PANEL_PADDING + titleH + gapAfterTitle + SLOT_SIZE + gapAfterSlot);
    }
}

void MinerUISystem::refreshProgressBar()
{
    MinerData* miner = ecsRef->getSystem<MinerSystem>()->getMiner(openMinerX, openMinerY);
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
