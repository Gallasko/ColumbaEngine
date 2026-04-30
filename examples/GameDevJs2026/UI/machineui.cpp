#include "machineui.h"
#include "craftingui.h"
#include "machinedemosystem.h"

#include "2D/simple2dobject.h"
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

    // Check "?" demo button click
    if (machineDemo and demoBtnBgEntityId != 0)
    {
        float btnSize = 20.0f;
        float btnX = getPanelX() + getPanelWidth() - PANEL_PADDING - btnSize;
        float btnY = getPanelY() + PANEL_PADDING;
        if (event.pos.x >= btnX and event.pos.x <= btnX + btnSize
            and event.pos.y >= btnY and event.pos.y <= btnY + btnSize)
        {
            std::string name = openMachineName;
            close();
            machineDemo->openDemo(name);
            return;
        }
    }
}

// ---------------------------------------------------------------------------
// Layout
// ---------------------------------------------------------------------------

float MachineUISystem::getPanelX() const
{
    float invW = InventoryUISystem::COLS * InventoryUISystem::SLOT_SIZE
               + (InventoryUISystem::COLS - 1) * InventoryUISystem::SLOT_SPACING
               + 2.0f * InventoryUISystem::PANEL_PADDING;
    float invX = (screenWidth - invW) * 0.5f;
    return invX - GAP_BETWEEN_PANELS - getPanelWidth();
}

float MachineUISystem::getPanelY() const
{
    return (screenHeight - getPanelHeight()) * 0.5f;
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
    float panelX = getPanelX();
    float panelY = getPanelY();
    float panelW = getPanelWidth();
    float panelH = getPanelHeight();

    // Input column starts at panelX + PANEL_PADDING
    float inputColX = panelX + PANEL_PADDING;
    // Output column starts after input column + arrow gap
    float outputColX = inputColX + SLOT_SIZE + ARROW_GAP;

    float slotsStartY = panelY + PANEL_PADDING + TITLE_H + GAP_AFTER_TITLE;

    float inputSlot0Y = slotsStartY;
    float inputSlot1Y = slotsStartY + SLOT_SIZE + SLOT_SPACING;
    // Output slot is vertically centred between the two input rows
    float outputSlotY = slotsStartY + (SLOT_SIZE + SLOT_SPACING) * 0.5f;

    float barY = slotsStartY + 2.0f * SLOT_SIZE + SLOT_SPACING + GAP_AFTER_SLOTS;

    cachedBarX    = inputColX;
    cachedBarMaxW = panelW - 2.0f * PANEL_PADDING;

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
        const char* initialTitle = openMachineName.empty() ? "Machine" : openMachineName.c_str();
        auto t = makeTTFText(ecsRef,
            panelX + PANEL_PADDING, panelY + PANEL_PADDING + 4.0f, 100.0f,
            FONT_PATH, initialTitle, TITLE_SCALE, {255.0f, 255.0f, 255.0f, 255.0f});
        t.get<ViewportComponent>()->setViewport(UI_VP);
        titleEntityId = t.entity->id;
    }

    // Input slots via SlotSystem
    {
        float inputSlotYs[2] = {inputSlot0Y, inputSlot1Y};
        for (int i = 0; i < 2; ++i)
        {
            auto slotRef = slotSystem->createSlot(
                SlotCategory::Input, static_cast<uint8_t>(i));
            inputSlotEntityIds[i] = slotRef.id;

            auto pos = slotRef.get<PositionComponent>();
            pos->setX(inputColX);
            pos->setY(inputSlotYs[i]);
        }
    }

    // Output slot via SlotSystem
    {
        auto slotRef = slotSystem->createSlot(SlotCategory::Output, 0);
        outputSlotEntityId = slotRef.id;

        auto pos = slotRef.get<PositionComponent>();
        pos->setX(outputColX);
        pos->setY(outputSlotY);
    }

    // Progress bar background
    {
        auto bg = makeSimple2DShape(ecsRef, Shape2D::Square, 0.0f, 0.0f,
            constant::Vector4D{40.0f, 40.0f, 50.0f, 200.0f});
        auto pos = bg.get<PositionComponent>();
        pos->setX(cachedBarX); pos->setY(barY); pos->setZ(98.0f);
        pos->setWidth(cachedBarMaxW); pos->setHeight(PROGRESS_H);
        bg.get<ViewportComponent>()->setViewport(UI_VP);
        progressBgEntityId = bg.entity->id;
    }

    // Progress bar fill
    {
        auto fill = makeSimple2DShape(ecsRef, Shape2D::Square, 0.0f, 0.0f,
            constant::Vector4D{80.0f, 200.0f, 80.0f, 255.0f});
        auto pos = fill.get<PositionComponent>();
        pos->setX(cachedBarX); pos->setY(barY); pos->setZ(99.0f);
        pos->setWidth(0.0f); pos->setHeight(PROGRESS_H);
        fill.get<ViewportComponent>()->setViewport(UI_VP);
        progressFillEntityId = fill.entity->id;
    }

    // "?" demo button (top-right of panel)
    {
        float btnSize = 20.0f;
        float btnX = panelX + panelW - PANEL_PADDING - btnSize;
        float btnY = panelY + PANEL_PADDING;
        auto bg = makeSimple2DShape(ecsRef, Shape2D::Square, 0.0f, 0.0f,
            constant::Vector4D{60.0f, 60.0f, 100.0f, 220.0f});
        auto pos = bg.get<PositionComponent>();
        pos->setX(btnX); pos->setY(btnY); pos->setZ(100.0f);
        pos->setWidth(btnSize); pos->setHeight(btnSize);
        bg.get<ViewportComponent>()->setViewport(UI_VP);
        demoBtnBgEntityId = bg.entity->id;

        auto txt = makeTTFText(ecsRef,
            btnX + 5.0f, btnY + 2.0f, 101.0f,
            FONT_PATH, "?", 0.35f, {255.0f, 255.0f, 255.0f, 255.0f});
        txt.get<ViewportComponent>()->setViewport(UI_VP);
        demoBtnTextEntityId = txt.entity->id;
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
