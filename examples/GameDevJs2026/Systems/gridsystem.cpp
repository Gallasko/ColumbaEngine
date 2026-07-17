#include "gridsystem.h"

#include "2D/simple2dobject.h"
#include "2D/texture.h"

#include <algorithm>

using namespace pg;

void GridSystem::save(Archive& archive)
{
    // Save terrain grid as flat list of uint8_t values
    std::vector<unsigned int> terrainFlat;
    terrainFlat.reserve(Grid::WIDTH * Grid::HEIGHT);
    for (int y = 0; y < Grid::HEIGHT; ++y)
        for (int x = 0; x < Grid::WIDTH; ++x)
            terrainFlat.push_back(static_cast<unsigned int>(terrainGrid[y][x]));
    serialize(archive, "terrain", terrainFlat);

    // Save buildings — only owner cells
    std::vector<SavedBuilding> buildings;
    for (int y = 0; y < Grid::HEIGHT; ++y)
    {
        for (int x = 0; x < Grid::WIDTH; ++x)
        {
            const auto& cell = grid.getCell(buildingLayer, x, y);
            if (cell.tileName.empty() or not cell.isOwner)
                continue;

            SavedBuilding sb;
            sb.x = x;
            sb.y = y;
            sb.tileName = cell.tileName;
            sb.direction = cell.direction;
            sb.enterDirection = cell.enterDirection;
            sb.conveyorTileIndex = LINE_RIGHT_1; // default

            // Find conveyor tile index if this is a belt
            if (cell.tileName == "Conveyor" and cell.entityId != 0)
            {
                for (const auto& conv : conveyors)
                {
                    if (conv.entityId == cell.entityId)
                    {
                        sb.conveyorTileIndex = conv.tileIndex;
                        break;
                    }
                }
            }
            buildings.push_back(sb);
        }
    }
    serialize(archive, "buildings", buildings);

    LOG_INFO("GridSystem", "saved " << terrainFlat.size() << " terrain cells, " << buildings.size() << " buildings");
}

void GridSystem::load(const UnserializedObject& serializedString)
{
    // init() already ran (layers exist, fresh terrain generated).
    // If we have save data, replace the fresh terrain and restore buildings.

    std::vector<unsigned int> terrainFlat;
    defaultDeserialize(serializedString, "terrain", terrainFlat);

    std::vector<SavedBuilding> buildings;
    defaultDeserialize(serializedString, "buildings", buildings);

    LOG_INFO("GridSystem", "loaded " << terrainFlat.size() << " terrain cells, " << buildings.size() << " buildings");

    if (terrainFlat.size() != static_cast<size_t>(Grid::WIDTH * Grid::HEIGHT))
        return;

    // Clear the freshly generated terrain entities
    for (uint64_t id : bgEntities)
        ecsRef->removeEntity(id);
    bgEntities.clear();

    // Restore terrain grid
    for (int y = 0; y < Grid::HEIGHT; ++y)
        for (int x = 0; x < Grid::WIDTH; ++x)
            terrainGrid[y][x] = static_cast<TerrainType>(terrainFlat[y * Grid::WIDTH + x]);

    // Reconstruct treeInstances from saved terrain
    treeInstances.clear();
    for (int y = 0; y <= Grid::HEIGHT - TREE_H; ++y)
    {
        for (int x = 0; x <= Grid::WIDTH - TREE_W; ++x)
        {
            bool allTree = true;
            for (int dy = 0; dy < TREE_H and allTree; ++dy)
                for (int dx = 0; dx < TREE_W and allTree; ++dx)
                    if (terrainGrid[y + dy][x + dx] != TerrainType::Tree)
                        allTree = false;
            if (allTree)
            {
                treeInstances.push_back({x, y});
                x += TREE_W - 1;
            }
        }
    }

    renderTerrain();

    // Restore buildings (without emitting BuildingPlacedEvent)
    for (const auto& sb : buildings)
    {
        const BuildingDef* def = registry->findByName(sb.tileName);
        if (def)
            placeBuildingInternal(buildingLayer, sb.x, sb.y, *def,
                                  sb.direction, sb.conveyorTileIndex, sb.enterDirection);
    }
}

void GridSystem::init()
{
    LOG_INFO("GridSystem", "init() — creating layers and generating fresh terrain");

    // Create default layers
    terrainLayer = grid.addLayer("terrain", 1.0f);
    buildingLayer = grid.addLayer("buildings", 2.0f);
    itemLayer = grid.addLayer("items", 3.0f);

    LOG_INFO("GridSystem", "layers created: terrain=" << terrainLayer
            << " building=" << buildingLayer << " item=" << itemLayer);

    // Generate fresh terrain (load() will replace it if a save exists)
    generateAndRenderTerrain();

    LOG_INFO("GridSystem", "init() complete");
}

void GridSystem::regenerateTerrain(uint32_t seed)
{
    LOG_INFO("GridSystem", "regenerating terrain with seed " << seed);

    for (uint64_t id : bgEntities)
        ecsRef->removeEntity(id);
    bgEntities.clear();

    generateAndRenderTerrain(seed);
}

void GridSystem::execute()
{
    if (animElapsed <= 0)
        return;

    // Advance conveyor animation globally — all conveyors stay in sync
    if (conveyors.empty())
    {
        // Reset accumulator so first placement doesn't fast-forward
        animElapsed = 0;
    }
    else if (animElapsed >= FRAME_DURATION_MS)
    {
        animElapsed -= FRAME_DURATION_MS;
        currentFrame = (currentFrame + 1) % NUM_ANIM_FRAMES;

        for (const auto& conv : conveyors)
        {
            auto ent = ecsRef->getEntity(conv.entityId);
            if (not ent)
                continue;

            auto tex = ent->get<Texture2DComponent>();
            size_t frameIndex = conv.tileIndex * NUM_ANIM_FRAMES + currentFrame;
            tex->setTexture("Conveyor_Belt." + std::to_string(frameIndex));
        }
    }
}

void GridSystem::placeBuilding(size_t layer, int x, int y, const BuildingDef& def, size_t direction, size_t conveyorTileIndex, size_t enterDir)
{
    // Check all footprint cells are free, in bounds, and not on blocking terrain.
    for (int dy = 0; dy < def.getFootprintH(); ++dy)
        for (int dx = 0; dx < def.getFootprintW(); ++dx)
            if (not grid.isInBounds(x + dx, y + dy) or
                not grid.getCell(layer, x + dx, y + dy).tileName.empty() or
                isBlockingTerrain(getTerrainAt(x + dx, y + dy)))
            {
                LOG_INFO("GridSystem", "placeBuilding rejected for " << def.name
                        << " at (" << x << "," << y << ") — footprint cell ("
                        << (x + dx) << "," << (y + dy) << ") blocked");
                return;
            }

    uint8_t actualEnterDir = (enterDir == SIZE_MAX) ? static_cast<uint8_t>(direction) : static_cast<uint8_t>(enterDir);
    LOG_INFO("GridSystem", "placeBuilding " << def.name << " at (" << x << "," << y
            << ") direction=" << direction << " enterDir=" << static_cast<unsigned>(actualEnterDir)
            << " conveyorTileIndex=" << conveyorTileIndex);
    placeBuildingInternal(layer, x, y, def, direction, conveyorTileIndex, actualEnterDir);
    LOG_INFO("GridSystem", "sending BuildingPlacedEvent (" << def.name << " at " << x << "," << y << ")");
    sendEvent(BuildingPlacedEvent{x, y, def.name});
}

void GridSystem::placeBuildingInternal(size_t layer, int x, int y, const BuildingDef& def,
                                       size_t direction, size_t conveyorTileIndex, uint8_t enterDir)
{
    auto [worldX, worldY] = grid.gridToWorld(x, y);
    float z = grid.getLayer(layer).zIndex;
    uint64_t entityId = 0;

    int fpW = def.getFootprintW();
    int fpH = def.getFootprintH();

    if (def.isAnimated and not def.textureName.empty())
    {
        // Animated sprite (conveyor belts) — always full-size, no overflow
        size_t frameIndex = conveyorTileIndex * NUM_ANIM_FRAMES + currentFrame;
        std::string texName = def.textureName + "." + std::to_string(frameIndex);

        auto tex = make2DTexture(ecsRef,
            static_cast<float>(Grid::TILE_SIZE * def.gridW),
            static_cast<float>(Grid::TILE_SIZE * def.gridH),
            texName);

        auto pos = tex.get<PositionComponent>();
        pos->setX(worldX);
        pos->setY(worldY);
        pos->setZ(z);

        tex.get<ViewportComponent>()->setViewport(GAME_VIEWPORT);
        entityId = tex.entity->id;

        conveyors.push_back({entityId, conveyorTileIndex});
    }
    else if (not def.textureName.empty() and def.hasOverflow())
    {
        // Split sprite: base (footprint) at building z, overflow at higher z
        int oh = def.overflowH();

        // Base entity — bottom portion at footprint position
        std::string baseTex = def.textureName + "_base.0";
        auto base = make2DTexture(ecsRef,
            static_cast<float>(Grid::TILE_SIZE * fpW),
            static_cast<float>(Grid::TILE_SIZE * fpH),
            baseTex);

        auto basePos = base.get<PositionComponent>();
        basePos->setX(worldX);
        basePos->setY(worldY);
        basePos->setZ(z);

        base.get<ViewportComponent>()->setViewport(GAME_VIEWPORT);
        entityId = base.entity->id;

        // Overflow entity — top portion above the footprint
        std::string overflowTex = def.textureName + "_overflow.0";
        auto overflow = make2DTexture(ecsRef,
            static_cast<float>(Grid::TILE_SIZE * def.gridW),
            static_cast<float>(Grid::TILE_SIZE * oh),
            overflowTex);

        auto overflowPos = overflow.get<PositionComponent>();
        overflowPos->setX(worldX);
        overflowPos->setY(worldY - static_cast<float>(Grid::TILE_SIZE * oh));
        overflowPos->setZ(z + 0.5f);

        overflow.get<ViewportComponent>()->setViewport(GAME_VIEWPORT);
        overflowEntities[entityId] = overflow.entity->id;
    }
    else if (not def.textureName.empty())
    {
        // Static texture, no overflow (use first atlas frame)
        std::string texName = def.textureName + ".0";
        auto tex = make2DTexture(ecsRef,
            static_cast<float>(Grid::TILE_SIZE * def.gridW),
            static_cast<float>(Grid::TILE_SIZE * def.gridH),
            texName);

        auto pos = tex.get<PositionComponent>();
        pos->setX(worldX);
        pos->setY(worldY);
        pos->setZ(z);

        tex.get<ViewportComponent>()->setViewport(GAME_VIEWPORT);
        entityId = tex.entity->id;
    }
    else
    {
        // Colored square placeholder
        auto shape = makeSimple2DShape(ecsRef, Shape2D::Square, 0.0f, 0.0f, def.color);
        auto pos = shape.get<PositionComponent>();
        pos->setX(worldX);
        pos->setY(worldY);
        pos->setZ(z);
        pos->setWidth(static_cast<float>(Grid::TILE_SIZE * fpW));
        pos->setHeight(static_cast<float>(Grid::TILE_SIZE * fpH));

        shape.get<ViewportComponent>()->setViewport(GAME_VIEWPORT);
        entityId = shape.entity->id;
    }

    // Mark only footprint cells in the grid
    for (int dy = 0; dy < fpH; ++dy)
    {
        for (int dx = 0; dx < fpW; ++dx)
        {
            auto& cell = grid.getCell(layer, x + dx, y + dy);
            cell.tileName = def.name;
            cell.direction = static_cast<uint8_t>(direction);
            cell.enterDirection = enterDir;
            cell.ownerX = static_cast<int8_t>(x);
            cell.ownerY = static_cast<int8_t>(y);
            cell.isOwner = (dx == 0 and dy == 0);
            cell.entityId = (dx == 0 and dy == 0) ? entityId : 0;
        }
    }
}

void GridSystem::removeBuilding(size_t layer, int x, int y)
{
    if (not grid.isInBounds(x, y))
    {
        LOG_INFO("GridSystem", "removeBuilding: out of bounds (" << x << "," << y << ")");
        return;
    }

    auto& cell = grid.getCell(layer, x, y);
    if (cell.tileName.empty())
    {
        LOG_INFO("GridSystem", "removeBuilding: empty cell (" << x << "," << y << ")");
        return;
    }

    LOG_INFO("GridSystem", "removeBuilding " << cell.tileName << " requested at (" << x << "," << y
            << ") isOwner=" << cell.isOwner);

    // Find the owner cell
    int ox = cell.isOwner ? x : static_cast<int>(cell.ownerX);
    int oy = cell.isOwner ? y : static_cast<int>(cell.ownerY);
    auto& ownerCell = grid.getCell(layer, ox, oy);

    // Remove entity and its overflow (if any)
    if (ownerCell.entityId != 0)
    {
        auto it = overflowEntities.find(ownerCell.entityId);
        if (it != overflowEntities.end())
        {
            ecsRef->removeEntity(it->second);
            overflowEntities.erase(it);
        }

        removeConveyorEntry(ownerCell.entityId);
        ecsRef->removeEntity(ownerCell.entityId);
    }

    // Look up building def to know the footprint
    const BuildingDef* def = registry->findByName(ownerCell.tileName);
    std::string savedTileName = ownerCell.tileName;
    int w = def ? def->getFootprintW() : 1;
    int h = def ? def->getFootprintH() : 1;

    // Clear all cells in the footprint
    for (int dy = 0; dy < h; ++dy)
    {
        for (int dx = 0; dx < w; ++dx)
        {
            if (grid.isInBounds(ox + dx, oy + dy))
                grid.getCell(layer, ox + dx, oy + dy) = CellData{};
        }
    }

    LOG_INFO("GridSystem", "sending BuildingRemovedEvent (" << savedTileName
            << " at " << ox << "," << oy << ")");
    sendEvent(BuildingRemovedEvent{ox, oy, savedTileName});

    // Update adjacent belts that may now be disconnected
    for (int dy = 0; dy < h; ++dy)
        for (int dx = 0; dx < w; ++dx)
            updateNeighborBelts(layer, ox + dx, oy + dy);
}

bool GridSystem::isNeighborConnected(size_t layer, int x, int y, uint8_t checkDir) const
{
    int nx = x + DIR_DX[checkDir];
    int ny = y + DIR_DY[checkDir];

    if (not grid.isInBounds(nx, ny))
        return false;

    const auto& neighbor = grid.getCell(layer, nx, ny);
    if (neighbor.tileName.empty())
        return false;

    // Non-belt buildings (machines) are always connectable
    if (neighbor.tileName != "Conveyor")
        return true;

    // Belt neighbor: the neighbor connects on the side facing us if either
    // its enter or exit direction points towards/away from us on that side.
    // The side of the neighbor facing us is the opposite of checkDir.
    uint8_t sideOfNeighborFacingUs = (checkDir + 2) % 4;

    // Neighbor's exit side faces us
    if (neighbor.direction == sideOfNeighborFacingUs)
        return true;

    // Neighbor's enter side faces us (enter direction is where items come FROM,
    // so the enter side is the opposite of enterDirection)
    uint8_t neighborEnterSide = (neighbor.enterDirection + 2) % 4;
    if (neighborEnterSide == sideOfNeighborFacingUs)
        return true;

    return false;
}

void GridSystem::updateConveyorTileIndex(size_t layer, int x, int y, size_t newTileIndex)
{
    if (not grid.isInBounds(x, y))
        return;

    auto& cell = grid.getCell(layer, x, y);
    if (cell.tileName != "Conveyor" or cell.entityId == 0)
        return;

    for (auto& conv : conveyors)
    {
        if (conv.entityId == cell.entityId)
        {
            conv.tileIndex = newTileIndex;

            auto ent = ecsRef->getEntity(conv.entityId);
            if (ent)
            {
                size_t frameIndex = newTileIndex * NUM_ANIM_FRAMES + currentFrame;
                ent->get<Texture2DComponent>()->setTexture(
                    "Conveyor_Belt." + std::to_string(frameIndex));
            }
            break;
        }
    }
}

void GridSystem::resolveAndUpdateBelt(size_t layer, int x, int y)
{
    if (not grid.isInBounds(x, y))
        return;

    const auto& cell = grid.getCell(layer, x, y);
    if (cell.tileName != "Conveyor" or cell.entityId == 0)
        return;

    // Check if this is a corner (tileIndex 0-7) — corners are always connected, skip
    for (const auto& conv : conveyors)
    {
        if (conv.entityId == cell.entityId)
        {
            if (conv.tileIndex <= CORNER_CCW_1_1)
                return;
            break;
        }
    }

    uint8_t exitDir = cell.direction;
    uint8_t backDir = (exitDir + 2) % 4;

    bool connectedFront = isNeighborConnected(layer, x, y, exitDir);
    bool connectedBack  = isNeighborConnected(layer, x, y, backDir);

    size_t newTileIndex = resolveLineTileVariant(exitDir, connectedBack, connectedFront);
    updateConveyorTileIndex(layer, x, y, newTileIndex);
}

void GridSystem::updateNeighborBelts(size_t layer, int x, int y)
{
    for (int dir = 0; dir < 4; ++dir)
    {
        resolveAndUpdateBelt(layer, x + DIR_DX[dir], y + DIR_DY[dir]);
    }
}

void GridSystem::generateAndRenderTerrain(uint32_t seed)
{
    GenerationParams params;
    params.seed = seed;
    params.requiredOres = {TerrainType::OreIron, TerrainType::OreCopper};

    auto gen = CanvasGenerator::generate(params);
    terrainGrid = gen.terrain;
    orePatches = gen.orePatches;
    treeInstances = gen.trees;

    renderTerrain();
}

void GridSystem::renderTerrain()
{
    // All terrain stays inside integer z planes that sit below the building
    // layer (z=2) and item layer (z=3). Within the same z-plane, render
    // order controls stacking — we draw ground first, then grass, then
    // single-tile overlays, and finally trees in a separate pass.
    constexpr float GROUND_Z  = 0.0f;
    constexpr float GRASS_Z   = 1.0f;
    constexpr float OVERLAY_Z = 1.0f;
    constexpr float TREE_Z    = 1.0f;

    // Two-pass autotile: cells whose grass autotile can't be represented
    // (encircled by ore, etc.) must render as plain dirt, AND their
    // neighbors must treat them as non-grass so the dirt edge continues
    // cleanly instead of turning into a stray grass corner. We compute
    // `renderAsDirt` to a fixpoint first, then consult it during both the
    // render-loop skip decision and the autotile neighbor lookup.
    RenderAsDirtGrid renderAsDirt{};
    bool changed = true;
    while (changed)
    {
        changed = false;
        for (int y = 0; y < Grid::HEIGHT; ++y)
        {
            for (int x = 0; x < Grid::WIDTH; ++x)
            {
                TerrainType t = terrainGrid[y][x];
                if (t != TerrainType::Grass
                 and t != TerrainType::Tree
                 and t != TerrainType::Rock)
                    continue;
                if (renderAsDirt[y][x])
                    continue;
                if (grassAutotileFrame(x, y, renderAsDirt) == GRASS_AS_DIRT)
                {
                    renderAsDirt[y][x] = true;
                    changed = true;
                }
            }
        }
    }

    // Per-cell pass: ground base + grass + single-tile overlays (ores, rocks).
    // Trees are rendered afterwards from the instance list so each occupies
    // exactly one 2x3 sprite anchored to its footprint.
    for (int y = 0; y < Grid::HEIGHT; ++y)
    {
        for (int x = 0; x < Grid::WIDTH; ++x)
        {
            auto [worldX, worldY] = grid.gridToWorld(x, y);

            // Always draw a ground base so gaps between non-grass tiles look natural.
            spawnTextureTile("Ground.0", worldX, worldY, GROUND_Z,
                Grid::TILE_SIZE, Grid::TILE_SIZE);

            TerrainType t = terrainGrid[y][x];

            // Grass is painted under grass, tree, and rock cells so that
            // transparent pixels in the tree/rock sprites show grass (not
            // dirt) behind them, and so the environment looks uniformly
            // green between decorations. Near ore patches the grass
            // auto-tiles to a dirt edge via grassAutotileFrame. Cells
            // flagged by the fixpoint above skip the overlay entirely so
            // the Ground.0 base shows through as plain dirt.
            if ((t == TerrainType::Grass
              or t == TerrainType::Tree
              or t == TerrainType::Rock)
             and not renderAsDirt[y][x])
            {
                uint16_t frame = grassAutotileFrame(x, y, renderAsDirt);
                spawnTextureTile("Environment_Tileset." + std::to_string(frame),
                    worldX, worldY, GRASS_Z, Grid::TILE_SIZE, Grid::TILE_SIZE);
            }

            if (isOre(t))
            {
                uint16_t frame = oreAutotileFrame(x, y, t);
                spawnTextureTile("Environment_Tileset." + std::to_string(frame),
                    worldX, worldY, OVERLAY_Z, Grid::TILE_SIZE, Grid::TILE_SIZE);
            }
            else if (t == TerrainType::Rock)
            {
                spawnTextureTile("Rock_Tile.0", worldX, worldY, OVERLAY_Z,
                    Grid::TILE_SIZE, Grid::TILE_SIZE);
            }
            // Grass handled above; trees are rendered in the pass below.
        }
    }

    // Whole-struct tree pass: one 2x3 sprite per TreeInstance, anchored flush with
    // its top-left cell so trees never clip past the canvas edges.
    for (const auto& tree : treeInstances)
    {
        auto [tx, ty] = grid.gridToWorld(tree.anchorX, tree.anchorY);
        spawnTextureTile("Tree.0", tx, ty, TREE_Z,
            static_cast<float>(Grid::TILE_SIZE * TREE_W),
            static_cast<float>(Grid::TILE_SIZE * TREE_H));
    }
}

uint16_t GridSystem::grassAutotileFrame(int x, int y, const RenderAsDirtGrid& renderAsDirt) const
{
    constexpr uint16_t ATLAS_COLS = 12;
    constexpr uint16_t GRASS_BASE = 13;
    static constexpr uint16_t GRASS_VARIATIONS[] = { 10, 11, 22, 23 };
    constexpr uint32_t VARIATION_EVERY = 12;

    auto isGrassLike = [&](int nx, int ny)
    {
        if (not grid.isInBounds(nx, ny))
            return true;
        if (isOre(terrainGrid[ny][nx]))
            return false;
        // A grass/tree/rock cell flagged as dirt by the fixpoint pass is
        // visually dirt, so it must break up grass autotiling just like
        // an ore cell would.
        if (renderAsDirt[ny][nx])
            return false;
        return true;
    };

    bool hasN = isGrassLike(x,     y - 1);
    bool hasS = isGrassLike(x,     y + 1);
    bool hasW = isGrassLike(x - 1, y);
    bool hasE = isGrassLike(x + 1, y);

    // The 9-tile autotile can only express 0/1/2 ore sides, and when
    // there are exactly 2 ore sides they must be adjacent (forming a
    // corner). Anything else has no valid tile, so render as dirt.
    int oreSides = 0;
    if (not hasN)
        ++oreSides;
    if (not hasS)
        ++oreSides;
    if (not hasW)
        ++oreSides;
    if (not hasE)
        ++oreSides;

    const bool oppositeOnly = (oreSides == 2) and
        (((not hasN) and (not hasS)) or ((not hasW) and (not hasE)));

    if (oreSides >= 3 or oppositeOnly)
        return GRASS_AS_DIRT;

    int localRow;
    if (not hasN and hasS)
        localRow = 0;
    else if (hasN and not hasS)
        localRow = 2;
    else
        localRow = 1;

    int localCol;
    if (not hasW and hasE)
        localCol = 0;
    else if (hasW and not hasE)
        localCol = 2;
    else
        localCol = 1;

    // If we landed on the center cell (all 4 cardinals are grass-like),
    // check diagonals for interior-corner transitions. Priority order
    // NW > NE > SW > SE is arbitrary but deterministic.
    if (localRow == 1 and localCol == 1 and hasN and hasS and hasE and hasW)
    {
        bool hasNW = isGrassLike(x - 1, y - 1);
        bool hasNE = isGrassLike(x + 1, y - 1);
        bool hasSW = isGrassLike(x - 1, y + 1);
        bool hasSE = isGrassLike(x + 1, y + 1);

        // Interior corner frames: cols 3-4, rows 0 and 2.
        // The tileset's top row of inner corners shows the SOUTH notches
        // (dirt bleeds in from the bottom), and the bottom row shows the
        // NORTH notches. The column ordering mirrors across rows (east-
        // then-west on the top, west-then-east on the bottom):
        //   frame  3 (col 3 row 0) -> SE inner corner
        //   frame  4 (col 4 row 0) -> SW inner corner
        //   frame 16 (col 3 row 2) -> NW inner corner
        //   frame 15 (col 4 row 1) -> NE inner corner
        if (not hasSE)
            return 3;
        if (not hasSW)
            return 4;
        if (not hasNW)
            return 16;
        if (not hasNE)
            return 15;

        // Fully surrounded by grass: use the base fill tile, sprinkling in
        // rare variations so the interior doesn't look flat.
        uint32_t h = static_cast<uint32_t>(x) * 73856093u
                   ^ static_cast<uint32_t>(y) * 19349663u;
        if (h % VARIATION_EVERY == 0)
            return GRASS_VARIATIONS[(h / VARIATION_EVERY)
                % (sizeof(GRASS_VARIATIONS) / sizeof(GRASS_VARIATIONS[0]))];
        return GRASS_BASE;
    }

    return static_cast<uint16_t>(localRow * ATLAS_COLS + localCol);
}

uint16_t GridSystem::oreAutotileFrame(int x, int y, TerrainType t) const
{
    // Atlas is row-major with 12 cols (see application.cpp registration).
    constexpr uint16_t ATLAS_COLS = 12;

    // Top-left frame index of each ore's 3x3 block.
    uint16_t baseOffset;
    switch (t)
    {
    case TerrainType::OreStone:
        baseOffset = 18 * ATLAS_COLS + 0; // 216
        break;
    case TerrainType::OreCoal:
        baseOffset = 18 * ATLAS_COLS + 3; // 219
        break;
    case TerrainType::OreIron:
        baseOffset = 18 * ATLAS_COLS + 6; // 222
        break;
    case TerrainType::OreCopper:
        baseOffset = 18 * ATLAS_COLS + 9; // 225
        break;
    default:
        baseOffset = 18 * ATLAS_COLS + 6;
        break;
    }

    auto sameOre = [&](int nx, int ny)
    {
        if (not grid.isInBounds(nx, ny))
            return false;
        return terrainGrid[ny][nx] == t;
    };

    bool hasN = sameOre(x,     y - 1);
    bool hasS = sameOre(x,     y + 1);
    bool hasW = sameOre(x - 1, y);
    bool hasE = sameOre(x + 1, y);

    // Pick the sub-row: top row if the cell has open sky above, bottom row
    // if it has open sky below, middle otherwise (includes the "fully
    // surrounded" and "fully isolated" cases, both of which render as the
    // solid center tile).
    int localRow;
    if (not hasN and hasS)
        localRow = 0;
    else if (hasN and not hasS)
        localRow = 2;
    else
        localRow = 1;

    int localCol;
    if (not hasW and hasE)
        localCol = 0;
    else if (hasW and not hasE)
        localCol = 2;
    else
        localCol = 1;

    return static_cast<uint16_t>(baseOffset + localRow * ATLAS_COLS + localCol);
}

void GridSystem::spawnTextureTile(const std::string& texName, float worldX, float worldY, float z,
                                  float width, float height)
{
    auto tex = make2DTexture(ecsRef, width, height, texName);
    auto pos = tex.get<PositionComponent>();
    pos->setX(worldX);
    pos->setY(worldY);
    pos->setZ(z);
    tex.get<ViewportComponent>()->setViewport(GAME_VIEWPORT);
    bgEntities.push_back(tex.entity->id);
}

void GridSystem::createConveyorEntity(CellData& cell, float worldX, float worldY, float z, size_t tileIndex)
{
    // Use the current global animation frame so new conveyors are immediately in sync
    size_t frameIndex = tileIndex * NUM_ANIM_FRAMES + currentFrame;
    std::string texName = "Conveyor_Belt." + std::to_string(frameIndex);

    auto tex = make2DTexture(ecsRef,
        static_cast<float>(Grid::TILE_SIZE),
        static_cast<float>(Grid::TILE_SIZE),
        texName);

    auto pos = tex.get<PositionComponent>();
    pos->setX(worldX);
    pos->setY(worldY);
    pos->setZ(z);

    tex.get<ViewportComponent>()->setViewport(GAME_VIEWPORT);

    cell.entityId = tex.entity->id;

    conveyors.push_back({cell.entityId, tileIndex});
}

void GridSystem::removeConveyorEntry(uint64_t entityId)
{
    conveyors.erase(
        std::remove_if(conveyors.begin(), conveyors.end(),
            [entityId](const ConveyorEntry& e) { return e.entityId == entityId; }),
        conveyors.end());
}

