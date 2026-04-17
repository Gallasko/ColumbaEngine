#include "manualmining.h"

#include "2D/simple2dobject.h"
#include "2D/texture.h"

#include <SDL2/SDL.h>

#include <algorithm>
#include <cstdio>
#include <cmath>

namespace {
    struct LambdaCallable : public pg::AbstractCallable
    {
        std::function<void()> fn;
        LambdaCallable(std::function<void()> f) : fn(std::move(f)) {}
        void call(pg::EntitySystem* const) noexcept override { if (fn) fn(); }
        void serialize(pg::Archive&) const noexcept override {}
    };
}

void ManualMiningSystem::init()
{
    createProgressBar();
    createErrorBar();
}

void ManualMiningSystem::onEvent(const TickEvent& event)
{
    tickAccumulator += event.tick;
    frameDelta += event.tick;
}

void ManualMiningSystem::onEvent(const InventoryOpenedEvent&)
{
    uiOpen = true;
}

void ManualMiningSystem::onEvent(const InventoryClosedEvent&)
{
    uiOpen = false;
}

uint8_t ManualMiningSystem::getEquippedToolTier() const
{
    if (not hotbar)
        return 0;
    const auto& item = hotbar->getSelectedItem();
    if (item.isEmpty())
        return 0;
    return itemRegistry->get(item.id).toolTier;
}

float ManualMiningSystem::getEquippedMiningSpeed() const
{
    if (not hotbar)
        return 1.0f;
    const auto& item = hotbar->getSelectedItem();
    if (item.isEmpty())
        return 1.0f;
    return itemRegistry->get(item.id).miningSpeedMult;
}

void ManualMiningSystem::execute()
{
    // Process accumulated time in 100ms chunks for idle fade
    while (tickAccumulator >= 100)
    {
        tickAccumulator -= 100;

        if (currentHits > 0)
        {
            barIdleTimer += 100;

            // Bar starts fading after BAR_IDLE_MS of no clicks;
            // progress resets when the fade completes (in tween onComplete)
            if (not barFadeStarted and barIdleTimer >= BAR_IDLE_MS)
            {
                barFadeStarted = true;
                startBarFadeOut();
            }
        }
    }

    // Tick ghost animations with real frame delta
    if (frameDelta > 0)
    {
        tickGhostAnimations(frameDelta);
        frameDelta = 0;
    }
}

void ManualMiningSystem::onProcessEvent(const OnMouseClick& event)
{
    if (not miningEnabled or uiOpen)
        return;

    if (event.button != SDL_BUTTON_LEFT)
        return;

    // Skip clicks on hotbar area
    if (event.pos.y > screenHeight - hotbarHeight)
        return;

    auto worldPos = cameraSystem->screenToWorld(event.pos.x, event.pos.y);
    auto [gx, gy] = gridSystem->getGrid().worldToGrid(worldPos.x, worldPos.y);

    if (not gridSystem->getGrid().isInBounds(gx, gy))
        return;

    // Don't mine if there's a building on this tile
    auto layer = gridSystem->getBuildingLayer();
    if (gridSystem->getCell(layer, gx, gy).tileId != 0)
        return;

    TerrainType terrain = gridSystem->getTerrainAt(gx, gy);
    if (not isMinableTerrain(terrain))
        return;

    // Check tool tier requirement
    uint8_t requiredTier = terrainTier(terrain);
    uint8_t equippedTier = getEquippedToolTier();
    if (equippedTier < requiredTier)
    {
        playWrongTierAnimation(gx, gy);
        return;
    }

    int baseHits = terrainHitsRequired(terrain);
    if (baseHits <= 0)
        return;

    float speedMult = getEquippedMiningSpeed();
    int hitsNeeded = std::max(1, static_cast<int>(std::ceil(baseHits / speedMult)));

    // If clicking a different tile or tool changed hits, reset progress
    if (gx != targetGridX or gy != targetGridY or hitsNeeded != requiredHits)
    {
        targetGridX = gx;
        targetGridY = gy;
        currentHits = 0;
        requiredHits = hitsNeeded;
    }

    currentHits++;
    decayTimer = 0;
    barIdleTimer = 0;
    barFadeStarted = false;
    cancelBarFade();
    updateProgressBar();

    if (currentHits >= requiredHits)
    {
        // Mining complete!
        ItemId itemId = terrainToItem(terrain);
        uint16_t count = 1;

        // Trees yield 2 wood, rocks yield 2 stone
        if (terrain == TerrainType::Tree or terrain == TerrainType::Rock)
            count = 2;

        sendEvent(PlayerGainItemEvent{itemId, count});

        // Ghost animation at the tile position
        auto [wx, wy] = gridSystem->getGrid().gridToWorld(gx, gy);
        spawnGhostAnimation(wx, wy, itemId);

        printf("Mined %s at (%d,%d) — gained %d x item %d\n",
            terrain == TerrainType::Tree ? "tree" :
            terrain == TerrainType::Rock ? "rock" : "ore",
            gx, gy, count, itemId);

        // Reset state (terrain stays — not depleted)
        currentHits = 0;
        targetGridX = -1;
        targetGridY = -1;
        barIdleTimer = 0;
        barFadeStarted = false;
        startBarFadeOut();
    }
}

void ManualMiningSystem::createProgressBar()
{
    // White outline (drawn behind the background, slightly larger)
    auto outline = makeSimple2DShape(ecsRef, Shape2D::Square, 0.0f, 0.0f,
        constant::Vector4D{255.0f, 255.0f, 255.0f, 220.0f});

    auto outlinePos = outline.get<PositionComponent>();
    outlinePos->setX(-1000.0f); // Hidden
    outlinePos->setZ(9.f);
    outlinePos->setWidth(BAR_WIDTH + BAR_OUTLINE * 2.0f);
    outlinePos->setHeight(BAR_HEIGHT + BAR_OUTLINE * 2.0f);
    outline.get<Simple2DObject>()->setViewport(GAME_VIEWPORT);
    progressOutlineEntityId = outline.entity->id;

    // Background bar (dark)
    auto bg = makeSimple2DShape(ecsRef, Shape2D::Square, 0.0f, 0.0f,
        constant::Vector4D{20.0f, 20.0f, 20.0f, 180.0f});

    auto bgPos = bg.get<PositionComponent>();
    bgPos->setX(-1000.0f); // Hidden
    bgPos->setZ(10.f);
    bgPos->setWidth(BAR_WIDTH);
    bgPos->setHeight(BAR_HEIGHT);
    bg.get<Simple2DObject>()->setViewport(GAME_VIEWPORT);
    progressBgEntityId = bg.entity->id;

    // Fill bar (green)
    auto fill = makeSimple2DShape(ecsRef, Shape2D::Square, 0.0f, 0.0f,
        constant::Vector4D{60.0f, 200.0f, 60.0f, 220.0f});

    auto fillPos = fill.get<PositionComponent>();
    fillPos->setX(-1000.0f); // Hidden
    fillPos->setZ(11.f);
    fillPos->setWidth(0.0f);
    fillPos->setHeight(BAR_HEIGHT);
    fill.get<Simple2DObject>()->setViewport(GAME_VIEWPORT);
    progressFillEntityId = fill.entity->id;
}

void ManualMiningSystem::updateProgressBar()
{
    if (targetGridX < 0 or targetGridY < 0)
        return;

    auto [wx, wy] = gridSystem->getGrid().gridToWorld(targetGridX, targetGridY);

    // Center the bar above the tile
    float tileSize = static_cast<float>(Grid::TILE_SIZE);
    float barX = wx + (tileSize - BAR_WIDTH) * 0.5f;
    float barY = wy + BAR_OFFSET_Y;

    // Position outline and restore full opacity
    auto outlineEnt = ecsRef->getEntity(progressOutlineEntityId);
    if (outlineEnt)
    {
        auto pos = outlineEnt->get<PositionComponent>();
        pos->setX(barX - BAR_OUTLINE);
        pos->setY(barY - BAR_OUTLINE);
        outlineEnt->get<Simple2DObject>()->setOpacity(220.0f);
    }

    // Position background and restore full opacity
    auto bgEnt = ecsRef->getEntity(progressBgEntityId);
    if (bgEnt)
    {
        auto pos = bgEnt->get<PositionComponent>();
        pos->setX(barX);
        pos->setY(barY);
        bgEnt->get<Simple2DObject>()->setOpacity(180.0f);
    }

    // Position and size fill, restore full opacity
    float fillRatio = static_cast<float>(currentHits) / static_cast<float>(requiredHits);
    float fillWidth = BAR_WIDTH * fillRatio;

    auto fillEnt = ecsRef->getEntity(progressFillEntityId);
    if (fillEnt)
    {
        auto pos = fillEnt->get<PositionComponent>();
        pos->setX(barX);
        pos->setY(barY);
        pos->setWidth(fillWidth);
        fillEnt->get<Simple2DObject>()->setOpacity(220.0f);
    }
}

void ManualMiningSystem::startBarFadeOut()
{
    cancelBarFade();

    uint64_t outlineId = progressOutlineEntityId;
    uint64_t bgId = progressBgEntityId;
    uint64_t fillId = progressFillEntityId;

    auto tweenEnt = ecsRef->createEntity();
    fadeTweenEntityId = tweenEnt->id;

    auto* ecs = ecsRef;

    auto onUpdate = [ecs, outlineId, bgId, fillId](const TweenValue& value) {
        float fade = std::get<float>(value);
        auto setAlpha = [ecs](uint64_t id, float baseAlpha, float f) {
            auto ent = ecs->getEntity(id);
            if (ent)
                ent->get<Simple2DObject>()->setOpacity(baseAlpha * f);
        };
        setAlpha(outlineId, 220.0f, fade);
        setAlpha(bgId, 180.0f, fade);
        setAlpha(fillId, 220.0f, fade);
    };

    auto onComplete = std::make_shared<LambdaCallable>([this]() {
        hideProgressBar();
        fadeTweenEntityId = 0;
        currentHits = 0;
        targetGridX = -1;
        targetGridY = -1;
    });

    ecsRef->_attach<TweenComponent>(tweenEnt,
        TweenValue{1.0f}, TweenValue{0.0f}, BAR_FADE_DURATION_MS,
        onUpdate, onComplete);
}

void ManualMiningSystem::cancelBarFade()
{
    if (fadeTweenEntityId != 0)
    {
        ecsRef->removeEntity(fadeTweenEntityId);
        fadeTweenEntityId = 0;

        // Restore full opacity
        auto setAlpha = [this](uint64_t id, float alpha) {
            auto ent = ecsRef->getEntity(id);
            if (ent)
                ent->get<Simple2DObject>()->setOpacity(alpha);
        };
        setAlpha(progressOutlineEntityId, 220.0f);
        setAlpha(progressBgEntityId, 180.0f);
        setAlpha(progressFillEntityId, 220.0f);
    }
}

void ManualMiningSystem::hideProgressBar()
{
    auto outlineEnt = ecsRef->getEntity(progressOutlineEntityId);
    if (outlineEnt)
        outlineEnt->get<PositionComponent>()->setX(-1000.0f);

    auto bgEnt = ecsRef->getEntity(progressBgEntityId);
    if (bgEnt)
        bgEnt->get<PositionComponent>()->setX(-1000.0f);

    auto fillEnt = ecsRef->getEntity(progressFillEntityId);
    if (fillEnt)
        fillEnt->get<PositionComponent>()->setX(-1000.0f);
}

void ManualMiningSystem::createErrorBar()
{
    // Red outline
    auto outline = makeSimple2DShape(ecsRef, Shape2D::Square, 0.0f, 0.0f,
        constant::Vector4D{200.0f, 60.0f, 60.0f, 220.0f});

    auto outlinePos = outline.get<PositionComponent>();
    outlinePos->setX(-1000.0f);
    outlinePos->setZ(9.f);
    outlinePos->setWidth(BAR_WIDTH + BAR_OUTLINE * 2.0f);
    outlinePos->setHeight(BAR_HEIGHT + BAR_OUTLINE * 2.0f);
    outline.get<Simple2DObject>()->setViewport(GAME_VIEWPORT);
    errorOutlineEntityId = outline.entity->id;

    // Red fill (full width)
    auto fill = makeSimple2DShape(ecsRef, Shape2D::Square, 0.0f, 0.0f,
        constant::Vector4D{220.0f, 50.0f, 50.0f, 220.0f});

    auto fillPos = fill.get<PositionComponent>();
    fillPos->setX(-1000.0f);
    fillPos->setZ(11.f);
    fillPos->setWidth(BAR_WIDTH);
    fillPos->setHeight(BAR_HEIGHT);
    fill.get<Simple2DObject>()->setViewport(GAME_VIEWPORT);
    errorFillEntityId = fill.entity->id;
}

void ManualMiningSystem::playWrongTierAnimation(int gx, int gy)
{
    cancelErrorAnimation();

    auto [wx, wy] = gridSystem->getGrid().gridToWorld(gx, gy);
    float tileSize = static_cast<float>(Grid::TILE_SIZE);
    float barX = wx + (tileSize - BAR_WIDTH) * 0.5f;
    float barY = wy + BAR_OFFSET_Y;

    // Position error bar above the tile, full width
    auto outlineEnt = ecsRef->getEntity(errorOutlineEntityId);
    if (outlineEnt)
    {
        auto pos = outlineEnt->get<PositionComponent>();
        pos->setX(barX - BAR_OUTLINE);
        pos->setY(barY - BAR_OUTLINE);
        pos->setWidth(BAR_WIDTH + BAR_OUTLINE * 2.0f);
        outlineEnt->get<Simple2DObject>()->setOpacity(220.0f);
    }

    auto fillEnt = ecsRef->getEntity(errorFillEntityId);
    if (fillEnt)
    {
        auto pos = fillEnt->get<PositionComponent>();
        pos->setX(barX);
        pos->setY(barY);
        pos->setWidth(BAR_WIDTH);
        fillEnt->get<Simple2DObject>()->setOpacity(220.0f);
    }

    // Tween: squeeze from full width to 0 while fading out
    uint64_t outlineId = errorOutlineEntityId;
    uint64_t fillId = errorFillEntityId;

    auto tweenEnt = ecsRef->createEntity();
    errorTweenEntityId = tweenEnt->id;

    auto* ecs = ecsRef;
    float barXCapture = barX;

    auto onUpdate = [ecs, outlineId, fillId, barXCapture](const TweenValue& value) {
        float t = std::get<float>(value); // 1.0 -> 0.0

        float width = BAR_WIDTH * t;
        float offsetX = (BAR_WIDTH - width) * 0.5f;

        auto oEnt = ecs->getEntity(outlineId);
        if (oEnt)
        {
            auto pos = oEnt->get<PositionComponent>();
            pos->setX(barXCapture - BAR_OUTLINE + offsetX);
            pos->setWidth(width + BAR_OUTLINE * 2.0f);
            oEnt->get<Simple2DObject>()->setOpacity(220.0f * t);
        }

        auto fEnt = ecs->getEntity(fillId);
        if (fEnt)
        {
            auto pos = fEnt->get<PositionComponent>();
            pos->setX(barXCapture + offsetX);
            pos->setWidth(width);
            fEnt->get<Simple2DObject>()->setOpacity(220.0f * t);
        }
    };

    auto onComplete = std::make_shared<LambdaCallable>([this]() {
        hideErrorBar();
        errorTweenEntityId = 0;
    });

    static constexpr float SQUEEZE_DURATION_MS = 400.0f;
    ecsRef->_attach<TweenComponent>(tweenEnt,
        TweenValue{1.0f}, TweenValue{0.0f}, SQUEEZE_DURATION_MS,
        onUpdate, onComplete);
}

void ManualMiningSystem::cancelErrorAnimation()
{
    if (errorTweenEntityId != 0)
    {
        ecsRef->removeEntity(errorTweenEntityId);
        errorTweenEntityId = 0;
    }
    hideErrorBar();
}

void ManualMiningSystem::hideErrorBar()
{
    auto outlineEnt = ecsRef->getEntity(errorOutlineEntityId);
    if (outlineEnt)
        outlineEnt->get<PositionComponent>()->setX(-1000.0f);

    auto fillEnt = ecsRef->getEntity(errorFillEntityId);
    if (fillEnt)
        fillEnt->get<PositionComponent>()->setX(-1000.0f);
}

void ManualMiningSystem::spawnGhostAnimation(float worldX, float worldY, ItemId itemId)
{
    const auto& def = itemRegistry->get(itemId);
    float size = 12.0f;
    float tileSize = static_cast<float>(Grid::TILE_SIZE);

    auto ghost = make2DTexture(ecsRef, size, size, def.textureName);
    auto pos = ghost.get<PositionComponent>();
    pos->setX(worldX + (tileSize - size) * 0.5f);
    pos->setY(worldY);
    pos->setZ(11.0f);
    ghost.get<Texture2DComponent>()->setViewport(GAME_VIEWPORT);
    ghost.get<Texture2DComponent>()->setOpacity(1.0f);

    GhostAnim anim;
    anim.entityId = ghost.entity->id;
    anim.startY = worldY;
    anim.endY = worldY - 24.0f;
    anim.elapsed = 0.0f;
    activeGhosts.push_back(anim);
}

void ManualMiningSystem::tickGhostAnimations(size_t deltaMs)
{
    float delta = static_cast<float>(deltaMs);

    for (auto it = activeGhosts.begin(); it != activeGhosts.end(); )
    {
        it->elapsed += delta;
        float t = std::min(it->elapsed / GhostAnim::DURATION, 1.0f);

        // Ease-out quad: t * (2 - t)
        float eased = t * (2.0f - t);

        auto ent = ecsRef->getEntity(it->entityId);
        if (ent)
        {
            ent->get<PositionComponent>()->setY(it->startY + (it->endY - it->startY) * eased);
            ent->get<Texture2DComponent>()->setOpacity(1.0f - t);
        }

        if (t >= 1.0f)
        {
            if (ent)
                ecsRef->removeEntity(it->entityId);
            it = activeGhosts.erase(it);
        }
        else
        {
            ++it;
        }
    }
}
