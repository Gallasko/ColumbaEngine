#pragma once

#include "ECS/system.h"
#include "ECS/callable.h"

#include "position.h"

#include "constant.h"

#include <string>

namespace pg
{
    struct PagePos
    {
        PagePos() : x(0), y(0) {}
        PagePos(int x, int y) : x(x), y(y) {}
        PagePos(const PagePos& other) : x(other.x), y(other.y) {}

        ~PagePos() {};

        PagePos& operator=(const PagePos& other)
        {
            x = other.x;
            y = other.y;

            return *this;
        }

        bool operator==(const PagePos& other) const
        {
            return other.x == x and other.y == y;
        }

        bool operator!=(const PagePos& other) const
        {
            return not (*this == other);
        }

        bool operator<(const PagePos& other) const
        {
            return std::to_string(x) + std::to_string(y) < std::to_string(other.x) + std::to_string(other.y);
        }

        int x, y;
    };
}

template <>
struct std::hash<pg::PagePos>
{
    std::size_t operator()(const pg::PagePos& k) const
    {
        using std::size_t;
        using std::hash;

        // Compute individual hash values for first,
        // second and third and combine them using XOR
        // and bit shifting:

        return hash<int>()(k.x)
                ^ (hash<int>()(k.y) << 1);
    }
};

namespace pg
{
    struct CollisionCell
    {
        CollisionCell(const PagePos& pagePos, const constant::Vector2D& pos, const constant::Vector2D& size) : pagePos(pagePos), pos(pos), size(size) {}
        CollisionCell(const CollisionCell& other) : pagePos(other.pagePos), pos(other.pos), size(other.size), ids(other.ids) {}

        /** Position of the associated page */
        const PagePos pagePos;
        /** Position of the cell */
        const constant::Vector2D pos;
        /** Size of the cell */
        const constant::Vector2D size;

        std::set<_unique_id> ids;
    };

    struct CollisionComponent : public Ctor, Dtor
    {
        CollisionComponent() {}

        CollisionComponent(size_t layerId) : layerId(layerId)
        {
            if (scale == 0)
            {
                LOG_ERROR("CollisionComponent", "scale must be a positive number");
                scale = 1;
            }
        }

        CollisionComponent(size_t layerId, float scale, const std::vector<size_t>& checkLayerId = {0}) : layerId(layerId), scale(scale), checkSpecificLayerFlag(true), checkLayerId(checkLayerId)
        {
            if (scale == 0)
            {
                LOG_ERROR("CollisionComponent", "scale must be a positive number");
                scale = 1;
            }
        }

        CollisionComponent(const CollisionComponent& other) : layerId(other.layerId), scale(other.scale), checkSpecificLayerFlag(other.checkSpecificLayerFlag), checkLayerId(other.checkLayerId), ecsRef(other.ecsRef), entityId(other.entityId), cells(other.cells), firstCell(other.firstCell), inserted(other.inserted) {}

        virtual void onCreation(EntityRef entity) override
        {
            ecsRef = entity->world();

            entityId = entity->id;
        }

        virtual void onDeletion(EntityRef entity) override;

        // Layer where this component reside
        size_t layerId = 0;

        float scale = 1;

        // When this flag is set, only check the corresponding layer instead of all of them
        bool checkSpecificLayerFlag = false;
        std::vector<size_t> checkLayerId = {};

        EntitySystem* ecsRef = nullptr;

        _unique_id entityId = 0;

        std::map<PagePos, std::vector<CollisionCell*>> cells;

        constant::Vector2D firstCell = {0, 0};
        bool inserted = false;
    };

    struct CollisionPage
    {
        CollisionPage(const PagePos& pos, const constant::Vector2D& size, const constant::Vector2D& cellSize) : pos(pos), size(size), cellSize(cellSize)
        {
            for (auto y = 0; y < size.y; y++)
            {
                for (auto x = 0; x < size.x; x++)
                {
                    cells.emplace_back(pos, constant::Vector2D{x, y}, cellSize);
                }
            }
        }

        void addId(CollisionComponent& comp, const constant::Vector2D& startPos, const constant::Vector2D& objSize);

        inline bool isEmpty() const
        {
            for (const auto& cell : cells)
            {
                if (not cell.ids.empty())
                {
                    return false;
                }
            }

            return true;
        }

        /** Position of the page in the grid */
        const PagePos pos;

        /** Size of the page (width = number of cell per row, height = number of rows)
            Total absolute size of page is width = this->size.x * cell.size.x, same for height */
        const constant::Vector2D size;

        /** Absolute size of a single cell */
        const constant::Vector2D cellSize;

        /** Array of cell managed by this page */
        std::vector<CollisionCell> cells;
    };

    struct CollisionEvent
    {
        CollisionEvent(_unique_id id1, _unique_id id2) : id1(id1), id2(id2) {}
        CollisionEvent(const CollisionEvent& other) : id1(other.id1), id2(other.id2) {}

        _unique_id id1;
        _unique_id id2;

        bool operator<(const CollisionEvent& other) const
        {
            return std::to_string(id1) + std::to_string(id2) < std::to_string(other.id1) + std::to_string(other.id2);
        }
    };

    struct CollisionSystem : public System<Own<CollisionComponent>, Ref<PositionComponent>, Listener<EntityChangedEvent>, InitSys>
    {
        // Todo make a ctor that load properties (pageSize, cellSi) from serialization
        CollisionSystem();

        virtual void init() override;

        void addComponentInGrid(CompRef<PositionComponent> pos, CompRef<CollisionComponent> comp);

        void removeComponentFromGrid(CollisionComponent* comp);

        std::set<_unique_id> resolveCollisionList(CompRef<PositionComponent> pos, CompRef<CollisionComponent> comp);

        bool testCollision(CompRef<PositionComponent> obj1, CompRef<PositionComponent> obj2) const;

        _unique_id findNeareastId(constant::Vector2D pos, size_t layerId, size_t radius);

        virtual void onEvent(const EntityChangedEvent& event) override;

        virtual void execute() override;

        constant::Vector2D pageSize;
        constant::Vector2D cellSize;

        std::map<size_t, std::unordered_map<PagePos, CollisionPage>> loadedPages;

        std::set<CollisionEvent> detectedCollisions;
    };
}