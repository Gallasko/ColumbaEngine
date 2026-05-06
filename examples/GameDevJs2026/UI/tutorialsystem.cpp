#include "tutorialsystem.h"

#include "2D/simple2dobject.h"
#include "UI/ttftext.h"

// Indexed by TutorialStep. Entries past Complete are unused.
const TutorialSystem::StepDef TutorialSystem::STEPS[] = {
    {"Gather resources",     "Click a tree or rock until the bar fills to gather wood or stone."},
    {"Open the inventory",   "Press TAB to open your inventory and crafting menu."},
    {"Unlock the pickaxe",   "Open the mission tab and validate \"First Steps\" to unlock the Stone Pickaxe recipe."},
    {"Craft a pickaxe",      "Use the crafting menu to build a Stone Pickaxe."},
    {"Equip the pickaxe",    "Drag the Stone Pickaxe to a hotbar slot."},
    {"Mine an ore",          "With the pickaxe selected, click an ore tile to mine it."},
    {"Unlock the furnace",   "Gather 10 stone, then validate the \"Stone Masonry\" mission to unlock the Furnace recipe."},
    {"Build a furnace",      "Craft a Furnace in the inventory's crafting menu."},
    {"Equip the furnace",    "Drag the Furnace to a hotbar slot."},
    {"Place the furnace",    "Select the furnace and click an empty grid tile to place it."},
    {"Tutorial complete",    ""},
};

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

bool TutorialSystem::recipeOutputs(size_t recipeIndex, ItemId id) const
{
    const Recipe& r = recipeRegistry->get(recipeIndex);
    for (const auto& out : r.outputs)
    {
        if (out.id == id)
            return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// Event handlers
// ---------------------------------------------------------------------------

void TutorialSystem::onEvent(const PlayerGainItemEvent& event)
{
    const auto step = static_cast<TutorialStep>(currentStep);

    if (step == TutorialStep::MineFirstResource &&
        (event.id == WOOD_ID || event.id == STONE_ID))
    {
        pendingAdvance = true;
    }
    else if (step == TutorialStep::MineOre &&
             (event.id == IRON_ORE_ID || event.id == COPPER_ORE_ID || event.id == COAL_ID))
    {
        pendingAdvance = true;
    }
}

void TutorialSystem::onEvent(const InventoryOpenedEvent&)
{
    if (static_cast<TutorialStep>(currentStep) == TutorialStep::OpenInventory)
        pendingAdvance = true;
}

void TutorialSystem::onEvent(const HandCraftCompletedEvent& event)
{
    const auto step = static_cast<TutorialStep>(currentStep);

    if (step == TutorialStep::CraftPickaxe &&
        recipeOutputs(event.recipeIndex, STONE_PICKAXE_ID))
    {
        pendingAdvance = true;
    }
    else if (step == TutorialStep::CraftFurnace &&
             recipeOutputs(event.recipeIndex, FURNACE_ID))
    {
        pendingAdvance = true;
    }
}

void TutorialSystem::onProcessEvent(const SlotDroppedEvent& event)
{
    if (event.category != SlotCategory::Hotbar)
        return;

    const auto step = static_cast<TutorialStep>(currentStep);

    if (step == TutorialStep::MovePickaxeToHotbar &&
        event.item.id == STONE_PICKAXE_ID)
    {
        pendingAdvance = true;
    }
    else if (step == TutorialStep::PlaceFurnaceInHotbar &&
             event.item.id == FURNACE_ID)
    {
        pendingAdvance = true;
    }
}

void TutorialSystem::onEvent(const BuildingPlacedEvent& event)
{
    if (static_cast<TutorialStep>(currentStep) == TutorialStep::PlaceFurnaceOnGrid &&
        event.tileName == "Furnace")
    {
        pendingAdvance = true;
    }
}

void TutorialSystem::onEvent(const AddFact& event)
{
    const auto step = static_cast<TutorialStep>(currentStep);

    // Mission validation always sets the completion fact to bool true via
    // worldFacts->setFact(...). We only care about transitions to truthy.
    if (not event.value.isTrue())
        return;

    if (step == TutorialStep::ValidateFirstSteps && event.name == "mission_tools")
        pendingAdvance = true;
    else if (step == TutorialStep::ValidateStoneMasonry && event.name == "mission_furnace")
        pendingAdvance = true;
}

// ---------------------------------------------------------------------------
// Execute
// ---------------------------------------------------------------------------

void TutorialSystem::execute()
{
    ensureCreated();

    if (pendingAdvance)
    {
        pendingAdvance = false;
        advanceStep();
    }
}

// ---------------------------------------------------------------------------
// Panel creation
// ---------------------------------------------------------------------------

void TutorialSystem::ensureCreated()
{
    if (created) return;
    created = true;

    int loaded = worldFacts->getFact<int>("tutorial_step", 0);

    // Save migration: legacy saves used a 5-step flow. Anyone with a step value
    // beyond the legacy total (or beyond the current Complete sentinel) is
    // treated as already done.
    static constexpr int LEGACY_TOTAL_STEPS = 5;
    if (loaded >= LEGACY_TOTAL_STEPS && loaded < TOTAL_STEPS)
    {
        loaded = TOTAL_STEPS; // -> Complete
        worldFacts->setFact("tutorial_step", loaded);
    }
    if (loaded < 0 || loaded > TOTAL_STEPS)
        loaded = TOTAL_STEPS;

    currentStep = loaded;

    // Loaded facts don't re-fire AddFact, so a player who validated a mission
    // and then quit before advancing the tutorial would be stuck on the
    // matching Validate* step on reload. Skip past those if already met.
    while (currentStep < TOTAL_STEPS)
    {
        const auto step = static_cast<TutorialStep>(currentStep);
        bool skip = false;
        if (step == TutorialStep::ValidateFirstSteps &&
            worldFacts->getFact<bool>("mission_tools", false))
            skip = true;
        else if (step == TutorialStep::ValidateStoneMasonry &&
                 worldFacts->getFact<bool>("mission_furnace", false))
            skip = true;

        if (not skip)
            break;
        ++currentStep;
    }
    worldFacts->setFact("tutorial_step", currentStep);

    if (currentStep >= TOTAL_STEPS)
        return;

    auto bd = makeSimple2DShape(ecsRef, Shape2D::Square, 0.0f, 0.0f,
        constant::Vector4D{15.0f, 15.0f, 25.0f, 200.0f});
    auto bdPos = bd.get<PositionComponent>();
    bdPos->setX(PANEL_X);
    bdPos->setY(PANEL_Y);
    bdPos->setZ(100.0f);
    bdPos->setWidth(PANEL_W);
    bdPos->setHeight(PANEL_H);
    bdPos->setVisibility(true);
    bd.get<ViewportComponent>()->setViewport(UI_VP);
    backdropId = bd.entity->id;

    auto title = makeTTFText(ecsRef,
        PANEL_X + PADDING, PANEL_Y + PADDING, 101.0f,
        FONT_PATH, STEPS[currentStep].title, TITLE_SCALE,
        {255.0f, 210.0f, 80.0f, 255.0f});
    title.get<ViewportComponent>()->setViewport(UI_VP);
    titleId = title.entity->id;

    auto body = makeTTFText(ecsRef,
        PANEL_X + PADDING, PANEL_Y + PADDING + 22.0f, 101.0f,
        FONT_PATH, STEPS[currentStep].body, BODY_SCALE,
        {210.0f, 210.0f, 210.0f, 255.0f});
    body.get<ViewportComponent>()->setViewport(UI_VP);
    bodyId = body.entity->id;

    visible = true;
}

// ---------------------------------------------------------------------------
// Step advancement
// ---------------------------------------------------------------------------

void TutorialSystem::advanceStep()
{
    currentStep++;
    worldFacts->setFact("tutorial_step", currentStep);

    if (currentStep >= TOTAL_STEPS)
    {
        hidePanel();
        return;
    }

    updateContent();
}

void TutorialSystem::updateContent()
{
    auto titleEnt = ecsRef->getEntity(titleId);
    if (titleEnt)
        titleEnt->get<TTFText>()->setText(STEPS[currentStep].title);

    auto bodyEnt = ecsRef->getEntity(bodyId);
    if (bodyEnt)
        bodyEnt->get<TTFText>()->setText(STEPS[currentStep].body);
}

void TutorialSystem::hidePanel()
{
    setEntityVisibility(backdropId, false);
    setEntityVisibility(titleId, false);
    setEntityVisibility(bodyId, false);
    visible = false;
}

void TutorialSystem::setEntityVisibility(uint64_t id, bool vis)
{
    if (id == 0) return;
    auto ent = ecsRef->getEntity(id);
    if (ent)
        ent->get<PositionComponent>()->setVisibility(vis);
}
