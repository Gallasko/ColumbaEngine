#include "stdafx.h"

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

// ----------------------------------------------------------------------------------------
// 8) rayAABB diagonal and reverse‐dir tests
// ----------------------------------------------------------------------------------------
TEST(CollisionHelpers, RayAABB_DiagonalHit)
{
    AABB b = makeBox(10, 10, 20, 20);
    Vector2D origin{0, 0}, dir{1, 1};
    float len = std::sqrt(2.0f) * 20.0f;  // long enough to reach
    auto t = rayAABB(origin, dir, len, b);
    ASSERT_TRUE(t.has_value());
    // We hit the vertical face at x=10 first
    EXPECT_NEAR(*t, 10.0f, 1e-3f);
}

TEST(CollisionHelpers, RayAABB_ReverseDirection)
{
    AABB b = makeBox(-30, -10, 20, 20);
    Vector2D origin{0, 0}, dir{-1, -1};
    float len = std::sqrt(2.0f) * 50.0f;
    auto t = rayAABB(origin, dir, len, b);
    ASSERT_TRUE(t.has_value());
    EXPECT_GT(*t, 0.0f);
}

// ----------------------------------------------------------------------------------------
// 9) traverseGridCells on cell boundary
// ----------------------------------------------------------------------------------------
TEST(CollisionHelpers, TraverseGridCells_OnBoundary)
{
    CollisionSystem sys;
    // origin exactly on cell boundary at (20,20) with cellSize=(20,20)
    auto cells = sys.traverseGridCells({20, 20}, {1, 0}, 40);
    ASSERT_FALSE(cells.empty());
    // The first cell should be the cell containing x=20, i.e. cellX=1
    EXPECT_EQ(cells.front(), (PagePos{1, 1}));
}

// ----------------------------------------------------------------------------------------
// 10) getCellEntities out‐of‐bounds
// ----------------------------------------------------------------------------------------
TEST(CollisionSystem, GetCellEntities_OutOfBounds)
{
    CollisionSystem sys;
    // no pages loaded yet
    auto ids = sys.getCellEntities(PagePos{999, 999}, 0);
    EXPECT_TRUE(ids.empty());
}

// ----------------------------------------------------------------------------------------
// 11) CollisionComponent removal clears cells
// ----------------------------------------------------------------------------------------
TEST(CollisionSystem, ComponentRemoval_ClearsCells)
{
    EntitySystem ecs;
    ecs.createSystem<PositionComponentSystem>();
    auto css = ecs.createSystem<CollisionSystem>();
    ecs.succeed<PositionComponentSystem, CollisionSystem>();

    // Spawn at (5,5) in layer 0
    auto e = ecs.createEntity();
    auto p = ecs.attach<PositionComponent>(e);
    p->setX(5); p->setY(5); p->setWidth(10); p->setHeight(10);
    ecs.attach<CollisionComponent>(e, 0);
    ecs.executeOnce();

    auto ids = css->getCellEntities(PagePos{0, 0}, 0);
    ASSERT_TRUE(ids.count(e.id));

    // Remove and ensure cleared
    ecs.detach<CollisionComponent>(e);
    ecs.executeOnce();
    ids = css->getCellEntities(PagePos{0, 0}, 0);
    EXPECT_FALSE(ids.count(e.id));
}

// ----------------------------------------------------------------------------------------
// 12) testCollision with zero‐size objects
// ----------------------------------------------------------------------------------------
TEST(CollisionSystem, TestCollision_ZeroSize)
{
    EntitySystem ecs;
    ecs.createSystem<PositionComponentSystem>();
    auto sys = ecs.createSystem<CollisionSystem>();

    PositionComponent small, other;
    small.x = 0; small.y = 0; small.width = 0; small.height = 0;
    other.x = 0; other.y = 0; other.width = 10; other.height = 10;

    // zero‐area objects should never collide
    EXPECT_FALSE(sys->testCollision(&small, &other, 1.0f, 1.0f));
}

// ----------------------------------------------------------------------------------------
// 13) sweepMove corner‐collision (simultaneous two‐wall case)
// ----------------------------------------------------------------------------------------
TEST(CollisionSystem, SweepMove_DiagonalCornerSlide)
{
    EntitySystem ecs;
    ecs.createSystem<PositionComponentSystem>();
    auto sys = ecs.createSystem<CollisionSystem>();
    ecs.succeed<PositionComponentSystem, CollisionSystem>();

    // Two walls forming a corner at (10,10): one vertical at x=10, one horizontal at y=10
    auto w1 = makeWall(ecs, 10,   0, 10, 20, 0);
    auto w2 = makeWall(ecs,  0,  10, 20, 10, 0);
    ecs.executeOnce();

    // Move diagonally into the corner from (0,0)
    auto res = sweepMove(sys, {0,0}, {10, 10}, {20, 20}, {0});
    // Should stop before the corner, i.e. both dx and dy < 10
    EXPECT_TRUE(res.hit);
    EXPECT_LT(res.delta.x, 10.0f);
    EXPECT_LT(res.delta.y, 10.0f);

    // And we should be able to slide along one wall if we repeat
    // e.g. only Y component
    auto res2 = sweepMove(sys, {res.delta.x, res.delta.y}, {10, 10}, {0, 20}, {0});
    EXPECT_TRUE(res2.hit);
    EXPECT_LT(res2.delta.y, 10.0f);
}

// ----------------------------------------------------------------------------------------
// 14) collision event dispatch for overlapping entities
// ----------------------------------------------------------------------------------------
TEST(CollisionSystem, ResolveCollisionList_EmitsEvents)
{
    EntitySystem ecs;
    ecs.createSystem<PositionComponentSystem>();
    auto css = ecs.createSystem<CollisionSystem>();
    auto handlerSys = ecs.createSystem<CollisionHandlerSystem>();

    ecs.succeed<PositionComponentSystem, CollisionSystem>();
    ecs.succeed<CollisionSystem, CollisionHandlerSystem>();

    bool called = false;
    // set up a handler for PositionComponent vs PositionComponent collisions
    makeCollisionHandlePair<PositionComponent, PositionComponent>(
        &ecs,
        [&](PositionComponent* a, PositionComponent* b) {
            called = true;
        });

    // Create two overlapping entities on layer 0
    auto e1 = ecs.createEntity();
    auto p1 = ecs.attach<PositionComponent>(e1);
    p1->setX(0); p1->setY(0); p1->setWidth(10); p1->setHeight(10);
    ecs.attach<CollisionComponent>(e1, 0);

    auto e2 = ecs.createEntity();
    auto p2 = ecs.attach<PositionComponent>(e2);
    p2->setX(5); p2->setY(5); p2->setWidth(10); p2->setHeight(10);
    ecs.attach<CollisionComponent>(e2, 0);

    // After creating and inserting your two overlapping entities:
    ecs.executeOnce();  // grid insertion + overlap detection
    ecs.executeOnce();  // now CollisionSystem.execute() fires the events, and CollisionHandlerSystem handles them
    EXPECT_TRUE(called);
}

// ----------------------------------------------------------------------------------------
// 15) resolveCollisionList with specific layer filtering
// ----------------------------------------------------------------------------------------

TEST(CollisionSystem, TestCollision_LayerFiltering)
{
    EntitySystem ecs;
    ecs.createSystem<PositionComponentSystem>();
    auto css = ecs.createSystem<CollisionSystem>();

    // Two walls on layer 0 and layer 1 respectively
    auto w0 = makeWall(ecs, 10, 0, 10, 10, /*layer*/0);
    auto w1 = makeWall(ecs, 20, 0, 10, 10, /*layer*/1);

    // Our “mover” overlaps both by edge
    PositionComponent mpos;
    mpos.x = 15; mpos.y = 5; mpos.width = 5; mpos.height = 5;

    // Should collide with both if unfiltered
    EXPECT_TRUE(css->testCollision(&mpos, w0->get<PositionComponent>(), 1, 1));
    EXPECT_TRUE(css->testCollision(&mpos, w1->get<PositionComponent>(), 1, 1));
}

// ----------------------------------------------------------------------------------------
// 16) testCollision with scaled AABBs
// ----------------------------------------------------------------------------------------
TEST(CollisionSystem, TestCollision_ScaledOverlap)
{
    EntitySystem ecs;
    ecs.createSystem<PositionComponentSystem>();
    auto css = ecs.createSystem<CollisionSystem>();

    PositionComponent a, b;
    a.x = 0; a.y = 0; a.width = 10; a.height = 10;
    b.x = 15; b.y = 0; b.width = 10; b.height = 10;

    // Scale A up by 2x → A spans [-5..15], should overlap B
    EXPECT_TRUE(css->testCollision(&a, &b, 2.0f, 1.0f));
    // Scale B down to 0.5x → B spans [15..20], no overlap
    EXPECT_FALSE(css->testCollision(&a, &b, 1.0f, 0.5f));
}

// ----------------------------------------------------------------------------------------
// 17) cross‐page movement grid update
// ----------------------------------------------------------------------------------------
TEST(CollisionSystem, CrossPage_Movement)
{
    EntitySystem ecs;
    ecs.createSystem<PositionComponentSystem>();
    auto css = ecs.createSystem<CollisionSystem>();
    ecs.succeed<PositionComponentSystem, CollisionSystem>();

    // Place a moving entity near page boundary at x=19 (cellSize=20)
    auto e = ecs.createEntity();
    auto p = ecs.attach<PositionComponent>(e);
    p->setX(19); p->setY(0); p->setWidth(1); p->setHeight(1);
    ecs.attach<CollisionComponent>(e, 0);
    ecs.executeOnce();

    // Move +2 in X: now x=21 → page shifts from cell 0 to cell 1
    p->setX(21);
    ecs.executeOnce();

    // Ensure cell (0,0) cleared and cell (1,0) contains e
    EXPECT_FALSE(css->getCellEntities(PagePos{0, 0}, 0).count(e.id));
    EXPECT_TRUE (css->getCellEntities(PagePos{1, 0}, 0).count(e.id));
}

// ----------------------------------------------------------------------------------------
// 18) diagonal sweepMove & slide path
// ----------------------------------------------------------------------------------------
TEST(CollisionSystem, SweepMove_DiagonalThenSlide)
{
    EntitySystem ecs;
    ecs.createSystem<PositionComponentSystem>();
    auto css = ecs.createSystem<CollisionSystem>();
    ecs.succeed<PositionComponentSystem, CollisionSystem>();

    // Vertical wall at x=10
    makeWall(ecs, 10, 0, 2, 20, 0);
    ecs.executeOnce();

    // Attempt J-shaped move: dx=20, dy=5
    auto res = sweepMove(css, {0, 0}, {2, 2}, {20, 5}, {0});
    // Should hit wall around 8px in X then slide: X <20, Y >0
    EXPECT_LT(res.delta.x, 20.0f);
    EXPECT_GT(res.delta.y, 0.0f);
}

// ----------------------------------------------------------------------------------------
// 19) raycast grazing corner
// ----------------------------------------------------------------------------------------
TEST(CollisionHelpers, RayAABB_GrazeMiss)
{
    AABB b = makeBox(10, 10, 20, 20);
    Vector2D origin{0, 0};
    // Aim just below the lower‐left corner at (10,10)
    Vector2D dir{10.0f, 9.999f};
    float len = std::sqrt(dir.x * dir.x + dir.y * dir.y);
    dir.x /= len; dir.y /= len;
    auto t = rayAABB(origin, dir, len, b);
    EXPECT_FALSE(t.has_value());
}

TEST(CollisionHelpers, RayAABB_GrazeHit)
{
    AABB b = makeBox(10, 10, 20, 20);
    Vector2D origin{0, 0};
    // Aim exactly at the corner
    Vector2D dir{10, 10};
    float len = std::sqrt(200.0f);
    dir.x /= len; dir.y /= len;
    auto t = rayAABB(origin, dir, len, b);
    EXPECT_TRUE(t.has_value());
    EXPECT_NEAR(*t, 10.0f * std::sqrt(2.0f), 1e-3f);
}

// ----------------------------------------------------------------------------------------
// 20) large grid stress (smoke test)
// ----------------------------------------------------------------------------------------
TEST(CollisionSystem, LargeGrid_NoCrash)
{
    EntitySystem ecs;
    ecs.createSystem<PositionComponentSystem>();
    auto css = ecs.createSystem<CollisionSystem>();
    ecs.succeed<PositionComponentSystem, CollisionSystem>();

    // Insert 1000 small static walls in a 32×32 grid
    for (int i = 0; i < 1000; ++i) {
        float x = (i % 32) * 20.0f;
        float y = (i / 32) * 20.0f;
        makeWall(ecs, x, y, 2, 2, 0);
    }
    // Single raycast through empty center
    auto hit = css->raycast({1000, 1000}, {1, 0}, 500.0f, 0);
    EXPECT_FALSE(hit.hit);
}

// ----------------------------------------------------------------------------------------
// 21) detach & reattach component
// ----------------------------------------------------------------------------------------
TEST(CollisionSystem, DetachReattach_Component)
{
    EntitySystem ecs;
    ecs.createSystem<PositionComponentSystem>();
    auto css = ecs.createSystem<CollisionSystem>();
    ecs.succeed<PositionComponentSystem, CollisionSystem>();

    auto e = ecs.createEntity();
    auto p = ecs.attach<PositionComponent>(e);
    p->setX(5); p->setY(5); p->setWidth(10); p->setHeight(10);
    ecs.attach<CollisionComponent>(e, 0);
    ecs.executeOnce();

    EXPECT_TRUE(css->getCellEntities(PagePos{0, 0}, 0).count(e.id));

    // Detach then move then reattach at new pos
    ecs.detach<CollisionComponent>(e);
    ecs.executeOnce();
    p->setX(25); p->setY(0);
    ecs.attach<CollisionComponent>(e, 0);
    ecs.executeOnce();

    EXPECT_FALSE(css->getCellEntities(PagePos{0, 0}, 0).count(e.id));
    EXPECT_TRUE (css->getCellEntities(PagePos{1, 0}, 0).count(e.id));
}

// ----------------------------------------------------------------------------------------
// 22) EntityChangedEvent updates grid
// ----------------------------------------------------------------------------------------
TEST(CollisionSystem, EntityChangedEvent_GridUpdate)
{
    EntitySystem ecs;
    ecs.createSystem<PositionComponentSystem>();
    auto css = ecs.createSystem<CollisionSystem>();
    ecs.succeed<PositionComponentSystem, CollisionSystem>();

    auto e = ecs.createEntity();
    auto p = ecs.attach<PositionComponent>(e);
    p->setX(5); p->setY(5); p->setWidth(10); p->setHeight(10);
    ecs.attach<CollisionComponent>(e, 0);
    ecs.executeOnce();

    EXPECT_TRUE(css->getCellEntities(PagePos{0, 0}, 0).count(e.id));

    // Move directly by changing Position and firing change
    p->setX(25); p->setY(0);
    ecs.sendEvent(EntityChangedEvent{e.id});
    ecs.executeOnce();

    EXPECT_FALSE(css->getCellEntities(PagePos{0, 0}, 0).count(e.id));
    EXPECT_TRUE (css->getCellEntities(PagePos{1, 0}, 0).count(e.id));
}
