#include "recipemachineuibase.h"
#include "craftingui.h"
#include "machinedemosystem.h"
#include "machineuihelpers.h"

#include "2D/simple2dobject.h"
#include "2D/position.h"
#include "UI/ttftext.h"

#include <SDL2/SDL.h>

// ---------------------------------------------------------------------------
// open / close
// ---------------------------------------------------------------------------

void RecipeMachineUIBase::open(int gridX, int gridY, const std::string& machineName)
{
    if (visible)
        close();

    openMachineX    = gridX;
    openMachineY    = gridY;
    openMachineName = machineName.empty() ? machineNameLabel : machineName;
    visible = true;

    // Switch the recipe panel into machine mode for this machine.
    if (auto* craftingUI = ecsRef->getSystem<CraftingUISystem>())
    {
        MachineData* machine = ecsRef->getSystem<CraftingSystem>()->getMachine(gridX, gridY);
        const Recipe* locked = machine ? machine->lockedRecipe : nullptr;

        craftingUI->setMachineFeedCallback([this](const Recipe& recipe) {
            feedMachineFromPlayer(recipe);
        });
        craftingUI->setMachineSelectCallback([this](const Recipe& recipe) {
            MachineData* m = ecsRef->getSystem<CraftingSystem>()->getMachine(openMachineX, openMachineY);
            if (m)
                m->lockedRecipe = &recipe;
        });

        craftingUI->setMachineMode(openMachineName, locked);
    }

    ensurePanelCreated();
    updateForMachineType();
    setPanelVisibility(true);
    syncAllSlots();
    refreshProgressBar();
}

void RecipeMachineUIBase::close()
{
    if (not visible)
        return;

    auto* slotSystem = ecsRef->getSystem<SlotSystem>();
    if (slotSystem->hasHeldItem())
        slotSystem->cancelHeld();

    // Sync all slots back to machine
    MachineData* machine = ecsRef->getSystem<CraftingSystem>()->getMachine(openMachineX, openMachineY);
    if (machine)
    {
        for (int i = 0; i < numInputs; ++i)
            syncSlotToMachine(static_cast<size_t>(i), true);
        syncSlotToMachine(0, false);
    }

    if (auto* craftingUI = ecsRef->getSystem<CraftingUISystem>())
    {
        craftingUI->setMachineFeedCallback(nullptr);
        craftingUI->setMachineSelectCallback(nullptr);
        craftingUI->clearMachineMode();
    }

    setPanelVisibility(false);
    visible = false;
    openMachineX = -1;
    openMachineY = -1;
    openMachineName.clear();

    auto* inventoryUI = ecsRef->getSystem<InventoryUISystem>();
    if (inventoryUI and inventoryUI->isOpen())
        inventoryUI->closeInventory();
}

// ---------------------------------------------------------------------------
// Events
// ---------------------------------------------------------------------------

void RecipeMachineUIBase::onProcessEvent(const OnSDLScanCode& event)
{
    if (not visible)
        return;
    if (event.key == SDL_SCANCODE_ESCAPE)
        close();
}

void RecipeMachineUIBase::onEvent(const InventoryClosedEvent&)
{
    if (visible)
        close();
}

void RecipeMachineUIBase::onProcessEvent(const TickEvent&)
{
    if (not visible)
        return;

    MachineData* machine = ecsRef->getSystem<CraftingSystem>()->getMachine(openMachineX, openMachineY);
    if (not machine)
    {
        close();
        return;
    }

    syncAllSlots();
    refreshProgressBar();
}

void RecipeMachineUIBase::onProcessEvent(const SlotPickedUpEvent& event)
{
    if (not visible)
        return;

    for (int i = 0; i < numInputs; ++i)
    {
        if (event.slotEntityId == inputSlotEntityIds[i])
        {
            syncSlotToMachine(static_cast<size_t>(i), true);
            return;
        }
    }
    if (event.slotEntityId == outputSlotEntityId)
        syncSlotToMachine(0, false);
}

void RecipeMachineUIBase::onProcessEvent(const SlotDroppedEvent& event)
{
    if (not visible)
        return;

    for (int i = 0; i < numInputs; ++i)
    {
        if (event.slotEntityId == inputSlotEntityIds[i])
        {
            syncSlotToMachine(static_cast<size_t>(i), true);
            return;
        }
    }
    if (event.slotEntityId == outputSlotEntityId)
        syncSlotToMachine(0, false);
}

void RecipeMachineUIBase::onProcessEvent(const OnMouseClick& event)
{
    if (not visible or event.button != SDL_BUTTON_LEFT)
        return;

    // "?" demo button click — read live position from the button entity.
    if (demoBtnBgEntityId != 0)
    {
        if (pg_machineui::hitButtonEntity(ecsRef, demoBtnBgEntityId,
                                          event.pos.x, event.pos.y))
        {
            std::string name = openMachineName;
            close();
            ecsRef->getSystem<MachineDemoSystem>()->openDemo(name);
        }
    }
}

// ---------------------------------------------------------------------------
// Panel creation
// ---------------------------------------------------------------------------

void RecipeMachineUIBase::ensurePanelCreated()
{
    if (panelCreated)
        return;

    createPanel();
    setPanelVisibility(false);
    panelCreated = true;
}

void RecipeMachineUIBase::setPanelVisibility(bool vis)
{
    pg_machineui::setEntityVisibility(ecsRef, backdropEntityId,     vis);
    pg_machineui::setEntityVisibility(ecsRef, titleEntityId,        vis);
    pg_machineui::setEntityVisibility(ecsRef, progressBgEntityId,   vis);
    pg_machineui::setEntityVisibility(ecsRef, progressFillEntityId, vis);
    pg_machineui::setEntityVisibility(ecsRef, demoBtnBgEntityId,    vis);
    pg_machineui::setEntityVisibility(ecsRef, demoBtnTextEntityId,  vis);

    for (int i = 0; i < numInputs; ++i)
    {
        auto ent = ecsRef->getEntity(inputSlotEntityIds[i]);
        if (ent)
            ent->get<PositionComponent>()->setVisibility(vis);
    }

    auto outEnt = ecsRef->getEntity(outputSlotEntityId);
    if (outEnt)
        outEnt->get<PositionComponent>()->setVisibility(vis);
}

void RecipeMachineUIBase::updateForMachineType()
{
    // Update title
    auto titleEnt = ecsRef->getEntity(titleEntityId);
    if (titleEnt and titleEnt->has<TTFText>())
        titleEnt->get<TTFText>()->setText(openMachineName.c_str());

    // Resize the backdrop + reflow slots based on the actual numInputs so
    // the panel doesn't leave dead space below the input column.
    const float slotsTop   = PANEL_PADDING + TITLE_H + GAP_AFTER_TITLE;
    const float slotsAreaH = numInputs * SLOT_SIZE + (numInputs - 1) * SLOT_SPACING;
    const float barTop     = slotsTop + slotsAreaH + GAP_AFTER_SLOTS;
    const float panelH     = barTop + PROGRESS_H + PANEL_PADDING;

    // Resize backdrop bg
    auto panelEnt = ecsRef->getEntity(backdropEntityId);
    if (panelEnt and panelEnt->has<Prefab>())
    {
        if (auto bgEnt = panelEnt->get<Prefab>()->getEntity("bg"))
            bgEnt->get<PositionComponent>()->setHeight(panelH);
    }

    // Input slot 0 always at top of slot area.
    if (auto slot0Ent = ecsRef->getEntity(inputSlotEntityIds[0]))
        slot0Ent->get<UiAnchor>()->setTopMargin(slotsTop);

    // Output: centred between two inputs for 2-input machines, top-aligned
    // with the single input for 1-input machines.
    const float outputSlotTop = (numInputs == 2) ? slotsTop + (SLOT_SIZE + SLOT_SPACING) * 0.5f : slotsTop;
    if (auto outEnt = ecsRef->getEntity(outputSlotEntityId))
        outEnt->get<UiAnchor>()->setTopMargin(outputSlotTop);

    // Progress bar
    if (auto barBgEnt = ecsRef->getEntity(progressBgEntityId))
        barBgEnt->get<UiAnchor>()->setTopMargin(barTop);

    if (auto barFillEnt = ecsRef->getEntity(progressFillEntityId))
        barFillEnt->get<UiAnchor>()->setTopMargin(barTop);

    // Hide unused input slot for 1-input machines
    if (numInputs < 2)
    {
        auto slot1Ent = ecsRef->getEntity(inputSlotEntityIds[1]);

        if (slot1Ent)
            slot1Ent->get<PositionComponent>()->setVisibility(false);
    }
}

void RecipeMachineUIBase::createPanel()
{
    const float panelW = getPanelWidth();
    const float panelH = getPanelHeight();

    const float inputColLeft  = PANEL_PADDING;
    const float outputColLeft = PANEL_PADDING + SLOT_SIZE + ARROW_GAP;
    const float slotsTop      = PANEL_PADDING + TITLE_H + GAP_AFTER_TITLE;
    const float inputSlot0Top = slotsTop;
    const float inputSlot1Top = slotsTop + SLOT_SIZE + SLOT_SPACING;
    const float outputSlotTop = slotsTop + (SLOT_SIZE + SLOT_SPACING) * 0.5f;
    const float barTop        = slotsTop + 2.0f * SLOT_SIZE + SLOT_SPACING + GAP_AFTER_SLOTS;

    cachedBarMaxW = panelW - 2.0f * PANEL_PADDING;

    uint64_t anchorTargetId = pg_machineui::resolveLeftPanelAnchor(ecsRef, ecsRef->getSystem<InventoryUISystem>());

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
        a->setRightAnchor(PosAnchor{anchorTargetId, AnchorType::Left});
        a->setRightMargin(GAP_BETWEEN_PANELS);
        a->setVerticalCenter(PosAnchor{anchorTargetId, AnchorType::VerticalCenter});

        if (auto bgEnt = panelEnt->get<Prefab>()->getEntity("bg"))
            ecsRef->attach<MouseLeftClickComponent>(bgEnt, makeCallable<PanelWasClickedEvent>(), MouseStateTrigger::OnPress);
    }

    {
        const std::string initialTitle = openMachineName.empty() ? machineNameLabel : openMachineName;
        auto titleEnt = factory->build("Text", PrefabParams{
            {"x", 0.0f}, {"y", 0.0f},
            {"z",        100.0f},
            {"font",     std::string(FONT_PATH)},
            {"text",     initialTitle},
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

    auto* slotSystem = ecsRef->getSystem<SlotSystem>();

    // Always create both input slot anchors (slot 1 is hidden for 1-input
    // machines via updateForMachineType).
    {
        const float inputSlotTops[2] = {inputSlot0Top, inputSlot1Top};
        for (int i = 0; i < 2; ++i)
        {
            auto slotRef = slotSystem->createSlot(SlotCategory::Input, static_cast<uint8_t>(i));
            inputSlotEntityIds[i] = slotRef.id;

            auto a = slotRef.get<UiAnchor>();
            a->setLeftAnchor(PosAnchor{backdropEntityId, AnchorType::Left});
            a->setTopAnchor(PosAnchor{backdropEntityId, AnchorType::Top});
            a->setLeftMargin(inputColLeft);
            a->setTopMargin(inputSlotTops[i]);
        }
    }

    {
        auto slotRef = slotSystem->createSlot(SlotCategory::Output, 0);
        outputSlotEntityId = slotRef.id;

        auto a = slotRef.get<UiAnchor>();
        a->setLeftAnchor(PosAnchor{backdropEntityId, AnchorType::Left});
        a->setTopAnchor(PosAnchor{backdropEntityId, AnchorType::Top});
        a->setLeftMargin(outputColLeft);
        a->setTopMargin(outputSlotTop);
    }

    // Progress bar background
    {
        auto bg = makeSimple2DShape(ecsRef, Shape2D::Square, 0.0f, 0.0f,
            constant::Vector4D{40.0f, 40.0f, 50.0f, 200.0f});
        auto pos = bg.get<PositionComponent>();
        pos->setZ(98.0f);
        pos->setWidth(cachedBarMaxW); pos->setHeight(PROGRESS_H);
        bg.get<ViewportComponent>()->setViewport(UI_VP);
        progressBgEntityId = bg.entity->id;

        auto a = ecsRef->attach<UiAnchor>(bg.entity);
        a->setLeftAnchor(PosAnchor{backdropEntityId, AnchorType::Left});
        a->setTopAnchor(PosAnchor{backdropEntityId, AnchorType::Top});
        a->setLeftMargin(PANEL_PADDING);
        a->setTopMargin(barTop);
    }

    // Progress bar fill
    {
        auto fill = makeSimple2DShape(ecsRef, Shape2D::Square, 0.0f, 0.0f,
            constant::Vector4D{80.0f, 200.0f, 80.0f, 255.0f});
        auto pos = fill.get<PositionComponent>();
        pos->setZ(99.0f);
        pos->setWidth(0.0f); pos->setHeight(PROGRESS_H);
        fill.get<ViewportComponent>()->setViewport(UI_VP);
        progressFillEntityId = fill.entity->id;

        auto a = ecsRef->attach<UiAnchor>(fill.entity);
        a->setLeftAnchor(PosAnchor{backdropEntityId, AnchorType::Left});
        a->setTopAnchor(PosAnchor{backdropEntityId, AnchorType::Top});
        a->setLeftMargin(PANEL_PADDING);
        a->setTopMargin(barTop);
    }

    // "?" demo button (top-right of panel)
    {
        const float btnSize = 20.0f;
        auto bg = makeSimple2DShape(ecsRef, Shape2D::Square, 0.0f, 0.0f,
            constant::Vector4D{60.0f, 60.0f, 100.0f, 220.0f});
        auto pos = bg.get<PositionComponent>();
        pos->setZ(100.0f);
        pos->setWidth(btnSize); pos->setHeight(btnSize);
        bg.get<ViewportComponent>()->setViewport(UI_VP);
        demoBtnBgEntityId = bg.entity->id;

        auto a = ecsRef->attach<UiAnchor>(bg.entity);
        a->setRightAnchor(PosAnchor{backdropEntityId, AnchorType::Right});
        a->setTopAnchor(PosAnchor{backdropEntityId, AnchorType::Top});
        a->setRightMargin(PANEL_PADDING);
        a->setTopMargin(PANEL_PADDING);

        auto txt = makeTTFText(ecsRef, 0.0f, 0.0f, 101.0f,
            FONT_PATH, "?", 0.35f, {255.0f, 255.0f, 255.0f, 255.0f});
        txt.get<ViewportComponent>()->setViewport(UI_VP);
        demoBtnTextEntityId = txt.entity->id;

        auto ta = ecsRef->attach<UiAnchor>(txt.entity);
        ta->setLeftAnchor(PosAnchor{demoBtnBgEntityId, AnchorType::Left});
        ta->setTopAnchor(PosAnchor{demoBtnBgEntityId, AnchorType::Top});
        ta->setLeftMargin(5.0f);
        ta->setTopMargin(2.0f);
    }
}

// ---------------------------------------------------------------------------
// Sync helpers
// ---------------------------------------------------------------------------

void RecipeMachineUIBase::syncAllSlots()
{
    MachineData* machine = ecsRef->getSystem<CraftingSystem>()->getMachine(openMachineX, openMachineY);
    if (not machine)
        return;

    auto* slotSystem = ecsRef->getSystem<SlotSystem>();
    for (int i = 0; i < numInputs; ++i)
        slotSystem->syncSlotVisual(inputSlotEntityIds[i],
            machine->inputSlots.getSlot(static_cast<size_t>(i)));

    slotSystem->syncSlotVisual(outputSlotEntityId, machine->outputSlots.getSlot(0));
}

void RecipeMachineUIBase::syncSlotToMachine(size_t slotIndex, bool isInput)
{
    MachineData* machine = ecsRef->getSystem<CraftingSystem>()->getMachine(openMachineX, openMachineY);
    if (not machine)
        return;

    auto* slotSystem = ecsRef->getSystem<SlotSystem>();
    if (isInput)
    {
        auto* sc = slotSystem->getSlotComponent(inputSlotEntityIds[slotIndex]);
        if (sc)
            machine->inputSlots.getSlot(slotIndex) = sc->stack;
    }
    else
    {
        auto* sc = slotSystem->getSlotComponent(outputSlotEntityId);
        if (sc)
            machine->outputSlots.getSlot(0) = sc->stack;
    }
}

void RecipeMachineUIBase::refreshProgressBar()
{
    MachineData* machine = ecsRef->getSystem<CraftingSystem>()->getMachine(openMachineX, openMachineY);
    if (not machine)
        return;

    float progress = 0.0f;
    if (machine->currentRecipe and machine->currentRecipe->craftTimeMs > 0)
    {
        progress = static_cast<float>(machine->craftProgress) / static_cast<float>(machine->currentRecipe->craftTimeMs);
        if (progress > 1.0f)
            progress = 1.0f;
    }

    auto fillEnt = ecsRef->getEntity(progressFillEntityId);
    if (fillEnt)
        fillEnt->get<PositionComponent>()->setWidth(cachedBarMaxW * progress);
}

// ---------------------------------------------------------------------------
// Feed machine from player inventory (double-click callback)
// ---------------------------------------------------------------------------

void RecipeMachineUIBase::feedMachineFromPlayer(const Recipe& recipe)
{
    MachineData* machine = ecsRef->getSystem<CraftingSystem>()->getMachine(openMachineX, openMachineY);
    if (not machine)
        return;

    auto* playerInv = ecsRef->getSystem<PlayerInventorySystem>();
    for (const auto& ing : recipe.inputs)
        if (not playerInv->hasItem(ing.id, ing.count))
            return;

    for (const auto& ing : recipe.inputs)
    {
        playerInv->getInventory().remove(ing.id, ing.count);
        machine->inputSlots.insert(ing.id, ing.count, *itemRegistry);
    }

    syncAllSlots();
    if (auto* inventoryUI = ecsRef->getSystem<InventoryUISystem>())
        inventoryUI->refreshAllSlots();
}
