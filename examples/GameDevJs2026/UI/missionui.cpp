#include "missionui.h"

#include "2D/simple2dobject.h"
#include "2D/texture.h"
#include "UI/ttftext.h"

#include <SDL2/SDL.h>
#include <cstdio>
#include <algorithm>

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

    // Rebuild filtered defs and auto-select first actionable mission
    MissionCategory cat = (currentTab == 0) ? MissionCategory::Main : MissionCategory::Endgame;
    if (currentTab < 2)
        filteredDefs = getFilteredDefs(cat);
    else
        filteredDefs.clear();

    selectedDefIndex = SIZE_MAX;
    for (size_t i = 0; i < filteredDefs.size(); ++i)
    {
        if (missionSystem->isMissionUnlocked(filteredDefs[i]) and
            not missionSystem->isMissionCompleted(filteredDefs[i]))
        {
            selectedDefIndex = filteredDefs[i];
            break;
        }
    }
    if (selectedDefIndex == SIZE_MAX and not filteredDefs.empty())
        selectedDefIndex = filteredDefs[0];

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

    // Tab clicks (breadcrumb text areas)
    for (size_t t = 0; t < NUM_TABS; ++t)
    {
        if (isClickInRect(mx, my, tabButtons[t].x, tabButtons[t].y,
                          tabButtons[t].w, TITLE_H))
        {
            switchTab(t);
            return;
        }
    }

    // Left column: row clicks
    for (size_t i = 0; i < MAX_LIST_ROWS and i < filteredDefs.size(); ++i)
    {
        if (isClickInRect(mx, my, listRows[i].rowX, listRows[i].rowY,
                          listRows[i].rowW, LIST_ROW_H))
        {
            selectMission(filteredDefs[i]);
            return;
        }
    }

    // Right column: action button click
    if (isClickInRect(mx, my, actionBtnX, actionBtnY, actionBtnW, ACTION_BTN_H))
    {
        if (currentTab == 2)
        {
            // Shop buy
            uint32_t cost = static_cast<uint32_t>(missionSystem->getExtraSlotCost());
            if (playerInv and playerInv->spendTickets(cost))
                missionSystem->purchaseExtraSlot();
            return;
        }

        if (selectedDefIndex == SIZE_MAX)
            return;

        size_t defIndex = selectedDefIndex;
        bool unlocked = missionSystem->isMissionUnlocked(defIndex);
        bool completed = missionSystem->isMissionCompleted(defIndex);
        const auto& def = missionSystem->getDefs()[defIndex];

        if (not unlocked or (completed and not def.repeatable))
            return;

        // Check if this mission is already active and completed (claimable)
        const auto& active = missionSystem->getActive();
        for (size_t ai = 0; ai < active.size(); ++ai)
        {
            if (active[ai].defIndex == defIndex and active[ai].completed)
            {
                missionSystem->claimMission(ai);
                return;
            }
        }

        if (currentTab == 0)
        {
            missionSystem->validateMainMission(defIndex);
        }
        else if (currentTab == 1)
        {
            if (missionSystem->canStartMission(defIndex))
            {
                pendingStart.defIndex = defIndex;
                pendingStart.active = true;
                close();
                showDepotSelectionPrompt();
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Tabs & Selection
// ---------------------------------------------------------------------------

void MissionUISystem::switchTab(size_t tab)
{
    if (tab == currentTab)
        return;
    currentTab = tab;

    if (currentTab < 2)
    {
        MissionCategory cat = (currentTab == 0) ? MissionCategory::Main : MissionCategory::Endgame;
        filteredDefs = getFilteredDefs(cat);
    }
    else
    {
        filteredDefs.clear();
    }

    // Auto-select first actionable mission
    selectedDefIndex = SIZE_MAX;
    for (size_t i = 0; i < filteredDefs.size(); ++i)
    {
        if (missionSystem->isMissionUnlocked(filteredDefs[i]) and
            not missionSystem->isMissionCompleted(filteredDefs[i]))
        {
            selectedDefIndex = filteredDefs[i];
            break;
        }
    }
    if (selectedDefIndex == SIZE_MAX and not filteredDefs.empty())
        selectedDefIndex = filteredDefs[0];

    refresh();
}

void MissionUISystem::selectMission(size_t defIndex)
{
    if (defIndex == selectedDefIndex)
        return;
    selectedDefIndex = defIndex;
    refreshLeftColumn();
    refreshRightColumn();
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
    float listContentH = MAX_LIST_ROWS * (LIST_ROW_H + SEPARATOR_H);
    panelH = PADDING + TITLE_H + DIVIDER_H + std::max(listContentH, 340.0f) + PADDING;

    float px = getPanelX();
    float py = getPanelY();

    // --- Backdrop ---
    {
        auto bd = makeRoundedRect2DShape(ecsRef, 8.0f, PANEL_W, panelH, C::BG);
        auto pos = bd.get<PositionComponent>();
        pos->setX(px); pos->setY(py); pos->setZ(97.0f);
        bd.get<ViewportComponent>()->setViewport(UI_VP);
        backdropId = bd.entity->id;

        ecsRef->attach<MouseLeftClickComponent>(bd.entity,
            makeCallable<PanelWasClickedEvent>(), MouseStateTrigger::OnPress);
    }

    float curY = py + PADDING;

    // --- Title "MISSIONS" ---
    {
        auto t = makeTTFText(ecsRef, px + PADDING, curY, 100.0f,
            FONT_BOLD, "MISSIONS", SCALE_TITLE, C::TEXT);
        t.get<ViewportComponent>()->setViewport(UI_VP);
        titleTextId = t.entity->id;
    }

    // --- Tab breadcrumbs (right-aligned) ---
    {
        static const char* TAB_LABELS[NUM_TABS] = {"main", "missions", "shop"};
        // Position tabs from right to left, before close button
        float tabStartX = px + PANEL_W - PADDING - CLOSE_SIZE - 8.0f;

        // First measure approximate widths (rough: 6px per char at this scale)
        float charW = 5.5f;
        float slashW = 12.0f;

        float totalTabW = 0;
        for (size_t t = 0; t < NUM_TABS; ++t)
            totalTabW += strlen(TAB_LABELS[t]) * charW;
        totalTabW += (NUM_TABS - 1) * slashW;

        float tx = tabStartX - totalTabW;

        for (size_t t = 0; t < NUM_TABS; ++t)
        {
            float labelW = strlen(TAB_LABELS[t]) * charW;
            tabButtons[t].x = tx;
            tabButtons[t].y = curY + 4.0f;
            tabButtons[t].w = labelW;

            auto txt = makeTTFText(ecsRef, tx, curY + 6.0f, 100.0f,
                FONT_MEDIUM, TAB_LABELS[t], SCALE_TAB,
                (t == currentTab) ? C::TEXT : C::TEXT_DIM);
            txt.get<ViewportComponent>()->setViewport(UI_VP);
            tabButtons[t].textId = txt.entity->id;

            tx += labelW;

            // Add slash separator between tabs
            if (t < NUM_TABS - 1)
            {
                auto slash = makeTTFText(ecsRef, tx + 2.0f, curY + 6.0f, 100.0f,
                    FONT_LIGHT, "/", SCALE_TAB, C::TEXT_DIM);
                slash.get<ViewportComponent>()->setViewport(UI_VP);
                if (t == 0) tabSlash1Id = slash.entity->id;
                else tabSlash2Id = slash.entity->id;
                tx += slashW;
            }
        }
    }

    // --- Close button ---
    {
        float cbx = px + PANEL_W - PADDING - CLOSE_SIZE;
        auto bg = makeRoundedRect2DShape(ecsRef, 4.0f, CLOSE_SIZE, CLOSE_SIZE, C::PANEL);
        auto pos = bg.get<PositionComponent>();
        pos->setX(cbx); pos->setY(curY); pos->setZ(99.0f);
        bg.get<ViewportComponent>()->setViewport(UI_VP);
        closeBtnBgId = bg.entity->id;

        auto txt = makeTTFText(ecsRef, cbx + 7.0f, curY + 4.0f, 100.0f,
            FONT_MEDIUM, "x", SCALE_TAB, C::TEXT_DIM);
        txt.get<ViewportComponent>()->setViewport(UI_VP);
        closeBtnTextId = txt.entity->id;
    }
    curY += TITLE_H;

    // --- Horizontal divider ---
    {
        auto div = makeSimple2DShape(ecsRef, Shape2D::Square, 0.0f, 0.0f, C::DIVIDER);
        auto pos = div.get<PositionComponent>();
        pos->setX(px + PADDING); pos->setY(curY); pos->setZ(98.5f);
        pos->setWidth(PANEL_W - 2 * PADDING); pos->setHeight(DIVIDER_H);
        div.get<ViewportComponent>()->setViewport(UI_VP);
        topDividerId = div.entity->id;
    }
    curY += DIVIDER_H;

    float contentY = curY;

    // --- Vertical column divider ---
    {
        float divX = px + LEFT_W;
        float divH = panelH - (contentY - py) - PADDING;
        auto div = makeSimple2DShape(ecsRef, Shape2D::Square, 0.0f, 0.0f, C::DIVIDER);
        auto pos = div.get<PositionComponent>();
        pos->setX(divX); pos->setY(contentY); pos->setZ(98.5f);
        pos->setWidth(1.0f); pos->setHeight(divH);
        div.get<ViewportComponent>()->setViewport(UI_VP);
        columnDividerId = div.entity->id;
    }

    createLeftColumn(px, contentY);
    createRightColumn(px, contentY);
}

void MissionUISystem::createLeftColumn(float px, float contentY)
{
    float colX = px;
    float rowW = LEFT_W;

    for (size_t i = 0; i < MAX_LIST_ROWS; ++i)
    {
        float rowY = contentY + i * (LIST_ROW_H + SEPARATOR_H);
        auto& row = listRows[i];
        row.rowX = colX;
        row.rowY = rowY;
        row.rowW = rowW;

        // Row background
        {
            auto bg = makeSimple2DShape(ecsRef, Shape2D::Square, 0.0f, 0.0f, C::TRANSPARENT);
            auto pos = bg.get<PositionComponent>();
            pos->setX(colX); pos->setY(rowY); pos->setZ(98.0f);
            pos->setWidth(rowW); pos->setHeight(LIST_ROW_H);
            bg.get<ViewportComponent>()->setViewport(UI_VP);
            row.bgId = bg.entity->id;
        }

        // Status square (border = outer rect)
        float sqX = colX + PADDING;
        float sqY = rowY + (LIST_ROW_H - STATUS_SQ_SIZE) * 0.5f;
        {
            auto sq = makeRoundedRect2DShape(ecsRef, STATUS_SQ_RAD, STATUS_SQ_SIZE, STATUS_SQ_SIZE, C::TEXT_DIM);
            auto pos = sq.get<PositionComponent>();
            pos->setX(sqX); pos->setY(sqY); pos->setZ(98.5f);
            sq.get<ViewportComponent>()->setViewport(UI_VP);
            row.statusBorderId = sq.entity->id;
        }

        // Status square (fill = inner rect)
        {
            float inset = 2.0f;
            auto sq = makeRoundedRect2DShape(ecsRef, STATUS_SQ_RAD,
                STATUS_SQ_SIZE - 2 * inset, STATUS_SQ_SIZE - 2 * inset, C::BG);
            auto pos = sq.get<PositionComponent>();
            pos->setX(sqX + inset); pos->setY(sqY + inset); pos->setZ(98.6f);
            sq.get<ViewportComponent>()->setViewport(UI_VP);
            row.statusFillId = sq.entity->id;
        }

        // Mission name
        {
            float nameX = colX + PADDING + STATUS_SQ_SIZE + 8.0f;
            auto name = makeTTFText(ecsRef, nameX, rowY + 8.0f, 100.0f,
                FONT_LIGHT, "", SCALE_LIST, C::TEXT);
            name.get<ViewportComponent>()->setViewport(UI_VP);
            row.nameId = name.entity->id;
        }

        // GO pill
        {
            float pillX = colX + rowW - PADDING - GO_PILL_W;
            float pillY = rowY + (LIST_ROW_H - GO_PILL_H) * 0.5f;
            auto pill = makeRoundedRect2DShape(ecsRef, GO_PILL_RAD, GO_PILL_W, GO_PILL_H, C::ACCENT);
            auto pos = pill.get<PositionComponent>();
            pos->setX(pillX); pos->setY(pillY); pos->setZ(99.0f);
            pos->setVisibility(false);
            pill.get<ViewportComponent>()->setViewport(UI_VP);
            row.goPillBgId = pill.entity->id;

            auto txt = makeTTFText(ecsRef, pillX + 8.0f, pillY + 2.0f, 100.0f,
                FONT_BOLD, "GO", SCALE_PILL, C::WHITE);
            txt.get<PositionComponent>()->setVisibility(false);
            txt.get<ViewportComponent>()->setViewport(UI_VP);
            row.goPillTextId = txt.entity->id;
        }

        // Separator line
        {
            float sepY = rowY + LIST_ROW_H;
            auto sep = makeSimple2DShape(ecsRef, Shape2D::Square, 0.0f, 0.0f, C::DIVIDER);
            auto pos = sep.get<PositionComponent>();
            pos->setX(colX + PADDING); pos->setY(sepY); pos->setZ(98.3f);
            pos->setWidth(rowW - PADDING); pos->setHeight(SEPARATOR_H);
            sep.get<ViewportComponent>()->setViewport(UI_VP);
            row.separatorId = sep.entity->id;
        }
    }
}

void MissionUISystem::createRightColumn(float px, float contentY)
{
    float colX = px + LEFT_W + DETAIL_PAD;
    float colW = RIGHT_W - 2 * DETAIL_PAD;
    float curY = contentY + DETAIL_PAD;

    // "MISSION" label
    {
        auto lbl = makeTTFText(ecsRef, colX, curY, 100.0f,
            FONT_MEDIUM, "MISSION", SCALE_LABEL, C::TEXT_DIM);
        lbl.get<ViewportComponent>()->setViewport(UI_VP);
        detailMissionLabelId = lbl.entity->id;
    }
    curY += 18.0f;

    // Mission name (large display)
    {
        auto name = makeTTFText(ecsRef, colX, curY, 100.0f,
            FONT_BOLD, "", SCALE_DISPLAY, C::TEXT);
        name.get<ViewportComponent>()->setViewport(UI_VP);
        detailNameId = name.entity->id;
    }
    curY += 28.0f;

    // Description — anchored left+right to backdrop so width follows column
    {
        auto desc = makeTTFText(ecsRef, 0.0f, curY, 100.0f,
            FONT_LIGHT, "", SCALE_BODY, C::TEXT_DIM);
        desc.get<ViewportComponent>()->setViewport(UI_VP);
        desc.entity->get<TTFText>()->setWrap(true);
        detailDescId = desc.entity->id;

        auto anchor = ecsRef->attach<UiAnchor>(desc.entity);
        anchor->setLeftAnchor(PosAnchor{backdropId, AnchorType::Left});
        anchor->setLeftMargin(LEFT_W + DETAIL_PAD);
        anchor->setRightAnchor(PosAnchor{backdropId, AnchorType::Right});
        anchor->setRightMargin(DETAIL_PAD);
    }
    curY += 44.0f;

    // --- Cost block ---
    {
        // "COST" label
        auto lbl = makeTTFText(ecsRef, colX, curY, 100.0f,
            FONT_MEDIUM, "COST", SCALE_LABEL, C::TEXT_DIM);
        lbl.get<ViewportComponent>()->setViewport(UI_VP);
        costLabelId = lbl.entity->id;
        curY += 16.0f;

        // Outer border rect (1px, dim)
        auto outer = makeRoundedRect2DShape(ecsRef, BLOCK_RADIUS, colW, BLOCK_H, C::COST_BRD);
        auto opos = outer.get<PositionComponent>();
        opos->setX(colX); opos->setY(curY); opos->setZ(98.2f);
        outer.get<ViewportComponent>()->setViewport(UI_VP);
        costBlockBorderId = outer.entity->id;

        // Inner fill rect
        auto inner = makeRoundedRect2DShape(ecsRef, BLOCK_RADIUS - 1.0f,
            colW - 2 * COST_BORDER, BLOCK_H - 2 * COST_BORDER, C::BLOCK_FILL);
        auto ipos = inner.get<PositionComponent>();
        ipos->setX(colX + COST_BORDER); ipos->setY(curY + COST_BORDER); ipos->setZ(98.3f);
        inner.get<ViewportComponent>()->setViewport(UI_VP);
        costBlockFillId = inner.entity->id;

        // Cost items (icon + count)
        float itemX = colX + 12.0f;
        float itemY = curY + (BLOCK_H - ICON_SIZE) * 0.5f;
        for (size_t r = 0; r < MAX_COST_ITEMS; ++r)
        {
            float rx = itemX + r * 70.0f;

            auto icon = make2DTexture(ecsRef, ICON_SIZE, ICON_SIZE, "Items.0");
            auto iconPos = icon.get<PositionComponent>();
            iconPos->setX(rx); iconPos->setY(itemY); iconPos->setZ(99.0f);
            iconPos->setVisibility(false);
            icon.get<ViewportComponent>()->setViewport(UI_VP);
            costItems[r].iconId = icon.entity->id;

            auto cnt = makeTTFText(ecsRef, rx + ICON_SIZE + 4.0f, curY + 14.0f, 100.0f,
                FONT_MEDIUM, "", SCALE_NUM, C::TEXT);
            cnt.get<PositionComponent>()->setVisibility(false);
            cnt.get<ViewportComponent>()->setViewport(UI_VP);
            costItems[r].countTextId = cnt.entity->id;
        }
    }
    curY += BLOCK_H + 12.0f;

    // --- Reward block ---
    {
        // "REWARD" label
        auto lbl = makeTTFText(ecsRef, colX, curY, 100.0f,
            FONT_MEDIUM, "REWARD", SCALE_LABEL, C::TEXT_DIM);
        lbl.get<ViewportComponent>()->setViewport(UI_VP);
        rewardLabelId = lbl.entity->id;
        curY += 16.0f;

        // Outer border rect (2px, brighter)
        auto outer = makeRoundedRect2DShape(ecsRef, BLOCK_RADIUS, colW, BLOCK_H, C::REWARD_BRD);
        auto opos = outer.get<PositionComponent>();
        opos->setX(colX); opos->setY(curY); opos->setZ(98.2f);
        outer.get<ViewportComponent>()->setViewport(UI_VP);
        rewardBlockBorderId = outer.entity->id;

        // Inner fill rect (slightly brighter to pop)
        constant::Vector4D rewardFill = {48.0f, 52.0f, 60.0f, 255.0f};
        auto inner = makeRoundedRect2DShape(ecsRef, BLOCK_RADIUS - 1.0f,
            colW - 2 * REWARD_BORDER, BLOCK_H - 2 * REWARD_BORDER, rewardFill);
        auto ipos = inner.get<PositionComponent>();
        ipos->setX(colX + REWARD_BORDER); ipos->setY(curY + REWARD_BORDER); ipos->setZ(98.3f);
        inner.get<ViewportComponent>()->setViewport(UI_VP);
        rewardBlockFillId = inner.entity->id;

        // Reward items (icon + count)
        float itemX = colX + 12.0f;
        float itemY = curY + (BLOCK_H - ICON_SIZE) * 0.5f;
        for (size_t r = 0; r < MAX_REWARD_ITEMS; ++r)
        {
            float rx = itemX + r * 70.0f;

            auto icon = make2DTexture(ecsRef, ICON_SIZE, ICON_SIZE, "Items.0");
            auto iconPos = icon.get<PositionComponent>();
            iconPos->setX(rx); iconPos->setY(itemY); iconPos->setZ(99.0f);
            iconPos->setVisibility(false);
            icon.get<ViewportComponent>()->setViewport(UI_VP);
            rewardItems[r].iconId = icon.entity->id;

            auto cnt = makeTTFText(ecsRef, rx + ICON_SIZE + 4.0f, curY + 14.0f, 100.0f,
                FONT_MEDIUM, "", SCALE_NUM, C::TEXT);
            cnt.get<PositionComponent>()->setVisibility(false);
            cnt.get<ViewportComponent>()->setViewport(UI_VP);
            rewardItems[r].countTextId = cnt.entity->id;
        }
    }
    curY += BLOCK_H + 8.0f;

    // --- Unlock label ---
    {
        auto lbl = makeTTFText(ecsRef, colX, curY, 100.0f,
            FONT_MEDIUM, "", SCALE_BODY, C::ACCENT);
        lbl.get<PositionComponent>()->setVisibility(false);
        lbl.get<ViewportComponent>()->setViewport(UI_VP);
        unlockLabelId = lbl.entity->id;
    }
    curY += 22.0f;

    // --- Progress bar (for active endgame missions) ---
    {
        auto pbBg = makeSimple2DShape(ecsRef, Shape2D::Square, 0.0f, 0.0f,
            constant::Vector4D{50.0f, 50.0f, 60.0f, 200.0f});
        auto pbPos = pbBg.get<PositionComponent>();
        pbPos->setX(colX); pbPos->setY(curY); pbPos->setZ(98.5f);
        pbPos->setWidth(colW); pbPos->setHeight(PROGRESS_H);
        pbPos->setVisibility(false);
        pbBg.get<ViewportComponent>()->setViewport(UI_VP);
        detailProgressBgId = pbBg.entity->id;

        auto pbFill = makeSimple2DShape(ecsRef, Shape2D::Square, 0.0f, 0.0f,
            constant::Vector4D{80.0f, 160.0f, 80.0f, 220.0f});
        auto pfPos = pbFill.get<PositionComponent>();
        pfPos->setX(colX); pfPos->setY(curY); pfPos->setZ(98.6f);
        pfPos->setWidth(0.0f); pfPos->setHeight(PROGRESS_H);
        pfPos->setVisibility(false);
        pbFill.get<ViewportComponent>()->setViewport(UI_VP);
        detailProgressFillId = pbFill.entity->id;

        auto pTxt = makeTTFText(ecsRef, colX + colW + 6.0f, curY - 2.0f, 100.0f,
            FONT_MEDIUM, "", SCALE_NUM, C::TEXT_DIM);
        pTxt.get<PositionComponent>()->setVisibility(false);
        pTxt.get<ViewportComponent>()->setViewport(UI_VP);
        detailProgressTextId = pTxt.entity->id;
    }
    curY += PROGRESS_H + 12.0f;

    // --- Shop section labels (tab 2 only, positioned in detail area) ---
    {
        auto lbl = makeTTFText(ecsRef, colX, contentY + DETAIL_PAD + 46.0f, 100.0f,
            FONT_LIGHT, "", SCALE_BODY, C::TEXT_DIM);
        lbl.get<PositionComponent>()->setVisibility(false);
        lbl.get<ViewportComponent>()->setViewport(UI_VP);
        shopCostTextId = lbl.entity->id;

        auto shopName = makeTTFText(ecsRef, colX, contentY + DETAIL_PAD + 18.0f, 100.0f,
            FONT_BOLD, "Extra Mission Slot", SCALE_DISPLAY, C::TEXT);
        shopName.get<PositionComponent>()->setVisibility(false);
        shopName.get<ViewportComponent>()->setViewport(UI_VP);
        shopLabelId = shopName.entity->id;
    }

    // --- Action button (full-width at bottom of detail pane) ---
    {
        float py2 = getPanelY();
        float btnY = py2 + panelH - PADDING - ACTION_BTN_H;
        actionBtnX = colX;
        actionBtnY = btnY;
        actionBtnW = colW;

        auto bg = makeRoundedRect2DShape(ecsRef, ACTION_BTN_RAD, colW, ACTION_BTN_H, C::ACCENT);
        auto pos = bg.get<PositionComponent>();
        pos->setX(colX); pos->setY(btnY); pos->setZ(99.0f);
        bg.get<ViewportComponent>()->setViewport(UI_VP);
        actionBtnBgId = bg.entity->id;

        auto txt = makeTTFText(ecsRef, 0.0f, btnY + 8.0f, 100.0f,
            FONT_BOLD, "Start Mission", SCALE_BTN, C::WHITE);
        txt.get<ViewportComponent>()->setViewport(UI_VP);
        actionBtnTextId = txt.entity->id;

        auto txtAnchor = ecsRef->attach<UiAnchor>(txt.entity);
        txtAnchor->setHorizontalCenter(PosAnchor{actionBtnBgId, AnchorType::HorizontalCenter});
    }
}

// ---------------------------------------------------------------------------
// Visibility
// ---------------------------------------------------------------------------

void MissionUISystem::setPanelVisibility(bool vis)
{
    // Chrome
    setEntityVisibility(backdropId, vis);
    setEntityVisibility(titleTextId, vis);
    setEntityVisibility(closeBtnBgId, vis);
    setEntityVisibility(closeBtnTextId, vis);
    setEntityVisibility(topDividerId, vis);
    setEntityVisibility(columnDividerId, vis);
    setEntityVisibility(tabSlash1Id, vis);
    setEntityVisibility(tabSlash2Id, vis);

    for (size_t t = 0; t < NUM_TABS; ++t)
        setEntityVisibility(tabButtons[t].textId, vis);

    if (not vis)
    {
        // Hide left column
        for (size_t i = 0; i < MAX_LIST_ROWS; ++i)
        {
            auto& r = listRows[i];
            setEntityVisibility(r.bgId, false);
            setEntityVisibility(r.statusBorderId, false);
            setEntityVisibility(r.statusFillId, false);
            setEntityVisibility(r.nameId, false);
            setEntityVisibility(r.goPillBgId, false);
            setEntityVisibility(r.goPillTextId, false);
            setEntityVisibility(r.separatorId, false);
        }

        // Hide right column
        setEntityVisibility(detailMissionLabelId, false);
        setEntityVisibility(detailNameId, false);
        setEntityVisibility(detailDescId, false);
        setEntityVisibility(costLabelId, false);
        setEntityVisibility(costBlockBorderId, false);
        setEntityVisibility(costBlockFillId, false);
        for (auto& c : costItems)
        {
            setEntityVisibility(c.iconId, false);
            setEntityVisibility(c.countTextId, false);
        }
        setEntityVisibility(rewardLabelId, false);
        setEntityVisibility(rewardBlockBorderId, false);
        setEntityVisibility(rewardBlockFillId, false);
        for (auto& r : rewardItems)
        {
            setEntityVisibility(r.iconId, false);
            setEntityVisibility(r.countTextId, false);
        }
        setEntityVisibility(unlockLabelId, false);
        setEntityVisibility(detailProgressBgId, false);
        setEntityVisibility(detailProgressFillId, false);
        setEntityVisibility(detailProgressTextId, false);
        setEntityVisibility(actionBtnBgId, false);
        setEntityVisibility(actionBtnTextId, false);
        setEntityVisibility(shopLabelId, false);
        setEntityVisibility(shopCostTextId, false);
    }
}

// ---------------------------------------------------------------------------
// Refresh
// ---------------------------------------------------------------------------

void MissionUISystem::refresh()
{
    if (not visible)
        return;

    // Update tab text styling
    for (size_t t = 0; t < NUM_TABS; ++t)
        setEntityTextColor(tabButtons[t].textId, (t == currentTab) ? C::TEXT : C::TEXT_DIM);

    // Rebuild filtered defs if needed
    if (currentTab < 2)
    {
        MissionCategory cat = (currentTab == 0) ? MissionCategory::Main : MissionCategory::Endgame;
        filteredDefs = getFilteredDefs(cat);
    }

    refreshLeftColumn();
    refreshRightColumn();
}

void MissionUISystem::refreshLeftColumn()
{
    const auto& defs = missionSystem->getDefs();
    bool isShopTab = (currentTab == 2);

    for (size_t i = 0; i < MAX_LIST_ROWS; ++i)
    {
        auto& row = listRows[i];

        if (isShopTab)
        {
            // Shop tab: show single "Extra Mission Slot" entry
            bool show = (i == 0);
            setEntityVisibility(row.bgId, show);
            setEntityVisibility(row.nameId, show);
            setEntityVisibility(row.statusBorderId, show);
            setEntityVisibility(row.statusFillId, show);
            setEntityVisibility(row.separatorId, show);
            setEntityVisibility(row.goPillBgId, false);
            setEntityVisibility(row.goPillTextId, false);

            if (show)
            {
                setEntityText(row.nameId, "Extra Slot");
                bool isSelected = (selectedDefIndex == SIZE_MAX);

                auto bgEnt = ecsRef->getEntity(row.bgId);
                if (bgEnt)
                    bgEnt->get<Simple2DObject>()->setColors(isSelected ? C::SELECTED : C::TRANSPARENT);
                setEntityTextColor(row.nameId, isSelected ? C::WHITE : C::TEXT);

                // Status square: filled accent
                setEntityRoundedRectColor(row.statusBorderId, C::ACCENT);
                setEntityRoundedRectColor(row.statusFillId, C::ACCENT);
            }
            continue;
        }

        bool show = (i < filteredDefs.size());
        setEntityVisibility(row.bgId, show);
        setEntityVisibility(row.nameId, show);
        setEntityVisibility(row.statusBorderId, show);
        setEntityVisibility(row.statusFillId, show);
        setEntityVisibility(row.separatorId, show and (i + 1 < filteredDefs.size()));

        if (not show)
        {
            setEntityVisibility(row.goPillBgId, false);
            setEntityVisibility(row.goPillTextId, false);
            continue;
        }

        size_t defIndex = filteredDefs[i];
        const auto& def = defs[defIndex];
        bool unlocked = missionSystem->isMissionUnlocked(defIndex);
        bool completed = missionSystem->isMissionCompleted(defIndex);
        bool isSelected = (defIndex == selectedDefIndex);

        // Name
        setEntityText(row.nameId, def.name);

        // Row background
        auto bgEnt = ecsRef->getEntity(row.bgId);
        if (bgEnt)
            bgEnt->get<Simple2DObject>()->setColors(isSelected ? C::SELECTED : C::TRANSPARENT);

        // Text color and opacity
        if (not unlocked)
        {
            // Locked: dimmed text with lower opacity
            constant::Vector4D lockedText = {C::TEXT_DIM.x, C::TEXT_DIM.y, C::TEXT_DIM.z, 127.0f};
            setEntityTextColor(row.nameId, lockedText);
        }
        else
        {
            setEntityTextColor(row.nameId, isSelected ? C::WHITE : C::TEXT);
        }

        // Status square
        if (completed)
        {
            // Done: filled with accent
            setEntityRoundedRectColor(row.statusBorderId, C::ACCENT);
            setEntityRoundedRectColor(row.statusFillId, C::ACCENT);
        }
        else if (unlocked)
        {
            // Available: border only
            setEntityRoundedRectColor(row.statusBorderId, isSelected ? C::TEXT : C::TEXT_DIM);
            setEntityRoundedRectColor(row.statusFillId, isSelected ? C::SELECTED : C::BG);
        }
        else
        {
            // Locked: dim border, dim fill
            constant::Vector4D dimBorder = {C::TEXT_DIM.x, C::TEXT_DIM.y, C::TEXT_DIM.z, 80.0f};
            setEntityRoundedRectColor(row.statusBorderId, dimBorder);
            setEntityRoundedRectColor(row.statusFillId, C::BG);
        }

        // Locked row opacity for status squares
        if (not unlocked)
        {
            auto borderEnt = ecsRef->getEntity(row.statusBorderId);
            if (borderEnt and borderEnt->has<RoundedRect2DObject>())
                borderEnt->get<RoundedRect2DObject>()->setOpacity(80.0f);
            auto fillEnt = ecsRef->getEntity(row.statusFillId);
            if (fillEnt and fillEnt->has<RoundedRect2DObject>())
                fillEnt->get<RoundedRect2DObject>()->setOpacity(80.0f);
        }

        // GO pill: show on first actionable mission
        bool showGo = false;
        if (unlocked and not completed)
        {
            if (currentTab == 0 and missionSystem->canValidateMainMission(defIndex))
                showGo = true;
            else if (currentTab == 1 and missionSystem->canStartMission(defIndex))
                showGo = true;
        }
        setEntityVisibility(row.goPillBgId, showGo);
        setEntityVisibility(row.goPillTextId, showGo);
    }
}

void MissionUISystem::refreshRightColumn()
{
    const auto& defs = missionSystem->getDefs();
    const auto& active = missionSystem->getActive();
    bool isShopTab = (currentTab == 2);

    // Hide mission detail entities for shop tab
    bool showMissionDetail = (not isShopTab and selectedDefIndex != SIZE_MAX and selectedDefIndex < defs.size());

    setEntityVisibility(detailMissionLabelId, showMissionDetail);
    setEntityVisibility(detailNameId, showMissionDetail);
    setEntityVisibility(detailDescId, showMissionDetail);

    // Shop section
    setEntityVisibility(shopLabelId, isShopTab);
    setEntityVisibility(shopCostTextId, isShopTab);

    if (isShopTab)
    {
        size_t cost = missionSystem->getExtraSlotCost();
        size_t maxSlots = missionSystem->getMaxActive();
        setEntityText(shopCostTextId, "Cost: " + std::to_string(cost) + " Tickets. Current slots: " + std::to_string(maxSlots));

        // Action button for shop
        setEntityVisibility(actionBtnBgId, true);
        setEntityVisibility(actionBtnTextId, true);
        setEntityRoundedRectColor(actionBtnBgId, C::ACCENT);
        setEntityText(actionBtnTextId, "Buy Slot");

        // Hide mission-specific blocks
        setEntityVisibility(costLabelId, false);
        setEntityVisibility(costBlockBorderId, false);
        setEntityVisibility(costBlockFillId, false);
        for (auto& c : costItems)
        {
            setEntityVisibility(c.iconId, false);
            setEntityVisibility(c.countTextId, false);
        }
        setEntityVisibility(rewardLabelId, false);
        setEntityVisibility(rewardBlockBorderId, false);
        setEntityVisibility(rewardBlockFillId, false);
        for (auto& r : rewardItems)
        {
            setEntityVisibility(r.iconId, false);
            setEntityVisibility(r.countTextId, false);
        }
        setEntityVisibility(unlockLabelId, false);
        setEntityVisibility(detailProgressBgId, false);
        setEntityVisibility(detailProgressFillId, false);
        setEntityVisibility(detailProgressTextId, false);
        return;
    }

    if (not showMissionDetail)
    {
        // Hide everything when no mission selected
        setEntityVisibility(costLabelId, false);
        setEntityVisibility(costBlockBorderId, false);
        setEntityVisibility(costBlockFillId, false);
        for (auto& c : costItems)
        {
            setEntityVisibility(c.iconId, false);
            setEntityVisibility(c.countTextId, false);
        }
        setEntityVisibility(rewardLabelId, false);
        setEntityVisibility(rewardBlockBorderId, false);
        setEntityVisibility(rewardBlockFillId, false);
        for (auto& r : rewardItems)
        {
            setEntityVisibility(r.iconId, false);
            setEntityVisibility(r.countTextId, false);
        }
        setEntityVisibility(unlockLabelId, false);
        setEntityVisibility(actionBtnBgId, false);
        setEntityVisibility(actionBtnTextId, false);
        setEntityVisibility(detailProgressBgId, false);
        setEntityVisibility(detailProgressFillId, false);
        setEntityVisibility(detailProgressTextId, false);
        return;
    }

    const auto& def = defs[selectedDefIndex];
    bool unlocked = missionSystem->isMissionUnlocked(selectedDefIndex);
    bool completed = missionSystem->isMissionCompleted(selectedDefIndex);

    // Mission name and description
    setEntityText(detailNameId, def.name);
    setEntityText(detailDescId, def.description);

    // --- Cost / Obtain block ---
    bool hasCost = not def.deliveryRequirements.empty() or def.robotCoreCost > 0;
    setEntityVisibility(costLabelId, hasCost);
    setEntityVisibility(costBlockBorderId, hasCost);
    setEntityVisibility(costBlockFillId, hasCost);
    if (hasCost)
        setEntityText(costLabelId, def.consumeItems ? "COST" : "OBTAIN");

    if (hasCost and itemRegistry)
    {
        if (def.isDeliveryMission())
        {
            for (size_t r = 0; r < MAX_COST_ITEMS; ++r)
            {
                bool hasReq = r < def.deliveryRequirements.size();
                setEntityVisibility(costItems[r].iconId, hasReq);
                setEntityVisibility(costItems[r].countTextId, hasReq);

                if (hasReq)
                {
                    const auto& req = def.deliveryRequirements[r];
                    setEntityTexture(costItems[r].iconId, itemRegistry->get(req.itemId).textureName);
                    std::string prefix = def.consumeItems ? "-" : "";
                    setEntityText(costItems[r].countTextId, prefix + std::to_string(req.count));
                }
            }
        }
        else
        {
            // Timed mission: show core cost + duration
            bool hasCores = def.robotCoreCost > 0;
            setEntityVisibility(costItems[0].iconId, false);
            setEntityVisibility(costItems[0].countTextId, hasCores);
            if (hasCores)
            {
                std::string costStr = std::to_string(def.robotCoreCost) + " Core";
                if (def.durationMs > 0)
                    costStr += "  " + std::to_string(def.durationMs / 1000) + "s";
                setEntityText(costItems[0].countTextId, costStr);
            }
            for (size_t r = 1; r < MAX_COST_ITEMS; ++r)
            {
                setEntityVisibility(costItems[r].iconId, false);
                setEntityVisibility(costItems[r].countTextId, false);
            }
        }
    }
    else
    {
        for (size_t r = 0; r < MAX_COST_ITEMS; ++r)
        {
            setEntityVisibility(costItems[r].iconId, false);
            setEntityVisibility(costItems[r].countTextId, false);
        }
    }

    // --- Reward block ---
    bool hasReward = not def.rewards.empty();
    setEntityVisibility(rewardLabelId, hasReward);
    setEntityVisibility(rewardBlockBorderId, hasReward);
    setEntityVisibility(rewardBlockFillId, hasReward);

    if (hasReward and itemRegistry)
    {
        for (size_t r = 0; r < MAX_REWARD_ITEMS; ++r)
        {
            bool has = r < def.rewards.size();
            setEntityVisibility(rewardItems[r].iconId, has);
            setEntityVisibility(rewardItems[r].countTextId, has);

            if (has)
            {
                const auto& reward = def.rewards[r];
                setEntityTexture(rewardItems[r].iconId, itemRegistry->get(reward.itemId).textureName);
                setEntityText(rewardItems[r].countTextId, "+" + std::to_string(reward.count));
            }
        }
    }
    else
    {
        for (size_t r = 0; r < MAX_REWARD_ITEMS; ++r)
        {
            setEntityVisibility(rewardItems[r].iconId, false);
            setEntityVisibility(rewardItems[r].countTextId, false);
        }
    }

    // --- Unlock label ---
    bool hasUnlock = not def.unlockLabel.empty();
    setEntityVisibility(unlockLabelId, hasUnlock);
    if (hasUnlock)
        setEntityText(unlockLabelId, "Unlocks: " + def.unlockLabel);

    // --- Progress bar (for active missions) ---
    bool isActive = false;
    size_t activeIndex = 0;
    for (size_t ai = 0; ai < active.size(); ++ai)
    {
        if (active[ai].defIndex == selectedDefIndex)
        {
            isActive = true;
            activeIndex = ai;
            break;
        }
    }

    setEntityVisibility(detailProgressBgId, isActive);
    setEntityVisibility(detailProgressFillId, isActive);
    setEntityVisibility(detailProgressTextId, isActive);

    if (isActive)
    {
        const auto& m = active[activeIndex];
        float progress;
        if (def.isDeliveryMission())
            progress = missionSystem->getDeliveryProgress(m);
        else
            progress = def.durationMs > 0
                ? static_cast<float>(m.elapsedMs) / static_cast<float>(def.durationMs)
                : 1.0f;
        if (progress > 1.0f) progress = 1.0f;

        float colW = RIGHT_W - 2 * DETAIL_PAD;
        auto fillEnt = ecsRef->getEntity(detailProgressFillId);
        if (fillEnt)
            fillEnt->get<PositionComponent>()->setWidth(colW * progress);

        if (m.completed)
            setEntityText(detailProgressTextId, "DONE!");
        else if (def.isDeliveryMission())
        {
            const auto& req = def.deliveryRequirements[0];
            uint16_t have = missionSystem->getDeliveryCount(m, req);
            setEntityText(detailProgressTextId, std::to_string(have) + "/" + std::to_string(req.count));
        }
        else
        {
            int pct = static_cast<int>(progress * 100.0f);
            setEntityText(detailProgressTextId, std::to_string(pct) + "%");
        }
    }

    // --- Action button ---
    setEntityVisibility(actionBtnBgId, true);
    setEntityVisibility(actionBtnTextId, true);

    if (not unlocked)
    {
        setEntityRoundedRectColor(actionBtnBgId, C::LOCKED_BTN);
        setEntityText(actionBtnTextId, "Locked");
    }
    else if (isActive and active[activeIndex].completed)
    {
        setEntityRoundedRectColor(actionBtnBgId, C::ACCENT);
        setEntityText(actionBtnTextId, "Claim Rewards");
    }
    else if (isActive)
    {
        setEntityRoundedRectColor(actionBtnBgId, C::LOCKED_BTN);
        setEntityText(actionBtnTextId, "In Progress...");
    }
    else if (completed and not def.repeatable)
    {
        setEntityRoundedRectColor(actionBtnBgId, C::DONE_BTN);
        setEntityText(actionBtnTextId, "Completed");
    }
    else if (currentTab == 0)
    {
        if (missionSystem->canValidateMainMission(selectedDefIndex))
        {
            setEntityRoundedRectColor(actionBtnBgId, C::ACCENT);
            setEntityText(actionBtnTextId, "Complete Mission");
        }
        else
        {
            setEntityRoundedRectColor(actionBtnBgId, C::LOCKED_BTN);
            setEntityText(actionBtnTextId, "Gather Items");
        }
    }
    else if (currentTab == 1)
    {
        if (missionSystem->canStartMission(selectedDefIndex))
        {
            setEntityRoundedRectColor(actionBtnBgId, C::ACCENT);
            setEntityText(actionBtnTextId, "Start Mission");
        }
        else
        {
            setEntityRoundedRectColor(actionBtnBgId, C::LOCKED_BTN);
            setEntityText(actionBtnTextId, "Slots Full");
        }
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

        auto bg = makeRoundedRect2DShape(ecsRef, 6.0f, bannerW, bannerH, C::BG);
        auto pos = bg.get<PositionComponent>();
        pos->setX(bx); pos->setY(by); pos->setZ(101.0f);
        bg.get<ViewportComponent>()->setViewport(UI_VP);
        promptBgId = bg.entity->id;

        auto txt = makeTTFText(ecsRef, bx + 16.0f, by + 6.0f, 102.0f,
            FONT_MEDIUM, "Click a depot to start mission", SCALE_BODY,
            C::ACCENT);
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

void MissionUISystem::setEntityRoundedRectColor(uint64_t id, const constant::Vector4D& color)
{
    if (id == 0) return;
    auto ent = ecsRef->getEntity(id);
    if (ent and ent->has<RoundedRect2DObject>())
        ent->get<RoundedRect2DObject>()->setColors(color);
}

void MissionUISystem::setEntityTextColor(uint64_t id, const constant::Vector4D& color)
{
    if (id == 0) return;
    auto ent = ecsRef->getEntity(id);
    if (ent and ent->has<TTFText>())
        ent->get<TTFText>()->setColors(color);
}

bool MissionUISystem::isClickInRect(float cx, float cy, float rx, float ry, float rw, float rh) const
{
    return cx >= rx and cx <= rx + rw and cy >= ry and cy <= ry + rh;
}
