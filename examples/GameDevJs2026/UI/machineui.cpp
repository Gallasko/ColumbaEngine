#include "machineui.h"
#include "craftingui.h"
#include "machinedemosystem.h"

#include "2D/simple2dobject.h"
#include "2D/texture.h"
#include "UI/ttftext.h"

#include <SDL2/SDL.h>

// ---------------------------------------------------------------------------
// isClickOnPanel
// ---------------------------------------------------------------------------

bool MachineUISystem::isClickOnPanel(float x, float y) const
{
    if (not visible) return false;
    float px = getPanelX();
    float py = getPanelY();
    return x >= px and x <= px + getPanelWidth()
       and y >= py and y <= py + getPanelHeight();
}

// ---------------------------------------------------------------------------
// open / close
// ---------------------------------------------------------------------------

void MachineUISystem::open(int gridX, int gridY, uint16_t tileId)
{
    if (visible)
        close();

    openMachineX   = gridX;
    openMachineY   = gridY;
    openMachineType = tileId;
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
            if (m) m->lockedRecipe = &recipe;
        });
        craftingUI->setMachineMode(tileId, locked);
    }

    if (inventoryUI and not inventoryUI->isOpen())
        inventoryUI->openInventory();

    inventoryUI->setExternalClickCheck([this](float x, float y) {
        return isClickOnPanel(x, y);
    });

    ensurePanelCreated();
    updateForMachineType();
    setPanelVisibility(true);
    refreshAllSlots();
    refreshProgressBar();
}

void MachineUISystem::close()
{
    if (inventoryUI and inventoryUI->hasHeldItem())
        inventoryUI->cancelHeld();

    inventoryUI->setExternalClickCheck(nullptr);

    // Return the crafting-recipe panel to hand-craft mode.
    if (craftingUI)
    {
        craftingUI->setMachineFeedCallback(nullptr);
        craftingUI->setMachineSelectCallback(nullptr);
        craftingUI->clearMachineMode();
    }

    // Hide item/count entities
    for (int i = 0; i < 2; ++i)
    {
        setEntityVisibility(inputItemEntityId[i],  false);
        setEntityVisibility(inputCountEntityId[i], false);
    }
    setEntityVisibility(outputItemEntityId,  false);
    setEntityVisibility(outputCountEntityId, false);

    setPanelVisibility(false);
    visible = false;
    openMachineX   = -1;
    openMachineY   = -1;
    openMachineType = 0;
}

// ---------------------------------------------------------------------------
// Events
// ---------------------------------------------------------------------------

void MachineUISystem::onProcessEvent(const OnSDLScanCode& event)
{
    if (not visible) return;
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
    if (not visible) return;

    MachineData* machine = craftingSystem->getMachine(openMachineX, openMachineY);
    if (not machine)
    {
        close();
        return;
    }

    refreshAllSlots();
    refreshProgressBar();
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
            uint16_t tileId = openMachineType;
            close();
            machineDemo->openDemo(tileId);
            return;
        }
    }

    MachineData* machine = craftingSystem->getMachine(openMachineX, openMachineY);
    if (not machine)
        return;

    // Input slots
    int numInputs = (openMachineType == 6) ? 2 : 1;
    for (int i = 0; i < numInputs; ++i)
    {
        if (isClickOnInputSlot(static_cast<size_t>(i), event.pos.x, event.pos.y))
        {
            auto& slot = machine->inputSlots.getSlot(static_cast<size_t>(i));
            if (inventoryUI->hasHeldItem())
                inventoryUI->dropOnExternal(slot);
            else
                inventoryUI->pickUpFromExternal(slot);
            refreshAllSlots();
            return;
        }
    }

    // Output slot
    if (isClickOnOutputSlot(event.pos.x, event.pos.y))
    {
        auto& slot = machine->outputSlots.getSlot(0);
        if (inventoryUI->hasHeldItem())
            inventoryUI->dropOnExternal(slot);
        else
            inventoryUI->pickUpFromExternal(slot);
        refreshAllSlots();
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
    if (panelCreated) return;
    createPanel();
    setPanelVisibility(false);
    panelCreated = true;
}

void MachineUISystem::setPanelVisibility(bool vis)
{
    setEntityVisibility(backdropEntityId,     vis);
    setEntityVisibility(titleEntityId,        vis);
    setEntityVisibility(inputSlotBgEntityId[0], vis);
    setEntityVisibility(inputSlotBgEntityId[1], vis);
    setEntityVisibility(outputSlotBgEntityId, vis);
    setEntityVisibility(progressBgEntityId,   vis);
    setEntityVisibility(progressFillEntityId, vis);
    setEntityVisibility(demoBtnBgEntityId,    vis);
    setEntityVisibility(demoBtnTextEntityId,  vis);
}

void MachineUISystem::updateForMachineType()
{
    // Update title
    auto titleEnt = ecsRef->getEntity(titleEntityId);
    if (titleEnt and titleEnt->has<TTFText>())
    {
        const char* name = (openMachineType == 5) ? "Furnace" : "Assembler";
        titleEnt->get<TTFText>()->setText(name);
    }

    // Show/hide 2nd input slot based on machine type
    bool hasSecondInput = (openMachineType == 6);
    setEntityVisibility(inputSlotBgEntityId[1], hasSecondInput);
    setEntityVisibility(inputItemEntityId[1],   false); // refreshAllSlots will show if needed
    setEntityVisibility(inputCountEntityId[1],  false);
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

    // Cache layout for hit testing
    cachedInputSlotX[0] = inputColX;
    cachedInputSlotY[0] = inputSlot0Y;
    cachedInputSlotX[1] = inputColX;
    cachedInputSlotY[1] = inputSlot1Y;
    cachedOutputSlotX   = outputColX;
    cachedOutputSlotY   = outputSlotY;
    cachedBarX          = inputColX;
    cachedBarMaxW       = panelW - 2.0f * PANEL_PADDING;

    // Backdrop
    {
        auto bd = makeSimple2DShape(ecsRef, Shape2D::Square, 0.0f, 0.0f,
            constant::Vector4D{20.0f, 20.0f, 30.0f, 220.0f});
        auto pos = bd.get<PositionComponent>();
        pos->setX(panelX); pos->setY(panelY); pos->setZ(97.0f);
        pos->setWidth(panelW); pos->setHeight(panelH);
        bd.get<Simple2DObject>()->setViewport(UI_VP);
        backdropEntityId = bd.entity->id;
    }

    // Title
    {
        auto t = makeTTFText(ecsRef,
            panelX + PANEL_PADDING, panelY + PANEL_PADDING + 4.0f, 100.0f,
            FONT_PATH, "Machine", TITLE_SCALE, {255.0f, 255.0f, 255.0f, 255.0f});
        t.get<TTFText>()->setViewport(UI_VP);
        titleEntityId = t.entity->id;
    }

    // Input slot backgrounds
    for (int i = 0; i < 2; ++i)
    {
        float sy = (i == 0) ? inputSlot0Y : inputSlot1Y;
        auto slotBg = makeSimple2DShape(ecsRef, Shape2D::Square, 0.0f, 0.0f,
            constant::Vector4D{50.0f, 50.0f, 60.0f, 200.0f});
        auto pos = slotBg.get<PositionComponent>();
        pos->setX(inputColX); pos->setY(sy); pos->setZ(98.0f);
        pos->setWidth(SLOT_SIZE); pos->setHeight(SLOT_SIZE);
        slotBg.get<Simple2DObject>()->setViewport(UI_VP);
        inputSlotBgEntityId[i] = slotBg.entity->id;
    }

    // Output slot background
    {
        auto slotBg = makeSimple2DShape(ecsRef, Shape2D::Square, 0.0f, 0.0f,
            constant::Vector4D{50.0f, 50.0f, 60.0f, 200.0f});
        auto pos = slotBg.get<PositionComponent>();
        pos->setX(outputColX); pos->setY(outputSlotY); pos->setZ(98.0f);
        pos->setWidth(SLOT_SIZE); pos->setHeight(SLOT_SIZE);
        slotBg.get<Simple2DObject>()->setViewport(UI_VP);
        outputSlotBgEntityId = slotBg.entity->id;
    }

    // Progress bar background
    {
        auto bg = makeSimple2DShape(ecsRef, Shape2D::Square, 0.0f, 0.0f,
            constant::Vector4D{40.0f, 40.0f, 50.0f, 200.0f});
        auto pos = bg.get<PositionComponent>();
        pos->setX(cachedBarX); pos->setY(barY); pos->setZ(98.0f);
        pos->setWidth(cachedBarMaxW); pos->setHeight(PROGRESS_H);
        bg.get<Simple2DObject>()->setViewport(UI_VP);
        progressBgEntityId = bg.entity->id;
    }

    // Progress bar fill
    {
        auto fill = makeSimple2DShape(ecsRef, Shape2D::Square, 0.0f, 0.0f,
            constant::Vector4D{80.0f, 200.0f, 80.0f, 255.0f});
        auto pos = fill.get<PositionComponent>();
        pos->setX(cachedBarX); pos->setY(barY); pos->setZ(99.0f);
        pos->setWidth(0.0f); pos->setHeight(PROGRESS_H);
        fill.get<Simple2DObject>()->setViewport(UI_VP);
        progressFillEntityId = fill.entity->id;
    }

    // Item texture + count text entities for each slot
    auto makeItemAndCount = [&](float slotX, float slotY,
                                uint64_t& outItemId, uint64_t& outCountId)
    {
        float offset = (SLOT_SIZE - ITEM_SIZE) * 0.5f;
        auto itemTex = make2DTexture(ecsRef, ITEM_SIZE, ITEM_SIZE, "NoneIcon");
        auto iPos = itemTex.get<PositionComponent>();
        iPos->setX(slotX + offset); iPos->setY(slotY + offset); iPos->setZ(99.0f);
        iPos->setVisibility(false);
        itemTex.get<Texture2DComponent>()->setViewport(UI_VP);
        outItemId = itemTex.entity->id;

        auto countTxt = makeTTFText(ecsRef,
            slotX + SLOT_SIZE - 4.0f, slotY + SLOT_SIZE - 4.0f, 100.0f,
            FONT_PATH, "", TEXT_SCALE, {255.0f, 255.0f, 255.0f, 255.0f});
        countTxt.get<PositionComponent>()->setVisibility(false);
        countTxt.get<TTFText>()->setViewport(UI_VP);
        outCountId = countTxt.entity->id;
    };

    makeItemAndCount(inputColX, inputSlot0Y, inputItemEntityId[0], inputCountEntityId[0]);
    makeItemAndCount(inputColX, inputSlot1Y, inputItemEntityId[1], inputCountEntityId[1]);
    makeItemAndCount(outputColX, outputSlotY, outputItemEntityId, outputCountEntityId);

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
        bg.get<Simple2DObject>()->setViewport(UI_VP);
        demoBtnBgEntityId = bg.entity->id;

        auto txt = makeTTFText(ecsRef,
            btnX + 5.0f, btnY + 2.0f, 101.0f,
            FONT_PATH, "?", 0.35f, {255.0f, 255.0f, 255.0f, 255.0f});
        txt.get<TTFText>()->setViewport(UI_VP);
        demoBtnTextEntityId = txt.entity->id;
    }
}

// ---------------------------------------------------------------------------
// Refresh helpers
// ---------------------------------------------------------------------------

void MachineUISystem::refreshItemSlotDisplay(uint64_t itemEntId, uint64_t countEntId,
                                              float /*slotX*/, float /*slotY*/,
                                              const ItemStack& stack)
{
    if (stack.isEmpty())
    {
        setEntityVisibility(itemEntId,  false);
        setEntityVisibility(countEntId, false);
        return;
    }

    const auto& def = itemRegistry->get(stack.id);
    auto itemEnt = ecsRef->getEntity(itemEntId);
    if (itemEnt)
    {
        itemEnt->get<Texture2DComponent>()->setTexture(def.textureName);
        itemEnt->get<PositionComponent>()->setVisibility(true);
    }

    if (stack.count > 1)
    {
        auto countEnt = ecsRef->getEntity(countEntId);
        if (countEnt)
        {
            countEnt->get<TTFText>()->setText(std::to_string(stack.count));
            countEnt->get<PositionComponent>()->setVisibility(true);
        }
    }
    else
    {
        setEntityVisibility(countEntId, false);
    }
}

void MachineUISystem::refreshAllSlots()
{
    MachineData* machine = craftingSystem->getMachine(openMachineX, openMachineY);
    if (not machine) return;

    int numInputs = (openMachineType == 6) ? 2 : 1;
    for (int i = 0; i < numInputs; ++i)
    {
        const auto& stack = machine->inputSlots.getSlot(static_cast<size_t>(i));
        refreshItemSlotDisplay(inputItemEntityId[i], inputCountEntityId[i],
                               cachedInputSlotX[i], cachedInputSlotY[i], stack);
    }

    const auto& outStack = machine->outputSlots.getSlot(0);
    refreshItemSlotDisplay(outputItemEntityId, outputCountEntityId,
                           cachedOutputSlotX, cachedOutputSlotY, outStack);
}

void MachineUISystem::refreshProgressBar()
{
    MachineData* machine = craftingSystem->getMachine(openMachineX, openMachineY);
    if (not machine) return;

    float progress = 0.0f;
    if (machine->currentRecipe and machine->currentRecipe->craftTimeMs > 0)
    {
        progress = static_cast<float>(machine->craftProgress)
                 / static_cast<float>(machine->currentRecipe->craftTimeMs);
        if (progress > 1.0f) progress = 1.0f;
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
    if (not machine) return;

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

    refreshAllSlots();
    if (inventoryUI)
        inventoryUI->refreshAllSlots();
}

// ---------------------------------------------------------------------------
// Visibility helper
// ---------------------------------------------------------------------------

void MachineUISystem::setEntityVisibility(uint64_t id, bool vis)
{
    if (id == 0) return;
    auto ent = ecsRef->getEntity(id);
    if (ent)
        ent->get<PositionComponent>()->setVisibility(vis);
}
