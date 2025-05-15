#pragma once

#include "ECS/system.h"
#include "ECS/callable.h"

#include "Helpers/functiontraits.h"

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

        CollisionComponent(const CollisionComponent& other) : layerId(other.layerId), scale(other.scale), checkSpecificLayerFlag(other.checkSpecificLayerFlag), checkLayerId(other.checkLayerId), ecsRef(other.ecsRef), entityId(other.entityId), cells(other.cells), firstCellX(other.firstCellX), firstCellY(other.firstCellY), inserted(other.inserted) {}

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

        int firstCellX = 0;
        int firstCellY = 0;

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

    struct RaycastHit
    {
        _unique_id entityId;  // id of the object we hit
        bool hit;             // true if there is a hit
        constant::Vector2D hitPoint; // point where we hit
        float t;              // distance along ray where we hit
        constant::Vector2D normal;      // collision normal
    };

    struct CollisionSystem : public System<Own<CollisionComponent>, Ref<PositionComponent>, Listener<EntityChangedEvent>, InitSys>
    {
        // Todo make a ctor that load properties (pageSize, cellSi) from serialization
        CollisionSystem();

        virtual void init() override;

        void addComponentInGrid(CompRef<PositionComponent> pos, CompRef<CollisionComponent> comp);

        void removeComponentFromGrid(CollisionComponent* comp);

        std::set<_unique_id> resolveCollisionList(CompRef<PositionComponent> pos, CompRef<CollisionComponent> comp);

        bool testCollision(CompRef<PositionComponent> obj1, CompRef<PositionComponent> obj2, float scale1, float scale2) const;

        _unique_id findNeareastId(constant::Vector2D pos, size_t layerId, size_t radius);

        // Performs a raycast in world‐space:
        RaycastHit raycast(const constant::Vector2D& origin, const constant::Vector2D& dir, float maxDist, size_t layerId);

        std::vector<PagePos> traverseGridCells(constant::Vector2D origin, constant::Vector2D dir, float maxDist);

        // returns a reference to the set of entity‐IDs in (cellPos, layerId).
        // If the page or cell doesn't exist, returns an empty static set.
        const std::set<_unique_id>& getCellEntities(const PagePos& cellPos, size_t layerId) const;

        virtual void onEvent(const EntityChangedEvent& event) override;

        virtual void execute() override;

        constant::Vector2D pageSize;
        constant::Vector2D cellSize;

        std::map<size_t, std::unordered_map<PagePos, CollisionPage>> loadedPages;

        std::set<CollisionEvent> detectedCollisions;
    };

    struct SweepMoveResult
    {
        bool hit = false;
        constant::Vector2D delta = {0, 0};

        Entity *entity = nullptr;
    };

    /// Tries to move `pos` by `delta`.  Returns the actual movement applied (≤ delta),
    /// and writes `hit` if we ran into something.
    /// - originPos: the top‑left or center of your entity (consistent convention)
    /// - size: width/height of your entity’s AABB
    /// - layer: which collision layer ID to test against (e.g. walls)
    SweepMoveResult sweepMove(CollisionSystem*  collision, const constant::Vector2D& originPos, const constant::Vector2D& size, const constant::Vector2D& delta, const std::vector<size_t>& targetLayers);

    struct CollisionHandleBase
    {
        virtual ~CollisionHandleBase() = default;
        virtual void tryInvoke(EntitySystem* ecs, _unique_id id1, _unique_id id2) const = 0;
        virtual std::unique_ptr<CollisionHandleBase> clone() const = 0;
    };

    template <typename Comp1, typename Comp2>
    struct CollisionHandlePair : public CollisionHandleBase
    {
        CollisionHandlePair(std::function<void(Comp1*, Comp2*)> f) : fn(std::move(f)) {}

        CollisionHandlePair(const CollisionHandlePair& other) : fn(other.fn) {}

        CollisionHandlePair& operator=(const CollisionHandlePair& other)
        {
            fn = other.fn;

            return *this;
        }

        virtual void tryInvoke(EntitySystem* ecs, _unique_id id1, _unique_id id2) const override
        {
            auto* ca = ecs->getComponent<Comp1>(id1);
            auto* cb = ecs->getComponent<Comp2>(id2);

            if (ca and cb)
            {
                fn(ca, cb);
            }

            // and swap:
            if constexpr (not std::is_same_v<Comp1, Comp2>)
            {
                ca = ecs->getComponent<Comp1>(id2);
                cb = ecs->getComponent<Comp2>(id1);

                if (ca and cb)
                {
                    fn(ca, cb);
                }
            }
        }

        std::unique_ptr<CollisionHandleBase> clone() const override
        {
            return std::make_unique<CollisionHandlePair<Comp1, Comp2>>(*this);
        }

        std::function<void(Comp1*, Comp2*)> fn;
    };

    struct CollisionHandleExpert : public CollisionHandleBase
    {
        CollisionHandleExpert(
            std::function<void(Entity*, Entity*)> f,
            std::function<bool(Entity*)> filterEnt1 = [](Entity*) { return true; },
            std::function<bool(Entity*)> filterEnt2 = [](Entity*) { return true; }) :
            fn(std::move(f)), filterEnt1(std::move(filterEnt1)), filterEnt2(std::move(filterEnt2))
            {}

        CollisionHandleExpert(const CollisionHandleExpert& other) : fn(other.fn), filterEnt1(other.filterEnt1), filterEnt2(other.filterEnt2) {}

        CollisionHandleExpert& operator=(const CollisionHandleExpert& other)
        {
            if (this != &other)
            {
                fn = other.fn;
                filterEnt1 = other.filterEnt1;
                filterEnt2 = other.filterEnt2;
            }

            return *this;
        }

        virtual void tryInvoke(EntitySystem* ecs, _unique_id id1, _unique_id id2) const override
        {
            auto* ent1 = ecs->getEntity(id1);
            auto* ent2 = ecs->getEntity(id2);

            if (filterEnt1(ent1) and filterEnt2(ent2))
            {
                fn(ent1, ent2);
            }
            // and swap:
            else if (filterEnt1(ent2) and filterEnt2(ent1))
            {
                fn(ent2, ent1);
            }
        }

        std::unique_ptr<CollisionHandleBase> clone() const override
        {
            // make a brand‐new MyCollisionHandler by copying *this
            return std::make_unique<CollisionHandleExpert>(*this);
        }

        std::function<void(Entity*, Entity*)> fn;
        std::function<bool(Entity*)> filterEnt1;
        std::function<bool(Entity*)> filterEnt2;
    };

    struct CollisionHandleComponent
    {
        CollisionHandleComponent() = default;
        CollisionHandleComponent(std::unique_ptr<CollisionHandleBase> handler) : handler(std::move(handler)) {}
        CollisionHandleComponent(const CollisionHandleComponent& other)
        {
            handler = other.handler ? other.handler->clone() : nullptr;
        }

        CollisionHandleComponent& operator=(const CollisionHandleComponent& other)
        {
            if (this != &other)
            {
                handler = other.handler ? other.handler->clone() : nullptr;
            }

            return *this;
        }

        std::unique_ptr<CollisionHandleBase> handler;
    };

    struct CollisionHandlerSystem : public System<Listener<CollisionEvent>, Own<CollisionHandleComponent>, StoragePolicy>
    {
        virtual void onEvent(const CollisionEvent& event) override
        {
            for (const auto& comp : view<CollisionHandleComponent>())
            {
                comp->handler->tryInvoke(ecsRef, event.id1, event.id2);
            }
        }
    };

    template <typename Comp1, typename Comp2, typename Type>
    EntityRef makeCollisionHandlePair(Type* ecsRef, std::function<void(Comp1*, Comp2*)> fn)
    {
        auto ent = ecsRef->createEntity();

        auto handle = std::make_unique<CollisionHandlePair<Comp1, Comp2>>(fn);

        ecsRef->template attach<CollisionHandleComponent>(ent, std::move(handle));

        return ent;
    }

    template <typename Func, typename Type>
    EntityRef makeCollisionHandlePair(Type* ecsRef, Func fn)
    {
        static_assert(std::is_same_v<void, typename function_traits<Func>::return_type>, "must be void-returning");

        static_assert(function_traits<Func>::arity == 2, "must take exactly 2 arguments");

        // these are exactly the lambda-parameters, e.g. PlayerFlag*
        using Arg0 = typename function_traits<Func>::template arg<0>;
        using Arg1 = typename function_traits<Func>::template arg<1>;

        // ensure they really are pointers
        static_assert(std::is_pointer_v<Arg0> and std::is_pointer_v<Arg1>, "handler must take two raw pointers");

        // strip off the pointer → Comp1=PlayerFlag, Comp2=CollectibleFlag
        using Comp1 = std::remove_pointer_t<Arg0>;
        using Comp2 = std::remove_pointer_t<Arg1>;

        // now wrap your original fn in the exact std::function<void(Comp1*,Comp2*)>
        std::function<void(Comp1*, Comp2*)> wrapper = fn;

        return makeCollisionHandlePair<Comp1, Comp2>(ecsRef, std::move(wrapper));
    }

    template <typename Type>
    EntityRef makeCollisionHandle(Type* ecsRef,
        std::function<void(Entity*, Entity*)> fn,
        std::function<bool(Entity*)> filterEnt1 = [](Entity*) { return true; },
        std::function<bool(Entity*)> filterEnt2 = [](Entity*) { return true; })
    {
        auto ent = ecsRef->createEntity();

        auto handle = std::make_unique<CollisionHandleExpert>(fn, filterEnt1, filterEnt2);

        ecsRef->template attach<CollisionHandleComponent>(ent, std::move(handle));

        return ent;
    }
}