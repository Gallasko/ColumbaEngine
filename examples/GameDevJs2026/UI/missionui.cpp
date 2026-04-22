#include "missionui.h"
#include "inventoryui.h"

#include "2D/simple2dobject.h"
#include "2D/texture.h"
#include "UI/ttftext.h"

#include <SDL2/SDL.h>
#include <cstdio>

// ---------------------------------------------------------------------------
// Toggle / Open / Close
// ---------------------------------------------------------------------------

void MissionUISystem::toggle()
{
    if (visible)
        close();
    else
        open();
}

void MissionUISystem::open()
{
    if (visible)
        return;
    visible = true;
    ensurePanelCreated();
    refresh();
    setPanelVisibility(true);
}

void MissionUISystem::close()
{
    if (not visible)
        return;
    visible = false;
    setPanelVisibility(false);
}

void MissionUISystem::selectDepot(int depotX, int depotY)
{
    if (not pendingStart.active)
        return;

    size_t defIndex = pendingStart.defIndex;
    pendingStart.active = false;

    if (missionSystem->startMission(defIndex, depotX, depotY))
    {
        printf("MissionUI: mission started at depot (%d, %d)\n", depotX, depotY);
        open();
    }
    else
    {
        printf("MissionUI: failed to start mission at depot (%d, %d)\n", depotX, depotY);
        open();
    }
}

void MissionUISystem::cancelDepotSelection()
{
    pendingStart.active = false;
    printf("MissionUI: depot selection cancelled\n");
}

// ---------------------------------------------------------------------------
// Events
// ---------------------------------------------------------------------------

void MissionUISystem::onProcessEvent(const OnSDLScanCode& event)
{
    if (event.key == SDL_SCANCODE_ESCAPE)
    {
        if (pendingStart.active)
            cancelDepotSelection();
        else if (visible)
            close();
    }
}

void MissionUISystem::onEvent(const TickEvent&)
{
    // Refresh is handled in execute()
}

void MissionUISystem::execute()
{
    if (visible)
        refresh();
}

void MissionUISystem::onProcessEvent(const OnMouseClick& event)
{
    if (not visible or event.button != SDL_BUTTON_LEFT)
        return;

    float mx = event.pos.x;
    float my = event.pos.y;
    float px = getPanelX();
    float py = getPanelY();

    // Close button
    if (isClickInRect(mx, my, px + PANEL_W - PADDING - CLOSE_SIZE, py + PADDING,
                      CLOSE_SIZE, CLOSE_SIZE))
    {
        close();
        return;
    }

    // Available mission GO buttons — enter depot selection mode
    const auto& defs = missionSystem->getDefs();
    for (size_t i = 0; i < MAX_DEF_ROWS and i < defs.size(); ++i)
    {
        if (isClickInRect(mx, my, defRows[i].btnX, defRows[i].btnY, BTN_W, BTN_H))
        {
            if (missionSystem->canStartMission(i))
            {
                pendingStart.defIndex = i;
                pendingStart.active = true;
                close();
                printf("MissionUI: select a depot to start '%s'\n", defs[i].name.c_str());
            }
            return;
        }
    }

    // Active mission CLAIM buttons
    const auto& active = missionSystem->getActive();
    for (size_t i = 0; i < MAX_ACTIVE_ROWS and i < active.size(); ++i)
    {
        if (isClickInRect(mx, my, activeRows[i].btnX, activeRows[i].btnY, BTN_W, BTN_H))
        {
            if (active[i].completed)
                missionSystem->claimMission(i);
            return;
        }
    }

    // Shop buy button
    if (isClickInRect(mx, my, shopBtnX, shopBtnY, BTN_W, BTN_H))
    {
        // Check if player has enough tickets (ID 35)
        size_t cost = missionSystem->getExtraSlotCost();
        if (playerInv and playerInv->getInventory().hasAtLeast(35, static_cast<uint16_t>(cost)))
        {
            playerInv->getInventory().remove(35, static_cast<uint16_t>(cost));
            missionSystem->purchaseExtraSlot();
        }
    }
}

// ---------------------------------------------------------------------------
// Depot selection
// ---------------------------------------------------------------------------

void MissionUISystem::tryStartWithFirstAvailableDepot(size_t defIndex)
{
    const auto& def = missionSystem->getDefs()[defIndex];
    const auto& depots = depotSystem->getAllDepots();

    for (const auto& [key, depot] : depots)
    {
        if (def.robotCoreCost == 0)
        {
            // No core cost — use the first depot found
            missionSystem->startMission(defIndex, depot.ownerX, depot.ownerY);
            return;
        }

        // Count cores in this depot
        uint16_t cores = 0;
        for (const auto& slot : depot.inventory.slots)
        {
            if (slot.id == MissionSystem::ROBOT_CORE_ID)
                cores += slot.count;
        }
        if (cores >= def.robotCoreCost)
        {
            missionSystem->startMission(defIndex, depot.ownerX, depot.ownerY);
            return;
        }
    }

    if (depots.empty())
        printf("MissionUI: no depot built\n");
    else
        printf("MissionUI: no depot has enough Robot Cores (%u needed)\n", def.robotCoreCost);
}

// ---------------------------------------------------------------------------
// Panel creation
// ---------------------------------------------------------------------------

void MissionUISystem::ensurePanelCreated()
{
    if (panelCreated)
        return;
    createPanel();
    setPanelVisibility(false);
    panelCreated = true;
}

void MissionUISystem::createPanel()
{
    // Calculate panel height
    size_t numDefs = std::min(missionSystem->getDefs().size(), MAX_DEF_ROWS);
    panelH = PADDING + TITLE_H
           + SECTION_H + numDefs * (ROW_H + ROW_GAP)
           + SECTION_H + MAX_ACTIVE_ROWS * (ROW_H + ROW_GAP)
           + SECTION_H + ROW_H
           + PADDING;

    float px = getPanelX();
    float py = getPanelY();
    float curY = py + PADDING;

    // Backdrop
    {
        auto bd = makeSimple2DShape(ecsRef, Shape2D::Square, 0.0f, 0.0f,
            constant::Vector4D{20.0f, 20.0f, 30.0f, 230.0f});
        auto pos = bd.get<PositionComponent>();
        pos->setX(px); pos->setY(py); pos->setZ(97.0f);
        pos->setWidth(PANEL_W); pos->setHeight(panelH);
        bd.get<ViewportComponent>()->setViewport(UI_VP);
        backdropId = bd.entity->id;

        ecsRef->attach<MouseLeftClickComponent>(bd.entity,
            makeCallable<PanelWasClickedEvent>(), MouseStateTrigger::OnPress);
    }

    // Title
    {
        auto t = makeTTFText(ecsRef, px + PADDING, curY, 100.0f,
            FONT_PATH, "MISSIONS", TITLE_SCALE, {255.0f, 255.0f, 255.0f, 255.0f});
        t.get<ViewportComponent>()->setViewport(UI_VP);
        titleTextId = t.entity->id;
    }

    // Close button
    {
        float cbx = px + PANEL_W - PADDING - CLOSE_SIZE;
        auto bg = makeSimple2DShape(ecsRef, Shape2D::Square, 0.0f, 0.0f,
            constant::Vector4D{180.0f, 60.0f, 60.0f, 200.0f});
        auto pos = bg.get<PositionComponent>();
        pos->setX(cbx); pos->setY(curY); pos->setZ(99.0f);
        pos->setWidth(CLOSE_SIZE); pos->setHeight(CLOSE_SIZE);
        bg.get<ViewportComponent>()->setViewport(UI_VP);
        closeBtnBgId = bg.entity->id;

        auto txt = makeTTFText(ecsRef, cbx + 6.0f, curY + 2.0f, 100.0f,
            FONT_PATH, "X", BTN_TEXT_SCALE, {255.0f, 255.0f, 255.0f, 255.0f});
        txt.get<ViewportComponent>()->setViewport(UI_VP);
        closeBtnTextId = txt.entity->id;
    }
    curY += TITLE_H;

    // --- Available section ---
    {
        auto h = makeTTFText(ecsRef, px + PADDING, curY, 100.0f,
            FONT_PATH, "AVAILABLE:", TEXT_SCALE, {180.0f, 180.0f, 200.0f, 255.0f});
        h.get<ViewportComponent>()->setViewport(UI_VP);
        availHeaderId = h.entity->id;
    }
    curY += SECTION_H;

    size_t numDefRows = std::min(missionSystem->getDefs().size(), MAX_DEF_ROWS);
    for (size_t i = 0; i < MAX_DEF_ROWS; ++i)
    {
        float rowY = curY + i * (ROW_H + ROW_GAP);
        auto& row = defRows[i];

        // Row bg
        auto bg = makeSimple2DShape(ecsRef, Shape2D::Square, 0.0f, 0.0f,
            constant::Vector4D{35.0f, 35.0f, 45.0f, 180.0f});
        auto pos = bg.get<PositionComponent>();
        pos->setX(px + PADDING); pos->setY(rowY); pos->setZ(98.0f);
        pos->setWidth(PANEL_W - 2 * PADDING); pos->setHeight(ROW_H);
        bg.get<ViewportComponent>()->setViewport(UI_VP);
        row.bgId = bg.entity->id;

        // Name
        auto name = makeTTFText(ecsRef, px + PADDING + 4.0f, rowY + 4.0f, 100.0f,
            FONT_PATH, "", TEXT_SCALE, {255.0f, 255.0f, 255.0f, 255.0f});
        name.get<ViewportComponent>()->setViewport(UI_VP);
        row.nameId = name.entity->id;

        // Info (cost + duration)
        auto info = makeTTFText(ecsRef, px + PADDING + 180.0f, rowY + 4.0f, 100.0f,
            FONT_PATH, "", TEXT_SCALE, {180.0f, 180.0f, 180.0f, 255.0f});
        info.get<ViewportComponent>()->setViewport(UI_VP);
        row.infoId = info.entity->id;

        // Button
        float bx = px + PANEL_W - PADDING - BTN_W - 4.0f;
        row.btnX = bx;
        row.btnY = rowY + 3.0f;

        auto btnBg = makeSimple2DShape(ecsRef, Shape2D::Square, 0.0f, 0.0f,
            constant::Vector4D{60.0f, 120.0f, 60.0f, 200.0f});
        auto bpos = btnBg.get<PositionComponent>();
        bpos->setX(bx); bpos->setY(row.btnY); bpos->setZ(99.0f);
        bpos->setWidth(BTN_W); bpos->setHeight(BTN_H);
        btnBg.get<ViewportComponent>()->setViewport(UI_VP);
        row.btnBgId = btnBg.entity->id;

        auto btnTxt = makeTTFText(ecsRef, bx + 12.0f, row.btnY + 3.0f, 100.0f,
            FONT_PATH, "GO", BTN_TEXT_SCALE, {255.0f, 255.0f, 255.0f, 255.0f});
        btnTxt.get<ViewportComponent>()->setViewport(UI_VP);
        row.btnTextId = btnTxt.entity->id;
    }
    curY += numDefRows * (ROW_H + ROW_GAP);

    // --- Active section ---
    {
        auto h = makeTTFText(ecsRef, px + PADDING, curY, 100.0f,
            FONT_PATH, "ACTIVE:", TEXT_SCALE, {180.0f, 180.0f, 200.0f, 255.0f});
        h.get<ViewportComponent>()->setViewport(UI_VP);
        activeHeaderId = h.entity->id;
    }
    curY += SECTION_H;

    for (size_t i = 0; i < MAX_ACTIVE_ROWS; ++i)
    {
        float rowY = curY + i * (ROW_H + ROW_GAP);
        auto& row = activeRows[i];

        // Row bg
        auto bg = makeSimple2DShape(ecsRef, Shape2D::Square, 0.0f, 0.0f,
            constant::Vector4D{35.0f, 35.0f, 45.0f, 180.0f});
        auto pos = bg.get<PositionComponent>();
        pos->setX(px + PADDING); pos->setY(rowY); pos->setZ(98.0f);
        pos->setWidth(PANEL_W - 2 * PADDING); pos->setHeight(ROW_H);
        bg.get<ViewportComponent>()->setViewport(UI_VP);
        row.bgId = bg.entity->id;

        // Name
        auto name = makeTTFText(ecsRef, px + PADDING + 4.0f, rowY + 4.0f, 100.0f,
            FONT_PATH, "", TEXT_SCALE, {255.0f, 255.0f, 255.0f, 255.0f});
        name.get<ViewportComponent>()->setViewport(UI_VP);
        row.nameId = name.entity->id;

        // Progress bar bg
        float pbX = px + PADDING + 140.0f;
        float pbY = rowY + (ROW_H - PROGRESS_H) * 0.5f;
        float pbW = 100.0f;

        auto pbBg = makeSimple2DShape(ecsRef, Shape2D::Square, 0.0f, 0.0f,
            constant::Vector4D{50.0f, 50.0f, 60.0f, 200.0f});
        auto pbPos = pbBg.get<PositionComponent>();
        pbPos->setX(pbX); pbPos->setY(pbY); pbPos->setZ(98.5f);
        pbPos->setWidth(pbW); pbPos->setHeight(PROGRESS_H);
        pbBg.get<ViewportComponent>()->setViewport(UI_VP);
        row.progressBgId = pbBg.entity->id;

        // Progress bar fill
        auto pbFill = makeSimple2DShape(ecsRef, Shape2D::Square, 0.0f, 0.0f,
            constant::Vector4D{80.0f, 160.0f, 80.0f, 220.0f});
        auto pfPos = pbFill.get<PositionComponent>();
        pfPos->setX(pbX); pfPos->setY(pbY); pfPos->setZ(98.6f);
        pfPos->setWidth(0.0f); pfPos->setHeight(PROGRESS_H);
        pbFill.get<ViewportComponent>()->setViewport(UI_VP);
        row.progressFillId = pbFill.entity->id;

        // Status text (percent or "DONE")
        auto status = makeTTFText(ecsRef, pbX + pbW + 4.0f, rowY + 4.0f, 100.0f,
            FONT_PATH, "", TEXT_SCALE, {180.0f, 180.0f, 180.0f, 255.0f});
        status.get<ViewportComponent>()->setViewport(UI_VP);
        row.statusId = status.entity->id;

        // CLAIM button
        float bx = px + PANEL_W - PADDING - BTN_W - 4.0f;
        row.btnX = bx;
        row.btnY = rowY + 3.0f;

        auto btnBg = makeSimple2DShape(ecsRef, Shape2D::Square, 0.0f, 0.0f,
            constant::Vector4D{60.0f, 100.0f, 180.0f, 200.0f});
        auto bpos = btnBg.get<PositionComponent>();
        bpos->setX(bx); bpos->setY(row.btnY); bpos->setZ(99.0f);
        bpos->setWidth(BTN_W); bpos->setHeight(BTN_H);
        btnBg.get<ViewportComponent>()->setViewport(UI_VP);
        row.btnBgId = btnBg.entity->id;

        auto btnTxt = makeTTFText(ecsRef, bx + 4.0f, row.btnY + 3.0f, 100.0f,
            FONT_PATH, "CLAIM", BTN_TEXT_SCALE, {255.0f, 255.0f, 255.0f, 255.0f});
        btnTxt.get<ViewportComponent>()->setViewport(UI_VP);
        row.btnTextId = btnTxt.entity->id;
    }
    curY += MAX_ACTIVE_ROWS * (ROW_H + ROW_GAP);

    // --- Shop section ---
    {
        auto h = makeTTFText(ecsRef, px + PADDING, curY, 100.0f,
            FONT_PATH, "SHOP:", TEXT_SCALE, {180.0f, 180.0f, 200.0f, 255.0f});
        h.get<ViewportComponent>()->setViewport(UI_VP);
        shopHeaderId = h.entity->id;
    }
    curY += SECTION_H;

    {
        auto lbl = makeTTFText(ecsRef, px + PADDING + 4.0f, curY + 4.0f, 100.0f,
            FONT_PATH, "Extra Mission Slot", TEXT_SCALE, {255.0f, 255.0f, 255.0f, 255.0f});
        lbl.get<ViewportComponent>()->setViewport(UI_VP);
        shopLabelId = lbl.entity->id;

        float bx = px + PANEL_W - PADDING - BTN_W - 4.0f;
        shopBtnX = bx;
        shopBtnY = curY + 3.0f;

        auto btnBg = makeSimple2DShape(ecsRef, Shape2D::Square, 0.0f, 0.0f,
            constant::Vector4D{180.0f, 160.0f, 60.0f, 200.0f});
        auto bpos = btnBg.get<PositionComponent>();
        bpos->setX(bx); bpos->setY(shopBtnY); bpos->setZ(99.0f);
        bpos->setWidth(BTN_W); bpos->setHeight(BTN_H);
        btnBg.get<ViewportComponent>()->setViewport(UI_VP);
        shopBtnBgId = btnBg.entity->id;

        auto btnTxt = makeTTFText(ecsRef, bx + 8.0f, shopBtnY + 3.0f, 100.0f,
            FONT_PATH, "BUY", BTN_TEXT_SCALE, {255.0f, 255.0f, 255.0f, 255.0f});
        btnTxt.get<ViewportComponent>()->setViewport(UI_VP);
        shopBtnTextId = btnTxt.entity->id;
    }
}

// ---------------------------------------------------------------------------
// Visibility
// ---------------------------------------------------------------------------

void MissionUISystem::setPanelVisibility(bool vis)
{
    setEntityVisibility(backdropId, vis);
    setEntityVisibility(titleTextId, vis);
    setEntityVisibility(closeBtnBgId, vis);
    setEntityVisibility(closeBtnTextId, vis);
    setEntityVisibility(availHeaderId, vis);
    setEntityVisibility(activeHeaderId, vis);
    setEntityVisibility(shopHeaderId, vis);
    setEntityVisibility(shopLabelId, vis);
    setEntityVisibility(shopBtnBgId, vis);
    setEntityVisibility(shopBtnTextId, vis);

    for (size_t i = 0; i < MAX_DEF_ROWS; ++i)
    {
        auto& r = defRows[i];
        setEntityVisibility(r.bgId, vis);
        setEntityVisibility(r.nameId, vis);
        setEntityVisibility(r.infoId, vis);
        setEntityVisibility(r.btnBgId, vis);
        setEntityVisibility(r.btnTextId, vis);
    }

    for (size_t i = 0; i < MAX_ACTIVE_ROWS; ++i)
    {
        auto& r = activeRows[i];
        setEntityVisibility(r.bgId, vis);
        setEntityVisibility(r.nameId, vis);
        setEntityVisibility(r.progressBgId, vis);
        setEntityVisibility(r.progressFillId, vis);
        setEntityVisibility(r.statusId, vis);
        setEntityVisibility(r.btnBgId, vis);
        setEntityVisibility(r.btnTextId, vis);
    }
}

// ---------------------------------------------------------------------------
// Refresh
// ---------------------------------------------------------------------------

void MissionUISystem::refresh()
{
    if (not visible)
        return;

    const auto& defs = missionSystem->getDefs();
    const auto& active = missionSystem->getActive();

    // Update available mission rows
    for (size_t i = 0; i < MAX_DEF_ROWS; ++i)
    {
        auto& row = defRows[i];
        bool show = i < defs.size();

        setEntityVisibility(row.bgId, show);
        setEntityVisibility(row.nameId, show);
        setEntityVisibility(row.infoId, show);
        setEntityVisibility(row.btnBgId, show);
        setEntityVisibility(row.btnTextId, show);

        if (not show)
            continue;

        const auto& def = defs[i];
        setEntityText(row.nameId, def.name);

        std::string info;
        if (def.isDeliveryMission())
        {
            for (const auto& req : def.deliveryRequirements)
                info += std::to_string(req.count) + "x #" + std::to_string(req.itemId) + " ";
        }
        else
        {
            info = std::to_string(def.durationMs / 1000) + "s  "
                 + std::to_string(def.robotCoreCost) + " Core";
        }
        setEntityText(row.infoId, info);

        bool unlocked = missionSystem->isMissionUnlocked(i);
        bool canStart = missionSystem->canStartMission(i);

        if (not unlocked)
        {
            setEntityText(row.btnTextId, "LOCKED");
            auto btnEnt = ecsRef->getEntity(row.btnBgId);
            if (btnEnt)
                btnEnt->get<Simple2DObject>()->setColors({80.0f, 80.0f, 80.0f, 200.0f});
        }
        else if (not canStart)
        {
            setEntityText(row.btnTextId, "FULL");
            auto btnEnt = ecsRef->getEntity(row.btnBgId);
            if (btnEnt)
                btnEnt->get<Simple2DObject>()->setColors({80.0f, 80.0f, 80.0f, 200.0f});
        }
        else
        {
            setEntityText(row.btnTextId, "GO");
            auto btnEnt = ecsRef->getEntity(row.btnBgId);
            if (btnEnt)
                btnEnt->get<Simple2DObject>()->setColors({60.0f, 120.0f, 60.0f, 200.0f});
        }
    }

    // Update active header
    std::string activeHeader = "ACTIVE (" + std::to_string(active.size()) + "/"
                              + std::to_string(missionSystem->getMaxActive()) + "):";
    setEntityText(activeHeaderId, activeHeader);

    // Update active mission rows
    for (size_t i = 0; i < MAX_ACTIVE_ROWS; ++i)
    {
        auto& row = activeRows[i];
        bool show = i < active.size();

        setEntityVisibility(row.bgId, show);
        setEntityVisibility(row.nameId, show);
        setEntityVisibility(row.progressBgId, show);
        setEntityVisibility(row.progressFillId, show);
        setEntityVisibility(row.statusId, show);
        setEntityVisibility(row.btnBgId, show and active.size() > i and active[i].completed);
        setEntityVisibility(row.btnTextId, show and active.size() > i and active[i].completed);

        if (not show)
            continue;

        const auto& m = active[i];
        const auto& def = defs[m.defIndex];

        setEntityText(row.nameId, def.name);

        float progress;
        if (def.isDeliveryMission())
            progress = missionSystem->getDeliveryProgress(m);
        else
            progress = def.durationMs > 0
                ? static_cast<float>(m.elapsedMs) / static_cast<float>(def.durationMs)
                : 1.0f;
        if (progress > 1.0f) progress = 1.0f;

        // Update progress bar fill width
        auto fillEnt = ecsRef->getEntity(row.progressFillId);
        if (fillEnt)
            fillEnt->get<PositionComponent>()->setWidth(100.0f * progress);

        if (m.completed)
        {
            setEntityText(row.statusId, "DONE!");
            auto btnEnt = ecsRef->getEntity(row.btnBgId);
            if (btnEnt)
                btnEnt->get<Simple2DObject>()->setColors({60.0f, 100.0f, 180.0f, 200.0f});
        }
        else if (def.isDeliveryMission())
        {
            // Show delivery count for first requirement
            const auto& req = def.deliveryRequirements[0];
            uint16_t have = missionSystem->getDeliveryCount(m, req);
            setEntityText(row.statusId, std::to_string(have) + "/" + std::to_string(req.count));
        }
        else
        {
            int pct = static_cast<int>(progress * 100.0f);
            setEntityText(row.statusId, std::to_string(pct) + "%");
        }
    }

    // Update shop
    size_t cost = missionSystem->getExtraSlotCost();
    std::string shopLabel = "Extra Mission Slot  " + std::to_string(cost) + " Tickets";
    setEntityText(shopLabelId, shopLabel);
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

void MissionUISystem::setEntityVisibility(uint64_t id, bool vis)
{
    if (id == 0) return;
    auto ent = ecsRef->getEntity(id);
    if (ent)
        ent->get<PositionComponent>()->setVisibility(vis);
}

void MissionUISystem::setEntityText(uint64_t id, const std::string& text)
{
    if (id == 0) return;
    auto ent = ecsRef->getEntity(id);
    if (ent and ent->has<TTFText>())
        ent->get<TTFText>()->setText(text);
}

bool MissionUISystem::isClickInRect(float cx, float cy, float rx, float ry, float rw, float rh) const
{
    return cx >= rx and cx <= rx + rw and cy >= ry and cy <= ry + rh;
}
