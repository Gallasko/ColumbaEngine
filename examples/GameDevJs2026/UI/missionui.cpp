#include "missionui.h"

#include "2D/simple2dobject.h"
#include "2D/position.h"
#include "2D/texture.h"
#include "UI/ttftext.h"

#include "tooltipsystem.h"

#include <SDL2/SDL.h>
#include <algorithm>

namespace
{
    // Hit-test against an entity's resolved position+size.
    bool hitEntity(pg::EntitySystem* ecs, uint64_t id, float mx, float my)
    {
        if (id == 0) return false;
        auto ent = ecs->getEntity(id);
        if (not ent) return false;
        auto pos = ent->get<pg::PositionComponent>();
        return mx >= pos->getX() and mx <= pos->getX() + pos->getWidth()
           and my >= pos->getY() and my <= pos->getY() + pos->getHeight();
    }
}

// ---------------------------------------------------------------------------
// Toggle / Open / Close
// ---------------------------------------------------------------------------

void MissionUISystem::toggle()
{
    bool wasVisible = visible.load(std::memory_order_acquire);
    LOG_INFO("MissionUI", "toggle() — currently visible=" << static_cast<int>(wasVisible));
    if (wasVisible)
        close();
    else
        open();
}

void MissionUISystem::open()
{
    bool wasVisible = visible.load(std::memory_order_acquire);
    LOG_INFO("MissionUI", "open() entry — visible=" << static_cast<int>(wasVisible)
            << ", panelCreated=" << static_cast<int>(panelCreated));
    if (wasVisible)
    {
        LOG_INFO("MissionUI", "open() early-return (already visible)");
        return;
    }
    visible.store(true, std::memory_order_release);
    ecsRef->sendEvent(MissionUIOpenedEvent{});
    ensurePanelCreated();

    // Rebuild filtered defs and auto-select first actionable mission
    MissionCategory cat = (currentTab == 0) ? MissionCategory::Main : MissionCategory::Endgame;
    if (currentTab < 2)
        filteredDefs = getFilteredDefs(cat);
    else
        filteredDefs.clear();

    selectedDefIndex = SIZE_MAX;
    auto* missionSystem = ecsRef->getSystem<MissionSystem>();
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

    LOG_INFO("MissionUI", "open() — tab=" << currentTab << ", filteredDefs=" << filteredDefs.size()
            << ", selectedDefIndex=" << selectedDefIndex);

    refresh();
    setPanelVisibility(true);
    LOG_INFO("MissionUI", "open() done");
}

void MissionUISystem::close()
{
    bool wasVisible = visible.load(std::memory_order_acquire);
    LOG_INFO("MissionUI", "close() entry — visible=" << static_cast<int>(wasVisible)
            << ", panelCreated=" << static_cast<int>(panelCreated));
    if (not wasVisible)
    {
        LOG_INFO("MissionUI", "close() early-return (not visible)");
        return;
    }
    visible.store(false, std::memory_order_release);
    setPanelVisibility(false);
    // Clear tooltip via event so the mutation runs on TooltipSystem's task,
    // not ours. Direct setHoveredItem() would race with TooltipSystem::tick.
    LOG_INFO("MissionUI", "close() — sending tooltip clear event");
    ecsRef->sendEvent(SetTooltipHoveredItemEvent{ITEM_NONE});
    ecsRef->sendEvent(MissionUIClosedEvent{});
    LOG_INFO("MissionUI", "close() done");
}

void MissionUISystem::selectDepot(int depotX, int depotY)
{
    if (not pendingStart.active.load(std::memory_order_acquire))
        return;

    size_t defIndex = pendingStart.defIndex;
    pendingStart.active.store(false, std::memory_order_release);
    hideDepotSelectionPrompt();

    sendEvent(UICommandEvent{"mission.start", ElementMap{
        {"defIndex", defIndex},
        {"depotX",   depotX},
        {"depotY",   depotY},
    }});
    LOG_INFO("MissionUI", "emitted mission.start at depot (" << depotX << ", " << depotY << ")");
}

void MissionUISystem::cancelDepotSelection()
{
    pendingStart.active.store(false, std::memory_order_release);
    hideDepotSelectionPrompt();
}

// ---------------------------------------------------------------------------
// Events
// ---------------------------------------------------------------------------

void MissionUISystem::onProcessEvent(const OnSDLScanCode& event)
{
    if (event.key == SDL_SCANCODE_ESCAPE)
    {
        if (pendingStart.active.load(std::memory_order_acquire))
            cancelDepotSelection();
        else if (visible.load(std::memory_order_acquire))
            close();
    }
}

void MissionUISystem::onEvent(const TickEvent&)
{
}

void MissionUISystem::onProcessEvent(const OnMissionIconHoverEnter& event)
{
    if (not visible.load(std::memory_order_acquire))
        return;
    auto it = iconItemMap.find(event.iconEntityId);
    if (it == iconItemMap.end())
        return;
    // Send event so TooltipSystem mutation runs on its own task.
    ecsRef->sendEvent(SetTooltipHoveredItemEvent{it->second});
}

void MissionUISystem::onProcessEvent(const OnMissionIconHoverLeave& event)
{
    auto it = iconItemMap.find(event.iconEntityId);
    if (it == iconItemMap.end())
        return;
    ecsRef->sendEvent(SetTooltipHoveredItemEvent{ITEM_NONE});
}

void MissionUISystem::onProcessEvent(const MissionUIOpenRequest&)
{
    open();
}

void MissionUISystem::onProcessEvent(const MissionUICloseRequest&)
{
    close();
}

void MissionUISystem::onProcessEvent(const MissionUIToggleRequest&)
{
    toggle();
}

void MissionUISystem::onProcessEvent(const MissionUISelectDepotRequest& event)
{
    selectDepot(event.depotX, event.depotY);
}

void MissionUISystem::execute()
{
    if (visible.load(std::memory_order_acquire))
        refresh();
}

void MissionUISystem::onProcessEvent(const OnMouseClick& event)
{
    if (not visible.load(std::memory_order_acquire) or event.button != SDL_BUTTON_LEFT)
        return;

    float mx = event.pos.x;
    float my = event.pos.y;

    // Close button
    if (hitEntity(ecsRef, closeBtnBgId, mx, my))
    {
        close();
        return;
    }

    // Tab clicks (pill backgrounds)
    for (size_t t = 0; t < NUM_TABS; ++t)
    {
        if (hitEntity(ecsRef, tabButtons[t].bgId, mx, my))
        {
            switchTab(t);
            return;
        }
    }

    // Left column: row clicks
    for (size_t i = 0; i < MAX_LIST_ROWS and i < filteredDefs.size(); ++i)
    {
        if (hitEntity(ecsRef, listRows[i].bgId, mx, my))
        {
            selectMission(filteredDefs[i]);
            return;
        }
    }

    // Right column: action button click
    if (hitEntity(ecsRef, actionBtnBgId, mx, my))
    {
        auto* missionSystem = ecsRef->getSystem<MissionSystem>();
        if (currentTab == 2)
        {
            // Shop buy
            uint32_t cost = static_cast<uint32_t>(missionSystem->getExtraSlotCost());
            auto* playerInv = ecsRef->getSystem<PlayerInventorySystem>();
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
                pendingStart.active.store(true, std::memory_order_release);
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
    auto* missionSystem = ecsRef->getSystem<MissionSystem>();
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
    const auto& defs = ecsRef->getSystem<MissionSystem>()->getDefs();
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
    auto* missionSystem = ecsRef->getSystem<MissionSystem>();
    auto* depotSystem   = ecsRef->getSystem<DepotSystem>();
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
        LOG_INFO("MissionUI", "no depot built");
    else
        LOG_INFO("MissionUI", "no depot has enough Robot Cores (" << def.robotCoreCost << " needed)");
}

// ---------------------------------------------------------------------------
// Panel creation
// ---------------------------------------------------------------------------

void MissionUISystem::ensurePanelCreated()
{
    if (panelCreated)
    {
        LOG_INFO("MissionUI", "ensurePanelCreated() — already created, skipping");
        return;
    }
    LOG_INFO("MissionUI", "ensurePanelCreated() — building panel");
    createPanel();
    setPanelVisibility(false);
    panelCreated = true;
    LOG_INFO("MissionUI", "ensurePanelCreated() — done, panelCreated=true");
}

void MissionUISystem::onEvent(const ResizeEvent& event)
{
    screenWidth = event.width;
    screenHeight = event.height;
    // Anchors handle reposition automatically; no panel rebuild needed.
}

std::vector<uint64_t> MissionUISystem::collectAllPanelEntityIds() const
{
    std::vector<uint64_t> ids;
    auto add = [&ids](uint64_t id) { if (id != 0) ids.push_back(id); };

    add(backdropId);
    add(closeBtnBgId);
    add(closeBtnTextId);
    add(topDividerId);
    add(columnDividerId);

    for (size_t t = 0; t < NUM_TABS; ++t)
    {
        add(tabButtons[t].bgId);
        add(tabButtons[t].textId);
    }

    for (size_t i = 0; i < MAX_LIST_ROWS; ++i)
    {
        const auto& r = listRows[i];
        add(r.bgId);
        add(r.statusBorderId);
        add(r.statusFillId);
        add(r.nameId);
        add(r.goPillBgId);
        add(r.goPillTextId);
        add(r.separatorId);
    }

    add(detailMissionLabelId);
    add(detailNameId);
    add(detailDescId);

    add(costLabelId);
    add(costBlockBorderId);
    add(costBlockFillId);
    for (const auto& c : costItems)
    {
        add(c.iconId);
        add(c.countTextId);
    }

    add(rewardLabelId);
    add(rewardBlockBorderId);
    add(rewardBlockFillId);
    for (const auto& r : rewardItems)
    {
        add(r.iconId);
        add(r.countTextId);
    }

    add(unlockLabelId);
    add(detailProgressBgId);
    add(detailProgressFillId);
    add(detailProgressTextId);
    add(actionBtnBgId);
    add(actionBtnTextId);
    add(shopLabelId);
    add(shopCostTextId);
    add(promptBgId);
    add(promptTextId);

    return ids;
}

void MissionUISystem::destroyPanel()
{
    auto ids = collectAllPanelEntityIds();
    LOG_INFO("MissionUI", "destroyPanel() — removing " << ids.size()
            << " entities, iconItemMap had " << iconItemMap.size() << " entries");
    for (auto id : ids)
        ecsRef->removeEntity(id);

    backdropId = 0;
    closeBtnBgId = closeBtnTextId = 0;
    topDividerId = columnDividerId = 0;

    for (size_t t = 0; t < NUM_TABS; ++t)
        tabButtons[t] = {};

    for (size_t i = 0; i < MAX_LIST_ROWS; ++i)
        listRows[i] = {};

    detailMissionLabelId = detailNameId = detailDescId = 0;
    costLabelId = costBlockBorderId = costBlockFillId = 0;
    for (auto& c : costItems) c = {};
    rewardLabelId = rewardBlockBorderId = rewardBlockFillId = 0;
    for (auto& r : rewardItems) r = {};
    unlockLabelId = 0;
    detailProgressBgId = detailProgressFillId = detailProgressTextId = 0;
    actionBtnBgId = actionBtnTextId = 0;
    shopLabelId = shopCostTextId = 0;
    promptBgId = promptTextId = 0;
    promptVisible = false;

    iconItemMap.clear();
    panelCreated = false;
    LOG_INFO("MissionUI", "destroyPanel() done — panelCreated=false");
}

void MissionUISystem::createPanel()
{
    LOG_INFO("MissionUI", "createPanel() entry");
    float listContentH = MAX_LIST_ROWS * (LIST_ROW_H + SEPARATOR_H);
    panelH = PADDING + TITLE_H + DIVIDER_H + std::max(listContentH, 340.0f) + PADDING;

    auto windowEnt = ecsRef->getEntity("__MainWindow");
    uint64_t windowId = windowEnt ? windowEnt->id : 0;

    // --- Backdrop — anchored to __MainWindow center ---
    {
        auto bd = makeRoundedRect2DShape(ecsRef, 8.0f, PANEL_W, panelH, C::BG);
        auto pos = bd.get<PositionComponent>();
        pos->setZ(97.0f);
        bd.get<ViewportComponent>()->setViewport(UI_VP);
        backdropId = bd.entity->id;

        auto a = ecsRef->attach<UiAnchor>(bd.entity);
        if (windowId != 0)
        {
            a->setHorizontalCenter(PosAnchor{windowId, AnchorType::HorizontalCenter});
            a->setVerticalCenter(PosAnchor{windowId, AnchorType::VerticalCenter});
        }

        ecsRef->attach<MouseLeftClickComponent>(bd.entity,
            makeCallable<PanelWasClickedEvent>(), MouseStateTrigger::OnPress);
    }

    // Y-offset within the panel, used as topMargin when anchoring children
    float curOff = PADDING;

    // --- Tab bar (left-aligned pill tabs, replaces title) ---
    {
        static const char* TAB_LABELS[NUM_TABS] = {"Main", "Missions", "Shop"};
        float charW = 6.0f;
        float tabPadH = 12.0f;
        float tabGap = 6.0f;
        float tabH = TITLE_H;
        float tabRad = 5.0f;

        float tabLeftOff = PADDING;

        for (size_t t = 0; t < NUM_TABS; ++t)
        {
            float labelW = strlen(TAB_LABELS[t]) * charW;
            float pillW = labelW + tabPadH * 2.0f;

            auto bg = makeRoundedRect2DShape(ecsRef, tabRad, pillW, tabH,
                (t == currentTab) ? C::SELECTED : C::PANEL);
            bg.get<PositionComponent>()->setZ(99.0f);
            bg.get<ViewportComponent>()->setViewport(UI_VP);
            tabButtons[t].bgId = bg.entity->id;

            auto bgA = ecsRef->attach<UiAnchor>(bg.entity);
            bgA->setLeftAnchor(PosAnchor{backdropId, AnchorType::Left});
            bgA->setTopAnchor(PosAnchor{backdropId, AnchorType::Top});
            bgA->setLeftMargin(tabLeftOff);
            bgA->setTopMargin(curOff);

            auto txt = makeTTFText(ecsRef, 0.0f, 0.0f, 100.0f,
                FONT_MEDIUM, TAB_LABELS[t], SCALE_LIST,
                (t == currentTab) ? C::TEXT : C::TEXT_DIM);
            txt.get<ViewportComponent>()->setViewport(UI_VP);
            tabButtons[t].textId = txt.entity->id;

            auto txtAnchor = ecsRef->attach<UiAnchor>(txt.entity);
            txtAnchor->setHorizontalCenter(PosAnchor{tabButtons[t].bgId, AnchorType::HorizontalCenter});
            txtAnchor->setVerticalCenter(PosAnchor{tabButtons[t].bgId, AnchorType::VerticalCenter});

            tabLeftOff += pillW + tabGap;
        }
    }

    // --- Close button (vertically centered in title bar, top-right) ---
    {
        float cbOffY = (TITLE_H - CLOSE_SIZE) * 0.5f;
        auto bg = makeRoundedRect2DShape(ecsRef, 4.0f, CLOSE_SIZE, CLOSE_SIZE,
            {180.0f, 60.0f, 60.0f, 255.0f});
        bg.get<PositionComponent>()->setZ(99.0f);
        bg.get<ViewportComponent>()->setViewport(UI_VP);
        closeBtnBgId = bg.entity->id;

        auto a = ecsRef->attach<UiAnchor>(bg.entity);
        a->setRightAnchor(PosAnchor{backdropId, AnchorType::Right});
        a->setTopAnchor(PosAnchor{backdropId, AnchorType::Top});
        a->setRightMargin(PADDING);
        a->setTopMargin(curOff + cbOffY);

        auto txt = makeTTFText(ecsRef, 0.0f, 0.0f, 100.0f,
            FONT_BOLD, "X", SCALE_TAB, C::WHITE);
        txt.get<ViewportComponent>()->setViewport(UI_VP);
        closeBtnTextId = txt.entity->id;

        auto txtAnchor = ecsRef->attach<UiAnchor>(txt.entity);
        txtAnchor->setHorizontalCenter(PosAnchor{closeBtnBgId, AnchorType::HorizontalCenter});
        txtAnchor->setVerticalCenter(PosAnchor{closeBtnBgId, AnchorType::VerticalCenter});
    }
    curOff += TITLE_H + 2.0f;

    // --- Horizontal divider ---
    {
        auto div = makeSimple2DShape(ecsRef, Shape2D::Square, 0.0f, 0.0f, C::DIVIDER);
        auto pos = div.get<PositionComponent>();
        pos->setZ(99.0f);
        pos->setWidth(PANEL_W - 2 * PADDING); pos->setHeight(DIVIDER_H);
        div.get<ViewportComponent>()->setViewport(UI_VP);
        topDividerId = div.entity->id;

        auto a = ecsRef->attach<UiAnchor>(div.entity);
        a->setLeftAnchor(PosAnchor{backdropId, AnchorType::Left});
        a->setTopAnchor(PosAnchor{backdropId, AnchorType::Top});
        a->setLeftMargin(PADDING);
        a->setTopMargin(curOff);
    }
    curOff += DIVIDER_H;

    float contentOff = curOff;

    // --- Vertical column divider ---
    {
        float divH = panelH - contentOff - PADDING;
        auto div = makeSimple2DShape(ecsRef, Shape2D::Square, 0.0f, 0.0f, C::DIVIDER);
        auto pos = div.get<PositionComponent>();
        pos->setZ(99.0f);
        pos->setWidth(1.0f); pos->setHeight(divH);
        div.get<ViewportComponent>()->setViewport(UI_VP);
        columnDividerId = div.entity->id;

        auto a = ecsRef->attach<UiAnchor>(div.entity);
        a->setLeftAnchor(PosAnchor{backdropId, AnchorType::Left});
        a->setTopAnchor(PosAnchor{backdropId, AnchorType::Top});
        a->setLeftMargin(LEFT_W);
        a->setTopMargin(contentOff);
    }

    createLeftColumn(0.0f, contentOff);
    createRightColumn(0.0f, contentOff);
    LOG_INFO("MissionUI", "createPanel() done");
}

void MissionUISystem::createLeftColumn(float /*px*/, float contentOff)
{
    // px argument is unused; layout is panel-relative (anchored to backdrop).
    const float colLeft = 0.0f;       // left of panel
    const float rowW    = LEFT_W;

    auto anchorAt = [this](EntityRef ent, float leftMargin, float topMargin) {
        auto a = ecsRef->attach<UiAnchor>(ent);
        a->setLeftAnchor(PosAnchor{backdropId, AnchorType::Left});
        a->setTopAnchor(PosAnchor{backdropId, AnchorType::Top});
        a->setLeftMargin(leftMargin);
        a->setTopMargin(topMargin);
    };

    for (size_t i = 0; i < MAX_LIST_ROWS; ++i)
    {
        float rowOffY = contentOff + i * (LIST_ROW_H + SEPARATOR_H);
        auto& row = listRows[i];

        // Row background
        {
            auto bg = makeSimple2DShape(ecsRef, Shape2D::Square, 0.0f, 0.0f, C::TRANSPARENT);
            auto pos = bg.get<PositionComponent>();
            pos->setZ(98.0f);
            pos->setWidth(rowW); pos->setHeight(LIST_ROW_H);
            bg.get<ViewportComponent>()->setViewport(UI_VP);
            row.bgId = bg.entity->id;
            anchorAt(bg.entity, colLeft, rowOffY);
        }

        // Status square (border)
        float sqOffX = colLeft + PADDING;
        float sqOffY = rowOffY + (LIST_ROW_H - STATUS_SQ_SIZE) * 0.5f;
        {
            auto sq = makeRoundedRect2DShape(ecsRef, STATUS_SQ_RAD, STATUS_SQ_SIZE, STATUS_SQ_SIZE, C::TEXT_DIM);
            sq.get<PositionComponent>()->setZ(99.0f);
            sq.get<ViewportComponent>()->setViewport(UI_VP);
            row.statusBorderId = sq.entity->id;
            anchorAt(sq.entity, sqOffX, sqOffY);
        }

        // Status square (fill, inset)
        {
            float inset = 2.0f;
            auto sq = makeRoundedRect2DShape(ecsRef, STATUS_SQ_RAD,
                STATUS_SQ_SIZE - 2 * inset, STATUS_SQ_SIZE - 2 * inset, C::BG);
            sq.get<PositionComponent>()->setZ(86.0f);
            sq.get<ViewportComponent>()->setViewport(UI_VP);
            row.statusFillId = sq.entity->id;
            anchorAt(sq.entity, sqOffX + inset, sqOffY + inset);
        }

        // Mission name
        {
            auto name = makeTTFText(ecsRef, 0.0f, 0.0f, 100.0f,
                FONT_LIGHT, "", SCALE_LIST, C::TEXT);
            name.get<ViewportComponent>()->setViewport(UI_VP);
            row.nameId = name.entity->id;
            anchorAt(name.entity, colLeft + PADDING + STATUS_SQ_SIZE + 8.0f, rowOffY + 8.0f);
        }

        // GO pill
        float pillOffX = colLeft + rowW - PADDING - GO_PILL_W;
        float pillOffY = rowOffY + (LIST_ROW_H - GO_PILL_H) * 0.5f;
        {
            auto pill = makeRoundedRect2DShape(ecsRef, GO_PILL_RAD, GO_PILL_W, GO_PILL_H, C::ACCENT);
            auto pos = pill.get<PositionComponent>();
            pos->setZ(99.0f);
            pos->setVisibility(false);
            pill.get<ViewportComponent>()->setViewport(UI_VP);
            row.goPillBgId = pill.entity->id;
            anchorAt(pill.entity, pillOffX, pillOffY);

            auto txt = makeTTFText(ecsRef, 0.0f, 0.0f, 100.0f,
                FONT_BOLD, "GO", SCALE_PILL, C::WHITE);
            txt.get<PositionComponent>()->setVisibility(false);
            txt.get<ViewportComponent>()->setViewport(UI_VP);
            row.goPillTextId = txt.entity->id;
            anchorAt(txt.entity, pillOffX + 8.0f, pillOffY + 2.0f);
        }

        // Separator line
        {
            auto sep = makeSimple2DShape(ecsRef, Shape2D::Square, 0.0f, 0.0f, C::DIVIDER);
            auto pos = sep.get<PositionComponent>();
            pos->setZ(99.0f);
            pos->setWidth(rowW - PADDING); pos->setHeight(SEPARATOR_H);
            sep.get<ViewportComponent>()->setViewport(UI_VP);
            row.separatorId = sep.entity->id;
            anchorAt(sep.entity, colLeft + PADDING, rowOffY + LIST_ROW_H);
        }
    }
}

void MissionUISystem::createRightColumn(float /*px*/, float contentOff)
{
    // px is unused; layout is panel-relative (anchored to backdrop).
    const float colOff = LEFT_W + DETAIL_PAD;        // x-offset of right col within panel
    const float colW   = RIGHT_W - 2 * DETAIL_PAD;
    float curOff = contentOff + DETAIL_PAD;

    auto anchorAt = [this](EntityRef ent, float leftMargin, float topMargin) {
        auto a = ecsRef->attach<UiAnchor>(ent);
        a->setLeftAnchor(PosAnchor{backdropId, AnchorType::Left});
        a->setTopAnchor(PosAnchor{backdropId, AnchorType::Top});
        a->setLeftMargin(leftMargin);
        a->setTopMargin(topMargin);
    };

    // "MISSION" label
    {
        auto lbl = makeTTFText(ecsRef, 0.0f, 0.0f, 100.0f,
            FONT_MEDIUM, "MISSION", SCALE_LABEL, C::TEXT_DIM);
        lbl.get<ViewportComponent>()->setViewport(UI_VP);
        detailMissionLabelId = lbl.entity->id;
        anchorAt(lbl.entity, colOff, curOff);
    }
    curOff += 18.0f;

    // Mission name (large display)
    {
        auto name = makeTTFText(ecsRef, 0.0f, 0.0f, 100.0f,
            FONT_BOLD, "", SCALE_DISPLAY, C::TEXT);
        name.get<ViewportComponent>()->setViewport(UI_VP);
        detailNameId = name.entity->id;
        anchorAt(name.entity, colOff, curOff);
    }
    curOff += 28.0f;

    // Description — anchored left+right to backdrop so width follows column
    {
        auto desc = makeTTFText(ecsRef, 0.0f, 0.0f, 100.0f,
            FONT_LIGHT, "", SCALE_BODY, C::TEXT_DIM);
        desc.get<ViewportComponent>()->setViewport(UI_VP);
        desc.entity->get<TTFText>()->setWrap(true);
        detailDescId = desc.entity->id;

        auto anchor = ecsRef->attach<UiAnchor>(desc.entity);
        anchor->setLeftAnchor(PosAnchor{backdropId, AnchorType::Left});
        anchor->setLeftMargin(LEFT_W + DETAIL_PAD);
        anchor->setRightAnchor(PosAnchor{backdropId, AnchorType::Right});
        anchor->setRightMargin(DETAIL_PAD);
        anchor->setTopAnchor(PosAnchor{backdropId, AnchorType::Top});
        anchor->setTopMargin(curOff);
    }
    curOff += 44.0f;

    // --- Cost block ---
    {
        auto lbl = makeTTFText(ecsRef, 0.0f, 0.0f, 100.0f,
            FONT_MEDIUM, "COST", SCALE_LABEL, C::TEXT_DIM);
        lbl.get<ViewportComponent>()->setViewport(UI_VP);
        costLabelId = lbl.entity->id;
        anchorAt(lbl.entity, colOff, curOff);
        curOff += 16.0f;

        auto outer = makeRoundedRect2DShape(ecsRef, BLOCK_RADIUS, colW, BLOCK_H, C::COST_BRD);
        outer.get<PositionComponent>()->setZ(98.0f);
        outer.get<ViewportComponent>()->setViewport(UI_VP);
        costBlockBorderId = outer.entity->id;
        anchorAt(outer.entity, colOff, curOff);

        auto inner = makeRoundedRect2DShape(ecsRef, BLOCK_RADIUS - 1.0f,
            colW - 2 * COST_BORDER, BLOCK_H - 2 * COST_BORDER, C::BLOCK_FILL);
        inner.get<PositionComponent>()->setZ(99.0f);
        inner.get<ViewportComponent>()->setViewport(UI_VP);
        costBlockFillId = inner.entity->id;
        anchorAt(inner.entity, colOff + COST_BORDER, curOff + COST_BORDER);

        float itemOffX = colOff + 12.0f;
        float itemOffY = curOff + (BLOCK_H - ICON_SIZE) * 0.5f;
        for (size_t r = 0; r < MAX_COST_ITEMS; ++r)
        {
            float rOff = itemOffX + r * 70.0f;

            auto icon = make2DTexture(ecsRef, ICON_SIZE, ICON_SIZE, "Items.0");
            auto iconPos = icon.get<PositionComponent>();
            iconPos->setZ(99.0f);
            iconPos->setVisibility(false);
            icon.get<ViewportComponent>()->setViewport(UI_VP);
            costItems[r].iconId = icon.entity->id;
            anchorAt(icon.entity, rOff, itemOffY);

            ecsRef->attach<MouseEnterComponent>(icon.entity,
                makeCallable<OnMissionIconHoverEnter>(OnMissionIconHoverEnter{icon.entity->id}));
            ecsRef->attach<MouseLeaveComponent>(icon.entity,
                makeCallable<OnMissionIconHoverLeave>(OnMissionIconHoverLeave{icon.entity->id}));

            auto cnt = makeTTFText(ecsRef, 0.0f, 0.0f, 100.0f,
                FONT_MEDIUM, "", SCALE_NUM, C::TEXT);
            cnt.get<PositionComponent>()->setVisibility(false);
            cnt.get<ViewportComponent>()->setViewport(UI_VP);
            costItems[r].countTextId = cnt.entity->id;
            anchorAt(cnt.entity, rOff + ICON_SIZE + 4.0f, curOff + 14.0f);
        }
    }
    curOff += BLOCK_H + 12.0f;

    // --- Reward block ---
    {
        auto lbl = makeTTFText(ecsRef, 0.0f, 0.0f, 100.0f,
            FONT_MEDIUM, "REWARD", SCALE_LABEL, C::TEXT_DIM);
        lbl.get<ViewportComponent>()->setViewport(UI_VP);
        rewardLabelId = lbl.entity->id;
        anchorAt(lbl.entity, colOff, curOff);
        curOff += 16.0f;

        auto outer = makeRoundedRect2DShape(ecsRef, BLOCK_RADIUS, colW, BLOCK_H, C::REWARD_BRD);
        outer.get<PositionComponent>()->setZ(98.0f);
        outer.get<ViewportComponent>()->setViewport(UI_VP);
        rewardBlockBorderId = outer.entity->id;
        anchorAt(outer.entity, colOff, curOff);

        constant::Vector4D rewardFill = {48.0f, 52.0f, 60.0f, 255.0f};
        auto inner = makeRoundedRect2DShape(ecsRef, BLOCK_RADIUS - 1.0f,
            colW - 2 * REWARD_BORDER, BLOCK_H - 2 * REWARD_BORDER, rewardFill);
        inner.get<PositionComponent>()->setZ(99.0f);
        inner.get<ViewportComponent>()->setViewport(UI_VP);
        rewardBlockFillId = inner.entity->id;
        anchorAt(inner.entity, colOff + REWARD_BORDER, curOff + REWARD_BORDER);

        float itemOffX = colOff + 12.0f;
        float itemOffY = curOff + (BLOCK_H - ICON_SIZE) * 0.5f;
        for (size_t r = 0; r < MAX_REWARD_ITEMS; ++r)
        {
            float rOff = itemOffX + r * 70.0f;

            auto icon = make2DTexture(ecsRef, ICON_SIZE, ICON_SIZE, "Items.0");
            auto iconPos = icon.get<PositionComponent>();
            iconPos->setZ(99.0f);
            iconPos->setVisibility(false);
            icon.get<ViewportComponent>()->setViewport(UI_VP);
            rewardItems[r].iconId = icon.entity->id;
            anchorAt(icon.entity, rOff, itemOffY);

            ecsRef->attach<MouseEnterComponent>(icon.entity,
                makeCallable<OnMissionIconHoverEnter>(OnMissionIconHoverEnter{icon.entity->id}));
            ecsRef->attach<MouseLeaveComponent>(icon.entity,
                makeCallable<OnMissionIconHoverLeave>(OnMissionIconHoverLeave{icon.entity->id}));

            auto cnt = makeTTFText(ecsRef, 0.0f, 0.0f, 100.0f,
                FONT_MEDIUM, "", SCALE_NUM, C::TEXT);
            cnt.get<PositionComponent>()->setVisibility(false);
            cnt.get<ViewportComponent>()->setViewport(UI_VP);
            rewardItems[r].countTextId = cnt.entity->id;
            anchorAt(cnt.entity, rOff + ICON_SIZE + 4.0f, curOff + 14.0f);
        }
    }
    curOff += BLOCK_H + 8.0f;

    // --- Unlock label ---
    {
        auto lbl = makeTTFText(ecsRef, 0.0f, 0.0f, 100.0f,
            FONT_MEDIUM, "", SCALE_BODY, C::ACCENT);
        lbl.get<PositionComponent>()->setVisibility(false);
        lbl.get<ViewportComponent>()->setViewport(UI_VP);
        unlockLabelId = lbl.entity->id;
        anchorAt(lbl.entity, colOff, curOff);
    }
    curOff += 22.0f;

    // --- Progress bar (for active endgame missions) ---
    {
        auto pbBg = makeSimple2DShape(ecsRef, Shape2D::Square, 0.0f, 0.0f,
            constant::Vector4D{50.0f, 50.0f, 60.0f, 200.0f});
        auto pbPos = pbBg.get<PositionComponent>();
        pbPos->setZ(98.0f);
        pbPos->setWidth(colW); pbPos->setHeight(PROGRESS_H);
        pbPos->setVisibility(false);
        pbBg.get<ViewportComponent>()->setViewport(UI_VP);
        detailProgressBgId = pbBg.entity->id;
        anchorAt(pbBg.entity, colOff, curOff);

        auto pbFill = makeSimple2DShape(ecsRef, Shape2D::Square, 0.0f, 0.0f,
            constant::Vector4D{80.0f, 160.0f, 80.0f, 220.0f});
        auto pfPos = pbFill.get<PositionComponent>();
        pfPos->setZ(99.0f);
        pfPos->setWidth(0.0f); pfPos->setHeight(PROGRESS_H);
        pfPos->setVisibility(false);
        pbFill.get<ViewportComponent>()->setViewport(UI_VP);
        detailProgressFillId = pbFill.entity->id;
        anchorAt(pbFill.entity, colOff, curOff);

        auto pTxt = makeTTFText(ecsRef, 0.0f, 0.0f, 100.0f,
            FONT_MEDIUM, "", SCALE_NUM, C::TEXT_DIM);
        pTxt.get<PositionComponent>()->setVisibility(false);
        pTxt.get<ViewportComponent>()->setViewport(UI_VP);
        detailProgressTextId = pTxt.entity->id;
        anchorAt(pTxt.entity, colOff + colW + 6.0f, curOff - 2.0f);
    }
    curOff += PROGRESS_H + 12.0f;

    // --- Shop section labels (tab 2 only, positioned in detail area) ---
    {
        auto lbl = makeTTFText(ecsRef, 0.0f, 0.0f, 100.0f,
            FONT_LIGHT, "", SCALE_BODY, C::TEXT_DIM);
        lbl.get<PositionComponent>()->setVisibility(false);
        lbl.get<ViewportComponent>()->setViewport(UI_VP);
        shopCostTextId = lbl.entity->id;
        anchorAt(lbl.entity, colOff, contentOff + DETAIL_PAD + 46.0f);

        auto shopName = makeTTFText(ecsRef, 0.0f, 0.0f, 100.0f,
            FONT_BOLD, "Extra Mission Slot", SCALE_DISPLAY, C::TEXT);
        shopName.get<PositionComponent>()->setVisibility(false);
        shopName.get<ViewportComponent>()->setViewport(UI_VP);
        shopLabelId = shopName.entity->id;
        anchorAt(shopName.entity, colOff, contentOff + DETAIL_PAD + 18.0f);
    }

    // --- Action button (full-width at bottom of detail pane) ---
    {
        float btnTopOff = panelH - PADDING - ACTION_BTN_H;

        auto bg = makeRoundedRect2DShape(ecsRef, ACTION_BTN_RAD, colW, ACTION_BTN_H, C::ACCENT);
        bg.get<PositionComponent>()->setZ(99.0f);
        bg.get<ViewportComponent>()->setViewport(UI_VP);
        actionBtnBgId = bg.entity->id;
        anchorAt(bg.entity, colOff, btnTopOff);

        auto txt = makeTTFText(ecsRef, 0.0f, 0.0f, 100.0f,
            FONT_BOLD, "Start Mission", SCALE_BTN, C::WHITE);
        txt.get<ViewportComponent>()->setViewport(UI_VP);
        actionBtnTextId = txt.entity->id;

        auto txtAnchor = ecsRef->attach<UiAnchor>(txt.entity);
        txtAnchor->setHorizontalCenter(PosAnchor{actionBtnBgId, AnchorType::HorizontalCenter});
        txtAnchor->setVerticalCenter(PosAnchor{actionBtnBgId, AnchorType::VerticalCenter});
    }
}

// ---------------------------------------------------------------------------
// Visibility
// ---------------------------------------------------------------------------

void MissionUISystem::setPanelVisibility(bool vis)
{
    LOG_INFO("MissionUI", "setPanelVisibility(" << static_cast<int>(vis)
            << ") — backdropId=" << backdropId);
    // Chrome
    setEntityVisibility(backdropId, vis);
    setEntityVisibility(closeBtnBgId, vis);
    setEntityVisibility(closeBtnTextId, vis);
    setEntityVisibility(topDividerId, vis);
    setEntityVisibility(columnDividerId, vis);

    for (size_t t = 0; t < NUM_TABS; ++t)
    {
        setEntityVisibility(tabButtons[t].bgId, vis);
        setEntityVisibility(tabButtons[t].textId, vis);
    }

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
    if (not visible.load(std::memory_order_acquire))
        return;

    // Update tab styling
    for (size_t t = 0; t < NUM_TABS; ++t)
    {
        setEntityTextColor(tabButtons[t].textId, (t == currentTab) ? C::TEXT : C::TEXT_DIM);
        setEntityRoundedRectColor(tabButtons[t].bgId, (t == currentTab) ? C::SELECTED : C::PANEL);
    }

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
    auto* missionSystem = ecsRef->getSystem<MissionSystem>();
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

        // Status square opacity
        {
            float opacity = unlocked ? 255.0f : 80.0f;
            auto borderEnt = ecsRef->getEntity(row.statusBorderId);
            if (borderEnt and borderEnt->has<RoundedRect2DObject>())
                borderEnt->get<RoundedRect2DObject>()->setOpacity(opacity);
            auto fillEnt = ecsRef->getEntity(row.statusFillId);
            if (fillEnt and fillEnt->has<RoundedRect2DObject>())
                fillEnt->get<RoundedRect2DObject>()->setOpacity(opacity);
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
    iconItemMap.clear();

    auto* missionSystem = ecsRef->getSystem<MissionSystem>();
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
                    iconItemMap[costItems[r].iconId] = req.itemId;
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
                iconItemMap[rewardItems[r].iconId] = reward.itemId;
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
        const float bannerW = 300.0f;
        const float bannerH = 32.0f;
        const float bannerTopMargin = 80.0f;

        auto windowEnt = ecsRef->getEntity("__MainWindow");
        uint64_t windowId = windowEnt ? windowEnt->id : 0;

        auto bg = makeRoundedRect2DShape(ecsRef, 6.0f, bannerW, bannerH, C::BG);
        auto pos = bg.get<PositionComponent>();
        pos->setZ(101.0f);
        bg.get<ViewportComponent>()->setViewport(UI_VP);
        promptBgId = bg.entity->id;

        auto bgA = ecsRef->attach<UiAnchor>(bg.entity);
        if (windowId != 0)
        {
            bgA->setHorizontalCenter(PosAnchor{windowId, AnchorType::HorizontalCenter});
            bgA->setTopAnchor(PosAnchor{windowId, AnchorType::Top});
            bgA->setTopMargin(bannerTopMargin);
        }

        auto txt = makeTTFText(ecsRef, 0.0f, 0.0f, 102.0f,
            FONT_MEDIUM, "Click a depot to start mission", SCALE_BODY,
            C::ACCENT);
        txt.get<ViewportComponent>()->setViewport(UI_VP);
        promptTextId = txt.entity->id;

        auto txtA = ecsRef->attach<UiAnchor>(txt.entity);
        txtA->setLeftAnchor(PosAnchor{promptBgId, AnchorType::Left});
        txtA->setTopAnchor(PosAnchor{promptBgId, AnchorType::Top});
        txtA->setLeftMargin(16.0f);
        txtA->setTopMargin(6.0f);
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
