#pragma once

#include "Systems/basicsystems.h"
#include "2D/simple2dobject.h"
#include "2D/texture.h"

#include "gridsystem.h"
#include "itemregistry.h"

using namespace pg;

struct BeltCell
{
    ItemId   itemId   = ITEM_NONE;
    uint64_t entityId = 0;  // ECS visual entity for the item sprite
};

struct BeltItemGrid
{
    static constexpr int WIDTH  = Grid::WIDTH;
    static constexpr int HEIGHT = Grid::HEIGHT;

    std::array<std::array<BeltCell, WIDTH>, HEIGHT> cells = {};

    BeltCell& get(int x, int y) { return cells[y][x]; }
    const BeltCell& get(int x, int y) const { return cells[y][x]; }
};

class TransportSystem : public System<InitSys, Listener<TickEvent>, Listener<BuildingRemovedEvent>>
{
public:
    static constexpr size_t TRANSPORT_TICK_MS = 250;

    TransportSystem(GridSystem* gridSystem, ItemRegistry* itemRegistry)
        : gridSystem(gridSystem), itemRegistry(itemRegistry) {}

    virtual std::string getSystemName() const override { return "Transport System"; }

    void init() override {}

    virtual void onEvent(const TickEvent& event) override
    {
        tickAccumulator += static_cast<size_t>(event.tick);
    }

    virtual void onEvent(const BuildingRemovedEvent& event) override
    {
        if (event.tileId == 4)
        {
            destroyItemVisual(event.x, event.y);
            beltGrid.get(event.x, event.y).itemId = ITEM_NONE;
        }
    }

    void execute() override
    {
        while (tickAccumulator >= TRANSPORT_TICK_MS)
        {
            tickAccumulator -= TRANSPORT_TICK_MS;
            transportTick();
        }
    }

    bool tryPlaceItem(int x, int y, ItemId id)
    {
        if (not gridSystem->getGrid().isInBounds(x, y)) return false;

        auto& cell = beltGrid.get(x, y);
        if (cell.itemId != ITEM_NONE) return false;

        const auto& buildingCell = gridSystem->getGrid().getCell(
            gridSystem->getBuildingLayer(), x, y);
        if (buildingCell.tileId != 4) return false;

        cell.itemId = id;
        createItemVisual(x, y, id);
        return true;
    }

    ItemId tryTakeItem(int x, int y)
    {
        if (not gridSystem->getGrid().isInBounds(x, y)) return ITEM_NONE;

        auto& cell = beltGrid.get(x, y);
        if (cell.itemId == ITEM_NONE) return ITEM_NONE;

        ItemId taken = cell.itemId;
        cell.itemId = ITEM_NONE;
        destroyItemVisual(x, y);
        return taken;
    }

    ItemId peekItem(int x, int y) const
    {
        if (not gridSystem->getGrid().isInBounds(x, y)) return ITEM_NONE;
        return beltGrid.get(x, y).itemId;
    }

    const BeltItemGrid& getBeltGrid() const { return beltGrid; }

private:
    struct MoveEntry
    {
        int fromX, fromY;
        int toX, toY;
        ItemId itemId;
    };

    static uint8_t travelDirection(int fromX, int fromY, int toX, int toY)
    {
        int dx = toX - fromX;
        int dy = toY - fromY;
        if (dx > 0) return 0;
        if (dy > 0) return 1;
        if (dx < 0) return 2;
        return 3;
    }

    void transportTick()
    {
        size_t buildingLayer = gridSystem->getBuildingLayer();
        const auto& grid = gridSystem->getGrid();

        std::vector<MoveEntry> moves;

        // Phase 1: collect intended moves for all belt items
        for (int y = 0; y < Grid::HEIGHT; ++y)
        {
            for (int x = 0; x < Grid::WIDTH; ++x)
            {
                auto& beltCell = beltGrid.get(x, y);
                if (beltCell.itemId == ITEM_NONE)
                    continue;

                const auto& cell = grid.getCell(buildingLayer, x, y);
                if (cell.tileId != 4)
                    continue;

                uint8_t dir = cell.direction;
                int nx = x + DIR_DX[dir];
                int ny = y + DIR_DY[dir];

                if (not grid.isInBounds(nx, ny))
                    continue;

                const auto& nextCell = grid.getCell(buildingLayer, nx, ny);

                // Only move to another belt cell (machine transfers are handled by CraftingSystem)
                if (nextCell.tileId == 4)
                    moves.push_back({x, y, nx, ny, beltCell.itemId});
            }
        }

        // Phase 2a: count targets and detect cycles
        std::array<std::array<uint8_t, Grid::WIDTH>, Grid::HEIGHT> targetCount = {};
        for (const auto& m : moves)
            targetCount[m.toY][m.toX]++;

        // Build spatial index: which move originates from each cell?
        std::array<std::array<int, Grid::WIDTH>, Grid::HEIGHT> moveOrigin;
        for (auto& row : moveOrigin) row.fill(-1);
        for (size_t i = 0; i < moves.size(); ++i)
            moveOrigin[moves[i].fromY][moves[i].fromX] = static_cast<int>(i);

        // Detect cycles via chain-following (only through uncontested cells)
        std::vector<bool> inCycle(moves.size(), false);
        std::vector<uint8_t> visited(moves.size(), 0); // 0=unvisited, 1=in-progress, 2=done

        for (size_t i = 0; i < moves.size(); ++i)
        {
            if (visited[i] != 0) continue;

            std::vector<size_t> chain;
            size_t cur = i;

            while (true)
            {
                if (visited[cur] == 2) break;
                if (visited[cur] == 1)
                {
                    // Found a cycle — mark all moves from cur onwards in the chain
                    size_t j = 0;
                    while (chain[j] != cur) ++j;
                    for (; j < chain.size(); ++j)
                        inCycle[chain[j]] = true;
                    break;
                }

                visited[cur] = 1;
                chain.push_back(cur);

                const auto& m = moves[cur];
                // Only follow through uncontested destinations
                if (targetCount[m.toY][m.toX] > 1) break;
                int next = moveOrigin[m.toY][m.toX];
                if (next < 0) break;
                cur = static_cast<size_t>(next);
            }

            for (size_t idx : chain)
                visited[idx] = 2;
        }

        // Phase 2b: apply cycle moves atomically (snapshot entity IDs first)
        std::vector<std::pair<size_t, uint64_t>> cycleEntityIds;
        for (size_t i = 0; i < moves.size(); ++i)
        {
            if (not inCycle[i]) continue;
            cycleEntityIds.push_back({i, beltGrid.get(moves[i].fromX, moves[i].fromY).entityId});
        }

        for (size_t i = 0; i < moves.size(); ++i)
        {
            if (not inCycle[i]) continue;
            beltGrid.get(moves[i].toX, moves[i].toY).itemId = moves[i].itemId;
        }

        for (const auto& [idx, entityId] : cycleEntityIds)
        {
            const auto& m = moves[idx];
            beltGrid.get(m.toX, m.toY).entityId = entityId;

            if (entityId != 0)
            {
                auto ent = ecsRef->getEntity(entityId);
                if (ent)
                {
                    float itemSize = static_cast<float>(Grid::TILE_SIZE) * 0.6f;
                    float offset = (Grid::TILE_SIZE - itemSize) * 0.5f;
                    auto [worldX, worldY] = grid.gridToWorld(m.toX, m.toY);
                    auto pos = ent->get<PositionComponent>();
                    pos->setX(worldX + offset);
                    pos->setY(worldY + offset);
                }
            }
        }

        // Phase 2c: round-robin for contested non-cycle cells
        std::array<std::array<int, Grid::WIDTH>, Grid::HEIGHT> winnerMoveIdx;
        for (auto& row : winnerMoveIdx) row.fill(-1);

        for (size_t i = 0; i < moves.size(); ++i)
        {
            if (inCycle[i]) continue;

            const auto& m = moves[i];
            if (targetCount[m.toY][m.toX] <= 1)
                continue;

            uint8_t dir = travelDirection(m.fromX, m.fromY, m.toX, m.toY);
            uint8_t last = lastServedDir[m.toY][m.toX];
            uint8_t dist = (dir + 4 - last - 1) % 4;

            int cur = winnerMoveIdx[m.toY][m.toX];
            if (cur < 0)
            {
                winnerMoveIdx[m.toY][m.toX] = static_cast<int>(i);
            }
            else
            {
                uint8_t curDir = travelDirection(moves[cur].fromX, moves[cur].fromY, m.toX, m.toY);
                uint8_t curDist = (curDir + 4 - last - 1) % 4;
                if (dist < curDist)
                    winnerMoveIdx[m.toY][m.toX] = static_cast<int>(i);
            }
        }

        // Phase 2d: determine which non-cycle moves are allowed
        std::vector<bool> allowed(moves.size(), false);
        for (size_t i = 0; i < moves.size(); ++i)
        {
            if (inCycle[i]) continue;
            const auto& m = moves[i];
            allowed[i] = (targetCount[m.toY][m.toX] <= 1)
                      or (winnerMoveIdx[m.toY][m.toX] == static_cast<int>(i));
        }

        // Phase 2e: compute canMove[] via chain propagation
        // A move can proceed if it's allowed AND its destination will be free
        // (either already empty, or the item there will also move away)
        std::vector<bool> canMove(moves.size(), false);

        for (size_t i = 0; i < moves.size(); ++i)
        {
            if (inCycle[i] or not allowed[i] or canMove[i]) continue;

            // Trace chain forward to find if it ends at a free cell
            std::vector<size_t> chain;
            size_t cur = i;
            bool reachesEmpty = false;

            while (true)
            {
                if (canMove[cur])
                {
                    // Destination item is already known to move away
                    reachesEmpty = true;
                    break;
                }

                if (inCycle[cur])
                    break; // Full cycle cells stay occupied — chain cannot enter

                if (not allowed[cur])
                    break;

                // Detect undetected cycles (loops with a contested entry point)
                bool revisit = false;
                for (size_t idx : chain)
                    if (idx == cur) { revisit = true; break; }
                if (revisit)
                    break;

                chain.push_back(cur);

                const auto& m = moves[cur];
                if (beltGrid.get(m.toX, m.toY).itemId == ITEM_NONE)
                {
                    reachesEmpty = true;
                    break;
                }

                // Follow to the move originating from destination cell
                int next = moveOrigin[m.toY][m.toX];
                if (next < 0) break;
                cur = static_cast<size_t>(next);
            }

            if (reachesEmpty)
            {
                for (size_t idx : chain)
                    canMove[idx] = true;
            }
        }

        // Phase 2f: apply canMove moves iteratively (tail-first)
        // Process moves whose destination is currently empty; as items move they
        // free cells for the next item in the chain, eliminating scan-order bias.
        std::vector<bool> applied(moves.size(), false);
        bool progress = true;
        while (progress)
        {
            progress = false;
            for (size_t i = 0; i < moves.size(); ++i)
            {
                if (not canMove[i] or applied[i]) continue;

                const auto& m = moves[i];
                if (beltGrid.get(m.toX, m.toY).itemId != ITEM_NONE)
                    continue;

                beltGrid.get(m.toX, m.toY).itemId = m.itemId;
                beltGrid.get(m.fromX, m.fromY).itemId = ITEM_NONE;
                moveItemVisual(m.fromX, m.fromY, m.toX, m.toY);

                if (targetCount[m.toY][m.toX] > 1)
                    lastServedDir[m.toY][m.toX] = travelDirection(m.fromX, m.fromY, m.toX, m.toY);

                applied[i] = true;
                progress = true;
            }
        }
    }

    void createItemVisual(int x, int y, ItemId id)
    {
        const auto& def = itemRegistry->get(id);
        auto [worldX, worldY] = gridSystem->getGrid().gridToWorld(x, y);
        float z = gridSystem->getGrid().getLayer(gridSystem->getItemLayer()).zIndex;

        float itemSize = static_cast<float>(Grid::TILE_SIZE) * 0.6f;
        float offset = (Grid::TILE_SIZE - itemSize) * 0.5f;

        uint64_t entityId = 0;

        if (not def.textureName.empty())
        {
            auto tex = make2DTexture(ecsRef, itemSize, itemSize, def.textureName);
            auto pos = tex.get<PositionComponent>();
            pos->setX(worldX + offset);
            pos->setY(worldY + offset);
            pos->setZ(z);
            tex.get<Texture2DComponent>()->setViewport(GAME_VIEWPORT);
            entityId = tex.entity->id;
        }
        else
        {
            // Color fallback: items are yellow-ish squares
            constant::Vector4D color = {200.0f, 200.0f, 60.0f, 255.0f};
            auto shape = makeSimple2DShape(ecsRef, Shape2D::Square, 0.0f, 0.0f, color);
            auto pos = shape.get<PositionComponent>();
            pos->setX(worldX + offset);
            pos->setY(worldY + offset);
            pos->setZ(z);
            pos->setWidth(itemSize);
            pos->setHeight(itemSize);
            shape.get<Simple2DObject>()->setViewport(GAME_VIEWPORT);
            entityId = shape.entity->id;
        }

        beltGrid.get(x, y).entityId = entityId;
    }

    void destroyItemVisual(int x, int y)
    {
        auto& cell = beltGrid.get(x, y);
        if (cell.entityId != 0)
        {
            ecsRef->removeEntity(cell.entityId);
            cell.entityId = 0;
        }
    }

    void moveItemVisual(int fromX, int fromY, int toX, int toY)
    {
        auto& src = beltGrid.get(fromX, fromY);
        auto& dst = beltGrid.get(toX, toY);

        dst.entityId = src.entityId;
        src.entityId = 0;

        if (dst.entityId != 0)
        {
            auto ent = ecsRef->getEntity(dst.entityId);
            if (ent)
            {
                float itemSize = static_cast<float>(Grid::TILE_SIZE) * 0.6f;
                float offset = (Grid::TILE_SIZE - itemSize) * 0.5f;
                auto [worldX, worldY] = gridSystem->getGrid().gridToWorld(toX, toY);
                auto pos = ent->get<PositionComponent>();
                pos->setX(worldX + offset);
                pos->setY(worldY + offset);
            }
        }
    }

    GridSystem* gridSystem = nullptr;
    ItemRegistry* itemRegistry = nullptr;

    BeltItemGrid beltGrid;
    size_t tickAccumulator = 0;

    // Round-robin state: last travel direction served at each cell
    std::array<std::array<uint8_t, Grid::WIDTH>, Grid::HEIGHT> lastServedDir = {};
};
