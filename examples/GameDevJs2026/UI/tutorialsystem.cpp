#include "tutorialsystem.h"

#include "2D/simple2dobject.h"
#include "UI/ttftext.h"

#include <SDL2/SDL.h>

// ---------------------------------------------------------------------------
// Step definitions
// ---------------------------------------------------------------------------

const TutorialSystem::StepDef TutorialSystem::STEPS[] = {
    {"Welcome!",            "Use WASD to move the camera and scroll to zoom."},
    {"Great!",              "Click on a tree or rock to gather resources."},
    {"Resources gathered!", "Press TAB to open your inventory."},
    {"Inventory opened!",   "Craft a Stone Pickaxe from the crafting menu."},
    {"Pickaxe crafted!",    "Equip it in hotbar. Mine coal or iron ore!"},
    {"Ore mined!",          "Craft a Furnace and place it on the ground."},
    {"Furnace placed!",     "Craft an Assembler (in Mach tab) and place it down."},
    {"Assembler placed!",   "Place a Miner on an ore deposit."},
    {"Miner placed!",       "Connect machines with Inserters."},
};

// ---------------------------------------------------------------------------
// Event handlers
// ---------------------------------------------------------------------------

void TutorialSystem::onEvent(const OnSDLScanCode& event)
{
    if (currentStep == 0)
    {
        if (event.key == SDL_SCANCODE_W || event.key == SDL_SCANCODE_A ||
            event.key == SDL_SCANCODE_S || event.key == SDL_SCANCODE_D)
            pendingAdvance = true;
    }
}

void TutorialSystem::onEvent(const PlayerGainItemEvent& event)
{
    // Step 1: gather wood or stone
    if (currentStep == 1 && (event.id == 15 || event.id == 4))
        pendingAdvance = true;

    // Step 4: mine coal or iron ore
    if (currentStep == 4 && (event.id == 1 || event.id == 3))
        pendingAdvance = true;
}

void TutorialSystem::onEvent(const InventoryOpenedEvent&)
{
    if (currentStep == 2)
        pendingAdvance = true;
}

void TutorialSystem::onEvent(const HandCraftCompletedEvent& event)
{
    if (currentStep != 3)
        return;

    if (event.recipeIndex < recipeRegistry->count())
    {
        const auto& recipe = recipeRegistry->get(event.recipeIndex);
        for (const auto& output : recipe.outputs)
        {
            if (output.id == 29) // Stone Pickaxe
            {
                pendingAdvance = true;
                break;
            }
        }
    }
}

void TutorialSystem::onEvent(const BuildingPlacedEvent& event)
{
    if (currentStep == 5 && event.tileName == "Furnace")
        pendingAdvance = true;
    if (currentStep == 6 && event.tileName == "Assembler")
        pendingAdvance = true;
    if (currentStep == 7 && event.tileName == "Miner")
        pendingAdvance = true;
    if (currentStep == 8 && event.tileName == "Inserter")
        pendingAdvance = true;
}

void TutorialSystem::onEvent(const TickEvent&)
{
    // Reserved for future use (e.g. initial delay before showing panel).
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

    currentStep = worldFacts->getFact<int>("tutorial_step", 0);

    if (currentStep >= TOTAL_STEPS)
        return;

    // Backdrop
    auto bd = makeSimple2DShape(ecsRef, Shape2D::Square, 0.0f, 0.0f,
        constant::Vector4D{15.0f, 15.0f, 25.0f, 200.0f});
    auto bdPos = bd.get<PositionComponent>();
    bdPos->setX(PANEL_X);
    bdPos->setY(PANEL_Y);
    bdPos->setZ(100.0f);
    bdPos->setWidth(PANEL_W);
    bdPos->setHeight(PANEL_H);
    bdPos->setVisibility(true);
    bd.get<Simple2DObject>()->setViewport(UI_VP);
    backdropId = bd.entity->id;

    // Title text (gold)
    auto title = makeTTFText(ecsRef,
        PANEL_X + PADDING, PANEL_Y + PADDING, 101.0f,
        FONT_PATH, STEPS[currentStep].title, TITLE_SCALE,
        {255.0f, 210.0f, 80.0f, 255.0f});
    title.get<TTFText>()->setViewport(UI_VP);
    titleId = title.entity->id;

    // Body text (light grey)
    auto body = makeTTFText(ecsRef,
        PANEL_X + PADDING, PANEL_Y + PADDING + 22.0f, 101.0f,
        FONT_PATH, STEPS[currentStep].body, BODY_SCALE,
        {210.0f, 210.0f, 210.0f, 255.0f});
    body.get<TTFText>()->setViewport(UI_VP);
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

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

void TutorialSystem::setEntityVisibility(uint64_t id, bool vis)
{
    if (id == 0) return;
    auto ent = ecsRef->getEntity(id);
    if (ent)
        ent->get<PositionComponent>()->setVisibility(vis);
}
