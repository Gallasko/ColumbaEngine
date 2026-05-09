#include "tutorialsystem.h"

#include "spotlightoverlaysystem.h"
#include "camerasystem.h"
#include "Renderer/camera.h"
#include "terrain.h"
#include "canvasgenerator.h"

#include "2D/simple2dobject.h"
#include "2D/position.h"
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
    {"Open the inventory",   "Click the inventory button to open your inventory and crafting menu."},
    {"Open the missions",    "Open the mission tab and validate \"First Steps\" to unlock the Stone Pickaxe recipe."},
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

    float sw = slotPos->getWidth();
    float sh = slotPos->getHeight();
    if (sw <= 0.0f or sh <= 0.0f)
        return;

    // Gold overlay placed above the slot's content (slot bg z=98, items z=99,
    // text z=100) so it pulses on top. Anchored to fill the slot via UiAnchor.
    auto overlay = makeSimple2DShape(ecsRef, Shape2D::Square, sw, sh,
        constant::Vector4D{255.0f, 215.0f, 100.0f, 200.0f});
    auto pos = overlay.get<PositionComponent>();
    pos->setZ(101.0f);
    overlay.get<ViewportComponent>()->setViewport(2);
    if (auto slotAnchor = slotEnt->get<UiAnchor>())
    {
        auto a = ecsRef->attach<UiAnchor>(overlay.entity);
        a->fillIn(slotAnchor);
    }

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
    LOG_INFO("Tutorial", "PlayerGainItemEvent id=" << event.id << " count=" << event.count
            << " step=" << currentStep);

    if (step == TutorialStep::MineFirstResource &&
        (event.id == WOOD_ID || event.id == STONE_ID))
    {
        LOG_INFO("Tutorial", "MineFirstResource satisfied — pendingAdvance");
        pendingAdvance = true;
    }
    else if (step == TutorialStep::MineOre &&
             (event.id == IRON_ORE_ID || event.id == COPPER_ORE_ID || event.id == COAL_ID))
    {
        LOG_INFO("Tutorial", "MineOre satisfied — pendingAdvance");
        pendingAdvance = true;
    }
}

void TutorialSystem::onEvent(const InventoryOpenedEvent&)
{
    const auto step = static_cast<TutorialStep>(currentStep);
    LOG_INFO("Tutorial", "InventoryOpenedEvent step=" << currentStep);
    if (step == TutorialStep::OpenInventory)
    {
        LOG_INFO("Tutorial", "OpenInventory satisfied — pendingAdvance");
        pendingAdvance = true;
        return;
    }

    // Steps whose spotlight target depends on inventory open/closed state
    // need to re-render when that state changes:
    // - ValidateFirstSteps/StoneMasonry: opening the inventory mid-step swaps
    //   the spotlight to "close it" guidance.
    // - CraftPickaxe/CraftFurnace: opening the inventory swaps the spotlight
    //   from the HUD inventory button to the crafting panel.
    if (step == TutorialStep::ValidateFirstSteps or
        step == TutorialStep::ValidateStoneMasonry or
        step == TutorialStep::CraftPickaxe or
        step == TutorialStep::CraftFurnace)
        presentCurrentStep();
}

void TutorialSystem::onEvent(const InventoryClosedEvent&)
{
    const auto step = static_cast<TutorialStep>(currentStep);
    LOG_INFO("Tutorial", "InventoryClosedEvent step=" << currentStep);
    if (step == TutorialStep::ValidateFirstSteps or
        step == TutorialStep::ValidateStoneMasonry or
        step == TutorialStep::CraftPickaxe or
        step == TutorialStep::CraftFurnace)
        presentCurrentStep();
}

void TutorialSystem::onEvent(const MissionUIOpenedEvent&)
{
    LOG_INFO("Tutorial", "MissionUIOpenedEvent step=" << currentStep);
    missionUIOpen = true;
    missionUiOpenedThisStep = true;
    const auto step = static_cast<TutorialStep>(currentStep);
    if (step == TutorialStep::ValidateFirstSteps or
        step == TutorialStep::ValidateStoneMasonry)
        presentCurrentStep();
}

void TutorialSystem::onEvent(const MissionUIClosedEvent&)
{
    LOG_INFO("Tutorial", "MissionUIClosedEvent step=" << currentStep);
    missionUIOpen = false;
    const auto step = static_cast<TutorialStep>(currentStep);
    if (step == TutorialStep::ValidateFirstSteps or
        step == TutorialStep::ValidateStoneMasonry)
        presentCurrentStep();
}

void TutorialSystem::onEvent(const HandCraftCompletedEvent& event)
{
    const auto step = static_cast<TutorialStep>(currentStep);
    LOG_INFO("Tutorial", "HandCraftCompletedEvent recipeIndex=" << event.recipeIndex
            << " step=" << currentStep);

    if (step == TutorialStep::CraftPickaxe &&
        recipeOutputs(event.recipeIndex, STONE_PICKAXE_ID))
    {
        LOG_INFO("Tutorial", "CraftPickaxe satisfied — pendingAdvance");
        pendingAdvance = true;
    }
    else if (step == TutorialStep::CraftFurnace &&
             recipeOutputs(event.recipeIndex, FURNACE_ID))
    {
        LOG_INFO("Tutorial", "CraftFurnace satisfied — pendingAdvance");
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
    LOG_INFO("Tutorial", "SlotDroppedEvent category=" << static_cast<int>(event.category)
            << " itemId=" << event.item.id << " step=" << currentStep);
    if (event.category != SlotCategory::Hotbar)
        return;

    const auto step = static_cast<TutorialStep>(currentStep);

    if (step == TutorialStep::MovePickaxeToHotbar &&
        event.item.id == STONE_PICKAXE_ID)
    {
        LOG_INFO("Tutorial", "MovePickaxeToHotbar satisfied — pendingAdvance");
        pendingAdvance = true;
    }
    else if (step == TutorialStep::PlaceFurnaceInHotbar &&
             event.item.id == FURNACE_ID)
    {
        LOG_INFO("Tutorial", "PlaceFurnaceInHotbar satisfied — pendingAdvance");
        pendingAdvance = true;
    }
}

void TutorialSystem::onEvent(const BuildingPlacedEvent& event)
{
    LOG_INFO("Tutorial", "BuildingPlacedEvent " << event.tileName
            << " at (" << event.x << "," << event.y << ") step=" << currentStep);
    if (static_cast<TutorialStep>(currentStep) == TutorialStep::PlaceFurnaceOnGrid &&
        event.tileName == "Furnace")
    {
        LOG_INFO("Tutorial", "PlaceFurnaceOnGrid satisfied — pendingAdvance");
        pendingAdvance = true;
    }
}

void TutorialSystem::onEvent(const AddFact& event)
{
    const auto step = static_cast<TutorialStep>(currentStep);
    LOG_INFO("Tutorial", "AddFact name=" << event.name << " step=" << currentStep);

    if (not event.value.isTrue())
        return;

    if (step == TutorialStep::ValidateFirstSteps && event.name == "mission_tools")
    {
        LOG_INFO("Tutorial", "ValidateFirstSteps satisfied (mission_tools) — pendingAdvance");
        pendingAdvance = true;
    }
    else if (step == TutorialStep::ValidateStoneMasonry && event.name == "mission_furnace")
    {
        LOG_INFO("Tutorial", "ValidateStoneMasonry satisfied (mission_furnace) — pendingAdvance");
        pendingAdvance = true;
    }
}

void TutorialSystem::onEvent(const TutorialSkipRequested&)
{
    LOG_INFO("Tutorial", "TutorialSkipRequested received (currentStep=" << currentStep << ")");
    if (currentStep >= TOTAL_STEPS)
        return;
    currentStep = TOTAL_STEPS;
    worldFacts->setFact("tutorial_step", currentStep);
    applyHudReveal();
    if (spotlight)
        spotlight->hide();
    LOG_INFO("Tutorial", "tutorial skipped to TOTAL_STEPS=" << TOTAL_STEPS);
}

// ---------------------------------------------------------------------------
// Execute
// ---------------------------------------------------------------------------

void TutorialSystem::execute()
{
    if (not initialized)
    {
        LOG_INFO("Tutorial", "execute() — first tick, running initOnFirstTick()");
        initOnFirstTick();
        initialized = true;
    }

    if (pendingAdvance)
    {
        LOG_INFO("Tutorial", "execute() — pendingAdvance set, advancing from step " << currentStep);
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
    LOG_INFO("Tutorial", "advanceStep — now currentStep=" << currentStep
            << " (TOTAL_STEPS=" << TOTAL_STEPS << ")");
    missionUiOpenedThisStep = false;
    worldFacts->setFact("tutorial_step", currentStep);
    applyHudReveal();
    presentCurrentStep();
}

void TutorialSystem::presentCurrentStep()
{
    if (not spotlight)
    {
        LOG_INFO("Tutorial", "presentCurrentStep — no spotlight ref, skipping");
        return;
    }

    if (currentStep >= TOTAL_STEPS)
    {
        LOG_INFO("Tutorial", "presentCurrentStep — tutorial complete, hiding spotlight");
        spotlight->hide();
        return;
    }

    LOG_INFO("Tutorial", "presentCurrentStep step=" << currentStep);

    const auto step = static_cast<TutorialStep>(currentStep);
    const auto& def = STEPS[currentStep];

    SpotlightOverlaySystem::Target target;
    auto arrowSide = SpotlightOverlaySystem::ArrowSide::Top;
    bool showSkip = (step == TutorialStep::MineFirstResource);
    std::string title = def.title;
    std::string body  = def.body;

    auto centeredScreenRect = [&](float w, float h) {
        SpotlightOverlaySystem::Target t;
        t.kind = SpotlightOverlaySystem::TargetKind::ScreenRect;
        t.sx = (screenWidth - w) * 0.5f;
        t.sy = (screenHeight - h) * 0.5f;
        t.sw = w;
        t.sh = h;
        return t;
    };

    auto namedUiTarget = [&](const char* name) {
        SpotlightOverlaySystem::Target t;
        auto ent = ecsRef->getEntity(name);
        if (ent)
        {
            t.kind = SpotlightOverlaySystem::TargetKind::UiEntity;
            t.entityId = ent->id;
        }
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
                // Trees occupy a 2x3 footprint of TerrainType::Tree cells —
                // findNearestTerrain may have returned any of them, so walk
                // up/left to the top-left cell and span the full footprint
                // so the spotlight covers the whole tree sprite. Rocks are
                // 1x1, no walking needed.
                int tlx = gx;
                int tly = gy;
                int spanW = 1;
                int spanH = 1;
                if (gridSystem and gridSystem->getTerrainAt(gx, gy) == TerrainType::Tree)
                {
                    while (tlx > 0 and gridSystem->getTerrainAt(tlx - 1, tly) == TerrainType::Tree)
                        --tlx;
                    while (tly > 0 and gridSystem->getTerrainAt(tlx, tly - 1) == TerrainType::Tree)
                        --tly;
                    spanW = TREE_W;
                    spanH = TREE_H;
                }
                target.kind  = SpotlightOverlaySystem::TargetKind::WorldTile;
                target.gridX = tlx;
                target.gridY = tly;
                target.gridW = spanW;
                target.gridH = spanH;
            }
            arrowSide = SpotlightOverlaySystem::ArrowSide::Top;
            break;
        }

        case TutorialStep::OpenInventory:
            // HUD inventory button is in the top-right corner; arrow points
            // from the left so it doesn't run off-screen.
            target = namedUiTarget("HudInventoryButton");
            arrowSide = SpotlightOverlaySystem::ArrowSide::Left;
            break;

        case TutorialStep::ValidateFirstSteps:
        case TutorialStep::ValidateStoneMasonry:
        {
            // Sub-state machine: once the player has opened the mission tab
            // even once during this step we drop the dim/arrow but leave the
            // corner panel up so the objective text remains visible until
            // they actually validate. Target::None skips the dim+arrow.
            if (missionUIOpen or missionUiOpenedThisStep)
                break;

            // Inventory left open from the previous step (or re-opened by
            // the player). Guide them to close it before pointing at the
            // mission button — the mission UI auto-closes inventory anyway,
            // but the redirect makes the next action obvious.
            if (inventoryUI and inventoryUI->isOpen())
            {
                target = namedUiTarget("InventoryPanel");
                arrowSide = SpotlightOverlaySystem::ArrowSide::Top;
                title = "Close the inventory";
                body  = "You don't have what you need yet — close the inventory and check the mission tab.";
                break;
            }

            target = namedUiTarget("HudMissionButton");
            arrowSide = SpotlightOverlaySystem::ArrowSide::Left;
            break;
        }

        case TutorialStep::CraftPickaxe:
        case TutorialStep::CraftFurnace:
            // CraftingPanel is anchored to the inventory and only laid out
            // once inventory is open. If the player has it closed, redirect
            // the spotlight to the HUD inventory button first; switch to the
            // crafting panel once they open it.
            if (inventoryUI and not inventoryUI->isOpen())
            {
                target = namedUiTarget("HudInventoryButton");
                arrowSide = SpotlightOverlaySystem::ArrowSide::Left;
                title = "Open the inventory";
                body  = "Open your inventory to access the crafting menu.";
            }
            else
            {
                target = namedUiTarget("CraftingPanel");
                arrowSide = SpotlightOverlaySystem::ArrowSide::Left;
            }
            break;

        case TutorialStep::MovePickaxeToHotbar:
        case TutorialStep::PlaceFurnaceInHotbar:
        {
            // Span the dim cutout to cover the inventory panel and the full
            // hotbar row (first slot through last slot), so the player can
            // see both the source and destination. Hotbar slot positions are
            // derived from HotbarSystem layout constants rather than read
            // from slot entities — for the pickaxe step the hotbar is being
            // revealed for the first time, and slot anchors may not have
            // resolved yet when the spotlight rect is computed.
            const float totalSlotsW = static_cast<float>(HOTBAR_SLOTS) * HotbarSystem::SLOT_SIZE
                                    + static_cast<float>(HOTBAR_SLOTS - 1) * HotbarSystem::SLOT_SPACING;
            const float hotbarLeft   = (screenWidth - totalSlotsW) * 0.5f;
            const float hotbarRight  = hotbarLeft + totalSlotsW;
            const float hotbarTop    = screenHeight - HotbarSystem::HOTBAR_HEIGHT
                                     + HotbarSystem::SLOT_PADDING;
            const float hotbarBottom = hotbarTop + HotbarSystem::SLOT_SIZE;

            auto invEnt = ecsRef->getEntity("InventoryPanel");
            if (invEnt)
            {
                auto invPos = invEnt->get<PositionComponent>();
                float left   = std::min(invPos->getX(), hotbarLeft);
                float right  = std::max(invPos->getX() + invPos->getWidth(), hotbarRight);
                float top    = std::min(invPos->getY(), hotbarTop);
                float bottom = std::max(invPos->getY() + invPos->getHeight(), hotbarBottom);

                target.kind = SpotlightOverlaySystem::TargetKind::ScreenRect;
                target.sx = left;
                target.sy = top;
                target.sw = right - left;
                target.sh = bottom - top;
                // Arrow target left as 0 (no entity) — the spotlight system
                // falls back to the rect's geometry, which is reliable since
                // we computed it from screen math.
            }
            arrowSide = SpotlightOverlaySystem::ArrowSide::Top;
            break;
        }

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
            // Auto-select the hotbar slot containing the furnace so the
            // player's next click places it without first having to scroll
            // the hotbar selection.
            if (hotbar)
                hotbar->selectSlotForItem(FURNACE_ID);
            target = centeredScreenRect(420.0f, 280.0f);
            arrowSide = SpotlightOverlaySystem::ArrowSide::Top;
            break;

        case TutorialStep::Complete:
            spotlight->hide();
            return;
    }

    spotlight->show(target, arrowSide, title, body, showSkip);
}
