#include "collisionsystem.h"

namespace pg
{
    namespace
    {
        constexpr const char * const DOM = "Collision System";
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
        float tmin = 0.0f, tmax = length;

        // for each axis X and Y
        for (int axis = 0; axis < 2; ++axis)
        {
            float o = axis==0 ? origin.x : origin.y;
            float d = axis==0 ? dir.x    : dir.y;
            float minB = axis==0 ? box.minX : box.minY;
            float maxB = axis==0 ? box.maxX : box.maxY;

            if (std::abs(d) < 1e-6f)
            {
                // Ray parallel to this axis—miss if outside slab
                if (o < minB || o > maxB)
                return std::nullopt;
            } 
            else 
            {
                // Compute intersection t’s of the ray with the box planes
                float invD = 1.0f / d;
                float t1 = (minB - o) * invD;
                float t2 = (maxB - o) * invD;

                // sort so t1 is entry, t2 exit
                if (t1 > t2) std::swap(t1, t2);

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

        if (comp->scale < 1)
        {
            x += (width * comp->scale) / 2.0f;
            y += (height * comp->scale) / 2.0f;
        }
        else if (comp->scale > 1)
        {
            x -= (width * comp->scale) / 2.0f;
            y -= (height * comp->scale) / 2.0f;
        }

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

                AABB box {
                    pos->x,
                    pos->y,
                    pos->x + pos->width * col->scale,
                    pos->y + pos->height * col->scale
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
        // convert world‐pos → cell‐coords
        int cellX = int(origin.x / cellSize.x);
        int cellY = int(origin.y / cellSize.y);
    
        // determine stepping direction & tDelta
        int stepX = dir.x > 0 ? +1 : -1;
        int stepY = dir.y > 0 ? +1 : -1;
        float tMaxX = ((cellX + (stepX > 0)) * cellSize.x - origin.x) / dir.x;
        float tMaxY = ((cellY + (stepY > 0)) * cellSize.y - origin.y) / dir.y;
        float tDeltaX = cellSize.x / std::abs(dir.x);
        float tDeltaY = cellSize.y / std::abs(dir.y);
    
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
}