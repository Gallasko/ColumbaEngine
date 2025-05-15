#include "collisionsystem.h"

namespace pg
{
    namespace
    {
        constexpr const char * const DOM = "Collision System";

        static constexpr float EPSILON = 1e-5f;
    }

    // Todo fix scaling !

    struct AABB
    {
        float minX, maxX;
        float minY, maxY;
    };

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
        float tmin = 0.0f, tmax = length + EPSILON;

        // for each axis X and Y
        for (int axis = 0; axis < 2; ++axis)
        {
            float o    = axis == 0 ? origin.x : origin.y;
            float d    = axis == 0 ? dir.x    : dir.y;
            float minB = axis == 0 ? box.minX : box.minY;
            float maxB = axis == 0 ? box.maxX : box.maxY;

            if (std::abs(d) < EPSILON)
            {
                // Ray parallel to this axis—miss if outside slab
                if (o < minB or o > maxB)
                    return std::nullopt;
            }
            else
            {
                // Compute intersection t’s of the ray with the box planes
                float invD = 1.0f / d;
                float t1 = (minB - o) * invD;
                float t2 = (maxB - o) * invD;

                // sort so t1 is entry, t2 exit
                if (t1 > t2)
                    std::swap(t1, t2);

                tmin = std::max(tmin, t1);
                tmax = std::min(tmax, t2);

                if (tmin > tmax)
                    return std::nullopt;
            }
        }

        // tmin is intersection point along [0,length]
        return tmin <= length ? std::optional<float>(tmin) : std::nullopt;
    }

    void CollisionComponent::onDeletion(EntityRef)
    {
        LOG_THIS_MEMBER("Collision Component");

        auto sys = ecsRef->getSystem<CollisionSystem>();

        sys->removeComponentFromGrid(this);
    }

    void CollisionPage::addId(CollisionComponent& comp, const constant::Vector2D& startPos, const constant::Vector2D& objSize)
    {
        int startX = startPos.x / cellSize.x;
        int startY = startPos.y / cellSize.y;

        int endX = startPos.x + objSize.x < cellSize.x * size.x ? startX + (objSize.x / cellSize.x) + 1 : size.x;
        int endY = startPos.y + objSize.y < cellSize.y * size.y ? startY + (objSize.y / cellSize.y) + 1 : size.y;

        for (int y = startY; y < endY; y++)
        {
            for (int x = startX; x < endX; x++)
            {
                cells[x + y * size.x].ids.insert(comp.entityId);
                comp.cells[pos].emplace_back(&cells[x + y * size.x]);
            }
        }
    }

    // Todo issue here when cellsize != pagesize
    CollisionSystem::CollisionSystem() : pageSize(10, 10), cellSize(20, 20)
    // CollisionSystem::CollisionSystem() : pageSize(5, 5), cellSize(5, 5)
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

    void CollisionSystem::addComponentInGrid(CompRef<PositionComponent> pos, CompRef<CollisionComponent> comp)
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

        x += (width * (comp->scale - 1)) / 2.0f;
        y += (height * (comp->scale - 1)) / 2.0f;

        auto pWidth = (pageSize.x * cellSize.x);
        auto pHeight = (pageSize.y * cellSize.y);

        int xPagePos = x / pWidth;
        int yPagePos = y / pHeight;

        int startY = y > 0 ? y - yPagePos * pHeight : yPagePos * pHeight - y;

        int usedWidth = 0, usedHeight = 0;

        int startingXPos = xPagePos;

        comp->firstCell = {x / cellSize.x, y / cellSize.y};

        for (int remainingHeight = height * comp->scale; remainingHeight > 0; remainingHeight -= usedHeight)
        {
            int startX = x > 0 ? x - xPagePos * pWidth : xPagePos * pWidth - x;

            for (int remainingWidth = width * comp->scale; remainingWidth > 0; remainingWidth -= usedWidth)
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

    std::set<_unique_id> CollisionSystem::resolveCollisionList(CompRef<PositionComponent> pos, CompRef<CollisionComponent> comp)
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

            if (ent and ent->has<PositionComponent>())
            {
                auto ui = ent->get<PositionComponent>();

                // Todo have different type of collision (AABB to AABB, circle to AABB, etc)
                auto test = testCollision(pos, ui);

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
    bool CollisionSystem::testCollision(CompRef<PositionComponent> obj1, CompRef<PositionComponent> obj2) const
    {
        LOG_THIS_MEMBER(DOM);

        float obj1Xmin = obj1->x;
        float obj1XMax = obj1->x + obj1->width;
        float obj1YMin = obj1->y;
        float obj1YMax = obj1->y + obj1->height;

        float obj2XMin = obj2->x;
        float obj2XMax = obj2->x + obj2->width;
        float obj2YMin = obj2->y;
        float obj2YMax = obj2->y + obj2->height;

        return obj1XMax > obj2XMin and
               obj1Xmin < obj2XMax and
               obj1YMax > obj2YMin and
               obj1YMin < obj2YMax;
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

        for (auto const& cellPos : cellsToCheck)
        {
            // lookup page, then cell index
            PagePos pageKey = { static_cast<int>(cellPos.x / pageSize.x),
                                static_cast<int>(cellPos.y / pageSize.y) };

            auto pageIt = loadedPages[layerId].find(pageKey);

            if (pageIt == loadedPages[layerId].end())
                continue;

            auto& page = pageIt->second;
            int localX = cellPos.x % int(pageSize.x);
            int localY = cellPos.y % int(pageSize.y);
            auto& ids  = page.cells[ localX + localY * pageSize.x ].ids;

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

                auto startX = pos->x + (width * (col->scale - 1)) / 2.0f;
                auto startY = pos->y + (height* (col->scale - 1)) / 2.0f;

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
            ((cellX + (stepX>0)) * cellSize.x - origin.x) / dir.x :
            std::numeric_limits<float>::infinity();

        float tMaxY = (stepY != 0) ?
            ((cellY + (stepY>0)) * cellSize.y - origin.y) / dir.y :
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
        PagePos pageKey { static_cast<int>(cellPos.x / pageSize.x), static_cast<int>(cellPos.y / pageSize.y) };

        auto layerIt = loadedPages.find(layerId);
        if (layerIt == loadedPages.end())
            return emptySet;

        auto pageIt = layerIt->second.find(pageKey);
        if (pageIt == layerIt->second.end())
            return emptySet;

        const CollisionPage& page = pageIt->second;

        // 2) compute local cell index within that page
        int localX = cellPos.x % static_cast<int>(pageSize.x);
        int localY = cellPos.y % static_cast<int>(pageSize.y);

        // guard against negative modulo if you support negative world coords
        if (localX < 0) localX += pageSize.x;
        if (localY < 0) localY += pageSize.y;

        int idx = localX + localY * pageSize.x;

        if (idx < 0 || idx >= (int)page.cells.size())
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

        // float x = ui->pos.x;
        // float y = ui->pos.y;

        // int xPagePos = x / cellSize.x;
        // int yPagePos = y / cellSize.y;

        // LOG_INFO(DOM, "Inserting entity in cell: " << xPagePos << ", y: " << yPagePos);

        removeComponentFromGrid(comp);
        addComponentInGrid(ui, comp);

        // Todo fix this to avoid moving an comp even when it doesn't change cell
        // if (not comp->inserted and comp->firstCell != constant::Vector2D{xPagePos, yPagePos})
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
    constant::Vector2D sweepMove(CollisionSystem* collision, const constant::Vector2D& originPos, const constant::Vector2D& size, const constant::Vector2D& delta, const std::vector<size_t>& targetLayers, bool& hit)
    {
        hit = false;

        // 1) compute center and half‑extents
        constant::Vector2D half = { size.x * 0.5f, size.y * 0.5f };
        constant::Vector2D center = originPos + half;

        // 2) traverse only the cells your ray crosses
        float moveLen = sqrt(delta.x * delta.x + delta.y * delta.y);

        if (moveLen < EPSILON)
            return {0,0};

        constant::Vector2D dir = { delta.x / moveLen, delta.y / moveLen };
        auto cells = collision->traverseGridCells(center, dir, moveLen);

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

                    auto startWallX = wpos->x + (wallWidth * (wcol->scale - 1)) / 2.0f;
                    auto startWallY = wpos->y + (wallHeight * (wcol->scale - 1)) / 2.0f;

                    // Build wall AABB
                    AABB wallBox {
                        startWallX,
                        startWallY,
                        startWallX + wallWidth * wcol->scale,
                        startWallY + wallHeight * wcol->scale
                    };

                    // Inflate by our half extents
                    AABB infBox {
                        wallBox.minX - half.x, wallBox.minY - half.y,
                        wallBox.maxX + half.x, wallBox.maxY + half.y
                    };

                    if (auto tOpt = rayAABB(center, dir, moveLen, infBox))
                    {
                        float t = *tOpt / moveLen;           // normalize to [0..1]
                        bestT = std::min(bestT, t);
                    }
                }
            }
        }

        // 4) apply movement up to just before contact
        float safeT = bestT > 0 ? bestT - 1e-3f : 0.f;

        if (safeT < 0) safeT = 0;

        constant::Vector2D applied = delta * safeT;
        hit = (bestT < 1.0f);

        // 5) update the originPos *outside* this helper
        return applied;
    }
}