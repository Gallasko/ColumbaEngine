#include "collisionsystem.h"

namespace pg
{
    namespace
    {
        constexpr const char * const DOM = "Collision System";

        static constexpr float EPSILON = 1e-5f;
    }

    // Todo fix scaling !

    /// Returns one of the four cardinal normals (+/–X or +/–Y)
    inline constant::Vector2D computeBoxNormal(const AABB& box, const constant::Vector2D& hitPoint)
    {
        // distance to each face
        float dLeft   =  hitPoint.x - box.minX;
        float dRight  =  box.maxX  - hitPoint.x;
        float dBottom =  hitPoint.y - box.minY;
        float dTop    =  box.maxY  - hitPoint.y;

        // find minimum penetration
        float minDist = std::min({ dLeft, dRight, dBottom, dTop });

        if (minDist == dLeft) {
            return { -1.f,  0.f };     // hit left face
        }
        else if (minDist == dRight) {
            return {  1.f,  0.f };     // hit right face
        }
        else if (minDist == dBottom) {
            return {  0.f, -1.f };     // hit bottom face
        }
        else {
            return {  0.f,  1.f };     // hit top face
        }
    }

    // returns t in [0,1] where ray(origin→end) first hits box,
    // or std::nullopt if no intersection.
    std::optional<float> rayAABB(const constant::Vector2D& origin, const constant::Vector2D& dir, float length, const AABB& box)
    {
        // t_enter/t_exit track the overlapping interval of the ray on all axes
        float tEnter = 0.0f;
        float tExit  = length;

        // === X axis slab ===
        if (std::abs(dir.x) < 1e-6f)
        {
            // Ray is parallel to X planes: if origin.x is outside box, no hit
            if (origin.x < box.minX or origin.x > box.maxX)
                return std::nullopt;
        }
        else
        {
            // Compute intersection distances with the two X planes
            float invDx = 1.0f / dir.x;
            float t1    = (box.minX - origin.x) * invDx;
            float t2    = (box.maxX - origin.x) * invDx;

            // Order them so t1 = entering, t2 = exiting
            if (t1 > t2)
                std::swap(t1, t2);

            // Narrow our valid interval
            tEnter = std::max(tEnter, t1);
            tExit  = std::min(tExit,  t2);

            // If interval is empty, ray misses
            if (tEnter > tExit)
                return std::nullopt;
        }

        // === Y axis slab ===
        if (std::abs(dir.y) < 1e-6f)
        {
            if (origin.y < box.minY or origin.y > box.maxY)
                return std::nullopt;
        }
        else
        {
            float invDy = 1.0f / dir.y;
            float t1    = (box.minY - origin.y) * invDy;
            float t2    = (box.maxY - origin.y) * invDy;

            if (t1 > t2)
                std::swap(t1, t2);

            tEnter = std::max(tEnter, t1);
            tExit  = std::min(tExit,  t2);

            if (tEnter > tExit)
                return std::nullopt;
        }

        // If the first valid intersection (tEnter) is within [0..length], we hit
        if (tEnter >= 0.0f and tEnter <= length)
        {
            return tEnter;
        }

        // Otherwise we missed (or maybe the box is “behind” us)
        return std::nullopt;
    }

    void CollisionComponent::onCreation(EntityRef entity)
    {
        ecsRef = entity->world();

        entityId = entity->id;

        // Immediately insert into the grid if we already have a PositionComponent
        if (entity->has<PositionComponent>())
        {
            auto pos = entity->get<PositionComponent>();

            if (auto sys = ecsRef->getSystem<CollisionSystem>())
            {
                sys->addComponentInGrid(pos, this);
            }
        }
    }

    void CollisionComponent::onDeletion(EntityRef)
    {
        LOG_THIS_MEMBER("Collision Component");

        auto sys = ecsRef->getSystem<CollisionSystem>();

        sys->removeComponentFromGrid(this);
    }

    void CollisionPage::addId(CollisionComponent& comp, const constant::Vector2D& startPos, const constant::Vector2D& objSize)
    {
        // compute integer cell indices (floored)
        int startX = static_cast<int>(std::floor(startPos.x / cellSize.x));
        int startY = static_cast<int>(std::floor(startPos.y / cellSize.y));

        // similarly floor the end
        int rawEndX = static_cast<int>(std::floor((startPos.x + objSize.x) / cellSize.x)) + 1;
        int rawEndY = static_cast<int>(std::floor((startPos.y + objSize.y) / cellSize.y)) + 1;

        // clamp into [0..size.x] / [0..size.y]
        int endX = std::min(static_cast<int>(size.x), std::max(0, rawEndX));
        int endY = std::min(static_cast<int>(size.y), std::max(0, rawEndY));

        // clamp start as well
        startX = std::min(static_cast<int>(size.x), std::max(0, startX));
        startY = std::min(static_cast<int>(size.y), std::max(0, startY));

        for (int y = startY; y < endY; y++)
        {
            for (int x = startX; x < endX; x++)
            {
                auto inserted = cells[x + y * size.x].ids.insert(comp.entityId);

                if (inserted.second)
                {
                    // LOG_INFO(DOM, "Insert entity " << comp.entityId << " in cell " << x << ", " << y);
                    comp.cells[pos].emplace_back(&cells[x + y * size.x]);
                }
                else
                {
                    // LOG_INFO(DOM, "Entity " << comp.entityId << " already in cell " << x << ", " << y);
                }
            }
        }
    }

    // Todo issue here when cellsize != pagesize
    // CollisionSystem::CollisionSystem() : pageSize(40, 40), cellSize(40, 40)
    CollisionSystem::CollisionSystem() : pageSize(10, 10), cellSize(20, 20)
    {
        LOG_THIS_MEMBER(DOM);
    }

    void CollisionSystem::init()
    {
        LOG_THIS_MEMBER(DOM);

        auto group = registerGroup<PositionComponent, CollisionComponent>();

        group->addOnGroup([this](EntityRef entity) {
            LOG_MILE(DOM, "Add entity " << entity->id << " to ui - collision group !");

            auto ui = entity->get<PositionComponent>();
            auto collision = entity->get<CollisionComponent>();

            addComponentInGrid(ui, collision);
        });

        // group->removeOfGroup([](EntitySystem* ecsRef, _unique_id id) {
        //     LOG_INFO(DOM, "Remove entity " << id << " of ui - collision group !");

        //     auto sys = ecsRef->getSystem<CollisionComponent>();

        //     sys->removeElement(id);
        // });
    }

    void CollisionSystem::addComponentInGrid(PositionComponent* pos, CollisionComponent* comp)
    {
        LOG_THIS_MEMBER(DOM);

        // LOG_INFO(DOM, "Add new entity in the grid !");

        float height = pos->height;
        float width = pos->width;

        if (height == 0 or width == 0)
        {
            LOG_INFO(DOM, "Object has no area so no collision needed");
            return;
        }

        float x = pos->x;
        float y = pos->y;

        x -= (width * (comp->scale - 1)) / 2.0f;
        y -= (height * (comp->scale - 1)) / 2.0f;

        auto pWidth = (pageSize.x * cellSize.x);
        auto pHeight = (pageSize.y * cellSize.y);

        int xPagePos = static_cast<int>(std::floor(x / pWidth));
        int yPagePos = static_cast<int>(std::floor(y / pHeight));

        int startY = y - yPagePos * pHeight;

        int usedWidth = 0, usedHeight = 0;

        int startingXPos = xPagePos;

        comp->firstCellX = static_cast<int>(std::floor(x / cellSize.x));
        comp->firstCellY = static_cast<int>(std::floor(y / cellSize.y));

        for (float remainingHeight = height * comp->scale; remainingHeight > 0; remainingHeight -= usedHeight)
        {
            // int startX = x > 0 ? x - xPagePos * pWidth : xPagePos * pWidth - x;
            int startX = x - xPagePos * pWidth;

            for (float remainingWidth = width * comp->scale; remainingWidth > 0; remainingWidth -= usedWidth)
            {
                PagePos key = {xPagePos, yPagePos};

                auto it = loadedPages[comp->layerId].find(key);
                if (it == loadedPages[comp->layerId].end())
                {
                    it = loadedPages[comp->layerId].emplace(key, CollisionPage{key, pageSize, cellSize}).first;
                }

                it->second.addId(*comp, {startX, startY}, {remainingWidth, remainingHeight});

                usedWidth = pWidth - startX;

                xPagePos++;

                startX = 0;
            }

            xPagePos = startingXPos;

            usedHeight = pHeight - startY;

            yPagePos++;

            startY = 0;
        }

        comp->inserted = true;

        resolveCollisionList(pos, comp);
    }

    void CollisionSystem::removeComponentFromGrid(CollisionComponent* comp)
    {
        LOG_THIS_MEMBER(DOM);

        std::set<PagePos> affectedPages;

        for (auto layer : comp->cells)
        {
            affectedPages.insert(layer.first);

            for (auto cell : layer.second)
            {
                cell->ids.erase(comp->entityId);
            }

            layer.second.clear();
        }

        comp->inserted = false;

        // Todo
        // for (const auto& page : affectedPages)
        // {
        //     if (loadedPages.at(page).isEmpty())
        //         loadedPages.erase(page);
        // }

        comp->cells.clear();
    }

    std::set<_unique_id> CollisionSystem::resolveCollisionList(PositionComponent* pos, CollisionComponent* comp)
    {
        LOG_THIS_MEMBER(DOM);

        // LOG_INFO(DOM, "Resolving collision list");

        std::set<_unique_id> touchedIds;

        // Get all ids in the same cell as our comp
        for (const auto& layer : comp->cells)
        {
            if (not comp->checkSpecificLayerFlag)
            {
                for (const auto& pagelist : loadedPages)
                {
                    // LOG_INFO(DOM, "Checking page: " << pagelist.first << " for cell: " << key << ", looking for: " << cell->pos.x << ", " << cell->pos.y);
                    const auto& it = pagelist.second.find(layer.first);
                    if (it != pagelist.second.end())
                    {
                        for (auto cell : layer.second)
                        {
                            const auto& ids = it->second.cells[cell->pos.x + cell->pos.y * it->second.size.x].ids;

                            // LOG_INFO(DOM, "Checking cell: " << cell->pos.x + cell->pos.y * it->second.size.x << " (" << cell->pos.x << ", " << cell->pos.y << ") in page: " << layer.first.x << ", " << layer.first.y);

                            touchedIds.insert(ids.begin(), ids.end());
                        }
                    }
                }
            }
            else
            {
                for (const auto& checkedlayerId : comp->checkLayerId)
                {
                    if (loadedPages.find(checkedlayerId) == loadedPages.end())
                        continue;

                    const auto& pagelist = loadedPages[checkedlayerId];
                    const auto& it = pagelist.find(layer.first);
                    if (it != pagelist.end())
                    {
                        for (auto cell : layer.second)
                        {
                            const auto& ids = it->second.cells[cell->pos.x + cell->pos.y * it->second.size.x].ids;

                            // LOG_INFO(DOM, "Checking cell: " << cell->pos.x + cell->pos.y * it->second.size.x << " (" << cell->pos.x << ", " << cell->pos.y << ") in page: " << layer.first.x << ", " << layer.first.y);

                            touchedIds.insert(ids.begin(), ids.end());
                        }
                    }
                }

            }

            // touchedIds.insert(cell->ids.begin(), cell->ids.end());
        }

        // LOG_INFO(DOM, "Got all cell");

        // We remove our id as we don't want to test the collision on ourselves
        touchedIds.erase(comp->entityId);

        std::set<_unique_id> collidedIds;

        for (auto id : touchedIds)
        {
            auto ent = ecsRef->getEntity(id);

            if (ent and ent->has<PositionComponent>() and ent->has<CollisionComponent>())
            {
                auto ui = ent->get<PositionComponent>();
                auto c2 = ent->get<CollisionComponent>();

                // Todo have different type of collision (AABB to AABB, circle to AABB, etc)
                auto test = testCollision(pos, ui, comp->scale, c2->scale);

                if (test)
                {
                    auto id1 = comp->entityId < id ? comp->entityId : id;
                    auto id2 = comp->entityId > id ? comp->entityId : id;

                    detectedCollisions.emplace(id1, id2);
                }
            }
        }

        // LOG_INFO(DOM, "collision calcul done");

        return collidedIds;
    }

    // TODO this only test collision for AABB to AABB for now, need to expend on it !
    bool CollisionSystem::testCollision(PositionComponent* obj1, PositionComponent* obj2, float scale1, float scale2) const
    {
        LOG_THIS_MEMBER(DOM);

        // compute scaled AABB for obj1
        float w1 = obj1->width  * scale1;
        float h1 = obj1->height * scale1;
        // center the scaled box around the original pos
        float x1 = obj1->x - (w1 - obj1->width ) * 0.5f;
        float y1 = obj1->y - (h1 - obj1->height) * 0.5f;

        // and for obj2
        float w2 = obj2->width  * scale2;
        float h2 = obj2->height * scale2;
        float x2 = obj2->x - (w2 - obj2->width ) * 0.5f;
        float y2 = obj2->y - (h2 - obj2->height) * 0.5f;

        // now do the standard AABB check
        return (x1 + w1) >= x2 and x1 <= (x2 + w2) and (y1 + h1) >= y2 and y1 <= (y2 + h2);
    }

    // Todo
    _unique_id CollisionSystem::findNeareastId(constant::Vector2D /* pos */, size_t /* layerId */, size_t /* radius */)
    {
        // PagePos key {pos.x, pos.y};

        // auto it = loadedPages[layerId].find(key);

        // if (it == loadedPages[layerId].end())
        // {

        // }
        // else
        // {

        // }

        return 0;
    }

    RaycastHit CollisionSystem::raycast(const constant::Vector2D& origin, const constant::Vector2D& dir, float maxDist, size_t layerId)
    {
        auto cellsToCheck = traverseGridCells(origin, dir, maxDist);

        RaycastHit bestHit;
        bestHit.hit = false;

        float bestT = maxDist;

        for (const auto& cellPos : cellsToCheck)
        {
            // lookup page, then cell index
            PagePos pageKey = { static_cast<int>(cellPos.x / pageSize.x),
                                static_cast<int>(cellPos.y / pageSize.y) };

            auto pageIt = loadedPages[layerId].find(pageKey);

            if (pageIt == loadedPages[layerId].end())
                continue;

            auto& page = pageIt->second;

            // 3) Compute local cell indices, wrapping negatives if needed
            int localX = int(cellPos.x) % int(pageSize.x);
            int localY = int(cellPos.y) % int(pageSize.y);

            if (localX < 0)
                localX += pageSize.x;

            if (localY < 0)
                localY += pageSize.y;

            int idx = localX + localY * int(pageSize.x);
            // guard against stray out‐of‐range
            if (idx < 0 or idx >= int(page.cells.size()))
                continue;

            const auto& ids = page.cells[idx].ids;

            // test each entity in the cell
            for (auto entId : ids)
            {
                auto ent = ecsRef->getEntity(entId);
                if (not ent or not ent->has<PositionComponent>() or not ent->has<CollisionComponent>())
                    continue;

                // build that entity’s AABB in world‐space
                auto pos = ent->get<PositionComponent>();
                auto col = ent->get<CollisionComponent>();

                auto width = pos->width;
                auto height = pos->height;

                auto startX = pos->x - (width  * (col->scale - 1)) / 2.0f;
                auto startY = pos->y - (height * (col->scale - 1)) / 2.0f;

                AABB box {
                    startX,
                    startY,
                    startX + width * col->scale,
                    startY + height * col->scale
                };

                // do the slab test we discussed
                if (auto tOpt = rayAABB(origin, dir, maxDist, box))
                {
                    float t = *tOpt;

                    if (t < bestT)
                    {
                        bestT = t;
                        constant::Vector2D hitPoint = origin + dir * t;
                        constant::Vector2D normal   = computeBoxNormal(box, hitPoint);
                        bestHit = RaycastHit{entId, true, hitPoint, t, normal};
                    }
                }
            }
        }

        return bestHit;
    }

    std::vector<PagePos> CollisionSystem::traverseGridCells(constant::Vector2D origin, constant::Vector2D dir, float maxDist)
    {
        std::vector<PagePos> cells;

        if (maxDist <= 0)
            return cells;

        // convert world‐pos → cell‐coords
        int cellX = int(origin.x / cellSize.x);
        int cellY = int(origin.y / cellSize.y);

        // determine stepping direction & tDelta
        int stepX = dir.x > 0 ? +1 : (dir.x < 0 ? -1 : 0);
        int stepY = dir.y > 0 ? +1 : (dir.y < 0 ? -1 : 0);
        float tMaxX = (stepX != 0) ?
            ((cellX + (stepX > 0)) * cellSize.x - origin.x) / dir.x :
            std::numeric_limits<float>::infinity();

        float tMaxY = (stepY != 0) ?
            ((cellY + (stepY > 0)) * cellSize.y - origin.y) / dir.y :
            std::numeric_limits<float>::infinity();

        float tDeltaX = (stepX != 0 ? cellSize.x / std::abs(dir.x) : std::numeric_limits<float>::infinity());
        float tDeltaY = (stepY != 0 ? cellSize.y / std::abs(dir.y) : std::numeric_limits<float>::infinity());


        float t = 0;

        while (t < maxDist)
        {
            cells.push_back(PagePos{cellX, cellY});

            if (tMaxX < tMaxY)
            {
                cellX  += stepX;
                t       = tMaxX;
                tMaxX  += tDeltaX;
            }
            else
            {
                cellY  += stepY;
                t       = tMaxY;
                tMaxY  += tDeltaY;
            }
        }

        return cells;
    }

    // a shared empty set we return when out of bounds
    static const std::set<_unique_id> emptySet;

    const std::set<_unique_id>& CollisionSystem::getCellEntities(const PagePos& cellPos, size_t layerId) const
    {
        // 1) find the page this cell lives in
        // integer‐divide by pageSize to get the page coords
        PagePos pageKey { static_cast<int>(std::floor(cellPos.x) / pageSize.x),
                          static_cast<int>(std::floor(cellPos.y) / pageSize.y) };

        auto layerIt = loadedPages.find(layerId);
        if (layerIt == loadedPages.end())
            return emptySet;

        auto pageIt = layerIt->second.find(pageKey);
        if (pageIt == layerIt->second.end())
            return emptySet;

        const CollisionPage& page = pageIt->second;

        // 2) compute local cell index within that page
        int localX = cellPos.x - pageKey.x * pageSize.x;
        int localY = cellPos.y - pageKey.y * pageSize.y;

        // guard against negative modulo if you support negative world coords
        if (localX < 0) localX += pageSize.x;
        if (localY < 0) localY += pageSize.y;

        int idx = localX + localY * pageSize.x;

        if (idx < 0 or idx >= (int)page.cells.size())
            return emptySet;

        // 3) return the set of IDs
        return page.cells[idx].ids;
    }


    void CollisionSystem::onEvent(const EntityChangedEvent& event)
    {
        LOG_THIS_MEMBER(DOM);

        auto entity = ecsRef->getEntity(event.id);

        if (not entity or not entity->has<PositionComponent>() or not entity->has<CollisionComponent>())
            return;

        // LOG_INFO(DOM, "Ui changed");

        auto ui = entity->get<PositionComponent>();
        auto comp = entity->get<CollisionComponent>();

        // float x = ui->x;
        // float y = ui->y;

        // int xPagePos = x / cellSize.x;
        // int yPagePos = y / cellSize.y;

        // LOG_INFO(DOM, "Inserting entity in cell: " << xPagePos << ", y: " << yPagePos);

        removeComponentFromGrid(comp);
        addComponentInGrid(ui, comp);

        // Todo fix this to avoid moving an comp even when it doesn't change cell
        // if (not comp->inserted or comp->firstCellX != xPagePos or comp->firstCellY != yPagePos)
        // {
        //     removeComponentFromGrid(comp);
        //     addComponentInGrid(ui, comp);
        // }
        // else
        // {
        //     LOG_INFO(DOM, "Entity didn't change cell");
        //     resolveCollisionList(ui, comp);
        // }
    }

    void CollisionSystem::execute()
    {
        for (const auto& collision : detectedCollisions)
        {
            ecsRef->sendEvent(collision);
        }

        detectedCollisions.clear();
    }

    /// Tries to move `pos` by `delta`.  Returns the actual movement applied (≤ delta),
    /// and writes `hit` if we ran into something.
    /// - originPos: the top‑left or center of your entity (consistent convention)
    /// - size: width/height of your entity’s AABB
    /// - layer: which collision layer ID to test against (e.g. walls)
    SweepMoveResult sweepMove(CollisionSystem* collision, const constant::Vector2D& originPos, const constant::Vector2D& size, const constant::Vector2D& delta, const std::vector<size_t>& targetLayers)
    {
        SweepMoveResult res;
        res.hit = false;

        // 1) compute center and half‑extents
        constant::Vector2D half = { size.x * 0.5f, size.y * 0.5f };
        constant::Vector2D center = originPos + half;

        // 2) traverse only the cells your ray crosses
        float moveLen = sqrt(delta.x * delta.x + delta.y * delta.y);

        if (moveLen < EPSILON)
            return res;

        constant::Vector2D dir = { delta.x / moveLen, delta.y / moveLen };

        // 1) decide which X offsets to use
        bool xZero = fabs(dir.x) < EPSILON;
        bool yZero = fabs(dir.y) < EPSILON;

        // build lists of candidate offsets
        std::vector<float> offsX, offsY;

        // if non‑zero, pick a single side; else emit both
        if (not xZero)
            offsX.push_back(dir.x > 0 ? +half.x : -half.x);
        else
        {
            offsX.push_back(+half.x);
            offsX.push_back(-half.x);
        }

        if (not yZero)
            offsY.push_back(dir.y > 0 ? +half.y : -half.y);
        else
        {
            offsY.push_back(+half.y);
            offsY.push_back(-half.y);
        }

        // now build a unique set of ray origins
        std::set<std::pair<float,float>> origins;

        for (float ox : offsX)
        {
            for (float oy : offsY)
            {
                origins.emplace(center.x + ox, center.y + oy);
            }
        }

        // traverse from *each* origin, collecting cells
        std::unordered_set<PagePos> cells;
        for (auto [rx, ry] : origins)
        {
            auto partial = collision->traverseGridCells({rx, ry}, dir, moveLen);

            cells.insert(partial.begin(), partial.end());
        }

        // 3) for each wall in those cells, do a ray vs. inflated‐AABB test
        float bestT = 1.0f;  // fraction of delta

        auto ecsRef = collision->world();

        for (auto& cellPos : cells)
        {
            for (auto layer : targetLayers)
            {
                for (auto wallId : collision->getCellEntities(cellPos, layer))
                {
                    auto wallEntity = ecsRef->getEntity(wallId);

                    if (not wallEntity or not wallEntity->has<PositionComponent>() or not wallEntity->has<CollisionComponent>())
                        continue;

                    auto wpos = wallEntity->get<PositionComponent>();
                    auto wcol = wallEntity->get<CollisionComponent>();

                    auto wallWidth = wpos->width;
                    auto wallHeight = wpos->height;

                    auto startWallX = wpos->x - (wallWidth * (wcol->scale - 1)) / 2.0f;
                    auto startWallY = wpos->y - (wallHeight * (wcol->scale - 1)) / 2.0f;

                    // Build wall AABB
                    // Inflate by our half extents
                    AABB infBox {
                        startWallX - half.x,
                        startWallY - half.y,
                        startWallX + wallWidth * wcol->scale + half.x,
                        startWallY + wallHeight * wcol->scale + half.y
                    };

                    auto tOpt = rayAABB(center, dir, moveLen, infBox);

                    if (tOpt)
                    {
                        float t = *tOpt / moveLen;           // normalize to [0..1]
                        // bestT = std::min(bestT, t);

                        if (t < bestT)
                        {
                            bestT = t;
                            res.entity = wallEntity;
                        }
                    }
                }
            }
        }

        res.hit = (bestT < 1.0f);
        float safeT;

        if (res.hit)
        {
          // we hit, so back off by a small epsilon to avoid overlap
          safeT = bestT - EPSILON;

          if (safeT < 0) safeT = 0;
        }
        else
        {
          // no hit: full movement
          safeT = 1.0f;
        }

        res.delta = delta * safeT;

        // 5) update the originPos *outside* this helper
        return res;
    }
}