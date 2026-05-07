#include "tutorialsystem.h"

#include "spotlightoverlaysystem.h"
#include "camerasystem.h"
#include "Renderer/camera.h"
#include "terrain.h"

#include "2D/simple2dobject.h"
#include "Systems/tween.h"

#include <cmath>

namespace
{
    // Local AbstractCallable adapter so we can pass a lambda as TweenComponent's
    // onComplete callback. Same pattern as manualmining.cpp.
    struct LambdaCallable : public pg::AbstractCallable
    {
        std::function<void()> fn;
        LambdaCallable(std::function<void()> f) : fn(std::move(f)) {}
        void call(pg::EntitySystem* const) noexcept override { if (fn) fn(); }
        void serialize(pg::Archive&) const noexcept override {}
    };
}

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

template <typename Match>
std::pair<int, int> TutorialSystem::findNearestTerrain(const Match& match) const
{
    if (not gridSystem or not cameraSystem)
        return {-1, -1};

    auto camEnt = cameraSystem->getCameraEntity();
    if (not camEnt)
        return {-1, -1};
    auto cam = camEnt->get<BaseCamera2D>();
    if (not cam)
        return {-1, -1};

    float centerWorldX = cam->x + cam->getWidth() * 0.5f;
    float centerWorldY = cam->y + cam->getHeight() * 0.5f;
    int centerGX = static_cast<int>(centerWorldX) / Grid::TILE_SIZE;
    int centerGY = static_cast<int>(centerWorldY) / Grid::TILE_SIZE;

    int bestGX = -1, bestGY = -1;
    int bestDistSq = std::numeric_limits<int>::max();

    for (int y = 0; y < Grid::HEIGHT; ++y)
    {
        for (int x = 0; x < Grid::WIDTH; ++x)
        {
            if (not match(gridSystem->getTerrainAt(x, y)))
                continue;
            int dx = x - centerGX;
            int dy = y - centerGY;
            int d2 = dx * dx + dy * dy;
            if (d2 < bestDistSq)
            {
                bestDistSq = d2;
                bestGX = x;
                bestGY = y;
            }
        }
    }
    return {bestGX, bestGY};
}

// Locate which slot (if any) holds `id` and spawn a fading gold overlay over
// it. Pulse fades out over PULSE_MS so the player has time to glance at the
// new item before it settles.
void TutorialSystem::pulseSlotForItem(ItemId id)
{
    if (not playerInv or not inventoryUI or not hotbar)
        return;

    static constexpr size_t MAIN_SLOTS = PlayerInventorySystem::MAIN_SLOTS;
    static constexpr size_t HOTBAR_START = PlayerInventorySystem::HOTBAR_START;

    const auto& inv = playerInv->getInventory();
    uint64_t targetSlotEntity = 0;

    for (size_t i = 0; i < MAIN_SLOTS; ++i)
    {
        if (inv.getSlot(i).id == id)
        {
            targetSlotEntity = inventoryUI->getSlotEntityId(i);
            break;
        }
    }

    if (targetSlotEntity == 0)
    {
        for (size_t i = 0; i < PlayerInventorySystem::HOTBAR_COUNT; ++i)
        {
            if (inv.getSlot(HOTBAR_START + i).id == id)
            {
                targetSlotEntity = hotbar->getSlotEntityId(i);
                break;
            }
        }
    }

    if (targetSlotEntity == 0)
        return;

    auto slotEnt = ecsRef->getEntity(targetSlotEntity);
    if (not slotEnt)
        return;
    auto slotPos = slotEnt->get<PositionComponent>();
    if (not slotPos)
        return;

    float sx = slotPos->getX();
    float sy = slotPos->getY();
    float sw = slotPos->getWidth();
    float sh = slotPos->getHeight();
    if (sw <= 0.0f or sh <= 0.0f)
        return;

    // Gold overlay placed above the slot's content (slot bg z=98, items z=99,
    // text z=100) so it pulses on top.
    auto overlay = makeSimple2DShape(ecsRef, Shape2D::Square, sw, sh,
        constant::Vector4D{255.0f, 215.0f, 100.0f, 200.0f});
    auto pos = overlay.get<PositionComponent>();
    pos->setX(sx);
    pos->setY(sy);
    pos->setZ(101.0f);
    overlay.get<ViewportComponent>()->setViewport(2);

    constexpr float PULSE_MS = 800.0f;
    uint64_t overlayId = overlay.entity->id;
    auto* ecs = ecsRef;

    auto onUpdate = [ecs, overlayId](const TweenValue& value) {
        float alpha = std::get<float>(value);
        auto ent = ecs->getEntity(overlayId);
        if (ent)
            ent->get<Simple2DObject>()->setOpacity(alpha);
    };

    auto onComplete = std::make_shared<LambdaCallable>([ecs, overlayId]() {
        ecs->removeEntity(overlayId);
    });

    auto tweenEnt = ecsRef->createEntity();
    ecsRef->_attach<TweenComponent>(tweenEnt,
        TweenValue{200.0f}, TweenValue{0.0f}, PULSE_MS,
        onUpdate, onComplete);
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

    // Pulse every output of the completed craft so the player can spot where
    // the new items landed in their inventory or hotbar — not just the
    // tutorial-gated pickaxe.
    if (recipeRegistry)
    {
        const Recipe& r = recipeRegistry->get(event.recipeIndex);
        for (const auto& out : r.outputs)
            pulseSlotForItem(out.id);
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

    if (not event.value.isTrue())
        return;

    if (step == TutorialStep::ValidateFirstSteps && event.name == "mission_tools")
        pendingAdvance = true;
    else if (step == TutorialStep::ValidateStoneMasonry && event.name == "mission_furnace")
        pendingAdvance = true;
}

void TutorialSystem::onEvent(const TutorialSkipRequested&)
{
    if (currentStep >= TOTAL_STEPS)
        return;
    currentStep = TOTAL_STEPS;
    worldFacts->setFact("tutorial_step", currentStep);
    applyHudReveal();
    if (spotlight)
        spotlight->hide();
}

// ---------------------------------------------------------------------------
// Execute
// ---------------------------------------------------------------------------

void TutorialSystem::execute()
{
    if (not initialized)
    {
        initOnFirstTick();
        initialized = true;
    }

    if (pendingAdvance)
    {
        pendingAdvance = false;
        advanceStep();
    }
}

void TutorialSystem::initOnFirstTick()
{
    int loaded = worldFacts->getFact<int>("tutorial_step", 0);

    // Save migration: legacy saves used a 5-step flow. Anyone with a step value
    // beyond the legacy total (or beyond the current Complete sentinel) is
    // treated as already done.
    static constexpr int LEGACY_TOTAL_STEPS = 5;
    if (loaded >= LEGACY_TOTAL_STEPS && loaded < TOTAL_STEPS)
    {
        loaded = TOTAL_STEPS;
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

    applyHudReveal();
    presentCurrentStep();
}

// Hide the hotbar and mission button while the tutorial hasn't introduced
// them. Once the matching step is reached they stay revealed for the rest
// of the run.
void TutorialSystem::applyHudReveal()
{
    if (not hotbar or not hudBar)
        return;

    const bool tutorialDone = currentStep >= TOTAL_STEPS;
    const bool hotbarRevealed =
        tutorialDone or
        currentStep >= static_cast<int>(TutorialStep::MovePickaxeToHotbar);
    const bool missionRevealed =
        tutorialDone or
        currentStep >= static_cast<int>(TutorialStep::ValidateFirstSteps);

    hotbar->setHotbarVisible(hotbarRevealed);
    hudBar->setMissionButtonVisible(missionRevealed);
}

// ---------------------------------------------------------------------------
// Step advancement & presentation
// ---------------------------------------------------------------------------

void TutorialSystem::advanceStep()
{
    currentStep++;
    worldFacts->setFact("tutorial_step", currentStep);
    applyHudReveal();
    presentCurrentStep();
}

void TutorialSystem::presentCurrentStep()
{
    if (not spotlight)
        return;

    if (currentStep >= TOTAL_STEPS)
    {
        spotlight->hide();
        return;
    }

    const auto step = static_cast<TutorialStep>(currentStep);
    const auto& def = STEPS[currentStep];

    SpotlightOverlaySystem::Target target;
    auto arrowSide = SpotlightOverlaySystem::ArrowSide::Top;
    bool showSkip = (step == TutorialStep::MineFirstResource);

    auto centeredScreenRect = [&](float w, float h) {
        SpotlightOverlaySystem::Target t;
        t.kind = SpotlightOverlaySystem::TargetKind::ScreenRect;
        t.sx = (screenWidth - w) * 0.5f;
        t.sy = (screenHeight - h) * 0.5f;
        t.sw = w;
        t.sh = h;
        return t;
    };

    auto bottomScreenRect = [&](float w, float h, float bottomMargin) {
        SpotlightOverlaySystem::Target t;
        t.kind = SpotlightOverlaySystem::TargetKind::ScreenRect;
        t.sx = (screenWidth - w) * 0.5f;
        t.sy = screenHeight - h - bottomMargin;
        t.sw = w;
        t.sh = h;
        return t;
    };

    auto topRightRect = [&](float w, float h, float topMargin, float rightMargin) {
        SpotlightOverlaySystem::Target t;
        t.kind = SpotlightOverlaySystem::TargetKind::ScreenRect;
        t.sx = screenWidth - w - rightMargin;
        t.sy = topMargin;
        t.sw = w;
        t.sh = h;
        return t;
    };

    switch (step)
    {
        case TutorialStep::MineFirstResource:
        {
            auto [gx, gy] = findNearestTerrain([](TerrainType t) {
                return t == TerrainType::Tree || t == TerrainType::Rock;
            });
            if (gx >= 0)
            {
                target.kind = SpotlightOverlaySystem::TargetKind::WorldTile;
                target.gridX = gx;
                target.gridY = gy;
                target.gridW = 1;
                target.gridH = 1;
            }
            arrowSide = SpotlightOverlaySystem::ArrowSide::Top;
            break;
        }

        case TutorialStep::OpenInventory:
            target = centeredScreenRect(360.0f, 240.0f);
            arrowSide = SpotlightOverlaySystem::ArrowSide::Bottom;
            break;

        case TutorialStep::ValidateFirstSteps:
        case TutorialStep::ValidateStoneMasonry:
            target = topRightRect(56.0f, 32.0f, 10.0f, 56.0f);
            arrowSide = SpotlightOverlaySystem::ArrowSide::Bottom;
            break;

        case TutorialStep::CraftPickaxe:
        case TutorialStep::CraftFurnace:
            target = centeredScreenRect(360.0f, 240.0f);
            arrowSide = SpotlightOverlaySystem::ArrowSide::Right;
            break;

        case TutorialStep::MovePickaxeToHotbar:
        case TutorialStep::PlaceFurnaceInHotbar:
            target = bottomScreenRect(440.0f, 56.0f, 10.0f);
            arrowSide = SpotlightOverlaySystem::ArrowSide::Top;
            break;

        case TutorialStep::MineOre:
        {
            auto [gx, gy] = findNearestTerrain([](TerrainType t) {
                return t == TerrainType::OreCoal ||
                       t == TerrainType::OreCopper ||
                       t == TerrainType::OreIron;
            });
            if (gx >= 0)
            {
                target.kind = SpotlightOverlaySystem::TargetKind::WorldTile;
                target.gridX = gx;
                target.gridY = gy;
            }
            arrowSide = SpotlightOverlaySystem::ArrowSide::Top;
            break;
        }

        case TutorialStep::PlaceFurnaceOnGrid:
            target = centeredScreenRect(420.0f, 280.0f);
            arrowSide = SpotlightOverlaySystem::ArrowSide::Top;
            break;

        case TutorialStep::Complete:
            spotlight->hide();
            return;
    }

    spotlight->show(target, arrowSide, def.title, def.body, showSkip);
}
