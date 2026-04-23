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
    hideDepotSelectionPrompt();

    if (missionSystem->startMission(defIndex, depotX, depotY))
        printf("MissionUI: mission started at depot (%d, %d)\n", depotX, depotY);
    else
        printf("MissionUI: failed to start mission at depot (%d, %d)\n", depotX, depotY);
}

void MissionUISystem::cancelDepotSelection()
{
    pendingStart.active = false;
    hideDepotSelectionPrompt();
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

    // Tab clicks
    for (size_t t = 0; t < NUM_TABS; ++t)
    {
        if (isClickInRect(mx, my, tabButtons[t].x, tabButtons[t].y,
                          tabButtons[t].w, TAB_H))
        {
            switchTab(t);
            return;
        }
    }

    // Available mission buttons
    for (size_t i = 0; i < MAX_DEF_ROWS and i < filteredDefs.size(); ++i)
    {
        if (isClickInRect(mx, my, defRows[i].btnX, defRows[i].btnY, BTN_W, BTN_H))
        {
            size_t defIndex = filteredDefs[i];

            if (currentTab == 0)
            {
                // Main tab: validate from player inventory
                missionSystem->validateMainMission(defIndex);
            }
            else if (currentTab == 1)
            {
                // Missions tab: enter depot selection
                if (missionSystem->canStartMission(defIndex))
                {
                    pendingStart.defIndex = defIndex;
                    pendingStart.active = true;
                    close();
                    showDepotSelectionPrompt();
                }
            }
            return;
        }
    }

    // Active mission CLAIM buttons (tab 1 only)
    if (currentTab == 1)
    {
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
    }

    // Shop buy button (tab 2 only)
    if (currentTab == 2)
    {
        if (isClickInRect(mx, my, shopBtnX, shopBtnY, BTN_W, BTN_H))
        {
            uint32_t cost = static_cast<uint32_t>(missionSystem->getExtraSlotCost());
            if (playerInv and playerInv->spendTickets(cost))
                missionSystem->purchaseExtraSlot();
        }
    }
}

// ---------------------------------------------------------------------------
// Tabs
// ---------------------------------------------------------------------------

void MissionUISystem::switchTab(size_t tab)
{
    if (tab == currentTab)
        return;
    currentTab = tab;
    refresh();
}

std::vector<size_t> MissionUISystem::getFilteredDefs(MissionCategory cat) const
{
    std::vector<size_t> result;
    const auto& defs = missionSystem->getDefs();
    for (size_t i = 0; i < defs.size(); ++i)
    {
        if (defs[i].category == cat)
            result.push_back(i);
    }
    return result;
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
            missionSystem->startMission(defIndex, depot.ownerX, depot.ownerY);
            return;
        }

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
    // Fixed panel height accommodating the largest tab content
    panelH = PADDING + TITLE_H
           + TAB_H + TAB_GAP
           + SECTION_H + MAX_DEF_ROWS * (ROW_H + ROW_GAP)
           + SECTION_H + MAX_ACTIVE_ROWS * (ROW_H + ROW_GAP)
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

    // --- Tab bar ---
    {
        static const char* TAB_LABELS[NUM_TABS] = {"Main", "Missions", "Shop"};
        float contentW = PANEL_W - 2.0f * PADDING;
        float tabW = (contentW - (NUM_TABS - 1) * 2.0f) / NUM_TABS;

        for (size_t t = 0; t < NUM_TABS; ++t)
        {
            float tx = px + PADDING + t * (tabW + 2.0f);
            tabButtons[t].x = tx;
            tabButtons[t].y = curY;
            tabButtons[t].w = tabW;

            auto bg = makeSimple2DShape(ecsRef, Shape2D::Square, 0.0f, 0.0f,
                constant::Vector4D{50.0f, 50.0f, 65.0f, 200.0f});
            auto pos = bg.get<PositionComponent>();
            pos->setX(tx); pos->setY(curY); pos->setZ(98.5f);
            pos->setWidth(tabW); pos->setHeight(TAB_H);
            bg.get<ViewportComponent>()->setViewport(UI_VP);
            tabButtons[t].bgId = bg.entity->id;

            auto txt = makeTTFText(ecsRef, tx + 8.0f, curY + 4.0f, 100.0f,
                FONT_PATH, TAB_LABELS[t], BTN_TEXT_SCALE, {255.0f, 255.0f, 255.0f, 255.0f});
            txt.get<ViewportComponent>()->setViewport(UI_VP);
            tabButtons[t].textId = txt.entity->id;
        }
    }
    curY += TAB_H + TAB_GAP;

    // --- Available section header ---
    {
        auto h = makeTTFText(ecsRef, px + PADDING, curY, 100.0f,
            FONT_PATH, "AVAILABLE:", TEXT_SCALE, {180.0f, 180.0f, 200.0f, 255.0f});
        h.get<ViewportComponent>()->setViewport(UI_VP);
        availHeaderId = h.entity->id;
    }
    curY += SECTION_H;

    // --- Available rows (with req icons) ---
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

        // Info text (fallback for timed missions)
        auto info = makeTTFText(ecsRef, px + PADDING + 160.0f, rowY + 4.0f, 100.0f,
            FONT_PATH, "", TEXT_SCALE, {180.0f, 180.0f, 180.0f, 255.0f});
        info.get<ViewportComponent>()->setViewport(UI_VP);
        row.infoId = info.entity->id;

        // Delivery requirement icons + counts
        for (size_t r = 0; r < MAX_REQS; ++r)
        {
            float rx = px + PADDING + 160.0f + r * 44.0f;
            float ry = rowY + (ROW_H - REQ_ICON_SIZE) * 0.5f;

            auto icon = make2DTexture(ecsRef, REQ_ICON_SIZE, REQ_ICON_SIZE, "Items.0");
            auto iconPos = icon.get<PositionComponent>();
            iconPos->setX(rx); iconPos->setY(ry); iconPos->setZ(99.0f);
            iconPos->setVisibility(false);
            icon.get<ViewportComponent>()->setViewport(UI_VP);
            row.reqs[r].iconId = icon.entity->id;

            auto cnt = makeTTFText(ecsRef, rx + REQ_ICON_SIZE + 2.0f, rowY + 4.0f, 100.0f,
                FONT_PATH, "", TEXT_SCALE, {220.0f, 220.0f, 220.0f, 255.0f});
            cnt.get<PositionComponent>()->setVisibility(false);
            cnt.get<ViewportComponent>()->setViewport(UI_VP);
            row.reqs[r].countTextId = cnt.entity->id;
        }

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
    curY += MAX_DEF_ROWS * (ROW_H + ROW_GAP);

    // --- Active section header ---
    {
        auto h = makeTTFText(ecsRef, px + PADDING, curY, 100.0f,
            FONT_PATH, "ACTIVE:", TEXT_SCALE, {180.0f, 180.0f, 200.0f, 255.0f});
        h.get<ViewportComponent>()->setViewport(UI_VP);
        activeHeaderId = h.entity->id;
    }
    curY += SECTION_H;

    // --- Active rows ---
    for (size_t i = 0; i < MAX_ACTIVE_ROWS; ++i)
    {
        float rowY = curY + i * (ROW_H + ROW_GAP);
        auto& row = activeRows[i];

        auto bg = makeSimple2DShape(ecsRef, Shape2D::Square, 0.0f, 0.0f,
            constant::Vector4D{35.0f, 35.0f, 45.0f, 180.0f});
        auto pos = bg.get<PositionComponent>();
        pos->setX(px + PADDING); pos->setY(rowY); pos->setZ(98.0f);
        pos->setWidth(PANEL_W - 2 * PADDING); pos->setHeight(ROW_H);
        bg.get<ViewportComponent>()->setViewport(UI_VP);
        row.bgId = bg.entity->id;

        auto nameE = makeTTFText(ecsRef, px + PADDING + 4.0f, rowY + 4.0f, 100.0f,
            FONT_PATH, "", TEXT_SCALE, {255.0f, 255.0f, 255.0f, 255.0f});
        nameE.get<ViewportComponent>()->setViewport(UI_VP);
        row.nameId = nameE.entity->id;

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

        auto pbFill = makeSimple2DShape(ecsRef, Shape2D::Square, 0.0f, 0.0f,
            constant::Vector4D{80.0f, 160.0f, 80.0f, 220.0f});
        auto pfPos = pbFill.get<PositionComponent>();
        pfPos->setX(pbX); pfPos->setY(pbY); pfPos->setZ(98.6f);
        pfPos->setWidth(0.0f); pfPos->setHeight(PROGRESS_H);
        pbFill.get<ViewportComponent>()->setViewport(UI_VP);
        row.progressFillId = pbFill.entity->id;

        auto status = makeTTFText(ecsRef, pbX + pbW + 4.0f, rowY + 4.0f, 100.0f,
            FONT_PATH, "", TEXT_SCALE, {180.0f, 180.0f, 180.0f, 255.0f});
        status.get<ViewportComponent>()->setViewport(UI_VP);
        row.statusId = status.entity->id;

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

    // --- Shop section (tab 2 content, placed after active area) ---
    // Reuse same curY area as active section for shop (they're never shown together)
    {
        // Position shop content where active section header would be
        float shopY = py + PADDING + TITLE_H + TAB_H + TAB_GAP + SECTION_H;

        auto lbl = makeTTFText(ecsRef, px + PADDING + 4.0f, shopY + 4.0f, 100.0f,
            FONT_PATH, "Extra Mission Slot", TEXT_SCALE, {255.0f, 255.0f, 255.0f, 255.0f});
        lbl.get<ViewportComponent>()->setViewport(UI_VP);
        shopLabelId = lbl.entity->id;

        float bx = px + PANEL_W - PADDING - BTN_W - 4.0f;
        shopBtnX = bx;
        shopBtnY = shopY + 3.0f;

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
    // Chrome (always visible when panel is open)
    setEntityVisibility(backdropId, vis);
    setEntityVisibility(titleTextId, vis);
    setEntityVisibility(closeBtnBgId, vis);
    setEntityVisibility(closeBtnTextId, vis);

    for (size_t t = 0; t < NUM_TABS; ++t)
    {
        setEntityVisibility(tabButtons[t].bgId, vis);
        setEntityVisibility(tabButtons[t].textId, vis);
    }

    // Content — let refresh() handle per-row visibility
    if (not vis)
    {
        setEntityVisibility(availHeaderId, false);
        for (size_t i = 0; i < MAX_DEF_ROWS; ++i)
        {
            auto& r = defRows[i];
            setEntityVisibility(r.bgId, false);
            setEntityVisibility(r.nameId, false);
            setEntityVisibility(r.infoId, false);
            setEntityVisibility(r.btnBgId, false);
            setEntityVisibility(r.btnTextId, false);
            for (size_t j = 0; j < MAX_REQS; ++j)
            {
                setEntityVisibility(r.reqs[j].iconId, false);
                setEntityVisibility(r.reqs[j].countTextId, false);
            }
        }

        setEntityVisibility(activeHeaderId, false);
        for (size_t i = 0; i < MAX_ACTIVE_ROWS; ++i)
        {
            auto& r = activeRows[i];
            setEntityVisibility(r.bgId, false);
            setEntityVisibility(r.nameId, false);
            setEntityVisibility(r.progressBgId, false);
            setEntityVisibility(r.progressFillId, false);
            setEntityVisibility(r.statusId, false);
            setEntityVisibility(r.btnBgId, false);
            setEntityVisibility(r.btnTextId, false);
        }

        setEntityVisibility(shopLabelId, false);
        setEntityVisibility(shopBtnBgId, false);
        setEntityVisibility(shopBtnTextId, false);
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

    // Update tab styling
    for (size_t t = 0; t < NUM_TABS; ++t)
    {
        auto bgEnt = ecsRef->getEntity(tabButtons[t].bgId);
        if (bgEnt)
        {
            if (t == currentTab)
                bgEnt->get<Simple2DObject>()->setColors({70.0f, 70.0f, 100.0f, 240.0f});
            else
                bgEnt->get<Simple2DObject>()->setColors({40.0f, 40.0f, 55.0f, 200.0f});
        }
    }

    // Filter defs for current tab
    bool showAvail = (currentTab == 0 or currentTab == 1);
    bool showActive = (currentTab == 1);
    bool showShop = (currentTab == 2);

    if (showAvail)
    {
        MissionCategory cat = (currentTab == 0) ? MissionCategory::Main : MissionCategory::Endgame;
        filteredDefs = getFilteredDefs(cat);
    }
    else
    {
        filteredDefs.clear();
    }

    // --- Available section ---
    setEntityVisibility(availHeaderId, showAvail);

    for (size_t i = 0; i < MAX_DEF_ROWS; ++i)
    {
        auto& row = defRows[i];
        bool show = showAvail and i < filteredDefs.size();

        setEntityVisibility(row.bgId, show);
        setEntityVisibility(row.nameId, show);
        setEntityVisibility(row.btnBgId, show);
        setEntityVisibility(row.btnTextId, show);

        if (not show)
        {
            setEntityVisibility(row.infoId, false);
            for (size_t r = 0; r < MAX_REQS; ++r)
            {
                setEntityVisibility(row.reqs[r].iconId, false);
                setEntityVisibility(row.reqs[r].countTextId, false);
            }
            continue;
        }

        size_t defIndex = filteredDefs[i];
        const auto& def = defs[defIndex];
        setEntityText(row.nameId, def.name);

        // Show delivery requirements with icons or fallback text
        if (def.isDeliveryMission() and itemRegistry)
        {
            setEntityVisibility(row.infoId, false);
            for (size_t r = 0; r < MAX_REQS; ++r)
            {
                bool hasReq = r < def.deliveryRequirements.size();
                setEntityVisibility(row.reqs[r].iconId, hasReq);
                setEntityVisibility(row.reqs[r].countTextId, hasReq);

                if (hasReq)
                {
                    const auto& req = def.deliveryRequirements[r];
                    setEntityTexture(row.reqs[r].iconId, itemRegistry->get(req.itemId).textureName);
                    setEntityText(row.reqs[r].countTextId, std::to_string(req.count));
                }
            }
        }
        else
        {
            // Timed mission: show duration + core cost as text
            for (size_t r = 0; r < MAX_REQS; ++r)
            {
                setEntityVisibility(row.reqs[r].iconId, false);
                setEntityVisibility(row.reqs[r].countTextId, false);
            }
            setEntityVisibility(row.infoId, true);
            std::string info = std::to_string(def.durationMs / 1000) + "s  "
                             + std::to_string(def.robotCoreCost) + " Core";
            setEntityText(row.infoId, info);
        }

        // Button state
        bool unlocked = missionSystem->isMissionUnlocked(defIndex);
        bool completed = missionSystem->isMissionCompleted(defIndex);

        if (currentTab == 0)
        {
            // Main tab: GO button greyed out unless player has all items
            if (not unlocked)
            {
                setEntityText(row.btnTextId, "LOCKED");
                auto btnEnt = ecsRef->getEntity(row.btnBgId);
                if (btnEnt)
                    btnEnt->get<Simple2DObject>()->setColors({80.0f, 80.0f, 80.0f, 200.0f});
            }
            else if (completed)
            {
                setEntityText(row.btnTextId, "DONE");
                auto btnEnt = ecsRef->getEntity(row.btnBgId);
                if (btnEnt)
                    btnEnt->get<Simple2DObject>()->setColors({60.0f, 60.0f, 80.0f, 200.0f});
            }
            else if (missionSystem->canValidateMainMission(defIndex))
            {
                setEntityText(row.btnTextId, "GO");
                auto btnEnt = ecsRef->getEntity(row.btnBgId);
                if (btnEnt)
                    btnEnt->get<Simple2DObject>()->setColors({60.0f, 120.0f, 60.0f, 200.0f});
            }
            else
            {
                setEntityText(row.btnTextId, "GO");
                auto btnEnt = ecsRef->getEntity(row.btnBgId);
                if (btnEnt)
                    btnEnt->get<Simple2DObject>()->setColors({80.0f, 80.0f, 80.0f, 200.0f});
            }
        }
        else
        {
            // Missions tab: existing GO / LOCKED / DONE / FULL states
            bool canStart = missionSystem->canStartMission(defIndex);

            if (not unlocked)
            {
                setEntityText(row.btnTextId, "LOCKED");
                auto btnEnt = ecsRef->getEntity(row.btnBgId);
                if (btnEnt)
                    btnEnt->get<Simple2DObject>()->setColors({80.0f, 80.0f, 80.0f, 200.0f});
            }
            else if (not def.repeatable and completed)
            {
                setEntityText(row.btnTextId, "DONE");
                auto btnEnt = ecsRef->getEntity(row.btnBgId);
                if (btnEnt)
                    btnEnt->get<Simple2DObject>()->setColors({60.0f, 60.0f, 80.0f, 200.0f});
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
    }

    // --- Active section (tab 1 only) ---
    setEntityVisibility(activeHeaderId, showActive);

    if (showActive)
    {
        std::string activeHeader = "ACTIVE (" + std::to_string(active.size()) + "/"
                                  + std::to_string(missionSystem->getMaxActive()) + "):";
        setEntityText(activeHeaderId, activeHeader);
    }

    for (size_t i = 0; i < MAX_ACTIVE_ROWS; ++i)
    {
        auto& row = activeRows[i];
        bool show = showActive and i < active.size();

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

    // --- Shop section (tab 2 only) ---
    setEntityVisibility(shopLabelId, showShop);
    setEntityVisibility(shopBtnBgId, showShop);
    setEntityVisibility(shopBtnTextId, showShop);

    if (showShop)
    {
        size_t cost = missionSystem->getExtraSlotCost();
        std::string shopLabel = "Extra Mission Slot  " + std::to_string(cost) + " Tickets";
        setEntityText(shopLabelId, shopLabel);
    }
}

// ---------------------------------------------------------------------------
// Depot selection prompt
// ---------------------------------------------------------------------------

void MissionUISystem::showDepotSelectionPrompt()
{
    if (promptBgId == 0)
    {
        float bannerW = 300.0f;
        float bannerH = 32.0f;
        float bx = (screenWidth - bannerW) * 0.5f;
        float by = 80.0f;

        auto bg = makeSimple2DShape(ecsRef, Shape2D::Square, 0.0f, 0.0f,
            constant::Vector4D{20.0f, 20.0f, 30.0f, 220.0f});
        auto pos = bg.get<PositionComponent>();
        pos->setX(bx); pos->setY(by); pos->setZ(101.0f);
        pos->setWidth(bannerW); pos->setHeight(bannerH);
        bg.get<ViewportComponent>()->setViewport(UI_VP);
        promptBgId = bg.entity->id;

        auto txt = makeTTFText(ecsRef, bx + 16.0f, by + 6.0f, 102.0f,
            FONT_PATH, "Click a depot to start mission", TEXT_SCALE,
            {255.0f, 230.0f, 80.0f, 255.0f});
        txt.get<ViewportComponent>()->setViewport(UI_VP);
        promptTextId = txt.entity->id;
    }

    setEntityVisibility(promptBgId, true);
    setEntityVisibility(promptTextId, true);
    promptVisible = true;
}

void MissionUISystem::hideDepotSelectionPrompt()
{
    if (promptBgId == 0)
        return;
    setEntityVisibility(promptBgId, false);
    setEntityVisibility(promptTextId, false);
    promptVisible = false;
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

void MissionUISystem::setEntityTexture(uint64_t id, const std::string& textureName)
{
    if (id == 0) return;
    auto ent = ecsRef->getEntity(id);
    if (ent and ent->has<Texture2DComponent>())
        ent->get<Texture2DComponent>()->setTexture(textureName);
}

bool MissionUISystem::isClickInRect(float cx, float cy, float rx, float ry, float rw, float rh) const
{
    return cx >= rx and cx <= rx + rw and cy >= ry and cy <= ry + rh;
}
