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

// helper to assert the wall is in the expected cell
static void expectWallInCell(CollisionSystem* sys, _unique_id wallId,
    int cellX, int cellY, int layerId)
{
    PagePos cell{cellX, cellY};
    const auto& ids = sys->getCellEntities(cell, layerId);
    ASSERT_TRUE(ids.count(wallId)) << "Expected wall " << wallId << " in cell ("
        << cellX << "," << cellY << ") on layer " << layerId
        << ", but getCellEntities returned: { "
        << [&] {
            std::string s;
            for (auto id : ids) s += std::to_string(id) + " ";

            return s;
        }() << "}";
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

TEST(CollisionHelpers, RayAABB_EntryBehind)
{
    AABB b = makeBox(-30, -10, 20, 20);
    Vector2D origin{0, 0}, dir{1, 0};
    auto t = rayAABB(origin, dir, 100, b);
    EXPECT_FALSE(t.has_value());
}

TEST(CollisionHelpers, RayAABB_EdgeGrazing)
{
    AABB b = makeBox(10, 10, 20, 20);
    Vector2D origin{0, 10}, dir{1, 0};
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

TEST(CollisionHelpers, ComputeBoxNormal_Right)
{
    AABB b = makeBox(0,0,10,10);
    Vector2D hit{10,5};
    auto n = computeBoxNormal(b, hit);
    EXPECT_FLOAT_EQ(n.x,  1.0f);
    EXPECT_FLOAT_EQ(n.y,  0.0f);
}

TEST(CollisionHelpers, ComputeBoxNormal_Bottom)
{
    AABB b = makeBox(0,0,10,10);
    Vector2D hit{5,0};
    auto n = computeBoxNormal(b, hit);
    EXPECT_FLOAT_EQ(n.x,  0.0f);
    EXPECT_FLOAT_EQ(n.y, -1.0f);
}

TEST(CollisionHelpers, ComputeBoxNormal_Top)
{
    AABB b = makeBox(0, 0, 10, 10);
    Vector2D hit {5, 10};
    auto n = computeBoxNormal(b, hit);
    EXPECT_FLOAT_EQ(n.x,  0.0f);
    EXPECT_FLOAT_EQ(n.y,  1.0f);
}

TEST(CollisionHelpers, ComputeBoxNormal_Corner)
{
    AABB b = makeBox(0,0,10,10);
    Vector2D hit{0,0};
    auto n = computeBoxNormal(b,hit);
    EXPECT_FLOAT_EQ(n.x, -1.0f);
    EXPECT_FLOAT_EQ(n.y,  0.0f);
}

// 3) testCollision AABB vs AABB -----------------------------------------------
TEST(CollisionSystem, AABB_Overlap)
{
    EntitySystem ecs;
    ecs.createSystem<PositionComponentSystem>();
    auto sys = ecs.createSystem<CollisionSystem>();

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
    EXPECT_FALSE(cells.empty());
    EXPECT_EQ(cells.front(), (PagePos{0,0}));
}

TEST(CollisionHelpers, TraverseGridCells_Vertical)
{
    CollisionSystem sys;
    auto cells = sys.traverseGridCells({5, 5}, {0,1}, 50);
    EXPECT_FALSE(cells.empty());
    EXPECT_EQ(cells.front(), (PagePos{0,0}));
}

TEST(CollisionHelpers, TraverseGridCells_Diagonal)
{
    CollisionSystem sys;
    auto cells = sys.traverseGridCells({5,5}, {1,1}, 50);
    EXPECT_GE(cells.size(), 2u);
    EXPECT_EQ(cells.front(), (PagePos{0,0}));
}

TEST(CollisionHelpers, TraverseGridCells_ZeroLength)
{
    CollisionSystem sys;
    auto cells = sys.traverseGridCells({0,0}, {1,1}, 0);
    EXPECT_TRUE(cells.empty());
}

// 5) sweepMove ---------------------------------------------------------------
static EntityRef makeWall(EntitySystem& ecs, float x, float y, float w, float h, int layer)
{
    auto e = ecs.createEntity();
    auto p = ecs.attach<PositionComponent>(e);
    p->setX(x);
    p->setY(y);
    p->setWidth(w);
    p->setHeight(h);
    ecs.attach<CollisionComponent>(e, layer);
    return e;
}

TEST(CollisionSystem, SweepMove_NoObstacles)
{
    EntitySystem ecs;
    ecs.createSystem<PositionComponentSystem>();
    auto sys = ecs.createSystem<CollisionSystem>();
    ecs.succeed<PositionComponentSystem, CollisionSystem>();
    ecs.executeOnce();

    auto result = sweepMove(sys, {0,0}, {10,10}, {5,0}, {0});

    EXPECT_NEAR(result.delta.x, 5.0f, 1e-3f);
    EXPECT_FALSE(result.hit);
}

TEST(CollisionSystem, SweepMove_HeadOn_Right)
{
    EntitySystem ecs;
    ecs.createSystem<PositionComponentSystem>();
    auto sys = ecs.createSystem<CollisionSystem>();
    ecs.succeed<PositionComponentSystem, CollisionSystem>();

    auto wall = makeWall(ecs, 20, 0, 10, 10, 0);
    ecs.executeOnce();

    expectWallInCell(sys, wall.id, /*cellX=*/1, /*cellY=*/0, /*layer=*/0);

    auto res = sweepMove(sys, {0,0}, {10,10}, {20,0}, {0});

    // Todo tame the back off a bit so that the margin of 1e-2 can get small
    EXPECT_NEAR(res.delta.x, 10.0f - 10.0f * 1e-3f, 1e-2f);
    EXPECT_TRUE(res.hit);
}

TEST(CollisionSystem, SweepMove_HeadOn_Left)
{
    EntitySystem ecs;
    ecs.createSystem<PositionComponentSystem>();
    auto sys = ecs.createSystem<CollisionSystem>();
    ecs.succeed<PositionComponentSystem, CollisionSystem>();

    auto wall = makeWall(ecs, -20, 0, 10, 10, 0);
    ecs.executeOnce();

    expectWallInCell(sys, wall.id, /*cellX=*/-1, /*cellY=*/0, /*layer=*/0);

    auto res = sweepMove(sys, {0,0}, {10,10}, {-20,0}, {0});

    ecs.executeOnce();

    EXPECT_NEAR(res.delta.x, -(10.0f - 10.0f * 1e-3f), 1e-2f);
    EXPECT_TRUE(res.hit);
}

TEST(CollisionSystem, SweepMove_HeadOn_Up)
{
    EntitySystem ecs;
    ecs.createSystem<PositionComponentSystem>();
    auto sys = ecs.createSystem<CollisionSystem>();
    ecs.succeed<PositionComponentSystem, CollisionSystem>();

    auto wall = makeWall(ecs, 0, -20, 10, 10, 0);
    ecs.executeOnce();

    expectWallInCell(sys, wall.id, /*cellX=*/0, /*cellY=*/-1, /*layer=*/0);

    auto res = sweepMove(sys, {0,0}, {10,10}, {0,-20}, {0});

    ecs.executeOnce();

    EXPECT_NEAR(res.delta.y, -(10.0f - 10.0f * 1e-3f), 1e-2f);
    EXPECT_TRUE(res.hit);
}

TEST(CollisionSystem, SweepMove_HeadOn_Down)
{
    EntitySystem ecs;
    ecs.createSystem<PositionComponentSystem>();
    auto sys = ecs.createSystem<CollisionSystem>();
    ecs.succeed<PositionComponentSystem, CollisionSystem>();

    auto wall = makeWall(ecs, 0, 30, 10, 10, 0);
    ecs.executeOnce();

    expectWallInCell(sys, wall.id, /*cellX=*/0, /*cellY=*/1, /*layer=*/0);

    auto res = sweepMove(sys, {0,0}, {10,10}, {0,30}, {0});

    ecs.executeOnce();

    EXPECT_NEAR(res.delta.y, 20.0f * (1.0f - 1e-3f), 1e-2f);
    EXPECT_TRUE(res.hit);
}

// 6) raycast -----------------------------------------------------------------
TEST(CollisionSystem, Raycast_SingleTarget)
{
    EntitySystem ecs;
    ecs.createSystem<PositionComponentSystem>();
    auto colSys = ecs.createSystem<CollisionSystem>();
    ecs.succeed<PositionComponentSystem, CollisionSystem>();

    auto wall = makeWall(ecs, 10, 0, 10, 10, 2);
    ecs.executeOnce();

    expectWallInCell(colSys, wall.id, 0,0,2);

    auto hit = colSys->raycast({0,5}, {1,0}, 100, 2);
    ASSERT_TRUE(hit.hit);
    EXPECT_EQ(hit.entityId, wall.id);
    EXPECT_NEAR(hit.t, 10.0f, 1e-3f);
    EXPECT_FLOAT_EQ(hit.normal.x, -1.0f);
}

TEST(CollisionSystem, Raycast_MultiplePicksClosest)
{
    EntitySystem ecs;
    ecs.createSystem<PositionComponentSystem>();
    auto rs = ecs.createSystem<CollisionSystem>();
    ecs.succeed<PositionComponentSystem, CollisionSystem>();

    auto w1 = makeWall(ecs, 10, 0, 10, 10, 1);
    makeWall(ecs, 20, 0, 10, 10, 1);
    ecs.executeOnce();

    auto hit = rs->raycast({0,5}, {1,0}, 100, 1);
    ASSERT_TRUE(hit.hit);
    EXPECT_EQ(hit.entityId, w1.id);
    EXPECT_NEAR(hit.t, 10.0f, 1e-3f);
}

TEST(CollisionSystem, Raycast_Left)
{
    EntitySystem ecs;
    ecs.createSystem<PositionComponentSystem>();
    auto rs = ecs.createSystem<CollisionSystem>();
    ecs.succeed<PositionComponentSystem, CollisionSystem>();

    makeWall(ecs, -20, 0, 10, 10, 3);
    ecs.executeOnce();
    auto hit = rs->raycast({0,5}, {-1,0}, 100, 3);
    ASSERT_TRUE(hit.hit);
    EXPECT_FLOAT_EQ(hit.normal.x, 1.0f);
}

TEST(CollisionSystem, Raycast_Up)
{
    EntitySystem ecs;
    ecs.createSystem<PositionComponentSystem>();
    auto rs = ecs.createSystem<CollisionSystem>();
    ecs.succeed<PositionComponentSystem, CollisionSystem>();

    makeWall(ecs, 0, -20, 10, 10, 4);
    ecs.executeOnce();
    auto hit = rs->raycast({5,0}, {0,-1}, 100, 4);
    ASSERT_TRUE(hit.hit);
    EXPECT_FLOAT_EQ(hit.normal.y, 1.0f);
}

TEST(CollisionSystem, Raycast_Down)
{
    EntitySystem ecs;
    ecs.createSystem<PositionComponentSystem>();
    auto rs = ecs.createSystem<CollisionSystem>();
    ecs.succeed<PositionComponentSystem, CollisionSystem>();

    makeWall(ecs, 0, 20, 10, 10, 5);
    ecs.executeOnce();
    auto hit = rs->raycast({5,0}, {0,1}, 100, 5);
    ASSERT_TRUE(hit.hit);
    EXPECT_FLOAT_EQ(hit.normal.y, -1.0f);
}

// --- Grid insertion correctness ---------------------------------------------
TEST(CollisionSystem, GridInsertion_CorrectLayer)
{
    EntitySystem ecs;
    ecs.createSystem<PositionComponentSystem>();
    auto colSys = ecs.createSystem<CollisionSystem>();
    ecs.succeed<PositionComponentSystem, CollisionSystem>();

    // Place an entity at (5,5), size 10×10 on layer 7
    auto e = ecs.createEntity();
    auto p = ecs.attach<PositionComponent>(e);
    p->setX(5);
    p->setY(5);
    p->setWidth(10);
    p->setHeight(10);

    int layerId = 7;
    ecs.attach<CollisionComponent>(e, layerId);

    ecs.executeOnce();  // force grid insertion

    // In your system, cell size is 20×20, so (5,5) → cell (0,0)
    PagePos cell{0, 0};

    const auto& ids = colSys->getCellEntities(cell, layerId);
    EXPECT_TRUE(ids.count(e.id))
        << "Entity should have been inserted into cell (0,0) on layer " << layerId;
}

// --- Raycast layer filtering ------------------------------------------------
TEST(CollisionSystem, Raycast_IgnoresOtherLayers)
{
    EntitySystem ecs;
    ecs.createSystem<PositionComponentSystem>();
    auto rs = ecs.createSystem<CollisionSystem>();
    ecs.succeed<PositionComponentSystem, CollisionSystem>();

    // Create a wall at (10,0) on layer 1
    auto wall = ecs.createEntity();
    auto wp   = ecs.attach<PositionComponent>(wall);
    wp->setX(10);
    wp->setY(0);
    wp->setWidth(10);
    wp->setHeight(10);
    ecs.attach<CollisionComponent>(wall, /*layerId=*/1);

    ecs.executeOnce();  // grid insertion

    // Raycast on the *wrong* layer
    auto miss = rs->raycast({0,5}, {1,0}, 100, /*layerId=*/2);
    EXPECT_FALSE(miss.hit)
        << "Raycast should not hit the wall when querying layer 2";

    // Raycast on the *correct* layer
    auto hit = rs->raycast({0,5}, {1,0}, 100, /*layerId=*/1);
    ASSERT_TRUE(hit.hit);
    EXPECT_EQ(hit.entityId, wall.id)
        << "Raycast should hit the wall when querying its layer 1";
    EXPECT_NEAR(hit.t, 10.0f, 1e-3f);
    EXPECT_FLOAT_EQ(hit.normal.x, -1.0f);
}

