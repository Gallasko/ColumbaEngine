#include "placementoverlaysystem.h"

#include "2D/simple2dobject.h"
#include "Renderer/camera.h"

#include "camerasystem.h"
#include "hotbarsystem.h"
#include "terrain.h"

using namespace pg;

void PlacementOverlaySystem::init()
{
    if (created)
    {
        LOG_INFO("PlacementOverlay", "init() called but already created — skipping");
        return;
    }
    created = true;

    LOG_INFO("PlacementOverlay", "init() — allocating overlay pool of " << OVERLAY_POOL_SIZE << " entities");

    for (int i = 0; i < OVERLAY_POOL_SIZE; ++i)
    {
        auto sq = makeSimple2DShape(ecsRef, Shape2D::Square,
            static_cast<float>(Grid::TILE_SIZE),
            static_cast<float>(Grid::TILE_SIZE),
            constant::Vector4D{60.0f, 200.0f, 60.0f, 70.0f});
        auto pos = sq.get<PositionComponent>();
        pos->setX(-1000.0f);
        pos->setY(-1000.0f);
        pos->setZ(Z_OVERLAY);
        pos->setVisibility(false);
        sq.get<ViewportComponent>()->setViewport(GAME_VP);

        overlayIds[i] = sq.entity->id;
    }

    LOG_INFO("PlacementOverlay", "init() complete — pool ready");
}

// ---------------------------------------------------------------------------
// Mouse + tick: track cursor and reflow overlays.
// ---------------------------------------------------------------------------

void PlacementOverlaySystem::onProcessEvent(const OnSDLMouseMotion& event)
{
    cursorX = static_cast<float>(event.x);
    cursorY = static_cast<float>(event.y);
}

void PlacementOverlaySystem::onEvent(const TickEvent&)
{
    if (not created)
        return;

    auto* hotbar       = ecsRef->getSystem<HotbarSystem>();
    auto* cameraSystem = ecsRef->getSystem<CameraSystem>();
    auto* gridSystem   = ecsRef->getSystem<GridSystem>();
    if (not hotbar or not cameraSystem or not gridSystem)
        return;

    const BuildingDef* def = hotbar->getSelectedBuildingDef();

    if (def != lastDef)
    {
        LOG_INFO("PlacementOverlay", "selected building def changed (was " << (lastDef ? lastDef->name : "<none>")
                << ", now " << (def ? def->name : "<none>") << ") — flagging refresh");
        lastDef = def;
        pendingRefresh = true;
    }

    // No building selected — make sure the pool is hidden and bail.
    if (not def)
    {
        if (pendingRefresh)
        {
            LOG_INFO("PlacementOverlay", "no building selected and refresh pending — hiding all overlays");
            hideAll();
            pendingRefresh = false;
        }
        return;
    }

    // Resolve mouse position to a grid cell. Hide everything if the cursor is
    // off-grid so we don't draw stale overlays at the screen edge.
    int gx = -1, gy = -1;
    auto camEnt = cameraSystem->getCameraEntity();
    if (camEnt)
    {
        auto cam = camEnt->get<BaseCamera2D>();
        if (cam and cam->getWidth() > 0.0f)
        {
            float zoom = screenWidth / cam->getWidth();
            float worldX = cam->x + cursorX / zoom;
            float worldY = cam->y + cursorY / zoom;
            auto cell = gridSystem->getGrid().worldToGrid(worldX, worldY);
            gx = cell.first;
            gy = cell.second;
            if (gx < 0 or gx >= Grid::WIDTH or gy < 0 or gy >= Grid::HEIGHT)
            {
                gx = -1;
                gy = -1;
            }
        }
    }

    if (gx < 0)
    {
        if (pendingRefresh or lastGhostGX != gx or lastGhostGY != gy)
        {
            LOG_INFO("PlacementOverlay", "cursor off-grid (cursor=" << cursorX << "," << cursorY
                    << ") — hiding overlays");
            hideAll();
            lastGhostGX = gx;
            lastGhostGY = gy;
            pendingRefresh = false;
        }
        return;
    }

    if (gx == lastGhostGX and gy == lastGhostGY and not pendingRefresh)
        return;

    LOG_INFO("PlacementOverlay", "ghost footprint refresh: def=" << def->name
            << " grid=(" << gx << "," << gy << ") prev=(" << lastGhostGX << "," << lastGhostGY << ")"
            << " pendingRefresh=" << pendingRefresh);

    lastGhostGX = gx;
    lastGhostGY = gy;
    pendingRefresh = false;
    refreshGhostFootprint(*def, gx, gy);
}

void PlacementOverlaySystem::onEvent(const BuildingPlacedEvent& event)
{
    LOG_INFO("PlacementOverlay", "BuildingPlacedEvent received (tile=" << event.tileName
            << " at " << event.x << "," << event.y << ") — flagging refresh");
    pendingRefresh = true;
}

void PlacementOverlaySystem::onEvent(const BuildingRemovedEvent& event)
{
    LOG_INFO("PlacementOverlay", "BuildingRemovedEvent received (tile=" << event.tileName
            << " at " << event.x << "," << event.y << ") — flagging refresh");
    pendingRefresh = true;
}

// ---------------------------------------------------------------------------
// Logic
// ---------------------------------------------------------------------------

bool PlacementOverlaySystem::checkPlacementOK(int gx, int gy,
                                              const BuildingDef& def) const
{
    auto* gridSystem = ecsRef->getSystem<GridSystem>();
    auto layer = gridSystem->getBuildingLayer();
    for (int dy = 0; dy < def.getFootprintH(); ++dy)
    {
        for (int dx = 0; dx < def.getFootprintW(); ++dx)
        {
            int cx = gx + dx;
            int cy = gy + dy;
            if (not gridSystem->getGrid().isInBounds(cx, cy))
                return false;
            if (not gridSystem->getCell(layer, cx, cy).tileName.empty())
                return false;
            if (isBlockingTerrain(gridSystem->getTerrainAt(cx, cy)))
                return false;
        }
    }
    return true;
}

void PlacementOverlaySystem::refreshGhostFootprint(const BuildingDef& def,
                                                   int gx, int gy)
{
    const constant::Vector4D OK_COLOR  {60.0f, 200.0f, 60.0f, 110.0f};
    const constant::Vector4D BAD_COLOR {200.0f, 60.0f, 60.0f, 110.0f};

    auto* gridSystem = ecsRef->getSystem<GridSystem>();
    auto layer = gridSystem->getBuildingLayer();

    int fw = def.getFootprintW();
    int fh = def.getFootprintH();
    LOG_INFO("PlacementOverlay", "refreshGhostFootprint def=" << def.name
            << " origin=(" << gx << "," << gy << ") footprint=" << fw << "x" << fh);
    int idx = 0;
    for (int dy = 0; dy < fh; ++dy)
    {
        for (int dx = 0; dx < fw; ++dx)
        {
            if (idx >= OVERLAY_POOL_SIZE)
                break;

            int cx = gx + dx;
            int cy = gy + dy;

            // Per-cell validity: out-of-bounds, occupied, or blocking terrain
            // are individually red; everything else is green.
            bool cellOk = true;
            if (not gridSystem->getGrid().isInBounds(cx, cy))
                cellOk = false;
            else if (not gridSystem->getCell(layer, cx, cy).tileName.empty())
                cellOk = false;
            else if (isBlockingTerrain(gridSystem->getTerrainAt(cx, cy)))
                cellOk = false;

            auto ent = ecsRef->getEntity(overlayIds[idx]);
            if (ent)
            {
                ent->get<Simple2DObject>()->setColors(cellOk ? OK_COLOR : BAD_COLOR);
                auto pos = ent->get<PositionComponent>();
                pos->setX(static_cast<float>(cx * Grid::TILE_SIZE));
                pos->setY(static_cast<float>(cy * Grid::TILE_SIZE));
                pos->setVisibility(true);
            }
            ++idx;
        }
    }

    // Hide unused overlays.
    for (; idx < OVERLAY_POOL_SIZE; ++idx)
    {
        auto ent = ecsRef->getEntity(overlayIds[idx]);
        if (ent)
            ent->get<PositionComponent>()->setVisibility(false);
    }
}

void PlacementOverlaySystem::hideAll()
{
    for (int i = 0; i < OVERLAY_POOL_SIZE; ++i)
    {
        auto ent = ecsRef->getEntity(overlayIds[i]);
        if (ent)
            ent->get<PositionComponent>()->setVisibility(false);
    }
}
