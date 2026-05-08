#include "machineui.h"
#include "craftingui.h"
#include "machinedemosystem.h"

#include "2D/simple2dobject.h"
#include "2D/position.h"
#include "UI/ttftext.h"

#include <SDL2/SDL.h>

// ---------------------------------------------------------------------------
// open / close
// ---------------------------------------------------------------------------

void MachineUISystem::open(int gridX, int gridY, const std::string& tileName)
{
    if (visible)
        close();

    openMachineX    = gridX;
    openMachineY    = gridY;
    openMachineName = tileName;
    visible = true;

    // Switch the crafting-recipe panel to show this machine's recipes.
    // Must happen before openInventory() so it takes effect when the panel opens.
    if (craftingUI)
    {
        MachineData* machine = craftingSystem->getMachine(gridX, gridY);
        const Recipe* locked = machine ? machine->lockedRecipe : nullptr;

        craftingUI->setMachineFeedCallback([this](const Recipe& recipe) {
            feedMachineFromPlayer(recipe);
        });

        craftingUI->setMachineSelectCallback([this](const Recipe& recipe) {
            MachineData* m = craftingSystem->getMachine(openMachineX, openMachineY);
            if (m)
                m->lockedRecipe = &recipe;
        });

        craftingUI->setMachineMode(openMachineName, locked);
    }

    if (inventoryUI and not inventoryUI->isOpen())
        inventoryUI->openInventory();

    ensurePanelCreated();
    updateForMachineType();
    setPanelVisibility(true);
    syncAllSlots();
    refreshProgressBar();
}

void MachineUISystem::close()
{
    if (not visible)
        return;

    if (slotSystem->hasHeldItem())
        slotSystem->cancelHeld();

    // Sync all slots back to machine
    MachineData* machine = craftingSystem->getMachine(openMachineX, openMachineY);
    if (machine)
    {
        int numInputs = (openMachineName == "Assembler") ? 2 : 1;

        for (int i = 0; i < numInputs; ++i)
            syncSlotToMachine(static_cast<size_t>(i), true);

        syncSlotToMachine(0, false);
    }

    // Return the crafting-recipe panel to hand-craft mode.
    if (craftingUI)
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

    // Close the companion inventory (cascades to crafting).
    if (inventoryUI and inventoryUI->isOpen())
        inventoryUI->closeInventory();
}

// ---------------------------------------------------------------------------
// Events
// ---------------------------------------------------------------------------

void MachineUISystem::onProcessEvent(const OnSDLScanCode& event)
{
    if (not visible)
        return;
    if (event.key == SDL_SCANCODE_ESCAPE)
        close();
}

void MachineUISystem::onEvent(const InventoryClosedEvent&)
{
    if (visible)
        close();
}

void MachineUISystem::onProcessEvent(const TickEvent&)
{
    if (not visible)
        return;

    MachineData* machine = craftingSystem->getMachine(openMachineX, openMachineY);
    if (not machine)
    {
        close();
        return;
    }

    syncAllSlots();
    refreshProgressBar();
}

void MachineUISystem::onProcessEvent(const SlotPickedUpEvent& event)
{
    if (not visible)
        return;

    for (int i = 0; i < 2; ++i)
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

void MachineUISystem::onProcessEvent(const SlotDroppedEvent& event)
{
    if (not visible)
        return;

    for (int i = 0; i < 2; ++i)
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

void MachineUISystem::onProcessEvent(const OnMouseClick& event)
{
    if (not visible or event.button != SDL_BUTTON_LEFT)
        return;

    // Check "?" demo button click — read live position from the button entity
    // (anchors have resolved by the time clicks land on a visible panel).
    if (machineDemo and demoBtnBgEntityId != 0)
    {
        auto btnEnt = ecsRef->getEntity(demoBtnBgEntityId);
        if (btnEnt)
        {
            auto pos = btnEnt->get<PositionComponent>();
            float btnX = pos->getX();
            float btnY = pos->getY();
            float btnW = pos->getWidth();
            float btnH = pos->getHeight();
            if (event.pos.x >= btnX and event.pos.x <= btnX + btnW
                and event.pos.y >= btnY and event.pos.y <= btnY + btnH)
            {
                std::string name = openMachineName;
                close();
                machineDemo->openDemo(name);
                return;
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Panel creation
// ---------------------------------------------------------------------------

void MachineUISystem::ensurePanelCreated()
{
    if (panelCreated)
        return;
    createPanel();
    setPanelVisibility(false);
    panelCreated = true;
}

void MachineUISystem::setPanelVisibility(bool vis)
{
    setEntityVisibility(backdropEntityId,     vis);
    setEntityVisibility(titleEntityId,        vis);
    setEntityVisibility(progressBgEntityId,   vis);
    setEntityVisibility(progressFillEntityId, vis);
    setEntityVisibility(demoBtnBgEntityId,    vis);
    setEntityVisibility(demoBtnTextEntityId,  vis);

    for (int i = 0; i < 2; ++i)
    {
        auto ent = ecsRef->getEntity(inputSlotEntityIds[i]);
        if (ent)
            ent->get<PositionComponent>()->setVisibility(vis);
    }

    auto outEnt = ecsRef->getEntity(outputSlotEntityId);
    if (outEnt)
        outEnt->get<PositionComponent>()->setVisibility(vis);
}

void MachineUISystem::updateForMachineType()
{
    // Update title
    auto titleEnt = ecsRef->getEntity(titleEntityId);
    if (titleEnt and titleEnt->has<TTFText>())
        titleEnt->get<TTFText>()->setText(openMachineName.c_str());

    // Show/hide 2nd input slot based on machine type
    bool hasSecondInput = (openMachineName == "Assembler");
    auto ent = ecsRef->getEntity(inputSlotEntityIds[1]);
    if (ent)
        ent->get<PositionComponent>()->setVisibility(hasSecondInput);
}

void MachineUISystem::createPanel()
{
    const float panelW = getPanelWidth();
    const float panelH = getPanelHeight();

    // Per-row offsets within the panel (used as anchor margins)
    const float inputColLeft  = PANEL_PADDING;
    const float outputColLeft = PANEL_PADDING + SLOT_SIZE + ARROW_GAP;
    const float slotsTop      = PANEL_PADDING + TITLE_H + GAP_AFTER_TITLE;
    const float inputSlot0Top = slotsTop;
    const float inputSlot1Top = slotsTop + SLOT_SIZE + SLOT_SPACING;
    const float outputSlotTop = slotsTop + (SLOT_SIZE + SLOT_SPACING) * 0.5f;
    const float barTop        = slotsTop + 2.0f * SLOT_SIZE + SLOT_SPACING + GAP_AFTER_SLOTS;

    cachedBarMaxW = panelW - 2.0f * PANEL_PADDING;

    // Anchor target: machine panel sits left of the inventory panel.
    uint64_t anchorTargetId = 0;
    if (inventoryUI)
        anchorTargetId = inventoryUI->getBackdropEntityId();
    if (anchorTargetId == 0)
    {
        auto windowEnt = ecsRef->getEntity("__MainWindow");
        if (windowEnt) anchorTargetId = windowEnt->id;
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
        a->setRightAnchor(PosAnchor{anchorTargetId, AnchorType::Left});
        a->setRightMargin(GAP_BETWEEN_PANELS);
        a->setVerticalCenter(PosAnchor{anchorTargetId, AnchorType::VerticalCenter});

        if (auto bgEnt = panelEnt->get<Prefab>()->getEntity("bg"))
            ecsRef->attach<MouseLeftClickComponent>(bgEnt,
                makeCallable<PanelWasClickedEvent>(), MouseStateTrigger::OnPress);
    }

    {
        const std::string initialTitle = openMachineName.empty() ? "Machine" : openMachineName;
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

    // Input slots
    {
        const float inputSlotTops[2] = {inputSlot0Top, inputSlot1Top};
        for (int i = 0; i < 2; ++i)
        {
            auto slotRef = slotSystem->createSlot(
                SlotCategory::Input, static_cast<uint8_t>(i));
            inputSlotEntityIds[i] = slotRef.id;

            auto a = slotRef.get<UiAnchor>();
            a->setLeftAnchor(PosAnchor{backdropEntityId, AnchorType::Left});
            a->setTopAnchor(PosAnchor{backdropEntityId, AnchorType::Top});
            a->setLeftMargin(inputColLeft);
            a->setTopMargin(inputSlotTops[i]);
        }
    }

    // Output slot
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

void MachineUISystem::syncAllSlots()
{
    MachineData* machine = craftingSystem->getMachine(openMachineX, openMachineY);
    if (not machine)
        return;

    int numInputs = (openMachineName == "Assembler") ? 2 : 1;
    for (int i = 0; i < numInputs; ++i)
        slotSystem->syncSlotVisual(inputSlotEntityIds[i],
            machine->inputSlots.getSlot(static_cast<size_t>(i)));

    slotSystem->syncSlotVisual(outputSlotEntityId, machine->outputSlots.getSlot(0));
}

void MachineUISystem::syncSlotToMachine(size_t slotIndex, bool isInput)
{
    MachineData* machine = craftingSystem->getMachine(openMachineX, openMachineY);
    if (not machine)
        return;

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

void MachineUISystem::refreshProgressBar()
{
    MachineData* machine = craftingSystem->getMachine(openMachineX, openMachineY);
    if (not machine)
        return;

    float progress = 0.0f;
    if (machine->currentRecipe and machine->currentRecipe->craftTimeMs > 0)
    {
        progress = static_cast<float>(machine->craftProgress)
                 / static_cast<float>(machine->currentRecipe->craftTimeMs);
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

void MachineUISystem::feedMachineFromPlayer(const Recipe& recipe)
{
    MachineData* machine = craftingSystem->getMachine(openMachineX, openMachineY);
    if (not machine)
        return;

    // Check player has all ingredients before touching any inventory
    for (const auto& ing : recipe.inputs)
        if (not playerInv->hasItem(ing.id, ing.count))
            return;

    // Transfer each ingredient from player to machine input slots
    for (const auto& ing : recipe.inputs)
    {
        playerInv->getInventory().remove(ing.id, ing.count);
        machine->inputSlots.insert(ing.id, ing.count, *itemRegistry);
    }

    syncAllSlots();
    if (inventoryUI)
        inventoryUI->refreshAllSlots();
}

// ---------------------------------------------------------------------------
// Visibility helper
// ---------------------------------------------------------------------------

void MachineUISystem::setEntityVisibility(uint64_t id, bool vis)
{
    if (id == 0)
        return;
    auto ent = ecsRef->getEntity(id);
    if (ent)
        ent->get<PositionComponent>()->setVisibility(vis);
}
