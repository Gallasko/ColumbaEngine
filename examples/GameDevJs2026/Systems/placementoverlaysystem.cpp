#include "placementoverlaysystem.h"

#include "2D/simple2dobject.h"
#include "hotbarsystem.h"
#include "terrain.h"

void PlacementOverlaySystem::init()
{
    if (created)
        return;
    created = true;

    for (int y = 0; y < Grid::HEIGHT; ++y)
    {
        for (int x = 0; x < Grid::WIDTH; ++x)
        {
            auto sq = makeSimple2DShape(ecsRef, Shape2D::Square,
                static_cast<float>(Grid::TILE_SIZE),
                static_cast<float>(Grid::TILE_SIZE),
                constant::Vector4D{60.0f, 200.0f, 60.0f, 60.0f});
            auto pos = sq.get<PositionComponent>();
            pos->setX(static_cast<float>(x * Grid::TILE_SIZE));
            pos->setY(static_cast<float>(y * Grid::TILE_SIZE));
            pos->setZ(Z_OVERLAY);
            pos->setVisibility(false);
            sq.get<ViewportComponent>()->setViewport(GAME_VP);

            overlayIds[y * Grid::WIDTH + x] = sq.entity->id;
        }
    }
}

// ---------------------------------------------------------------------------
// Per-tick: detect selection changes and refresh.
// ---------------------------------------------------------------------------

void PlacementOverlaySystem::onEvent(const TickEvent&)
{
    if (not created or not hotbar)
        return;

    const BuildingDef* def = hotbar->getSelectedBuildingDef();

    if (def != lastDef)
    {
        lastDef = def;
        pendingRefresh = true;
    }

    if (not pendingRefresh)
        return;

    pendingRefresh = false;

    if (not def)
        hideAll();
    else
        refreshAll(def);
}

void PlacementOverlaySystem::onEvent(const BuildingPlacedEvent&)
{
    pendingRefresh = true;
}

void PlacementOverlaySystem::onEvent(const BuildingRemovedEvent&)
{
    pendingRefresh = true;
}

// ---------------------------------------------------------------------------
// Logic
// ---------------------------------------------------------------------------

bool PlacementOverlaySystem::checkPlacementOK(int gx, int gy,
                                              const BuildingDef& def) const
{
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

void PlacementOverlaySystem::refreshAll(const BuildingDef* def)
{
    const constant::Vector4D OK_COLOR  {60.0f, 200.0f, 60.0f, 70.0f};
    const constant::Vector4D BAD_COLOR {200.0f, 60.0f, 60.0f, 70.0f};

    for (int y = 0; y < Grid::HEIGHT; ++y)
    {
        for (int x = 0; x < Grid::WIDTH; ++x)
        {
            uint64_t id = overlayIds[y * Grid::WIDTH + x];
            auto ent = ecsRef->getEntity(id);
            if (not ent) continue;

            bool ok = checkPlacementOK(x, y, *def);
            ent->get<Simple2DObject>()->setColors(ok ? OK_COLOR : BAD_COLOR);
            ent->get<PositionComponent>()->setVisibility(true);
        }
    }
}

void PlacementOverlaySystem::hideAll()
{
    for (int i = 0; i < Grid::WIDTH * Grid::HEIGHT; ++i)
    {
        auto ent = ecsRef->getEntity(overlayIds[i]);
        if (ent)
            ent->get<PositionComponent>()->setVisibility(false);
    }
}
