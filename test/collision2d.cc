#include "gtest/gtest.h"

#include "2D/collisionsystem.h"
#include "2D/position.h"

using namespace pg;
using namespace pg::constant;

// Helpers --------------------------------------------------------------------
static AABB makeBox(float x, float y, float w, float h)
{
    return AABB{ x, y, x + w, y + h };
}

static EntityRef makeWall(EntitySystem& ecs, float x, float y, float w, float h, int layer)
{
    auto e = ecs.createEntity();
    auto p = ecs.attach<PositionComponent>(e);
    p->setX(x); p->setY(y); p->setWidth(w); p->setHeight(h);
    ecs.attach<CollisionComponent>(e, layer);
    return e;
}

// 1) rayAABB tests -----------------------------------------------------------
TEST(CollisionHelpers, RayAABB_DirectHit)
{
    AABB b = makeBox(10, 10, 20, 20);

    Vector2D origin {0, 20}, dir {1, 0};

    auto t = rayAABB(origin, dir, 100, b);

    ASSERT_TRUE(t.has_value());
    EXPECT_NEAR(*t, 10.0f, 1e-3f);
}

TEST(CollisionHelpers, RayAABB_Miss)
{
    AABB b = makeBox(10, 10, 20, 20);

    Vector2D origin {0, 0}, dir {1, 0};

    auto t = rayAABB(origin, dir, 5, b);

    EXPECT_FALSE(t.has_value());
}

TEST(CollisionHelpers, RayAABB_ParallelOutside)
{
    AABB b = makeBox(10, 10, 20, 20);

    Vector2D origin {0, 0}, dir {0, 1};

    // origin.x = 0 outside [10,30]
    auto t = rayAABB(origin, dir, 100, b);

    EXPECT_FALSE(t.has_value());
}

TEST(CollisionHelpers, RayAABB_ParallelInside)
{
    AABB b = makeBox(10, 10, 20, 20);

    Vector2D origin {20, 0}, dir {0, 1};

    auto t = rayAABB(origin, dir, 100, b);

    ASSERT_TRUE(t.has_value());
    EXPECT_NEAR(*t, 10.0f, 1e-3f);
}

// 2) computeBoxNormal tests ---------------------------------------------------
TEST(CollisionHelpers, ComputeBoxNormal_Left)
{
    AABB b = makeBox(0, 0, 10, 10);

    Vector2D hit {0, 5};

    auto n = computeBoxNormal(b, hit);

    EXPECT_FLOAT_EQ(n.x, -1.0f);
    EXPECT_FLOAT_EQ(n.y,  0.0f);
}
TEST(CollisionHelpers, ComputeBoxNormal_Top)
{
    AABB b = makeBox(0, 0, 10, 10);

    Vector2D hit {5, 10};

    auto n = computeBoxNormal(b, hit);

    EXPECT_FLOAT_EQ(n.x,  0.0f);
    EXPECT_FLOAT_EQ(n.y,  1.0f);
}

// 3) testCollision AABB vs AABB -----------------------------------------------
TEST(CollisionSystem, AABB_Overlap)
{
    EntitySystem ecs;
    ecs.createSystem<PositionComponentSystem>();
    auto sys = ecs.createSystem<CollisionSystem>();
    // dummy components
    PositionComponent p1, p2;

    p1.x = 0; p1.y = 0; p1.width = 10; p1.height = 10;
    p2.x = 5; p2.y = 5; p2.width = 10; p2.height = 10;

    EXPECT_TRUE(sys->testCollision(&p1, &p2, 1.0f, 1.0f));
}

TEST(CollisionSystem, AABB_TouchEdge)
{
    EntitySystem ecs;
    ecs.createSystem<PositionComponentSystem>();
    auto sys = ecs.createSystem<CollisionSystem>();
    PositionComponent p1, p2;

    p1.x = 0; p1.y = 0; p1.width = 10; p1.height = 10;
    p2.x = 10; p2.y = 0; p2.width = 10; p2.height = 10;

    EXPECT_TRUE(sys->testCollision(&p1, &p2, 1.0f, 1.0f));
}

TEST(CollisionSystem, AABB_NoOverlap)
{
    EntitySystem ecs;
    ecs.createSystem<PositionComponentSystem>();
    auto sys = ecs.createSystem<CollisionSystem>();
    // PositionComponent p1{0,0,10,10}, p2{11,0,10,10};
    PositionComponent p1, p2;

    p1.x = 0; p1.y = 0; p1.width = 10; p1.height = 10;
    p2.x = 11; p2.y = 0; p2.width = 10; p2.height = 10;

    EXPECT_FALSE(sys->testCollision(&p1, &p2, 1.0f, 1.0f));
}

// 4) traverseGridCells --------------------------------------------------------
TEST(CollisionHelpers, TraverseGridCells_Horizontal)
{
    CollisionSystem sys;
    auto cells = sys.traverseGridCells({5, 10}, {1,0}, 50);
    // should step through cells (0,0)→(1,0)→...
    EXPECT_FALSE(cells.empty());

    PagePos emptyPage{0, 0};

    EXPECT_EQ(cells.front(), emptyPage);
}

TEST(CollisionHelpers, TraverseGridCells_ZeroLength)
{
    EntitySystem ecs;
    ecs.createSystem<PositionComponentSystem>();
    auto sys = ecs.createSystem<CollisionSystem>();

    auto cells = sys->traverseGridCells({0,0}, {1,1}, 0);
    EXPECT_TRUE(cells.empty());
}

// 5) sweepMove ---------------------------------------------------------------
TEST(CollisionSystem, SweepMove_NoObstacles)
{
    EntitySystem ecs;
    ecs.createSystem<PositionComponentSystem>();
    auto sys = ecs.createSystem<CollisionSystem>();
    // empty ECS: no walls
    auto result = sweepMove(sys, {0,0}, {10,10}, {5,0}, {0});

    EXPECT_FLOAT_EQ(result.delta.x, 5.0f);
    EXPECT_FALSE(result.hit);
}
// to test an obstacle, you would need to spawn an entity with Position+Collision
// and layer=0 in the ECS, then call sweepMove against that layer...

// 6) raycast -----------------------------------------------------------------
TEST(CollisionSystem, Raycast_SingleTarget)
{
    EntitySystem ecs;
    ecs.createSystem<PositionComponentSystem>();
    auto colSys = ecs.createSystem<CollisionSystem>();
    ecs.succeed<PositionComponentSystem, CollisionSystem>();

    // spawn a “wall” at (10,0) size 10×10 on layer 2
    auto wall = ecs.createEntity();
    auto wp = ecs.attach<PositionComponent>(wall);
    wp->setX(10); wp->setY(0); wp->setWidth(10); wp->setHeight(10);
    auto wc = ecs.attach<CollisionComponent>(wall, /*layerId=*/2);

    ecs.executeOnce();  // triggers grid insertion

    auto hit = colSys->raycast({0,5}, {1,0}, 100, 2);
    EXPECT_TRUE(hit.hit);
    EXPECT_EQ(hit.entityId, wall.id);
    EXPECT_NEAR(hit.t, 10.0f, 1e-3f);
    EXPECT_FLOAT_EQ(hit.normal.x, -1.0f);
}

// 7) grid insertion/removal --------------------------------------------------
TEST(CollisionSystem, Grid_InsertRemove)
{
    EntitySystem ecs;
    ecs.createSystem<PositionComponentSystem>();
    auto colSys = ecs.createSystem<CollisionSystem>();
    ecs.succeed<PositionComponentSystem, CollisionSystem>();

    auto e = ecs.createEntity();
    auto p = ecs.attach<PositionComponent>(e);
    p->setX(5); p->setY(5); p->setWidth(10); p->setHeight(10);
    auto c = ecs.attach<CollisionComponent>(e, /*layerId=*/0);

    ecs.executeOnce();
    // cell (0,0) should contain e
    auto ids = colSys->getCellEntities(PagePos{0,0}, 0);
    EXPECT_TRUE(ids.count(e.id));

    // now delete component and trigger removal
    ecs.detach<CollisionComponent>(e);
    ecs.executeOnce();
    ids = colSys->getCellEntities(PagePos{0,0}, 0);
    EXPECT_FALSE(ids.count(e.id));
}


// --- rayAABB edge cases -----------------------------------------------------

TEST(CollisionHelpers, RayAABB_EntryBehind)
{
    // Box is behind the origin (ray points +X, box is at negative X)
    AABB b = makeBox(-30, -10, 20, 20);
    Vector2D origin{0, 0}, dir{1, 0};
    auto t = rayAABB(origin, dir, 100, b);
    EXPECT_FALSE(t.has_value());
}

TEST(CollisionHelpers, RayAABB_EdgeGrazing)
{
    // Ray skims the top edge at y=10
    AABB b = makeBox(10, 10, 20, 20);
    Vector2D origin{0, 10}, dir{1, 0};
    auto t = rayAABB(origin, dir, 100, b);
    ASSERT_TRUE(t.has_value());
    EXPECT_NEAR(*t, 10.0f, 1e-3f);
}

// --- computeBoxNormal additional faces --------------------------------------

TEST(CollisionHelpers, ComputeBoxNormal_Right)
{
    AABB b = makeBox(0,0,10,10);
    Vector2D hit{10,5};
    auto n = computeBoxNormal(b,hit);
    EXPECT_FLOAT_EQ(n.x,  1.0f);
    EXPECT_FLOAT_EQ(n.y,  0.0f);
}

TEST(CollisionHelpers, ComputeBoxNormal_Bottom)
{
    AABB b = makeBox(0,0,10,10);
    Vector2D hit{5,0};
    auto n = computeBoxNormal(b,hit);
    EXPECT_FLOAT_EQ(n.x,  0.0f);
    EXPECT_FLOAT_EQ(n.y, -1.0f);
}

TEST(CollisionHelpers, ComputeBoxNormal_Corner)
{
    // hits exactly at corner (0,0), both dLeft=dBottom=0 → picks left or bottom
    AABB b = makeBox(0,0,10,10);
    Vector2D hit{0,0};
    auto n = computeBoxNormal(b,hit);
    // because dLeft==dBottom==0 and left is first in minDist check
    EXPECT_FLOAT_EQ(n.x, -1.0f);
    EXPECT_FLOAT_EQ(n.y,  0.0f);
}

// --- testCollision with scaling ---------------------------------------------

TEST(CollisionSystem, AABB_ScaledOverlap)
{
    EntitySystem ecs;
    ecs.createSystem<PositionComponentSystem>();
    auto sys = ecs.createSystem<CollisionSystem>();
    PositionComponent p1, p2;
    // p1 at (0,0) size 10x10 scaled 2→20x20 centered
    p1.x=0; p1.y=0; p1.width=10; p1.height=10;
    p2.x=15; p2.y=15; p2.width=10; p2.height=10;
    // scale p1 so it overlaps p2
    EXPECT_TRUE(sys->testCollision(&p1, &p2, 2.0f, 1.0f));
}

// --- traverseGridCells vertical & diagonal ----------------------------------

TEST(CollisionHelpers, TraverseGridCells_Vertical)
{
    CollisionSystem sys;
    auto cells = sys.traverseGridCells({5, 5}, {0,1}, 50);
    ASSERT_FALSE(cells.empty());
    EXPECT_EQ(cells.front(), (PagePos{0,0}));
    EXPECT_EQ(cells.back().x, 0);
}

TEST(CollisionHelpers, TraverseGridCells_Diagonal)
{
    CollisionSystem sys;
    auto cells = sys.traverseGridCells({5,5}, {1,1}, 50);
    // should visit a roughly diagonal sequence
    ASSERT_GE(cells.size(), 2u);
    EXPECT_EQ(cells.front(), (PagePos{0,0}));
    EXPECT_NE(cells[1], cells.front());
}

// --- sweepMove edge cases & cardinal obstacles ------------------------------

TEST(CollisionSystem, SweepMove_TinyDelta)
{
    CollisionSystem sys;
    auto res = sweepMove(&sys, {0,0}, {10,10}, {1e-6f,0.0f}, {0});
    EXPECT_FLOAT_EQ(res.delta.x, 0.0f);
    EXPECT_FALSE(res.hit);
}

TEST(CollisionSystem, SweepMove_HeadOn_Right)
{
    EntitySystem ecs;
    ecs.createSystem<PositionComponentSystem>();
    auto sys = ecs.createSystem<CollisionSystem>();
    ecs.succeed<PositionComponentSystem, CollisionSystem>();
    // place a wall immediately to the right of a 10×10 mover at origin
    makeWall(ecs, 10, 0, 10, 10, 0);
    ecs.executeOnce();

    auto res = sweepMove(sys, {0,0}, {10,10}, {20,0}, {0});
    // should move up to just before hitting at x=10: delta ≈ 10 - epsilon
    EXPECT_NEAR(res.delta.x, 10.0f - 1e-3f, 1e-4f);
    EXPECT_TRUE(res.hit);
}

TEST(CollisionSystem, SweepMove_HeadOn_Left)
{
    EntitySystem ecs;
    ecs.createSystem<PositionComponentSystem>();
    auto sys = ecs.createSystem<CollisionSystem>();
    ecs.succeed<PositionComponentSystem, CollisionSystem>();
    makeWall(ecs, -20, 0, 10, 10, 0);
    ecs.executeOnce();

    auto res = sweepMove(sys, {0,0}, {10,10}, {-20,0}, {0});
    EXPECT_NEAR(res.delta.x, - (10.0f - 1e-3f), 1e-4f);
    EXPECT_TRUE(res.hit);
}

TEST(CollisionSystem, SweepMove_HeadOn_Up)
{
    EntitySystem ecs;
    ecs.createSystem<PositionComponentSystem>();
    auto sys = ecs.createSystem<CollisionSystem>();
    ecs.succeed<PositionComponentSystem, CollisionSystem>();
    makeWall(ecs, 0, -20, 10, 10, 0);
    ecs.executeOnce();

    auto res = sweepMove(sys, {0,0}, {10,10}, {0,-20}, {0});
    EXPECT_NEAR(res.delta.y, - (10.0f - 1e-3f), 1e-4f);
    EXPECT_TRUE(res.hit);
}

TEST(CollisionSystem, SweepMove_HeadOn_Down)
{
    EntitySystem ecs;
    ecs.createSystem<PositionComponentSystem>();
    auto sys = ecs.createSystem<CollisionSystem>();
    ecs.succeed<PositionComponentSystem, CollisionSystem>();
    makeWall(ecs, 0, 10, 10, 10, 0);
    ecs.executeOnce();

    auto res = sweepMove(sys, {0,0}, {10,10}, {0,20}, {0});
    EXPECT_NEAR(res.delta.y, 10.0f - 1e-3f, 1e-4f);
    EXPECT_TRUE(res.hit);
}

// --- raycast multiple & cardinal --------------------------------------------

TEST(CollisionSystem, Raycast_MultiplePicksClosest)
{
    EntitySystem ecs;
    ecs.createSystem<PositionComponentSystem>();
    auto rs = ecs.createSystem<CollisionSystem>();
    ecs.succeed<PositionComponentSystem, CollisionSystem>();

    // two walls, one at x=10, one at x=20
    auto w1 = makeWall(ecs, 10, 0, 10, 10, 1);
    auto w2 = makeWall(ecs, 20, 0, 10, 10, 1);
    ecs.executeOnce();

    auto hit = rs->raycast({0,5},{1,0},100,1);
    EXPECT_TRUE(hit.hit);
    EXPECT_EQ(hit.entityId, w1.id);
    EXPECT_NEAR(hit.t, 10.0f, 1e-3f);
}

TEST(CollisionSystem, Raycast_Left)
{
    EntitySystem ecs;
    ecs.createSystem<PositionComponentSystem>();
    auto rs = ecs.createSystem<CollisionSystem>();
    ecs.succeed<PositionComponentSystem, CollisionSystem>();

    makeWall(ecs, -20, 0, 10, 10, 2);
    ecs.executeOnce();
    auto hit = rs->raycast({0,5},{-1,0},100,2);
    ASSERT_TRUE(hit.hit);
    EXPECT_EQ(hit.normal.x, 1.0f);
}

TEST(CollisionSystem, Raycast_Up)
{
    EntitySystem ecs;
    ecs.createSystem<PositionComponentSystem>();
    auto rs = ecs.createSystem<CollisionSystem>();
    ecs.succeed<PositionComponentSystem, CollisionSystem>();

    makeWall(ecs, 0, -20, 10, 10, 3);
    ecs.executeOnce();
    auto hit = rs->raycast({5,0},{0,-1},100,3);
    ASSERT_TRUE(hit.hit);
    EXPECT_EQ(hit.normal.y, 1.0f);
}

TEST(CollisionSystem, Raycast_Down)
{
    EntitySystem ecs;
    ecs.createSystem<PositionComponentSystem>();
    auto rs = ecs.createSystem<CollisionSystem>();
    ecs.succeed<PositionComponentSystem, CollisionSystem>();

    makeWall(ecs, 0, 20, 10, 10, 4);
    ecs.executeOnce();
    auto hit = rs->raycast({5,0},{0,1},100,4);
    ASSERT_TRUE(hit.hit);
    EXPECT_EQ(hit.normal.y, -1.0f);
}
