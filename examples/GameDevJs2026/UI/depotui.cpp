#include "depotui.h"
#include "craftingui.h"

#include "2D/simple2dobject.h"
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
    depotDataSeenThisOpen = false;

    // Suppress crafting UI BEFORE opening inventory (prevents auto-open via InventoryOpenedEvent)
    if (craftingUI)
        craftingUI->setSuppressed(true);

    if (inventoryUI and not inventoryUI->isOpen())
        inventoryUI->openInventory();

    // Close crafting UI if it was already open before we set suppress
    if (craftingUI and craftingUI->isOpen())
        craftingUI->close();

    ensurePanelCreated();
    setPanelVisibility(true);
    syncAllSlots();
}

void DepotUISystem::close()
{
    if (not visible)
        return;

    if (slotSystem->hasHeldItem())
        slotSystem->cancelHeld();

    // Sync all slots back to depot
    DepotData* depot = depotSystem->getDepot(openDepotX, openDepotY);
    if (depot)
    {
        for (size_t i = 0; i < NUM_SLOTS; ++i)
            syncSlotToDepot(i, true);
        for (size_t i = 0; i < NUM_OUTPUT_SLOTS; ++i)
            syncSlotToDepot(i, false);
    }

    setPanelVisibility(false);
    visible = false;
    openDepotX = -1;
    openDepotY = -1;
    depotDataSeenThisOpen = false;

    if (craftingUI)
        craftingUI->setSuppressed(false);

    // Close the companion inventory we opened in open(). closeInventory is
    // idempotent and cascades to crafting via InventoryClosedEvent. The
    // visible-guard at the top of this function prevents the cascade from
    // re-entering close().
    if (inventoryUI and inventoryUI->isOpen())
        inventoryUI->closeInventory();
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
        // Auto-close on missing data is intended to handle "depot destroyed
        // while UI is open". Skip it on the first ticks after open so the
        // panel doesn't disappear when BuildingPlacedEvent for a freshly
        // placed depot hasn't been dispatched yet.
        if (depotDataSeenThisOpen)
            close();
        // Still refresh the mission section so it reflects current state.
        refreshMissionSection();
        return;
    }

    depotDataSeenThisOpen = true;
    syncAllSlots();
}

void DepotUISystem::onProcessEvent(const SlotPickedUpEvent& event)
{
    if (not visible)
        return;

    for (size_t i = 0; i < NUM_SLOTS; ++i)
    {
        if (event.slotEntityId == inputSlotEntityIds[i])
        {
            syncSlotToDepot(i, true);
            return;
        }
    }

    for (size_t i = 0; i < NUM_OUTPUT_SLOTS; ++i)
    {
        if (event.slotEntityId == outputSlotEntityIds[i])
        {
            syncSlotToDepot(i, false);
            return;
        }
    }
}

void DepotUISystem::onProcessEvent(const SlotDroppedEvent& event)
{
    if (not visible)
        return;

    for (size_t i = 0; i < NUM_SLOTS; ++i)
    {
        if (event.slotEntityId == inputSlotEntityIds[i])
        {
            syncSlotToDepot(i, true);
            return;
        }
    }

    for (size_t i = 0; i < NUM_OUTPUT_SLOTS; ++i)
    {
        if (event.slotEntityId == outputSlotEntityIds[i])
        {
            syncSlotToDepot(i, false);
            return;
        }
    }
}

void DepotUISystem::onProcessEvent(const OnMouseClick& event)
{
    if (not visible or event.button != SDL_BUTTON_LEFT)
        return;

    float mx = event.pos.x;
    float my = event.pos.y;

    // Mission section: CLAIM button for active mission
    if (missionSystem->hasActiveMissionAtDepot(openDepotX, openDepotY))
    {
        if (mx >= claimBtnX and mx <= claimBtnX + BTN_W
            and my >= claimBtnY and my <= claimBtnY + BTN_H)
        {
            size_t idx = missionSystem->getActiveMissionIndexForDepot(openDepotX, openDepotY);

            if (idx != SIZE_MAX)
            {
                const auto& active = missionSystem->getActive();
                if (idx < active.size() and active[idx].completed)
                {
                    missionSystem->claimMission(idx);
                    syncAllSlots();
                }
            }

            return;
        }
    }
    else
    {
        // START buttons for available missions
        for (size_t i = 0; i < MAX_MISSION_ROWS; ++i)
        {
            auto& row = missionRows[i];

            if (row.bgId == 0)
                continue;

            if (mx >= row.btnX and mx <= row.btnX + BTN_W
                and my >= row.btnY and my <= row.btnY + BTN_H)
            {
                if (missionSystem->canStartMission(row.defIndex))
                {
                    missionSystem->startMission(row.defIndex, openDepotX, openDepotY);
                    syncAllSlots();
                }

                return;
            }
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

float DepotUISystem::getMissionPanelX() const
{
    float invW = InventoryUISystem::COLS * InventoryUISystem::SLOT_SIZE
               + (InventoryUISystem::COLS - 1) * InventoryUISystem::SLOT_SPACING
               + 2.0f * InventoryUISystem::PANEL_PADDING;
    float invX = (screenWidth - invW) * 0.5f;

    return invX + invW + GAP_BETWEEN_PANELS;
}

float DepotUISystem::getMissionPanelY() const
{
    return (screenHeight - getMissionPanelHeight()) * 0.5f;
}

// ---------------------------------------------------------------------------
// Panel creation
// ---------------------------------------------------------------------------

void DepotUISystem::ensurePanelCreated()
{
    if (panelCreated)
        return;

    createPanel();
    setPanelVisibility(false);
    panelCreated = true;
}

void DepotUISystem::setPanelVisibility(bool vis)
{
    setEntityVisibility(backdropEntityId, vis);
    setEntityVisibility(titleEntityId,    vis);
    setEntityVisibility(outputTitleEntityId, vis);

    for (size_t i = 0; i < NUM_SLOTS; ++i)
    {
        auto ent = ecsRef->getEntity(inputSlotEntityIds[i]);
        if (ent)
            ent->get<PositionComponent>()->setVisibility(vis);
    }

    for (size_t i = 0; i < NUM_OUTPUT_SLOTS; ++i)
    {
        auto ent = ecsRef->getEntity(outputSlotEntityIds[i]);
        if (ent)
            ent->get<PositionComponent>()->setVisibility(vis);
    }

    // Mission panel (right side)
    setEntityVisibility(missionPanelBackdropId, vis);
    setEntityVisibility(missionSectionTitleId, vis);
    if (not vis)
    {
        // Hide all mission entities when closing
        setEntityVisibility(activeMissionNameId, false);
        setEntityVisibility(activeMissionProgressBgId, false);
        setEntityVisibility(activeMissionProgressFillId, false);
        setEntityVisibility(activeMissionStatusId, false);
        setEntityVisibility(activeMissionClaimBtnBgId, false);
        setEntityVisibility(activeMissionClaimBtnTextId, false);
        for (size_t i = 0; i < MAX_MISSION_ROWS; ++i)
        {
            setEntityVisibility(missionRows[i].bgId, false);
            setEntityVisibility(missionRows[i].nameId, false);
            setEntityVisibility(missionRows[i].infoId, false);
            setEntityVisibility(missionRows[i].btnBgId, false);
            setEntityVisibility(missionRows[i].btnTextId, false);
        }
    }
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

    // Input slots via SlotSystem
    for (size_t i = 0; i < NUM_SLOTS; ++i)
    {
        size_t col = i % COLS;
        size_t row = i / COLS;

        float sx = panelX + PANEL_PADDING + col * (SLOT_SIZE + SLOT_SPACING);
        float sy = slotsStartY + row * (SLOT_SIZE + SLOT_SPACING);

        auto slotRef = slotSystem->createSlot(
            SlotCategory::Input, static_cast<uint8_t>(i));
        inputSlotEntityIds[i] = slotRef.id;

        auto pos = slotRef.get<PositionComponent>();
        pos->setX(sx);
        pos->setY(sy);
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

    // Output slots via SlotSystem (OutputOnly — pick up only, no dropping)
    for (size_t i = 0; i < NUM_OUTPUT_SLOTS; ++i)
    {
        size_t col = i % COLS;
        size_t row = i / COLS;

        float sx = panelX + PANEL_PADDING + col * (SLOT_SIZE + SLOT_SPACING);
        float sy = outputSlotsStartY + row * (SLOT_SIZE + SLOT_SPACING);

        auto slotRef = slotSystem->createSlot(
            SlotCategory::Output, static_cast<uint8_t>(i),
            SlotFlags::OutputOnly,
            DEFAULT_SLOT_SIZE, DEFAULT_ITEM_SIZE,
            {45.0f, 55.0f, 50.0f, 200.0f});
        outputSlotEntityIds[i] = slotRef.id;

        auto pos = slotRef.get<PositionComponent>();
        pos->setX(sx);
        pos->setY(sy);
    }

    // --- Mission panel (RIGHT of inventory) ---
    {
        float rightX = getMissionPanelX();
        float rightY = getMissionPanelY();
        float rightW = getMissionPanelWidth();
        float rightH = getMissionPanelHeight();

        auto bd = makeSimple2DShape(ecsRef, Shape2D::Square, 0.0f, 0.0f,
            constant::Vector4D{20.0f, 20.0f, 30.0f, 220.0f});
        auto pos = bd.get<PositionComponent>();
        pos->setX(rightX); pos->setY(rightY); pos->setZ(97.0f);
        pos->setWidth(rightW); pos->setHeight(rightH);
        bd.get<ViewportComponent>()->setViewport(UI_VP);
        missionPanelBackdropId = bd.entity->id;

        ecsRef->attach<MouseLeftClickComponent>(bd.entity,
            makeCallable<PanelWasClickedEvent>(), MouseStateTrigger::OnPress);
    }
    createMissionSection();
}

// ---------------------------------------------------------------------------
// Sync helpers
// ---------------------------------------------------------------------------

void DepotUISystem::syncAllSlots()
{
    DepotData* depot = depotSystem->getDepot(openDepotX, openDepotY);

    if (depot)
    {
        for (size_t i = 0; i < NUM_SLOTS; ++i)
            slotSystem->syncSlotVisual(inputSlotEntityIds[i], depot->inventory.getSlot(i));

        for (size_t i = 0; i < NUM_OUTPUT_SLOTS; ++i)
            slotSystem->syncSlotVisual(outputSlotEntityIds[i], depot->output.getSlot(i));
    }

    // Mission section is queried via missionSystem by depot coords, so it
    // works even if depot data isn't registered yet (e.g. first frame after
    // placement, before BuildingPlacedEvent has been dispatched).
    refreshMissionSection();
}

void DepotUISystem::syncSlotToDepot(size_t slotIndex, bool isInput)
{
    DepotData* depot = depotSystem->getDepot(openDepotX, openDepotY);
    if (not depot)
        return;

    if (isInput)
    {
        auto* sc = slotSystem->getSlotComponent(inputSlotEntityIds[slotIndex]);
        if (sc)
            depot->inventory.getSlot(slotIndex) = sc->stack;
    }
    else
    {
        auto* sc = slotSystem->getSlotComponent(outputSlotEntityIds[slotIndex]);
        if (sc)
            depot->output.getSlot(slotIndex) = sc->stack;
    }
}

// ---------------------------------------------------------------------------
// Mission section
// ---------------------------------------------------------------------------

void DepotUISystem::createMissionSection()
{
    float panelX = getMissionPanelX();
    float panelW = getMissionPanelWidth();
    float curY = getMissionPanelY() + PANEL_PADDING;

    // Section title
    {
        auto t = makeTTFText(ecsRef,
            panelX + PANEL_PADDING, curY + 4.0f, 100.0f,
            FONT_PATH, "Mission", TITLE_SCALE, {180.0f, 180.0f, 200.0f, 255.0f});
        t.get<ViewportComponent>()->setViewport(UI_VP);
        missionSectionTitleId = t.entity->id;
    }
    curY += TITLE_H + GAP_AFTER_TITLE;

    // Active mission entities (initially hidden)
    {
        auto name = makeTTFText(ecsRef, panelX + PANEL_PADDING + 2.0f, curY + 2.0f, 100.0f,
            FONT_PATH, "", TEXT_SCALE, {255.0f, 255.0f, 255.0f, 255.0f});
        name.get<ViewportComponent>()->setViewport(UI_VP);
        name.get<PositionComponent>()->setVisibility(false);
        activeMissionNameId = name.entity->id;
    }

    float pbX = panelX + PANEL_PADDING;
    float pbY = curY + MISSION_ROW_H;
    float pbW = panelW - 2.0f * PANEL_PADDING;

    {
        auto bg = makeSimple2DShape(ecsRef, Shape2D::Square, 0.0f, 0.0f,
            constant::Vector4D{50.0f, 50.0f, 60.0f, 200.0f});
        auto pos = bg.get<PositionComponent>();
        pos->setX(pbX); pos->setY(pbY); pos->setZ(98.5f);
        pos->setWidth(pbW); pos->setHeight(PROGRESS_H);
        pos->setVisibility(false);
        bg.get<ViewportComponent>()->setViewport(UI_VP);
        activeMissionProgressBgId = bg.entity->id;
    }
    {
        auto fill = makeSimple2DShape(ecsRef, Shape2D::Square, 0.0f, 0.0f,
            constant::Vector4D{80.0f, 160.0f, 80.0f, 220.0f});
        auto pos = fill.get<PositionComponent>();
        pos->setX(pbX); pos->setY(pbY); pos->setZ(98.6f);
        pos->setWidth(0.0f); pos->setHeight(PROGRESS_H);
        pos->setVisibility(false);
        fill.get<ViewportComponent>()->setViewport(UI_VP);
        activeMissionProgressFillId = fill.entity->id;
    }
    {
        auto status = makeTTFText(ecsRef, pbX, pbY + PROGRESS_H + 2.0f, 100.0f,
            FONT_PATH, "", TEXT_SCALE, {180.0f, 180.0f, 180.0f, 255.0f});
        status.get<ViewportComponent>()->setViewport(UI_VP);
        status.get<PositionComponent>()->setVisibility(false);
        activeMissionStatusId = status.entity->id;
    }

    // CLAIM button for active mission
    float claimX = panelX + panelW - PANEL_PADDING - BTN_W;
    float claimY = pbY + PROGRESS_H + 2.0f;
    claimBtnX = claimX;
    claimBtnY = claimY;

    {
        auto bg = makeSimple2DShape(ecsRef, Shape2D::Square, 0.0f, 0.0f,
            constant::Vector4D{60.0f, 100.0f, 180.0f, 200.0f});
        auto pos = bg.get<PositionComponent>();
        pos->setX(claimX); pos->setY(claimY); pos->setZ(99.0f);
        pos->setWidth(BTN_W); pos->setHeight(BTN_H);
        pos->setVisibility(false);
        bg.get<ViewportComponent>()->setViewport(UI_VP);
        activeMissionClaimBtnBgId = bg.entity->id;
    }
    {
        auto txt = makeTTFText(ecsRef, claimX + 4.0f, claimY + 3.0f, 100.0f,
            FONT_PATH, "CLAIM", BTN_TEXT_SCALE, {255.0f, 255.0f, 255.0f, 255.0f});
        txt.get<ViewportComponent>()->setViewport(UI_VP);
        txt.get<PositionComponent>()->setVisibility(false);
        activeMissionClaimBtnTextId = txt.entity->id;
    }

    // Available mission rows (initially hidden)
    for (size_t i = 0; i < MAX_MISSION_ROWS; ++i)
    {
        float rowY = curY + i * (MISSION_ROW_H + MISSION_ROW_GAP);
        auto& row = missionRows[i];

        auto bg = makeSimple2DShape(ecsRef, Shape2D::Square, 0.0f, 0.0f,
            constant::Vector4D{35.0f, 35.0f, 45.0f, 180.0f});
        auto pos = bg.get<PositionComponent>();
        pos->setX(panelX + PANEL_PADDING); pos->setY(rowY); pos->setZ(98.0f);
        pos->setWidth(panelW - 2.0f * PANEL_PADDING); pos->setHeight(MISSION_ROW_H);
        pos->setVisibility(false);
        bg.get<ViewportComponent>()->setViewport(UI_VP);
        row.bgId = bg.entity->id;

        auto name = makeTTFText(ecsRef, panelX + PANEL_PADDING + 2.0f, rowY + 4.0f, 100.0f,
            FONT_PATH, "", TEXT_SCALE, {255.0f, 255.0f, 255.0f, 255.0f});
        name.get<ViewportComponent>()->setViewport(UI_VP);
        name.get<PositionComponent>()->setVisibility(false);
        row.nameId = name.entity->id;

        auto info = makeTTFText(ecsRef, panelX + PANEL_PADDING + 2.0f, rowY + 14.0f, 100.0f,
            FONT_PATH, "", 0.2f, {150.0f, 150.0f, 170.0f, 255.0f});
        info.get<ViewportComponent>()->setViewport(UI_VP);
        info.get<PositionComponent>()->setVisibility(false);
        row.infoId = info.entity->id;

        float bx = panelX + panelW - PANEL_PADDING - BTN_W;
        row.btnX = bx;
        row.btnY = rowY + 2.0f;

        auto btnBg = makeSimple2DShape(ecsRef, Shape2D::Square, 0.0f, 0.0f,
            constant::Vector4D{60.0f, 120.0f, 60.0f, 200.0f});
        auto bpos = btnBg.get<PositionComponent>();
        bpos->setX(bx); bpos->setY(row.btnY); bpos->setZ(99.0f);
        bpos->setWidth(BTN_W); bpos->setHeight(BTN_H);
        bpos->setVisibility(false);
        btnBg.get<ViewportComponent>()->setViewport(UI_VP);
        row.btnBgId = btnBg.entity->id;

        auto btnTxt = makeTTFText(ecsRef, bx + 6.0f, row.btnY + 3.0f, 100.0f,
            FONT_PATH, "START", BTN_TEXT_SCALE, {255.0f, 255.0f, 255.0f, 255.0f});
        btnTxt.get<ViewportComponent>()->setViewport(UI_VP);
        btnTxt.get<PositionComponent>()->setVisibility(false);
        row.btnTextId = btnTxt.entity->id;
    }
}

void DepotUISystem::refreshMissionSection()
{
    if (not missionSystem)
        return;

    const auto* activeMission = missionSystem->getActiveMissionForDepot(openDepotX, openDepotY);

    if (activeMission)
    {
        // Show active mission, hide available rows
        const auto& def = missionSystem->getDefs()[activeMission->defIndex];

        setEntityText(missionSectionTitleId, "Mission: " + def.name);
        setEntityText(activeMissionNameId, def.description);
        setEntityVisibility(activeMissionNameId, true);

        // Progress
        float progress;
        if (def.isDeliveryMission())
            progress = missionSystem->getDeliveryProgress(*activeMission);
        else
            progress = def.durationMs > 0
                ? static_cast<float>(activeMission->elapsedMs) / static_cast<float>(def.durationMs)
                : 1.0f;
        if (progress > 1.0f) progress = 1.0f;

        setEntityVisibility(activeMissionProgressBgId, true);
        setEntityVisibility(activeMissionProgressFillId, true);
        {
            float panelW = getMissionPanelWidth();
            float pbW = panelW - 2.0f * PANEL_PADDING;
            auto fillEnt = ecsRef->getEntity(activeMissionProgressFillId);
            if (fillEnt)
                fillEnt->get<PositionComponent>()->setWidth(pbW * progress);
        }

        // Status text
        if (activeMission->completed)
        {
            setEntityText(activeMissionStatusId, "DONE! Claim your reward.");
            setEntityVisibility(activeMissionStatusId, true);
            setEntityVisibility(activeMissionClaimBtnBgId, true);
            setEntityVisibility(activeMissionClaimBtnTextId, true);
        }
        else if (def.isDeliveryMission())
        {
            std::string statusStr;
            for (const auto& req : def.deliveryRequirements)
            {
                uint16_t have = missionSystem->getDeliveryCount(*activeMission, req);
                statusStr += std::to_string(have) + "/" + std::to_string(req.count) + " ";
            }
            setEntityText(activeMissionStatusId, statusStr);
            setEntityVisibility(activeMissionStatusId, true);
            setEntityVisibility(activeMissionClaimBtnBgId, false);
            setEntityVisibility(activeMissionClaimBtnTextId, false);
        }
        else
        {
            int pct = static_cast<int>(progress * 100.0f);
            setEntityText(activeMissionStatusId, std::to_string(pct) + "%");
            setEntityVisibility(activeMissionStatusId, true);
            setEntityVisibility(activeMissionClaimBtnBgId, false);
            setEntityVisibility(activeMissionClaimBtnTextId, false);
        }

        // Hide available rows
        for (size_t i = 0; i < MAX_MISSION_ROWS; ++i)
        {
            setEntityVisibility(missionRows[i].bgId, false);
            setEntityVisibility(missionRows[i].nameId, false);
            setEntityVisibility(missionRows[i].infoId, false);
            setEntityVisibility(missionRows[i].btnBgId, false);
            setEntityVisibility(missionRows[i].btnTextId, false);
        }
    }
    else
    {
        // No active mission — show available missions list
        setEntityText(missionSectionTitleId, "Missions");

        // Hide active mission entities
        setEntityVisibility(activeMissionNameId, false);
        setEntityVisibility(activeMissionProgressBgId, false);
        setEntityVisibility(activeMissionProgressFillId, false);
        setEntityVisibility(activeMissionStatusId, false);
        setEntityVisibility(activeMissionClaimBtnBgId, false);
        setEntityVisibility(activeMissionClaimBtnTextId, false);

        // Build list of available missions (unlocked, and either repeatable or not completed)
        const auto& defs = missionSystem->getDefs();
        size_t rowIdx = 0;
        for (size_t d = 0; d < defs.size() and rowIdx < MAX_MISSION_ROWS; ++d)
        {
            if (not missionSystem->isMissionUnlocked(d))
                continue;
            if (not defs[d].repeatable and missionSystem->isMissionCompleted(d))
                continue;

            auto& row = missionRows[rowIdx];
            row.defIndex = d;

            setEntityText(row.nameId, defs[d].name);

            std::string info;
            if (defs[d].isDeliveryMission())
            {
                for (const auto& req : defs[d].deliveryRequirements)
                    info += std::to_string(req.count) + "x " + itemRegistry->get(req.itemId).name + " ";
            }
            else
            {
                info = std::to_string(defs[d].durationMs / 1000) + "s  "
                     + std::to_string(defs[d].robotCoreCost) + " Core";
            }
            setEntityText(row.infoId, info);

            bool canStart = missionSystem->canStartMission(d);
            if (canStart)
            {
                setEntityText(row.btnTextId, "START");
                auto btnEnt = ecsRef->getEntity(row.btnBgId);
                if (btnEnt)
                    btnEnt->get<Simple2DObject>()->setColors({60.0f, 120.0f, 60.0f, 200.0f});
            }
            else
            {
                setEntityText(row.btnTextId, "FULL");
                auto btnEnt = ecsRef->getEntity(row.btnBgId);
                if (btnEnt)
                    btnEnt->get<Simple2DObject>()->setColors({80.0f, 80.0f, 80.0f, 200.0f});
            }

            setEntityVisibility(row.bgId, true);
            setEntityVisibility(row.nameId, true);
            setEntityVisibility(row.infoId, true);
            setEntityVisibility(row.btnBgId, true);
            setEntityVisibility(row.btnTextId, true);

            ++rowIdx;
        }

        // Hide unused rows
        for (size_t i = rowIdx; i < MAX_MISSION_ROWS; ++i)
        {
            setEntityVisibility(missionRows[i].bgId, false);
            setEntityVisibility(missionRows[i].nameId, false);
            setEntityVisibility(missionRows[i].infoId, false);
            setEntityVisibility(missionRows[i].btnBgId, false);
            setEntityVisibility(missionRows[i].btnTextId, false);
        }
    }
}

// ---------------------------------------------------------------------------
// Visibility / text helpers
// ---------------------------------------------------------------------------

void DepotUISystem::setEntityVisibility(uint64_t id, bool vis)
{
    if (id == 0)
        return;

    auto ent = ecsRef->getEntity(id);
    if (ent)
        ent->get<PositionComponent>()->setVisibility(vis);
}

void DepotUISystem::setEntityText(uint64_t id, const std::string& text)
{
    if (id == 0)
        return;

    auto ent = ecsRef->getEntity(id);
    if (ent and ent->has<TTFText>())
        ent->get<TTFText>()->setText(text);
}
