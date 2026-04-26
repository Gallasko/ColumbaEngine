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
    {"Inventory opened!",   "Click the mission button at the top-right to view available missions."},
    {"Good job!",           "Complete missions to unlock crafting recipes!"},
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
}

void TutorialSystem::onEvent(const InventoryOpenedEvent&)
{
    if (currentStep == 2)
        pendingAdvance = true;
}

void TutorialSystem::onEvent(const MissionUIOpenedEvent&)
{
    if (currentStep == 3)
        pendingAdvance = true;
}

void TutorialSystem::onEvent(const HandCraftCompletedEvent&)
{
    // No longer used for tutorial advancement.
}

void TutorialSystem::onEvent(const BuildingPlacedEvent&)
{
    // No longer used for tutorial advancement.
}

void TutorialSystem::onEvent(const TickEvent& event)
{
    // Auto-advance timer for steps 3 and 4
    if (autoAdvanceTimer > 0.0f)
    {
        autoAdvanceTimer -= event.tick;
        if (autoAdvanceTimer <= 0.0f)
        {
            autoAdvanceTimer = -1.0f;
            pendingAdvance = true;
        }
    }
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
    bd.get<ViewportComponent>()->setViewport(UI_VP);
    backdropId = bd.entity->id;

    // Title text (gold)
    auto title = makeTTFText(ecsRef,
        PANEL_X + PADDING, PANEL_Y + PADDING, 101.0f,
        FONT_PATH, STEPS[currentStep].title, TITLE_SCALE,
        {255.0f, 210.0f, 80.0f, 255.0f});
    title.get<ViewportComponent>()->setViewport(UI_VP);
    titleId = title.entity->id;

    // Body text (light grey)
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

    // Step 3 (open mission tab) waits for explicit user action — no timeout.
    // Step 4 (final "Good job!") auto-advances to dismiss the tutorial.
    if (currentStep == 4)
        autoAdvanceTimer = AUTO_ADVANCE_MS;
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
