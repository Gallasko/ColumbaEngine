#include "depotui.h"
#include "craftingui.h"

#include "2D/simple2dobject.h"
#include "2D/position.h"
#include "UI/ttftext.h"
#include "UI/utils.h"

#include <SDL2/SDL.h>

using namespace pg;

namespace
{
    // Hit-test against a button entity's resolved position+size.
    bool hitButtonEntity(pg::EntitySystem* ecs, uint64_t id, float mx, float my)
    {
        if (id == 0)
            return false;
        auto ent = ecs->getEntity(id);
        if (not ent)
            return false;
        auto pos = ent->get<pg::PositionComponent>();
        return mx >= pos->getX() and mx <= pos->getX() + pos->getWidth()
           and my >= pos->getY() and my <= pos->getY() + pos->getHeight();
    }
}

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

    auto* craftingUI  = ecsRef->getSystem<CraftingUISystem>();
    auto* inventoryUI = ecsRef->getSystem<InventoryUISystem>();

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

    auto* slotSystem = ecsRef->getSystem<SlotSystem>();
    if (slotSystem->hasHeldItem())
        slotSystem->cancelHeld();

    setPanelVisibility(false);
    visible = false;
    openDepotX = -1;
    openDepotY = -1;
    depotDataSeenThisOpen = false;

    if (auto* craftingUI = ecsRef->getSystem<CraftingUISystem>())
        craftingUI->setSuppressed(false);

    // Close the companion inventory we opened in open(). closeInventory is
    // idempotent and cascades to crafting via InventoryClosedEvent. The
    // visible-guard at the top of this function prevents the cascade from
    // re-entering close().
    auto* inventoryUI = ecsRef->getSystem<InventoryUISystem>();
    if (inventoryUI and inventoryUI->isOpen())
        inventoryUI->closeInventory();
}

// ---------------------------------------------------------------------------
// Events
// ---------------------------------------------------------------------------

void DepotUISystem::onProcessEvent(const OnSDLScanCode& event)
{
    if (not visible)
        return;
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
    if (not visible)
        return;

    DepotData* depot = ecsRef->getSystem<DepotSystem>()->getDepot(openDepotX, openDepotY);
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

void DepotUISystem::onProcessEvent(const OnMouseClick& event)
{
    if (not visible or event.button != SDL_BUTTON_LEFT)
        return;

    float mx = event.pos.x;
    float my = event.pos.y;

    auto* missionSystem = ecsRef->getSystem<MissionSystem>();

    // Mission section: CLAIM button for active mission
    if (missionSystem->hasActiveMissionAtDepot(openDepotX, openDepotY))
    {
        if (hitButtonEntity(ecsRef, activeMissionClaimBtnBgId, mx, my))
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

            if (hitButtonEntity(ecsRef, row.btnBgId, mx, my))
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
    const float panelW = getPanelWidth();
    const float panelH = getPanelHeight();

    const float slotsTop  = PANEL_PADDING + TITLE_H + GAP_AFTER_TITLE;
    const float outputTop = slotsTop + ROWS * (SLOT_SIZE + SLOT_SPACING) + SECTION_GAP;
    const float outputSlotsTop = outputTop + TITLE_H + GAP_AFTER_TITLE;

    // Anchor target: depot panel sits left of the inventory panel.
    uint64_t invPanelId = 0;
    if (auto* inventoryUI = ecsRef->getSystem<InventoryUISystem>())
        invPanelId = inventoryUI->getBackdropEntityId();
    uint64_t leftAnchorTargetId = invPanelId;
    if (leftAnchorTargetId == 0)
    {
        auto windowEnt = ecsRef->getEntity("__MainWindow");
        if (windowEnt)
            leftAnchorTargetId = windowEnt->id;
    }

    auto* factory = ecsRef->getSystem<PrefabFactoryRegistry>();
    {
        auto panelEnt = factory->build("Panel", PrefabParams{
            {"width", panelW}, {"height", panelH},
            {"r", 20.0f}, {"g", 20.0f}, {"b", 30.0f}, {"a", 220.0f},
            {"z", 97.0f},
            {"viewport", static_cast<int>(UI_VP)},
        });
        backdropEntityId = panelEnt->id;

        auto a = panelEnt->get<UiAnchor>();
        a->setRightAnchor(PosAnchor{leftAnchorTargetId, AnchorType::Left});
        a->setRightMargin(GAP_BETWEEN_PANELS);
        a->setVerticalCenter(PosAnchor{leftAnchorTargetId, AnchorType::VerticalCenter});

        if (auto bgEnt = panelEnt->get<Prefab>()->getEntity("bg"))
            ecsRef->attach<MouseLeftClickComponent>(bgEnt,
                makeCallable<PanelWasClickedEvent>(), MouseStateTrigger::OnPress);
    }

    {
        auto titleEnt = factory->build("Text", PrefabParams{
            {"x", 0.0f}, {"y", 0.0f},
            {"z",        100.0f},
            {"font",     std::string(FONT_PATH)},
            {"text",     std::string("Depot")},
            {"scale",    TITLE_SCALE},
            {"viewport", static_cast<int>(UI_VP)},
        });
        titleEntityId = titleEnt->id;

        auto a = ecsRef->attach<UiAnchor>(titleEnt);
        a->setLeftAnchor(PosAnchor{backdropEntityId, AnchorType::Left});
        a->setTopAnchor(PosAnchor{backdropEntityId, AnchorType::Top});
        a->setLeftMargin(PANEL_PADDING);
        a->setTopMargin(PANEL_PADDING + 4.0f);
    }

    // Input slots
    auto* slotSystem = ecsRef->getSystem<SlotSystem>();
    for (size_t i = 0; i < NUM_SLOTS; ++i)
    {
        size_t col = i % COLS;
        size_t row = i / COLS;

        auto slotRef = slotSystem->createSlot(
            SlotCategory::Input, static_cast<uint8_t>(i));
        inputSlotEntityIds[i] = slotRef.id;

        slotSystem->bindSlotChange(slotRef, [this, i](const ItemStack& s) {
            if (auto* d = ecsRef->getSystem<DepotSystem>()->getDepot(openDepotX, openDepotY))
                d->inventory.getSlot(i) = s;
        });

        auto a = slotRef.get<UiAnchor>();
        a->setLeftAnchor(PosAnchor{backdropEntityId, AnchorType::Left});
        a->setTopAnchor(PosAnchor{backdropEntityId, AnchorType::Top});
        a->setLeftMargin(PANEL_PADDING + col * (SLOT_SIZE + SLOT_SPACING));
        a->setTopMargin(slotsTop + row * (SLOT_SIZE + SLOT_SPACING));
    }

    // Output title
    {
        auto t = makeTTFText(ecsRef, 0.0f, 0.0f, 100.0f,
            FONT_PATH, "Output", TITLE_SCALE, {180.0f, 180.0f, 200.0f, 255.0f});
        t.get<ViewportComponent>()->setViewport(UI_VP);
        outputTitleEntityId = t.entity->id;

        auto a = ecsRef->attach<UiAnchor>(t.entity);
        a->setLeftAnchor(PosAnchor{backdropEntityId, AnchorType::Left});
        a->setTopAnchor(PosAnchor{backdropEntityId, AnchorType::Top});
        a->setLeftMargin(PANEL_PADDING);
        a->setTopMargin(outputTop + 4.0f);
    }

    // Output slots (OutputOnly — pick up only, no dropping)
    for (size_t i = 0; i < NUM_OUTPUT_SLOTS; ++i)
    {
        size_t col = i % COLS;
        size_t row = i / COLS;

        auto slotRef = slotSystem->createSlot(
            SlotCategory::Output, static_cast<uint8_t>(i),
            SlotFlags::OutputOnly,
            DEFAULT_SLOT_SIZE, DEFAULT_ITEM_SIZE,
            {45.0f, 55.0f, 50.0f, 200.0f});
        outputSlotEntityIds[i] = slotRef.id;

        slotSystem->bindSlotChange(slotRef, [this, i](const ItemStack& s) {
            if (auto* d = ecsRef->getSystem<DepotSystem>()->getDepot(openDepotX, openDepotY))
                d->output.getSlot(i) = s;
        });

        auto a = slotRef.get<UiAnchor>();
        a->setLeftAnchor(PosAnchor{backdropEntityId, AnchorType::Left});
        a->setTopAnchor(PosAnchor{backdropEntityId, AnchorType::Top});
        a->setLeftMargin(PANEL_PADDING + col * (SLOT_SIZE + SLOT_SPACING));
        a->setTopMargin(outputSlotsTop + row * (SLOT_SIZE + SLOT_SPACING));
    }

    // --- Mission panel (RIGHT of inventory) ---
    {
        const float rightW = getMissionPanelWidth();
        const float rightH = getMissionPanelHeight();

        // Anchor target: prefer inventory panel, else depot's own backdrop, else main window.
        uint64_t rightAnchorId = invPanelId;
        if (rightAnchorId == 0)
            rightAnchorId = leftAnchorTargetId;

        auto bd = makeSimple2DShape(ecsRef, Shape2D::Square, 0.0f, 0.0f,
            constant::Vector4D{20.0f, 20.0f, 30.0f, 220.0f});
        auto pos = bd.get<PositionComponent>();
        pos->setZ(97.0f);
        pos->setWidth(rightW); pos->setHeight(rightH);
        bd.get<ViewportComponent>()->setViewport(UI_VP);
        missionPanelBackdropId = bd.entity->id;

        auto a = ecsRef->attach<UiAnchor>(bd.entity);
        a->setLeftAnchor(PosAnchor{rightAnchorId, AnchorType::Right});
        a->setLeftMargin(GAP_BETWEEN_PANELS);
        a->setVerticalCenter(PosAnchor{rightAnchorId, AnchorType::VerticalCenter});

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
    DepotData* depot = ecsRef->getSystem<DepotSystem>()->getDepot(openDepotX, openDepotY);

    if (depot)
    {
        auto* slotSystem = ecsRef->getSystem<SlotSystem>();
        for (size_t i = 0; i < NUM_SLOTS; ++i)
            slotSystem->syncSlotVisual(inputSlotEntityIds[i], depot->inventory.getSlot(i));

        for (size_t i = 0; i < NUM_OUTPUT_SLOTS; ++i)
            slotSystem->syncSlotVisual(outputSlotEntityIds[i], depot->output.getSlot(i));
    }

    // Mission section is queried via MissionSystem by depot coords, so it
    // works even if depot data isn't registered yet (e.g. first frame after
    // placement, before BuildingPlacedEvent has been dispatched).
    refreshMissionSection();
}

// ---------------------------------------------------------------------------
// Mission section
// ---------------------------------------------------------------------------

void DepotUISystem::createMissionSection()
{
    const float panelW = getMissionPanelWidth();
    const float pbW = panelW - 2.0f * PANEL_PADDING;
    const uint64_t parentId = missionPanelBackdropId;

    auto anchorTo = [&](EntityRef ent,
                        AnchorType hSide, float hMargin,
                        float topMargin) {
        auto a = ecsRef->attach<UiAnchor>(ent);
        if (hSide == AnchorType::Left)
        {
            a->setLeftAnchor(PosAnchor{parentId, AnchorType::Left});
            a->setLeftMargin(hMargin);
        }
        else
        {
            a->setRightAnchor(PosAnchor{parentId, AnchorType::Right});
            a->setRightMargin(hMargin);
        }
        a->setTopAnchor(PosAnchor{parentId, AnchorType::Top});
        a->setTopMargin(topMargin);
    };

    // Section title — top-left of mission panel
    {
        auto t = makeTTFText(ecsRef, 0.0f, 0.0f, 100.0f,
            FONT_PATH, "Mission", TITLE_SCALE, {180.0f, 180.0f, 200.0f, 255.0f});
        t.get<ViewportComponent>()->setViewport(UI_VP);
        missionSectionTitleId = t.entity->id;
        anchorTo(t.entity, AnchorType::Left, PANEL_PADDING,
                 PANEL_PADDING + 4.0f);
    }

    const float curYBase = PANEL_PADDING + TITLE_H + GAP_AFTER_TITLE;

    // Active mission name
    {
        auto name = makeTTFText(ecsRef, 0.0f, 0.0f, 100.0f,
            FONT_PATH, "", TEXT_SCALE, {255.0f, 255.0f, 255.0f, 255.0f});
        name.get<ViewportComponent>()->setViewport(UI_VP);
        name.get<PositionComponent>()->setVisibility(false);
        activeMissionNameId = name.entity->id;
        anchorTo(name.entity, AnchorType::Left, PANEL_PADDING + 2.0f,
                 curYBase + 2.0f);
    }

    const float pbY = curYBase + MISSION_ROW_H;

    // Active mission progress bar background
    {
        auto bg = makeSimple2DShape(ecsRef, Shape2D::Square, 0.0f, 0.0f,
            constant::Vector4D{50.0f, 50.0f, 60.0f, 200.0f});
        auto pos = bg.get<PositionComponent>();
        pos->setZ(98.5f);
        pos->setWidth(pbW); pos->setHeight(PROGRESS_H);
        pos->setVisibility(false);
        bg.get<ViewportComponent>()->setViewport(UI_VP);
        activeMissionProgressBgId = bg.entity->id;
        anchorTo(bg.entity, AnchorType::Left, PANEL_PADDING,
                 pbY);
    }
    // Active mission progress bar fill
    {
        auto fill = makeSimple2DShape(ecsRef, Shape2D::Square, 0.0f, 0.0f,
            constant::Vector4D{80.0f, 160.0f, 80.0f, 220.0f});
        auto pos = fill.get<PositionComponent>();
        pos->setZ(98.6f);
        pos->setWidth(0.0f); pos->setHeight(PROGRESS_H);
        pos->setVisibility(false);
        fill.get<ViewportComponent>()->setViewport(UI_VP);
        activeMissionProgressFillId = fill.entity->id;
        anchorTo(fill.entity, AnchorType::Left, PANEL_PADDING,
                 pbY);
    }
    // Active mission status text
    {
        auto status = makeTTFText(ecsRef, 0.0f, 0.0f, 100.0f,
            FONT_PATH, "", TEXT_SCALE, {180.0f, 180.0f, 180.0f, 255.0f});
        status.get<ViewportComponent>()->setViewport(UI_VP);
        status.get<PositionComponent>()->setVisibility(false);
        activeMissionStatusId = status.entity->id;
        anchorTo(status.entity, AnchorType::Left, PANEL_PADDING,
                 pbY + PROGRESS_H + 2.0f);
    }

    // CLAIM button for active mission — anchored to right side of panel
    {
        auto bg = makeSimple2DShape(ecsRef, Shape2D::Square, 0.0f, 0.0f,
            constant::Vector4D{60.0f, 100.0f, 180.0f, 200.0f});
        auto pos = bg.get<PositionComponent>();
        pos->setZ(99.0f);
        pos->setWidth(BTN_W); pos->setHeight(BTN_H);
        pos->setVisibility(false);
        bg.get<ViewportComponent>()->setViewport(UI_VP);
        activeMissionClaimBtnBgId = bg.entity->id;
        anchorTo(bg.entity, AnchorType::Right, PANEL_PADDING,
                 pbY + PROGRESS_H + 2.0f);
    }
    {
        auto txt = makeTTFText(ecsRef, 0.0f, 0.0f, 100.0f,
            FONT_PATH, "CLAIM", BTN_TEXT_SCALE, {255.0f, 255.0f, 255.0f, 255.0f});
        txt.get<ViewportComponent>()->setViewport(UI_VP);
        txt.get<PositionComponent>()->setVisibility(false);
        activeMissionClaimBtnTextId = txt.entity->id;
        // Anchor to the claim button bg (left+top with small inset)
        auto a = ecsRef->attach<UiAnchor>(txt.entity);
        a->setLeftAnchor(PosAnchor{activeMissionClaimBtnBgId, AnchorType::Left});
        a->setTopAnchor(PosAnchor{activeMissionClaimBtnBgId, AnchorType::Top});
        a->setLeftMargin(4.0f);
        a->setTopMargin(3.0f);
    }

    // Available mission rows (initially hidden)
    for (size_t i = 0; i < MAX_MISSION_ROWS; ++i)
    {
        const float rowTopMargin = curYBase + i * (MISSION_ROW_H + MISSION_ROW_GAP);
        auto& row = missionRows[i];

        auto bg = makeSimple2DShape(ecsRef, Shape2D::Square, 0.0f, 0.0f,
            constant::Vector4D{35.0f, 35.0f, 45.0f, 180.0f});
        {
            auto pos = bg.get<PositionComponent>();
            pos->setZ(98.0f);
            pos->setWidth(pbW); pos->setHeight(MISSION_ROW_H);
            pos->setVisibility(false);
            bg.get<ViewportComponent>()->setViewport(UI_VP);
            row.bgId = bg.entity->id;
            anchorTo(bg.entity, AnchorType::Left, PANEL_PADDING,
                     rowTopMargin);
        }

        auto name = makeTTFText(ecsRef, 0.0f, 0.0f, 100.0f,
            FONT_PATH, "", TEXT_SCALE, {255.0f, 255.0f, 255.0f, 255.0f});
        name.get<ViewportComponent>()->setViewport(UI_VP);
        name.get<PositionComponent>()->setVisibility(false);
        row.nameId = name.entity->id;
        anchorTo(name.entity, AnchorType::Left, PANEL_PADDING + 2.0f,
                 rowTopMargin + 4.0f);

        auto info = makeTTFText(ecsRef, 0.0f, 0.0f, 100.0f,
            FONT_PATH, "", 0.2f, {150.0f, 150.0f, 170.0f, 255.0f});
        info.get<ViewportComponent>()->setViewport(UI_VP);
        info.get<PositionComponent>()->setVisibility(false);
        row.infoId = info.entity->id;
        anchorTo(info.entity, AnchorType::Left, PANEL_PADDING + 2.0f,
                 rowTopMargin + 14.0f);

        auto btnBg = makeSimple2DShape(ecsRef, Shape2D::Square, 0.0f, 0.0f,
            constant::Vector4D{60.0f, 120.0f, 60.0f, 200.0f});
        {
            auto bpos = btnBg.get<PositionComponent>();
            bpos->setZ(99.0f);
            bpos->setWidth(BTN_W); bpos->setHeight(BTN_H);
            bpos->setVisibility(false);
            btnBg.get<ViewportComponent>()->setViewport(UI_VP);
            row.btnBgId = btnBg.entity->id;
            anchorTo(btnBg.entity, AnchorType::Right, PANEL_PADDING,
                     rowTopMargin + 2.0f);
        }

        auto btnTxt = makeTTFText(ecsRef, 0.0f, 0.0f, 100.0f,
            FONT_PATH, "START", BTN_TEXT_SCALE, {255.0f, 255.0f, 255.0f, 255.0f});
        btnTxt.get<ViewportComponent>()->setViewport(UI_VP);
        btnTxt.get<PositionComponent>()->setVisibility(false);
        row.btnTextId = btnTxt.entity->id;
        // Anchor to its row button bg
        auto a = ecsRef->attach<UiAnchor>(btnTxt.entity);
        a->setLeftAnchor(PosAnchor{row.btnBgId, AnchorType::Left});
        a->setTopAnchor(PosAnchor{row.btnBgId, AnchorType::Top});
        a->setLeftMargin(6.0f);
        a->setTopMargin(3.0f);
    }
}

void DepotUISystem::refreshMissionSection()
{
    auto* missionSystem = ecsRef->getSystem<MissionSystem>();
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
        if (progress > 1.0f)
            progress = 1.0f;

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
    pg::setEntityVisibility(ecsRef, id, vis);
}

void DepotUISystem::setEntityText(uint64_t id, const std::string& text)
{
    if (id == 0)
        return;

    auto ent = ecsRef->getEntity(id);
    if (ent and ent->has<TTFText>())
        ent->get<TTFText>()->setText(text);
}
