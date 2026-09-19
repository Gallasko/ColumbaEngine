#include "stdafx.h"

#include <gtest/gtest.h>

#include "2D/decoratedshapes.h"

#include "Renderer/renderer.h"

#include "ECS/entitysystem.h"

#include "mocklogger.h"

namespace pg
{
    namespace test
    {
        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(decoratedshapes_test, hatch_material_layout)
        {
            MockLogger logger;
            EntitySystem ecs;
            MasterRenderer renderer;

            ecs.createSystem<PositionComponentSystem>();
            auto* sys = ecs.createSystem<HatchRect2DObjectSystem>(&renderer);

            auto shape = makeHatchRect2DShape(&ecs, 100.0f, 10.0f, {31.0f, 96.0f, 85.0f, 255.0f});

            ecs.executeOnce();

            const RenderCall& call = sys->entityRenderCalls.at(shape.id);
            ASSERT_EQ(call.data.size(), 14u);
            EXPECT_FLOAT_EQ(call.data[3],  100.0f);
            EXPECT_FLOAT_EQ(call.data[10], 6.0f);
            EXPECT_FLOAT_EQ(call.data[11], 2.0f);
            EXPECT_FLOAT_EQ(call.data[12], 45.0f);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(decoratedshapes_test, hatch_change_event_rebuilds)
        {
            MockLogger logger;
            EntitySystem ecs;
            MasterRenderer renderer;

            ecs.createSystem<PositionComponentSystem>();
            auto* sys = ecs.createSystem<HatchRect2DObjectSystem>(&renderer);

            auto shape = makeHatchRect2DShape(&ecs, 100.0f, 10.0f, {31.0f, 96.0f, 85.0f, 255.0f});
            ecs.executeOnce();

            shape.get<HatchRect2DObject>()->setSpacing(8.0f);
            ecs.executeOnce();

            const RenderCall& call = sys->entityRenderCalls.at(shape.id);
            EXPECT_FLOAT_EQ(call.data[10], 8.0f);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(decoratedshapes_test, dotted_material_layout)
        {
            MockLogger logger;
            EntitySystem ecs;
            MasterRenderer renderer;

            ecs.createSystem<PositionComponentSystem>();
            auto* sys = ecs.createSystem<DottedLine2DObjectSystem>(&renderer);

            auto shape = makeDottedLine2DShape(&ecs, 160.0f, {203.0f, 184.0f, 148.0f, 255.0f});

            ecs.executeOnce();

            const RenderCall& call = sys->entityRenderCalls.at(shape.id);
            ASSERT_EQ(call.data.size(), 12u);
            EXPECT_FLOAT_EQ(call.data[4],  2.5f);   // height = 2 * dotRadius + 1
            EXPECT_FLOAT_EQ(call.data[10], 4.0f);
            EXPECT_FLOAT_EQ(call.data[11], 0.75f);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(decoratedshapes_test, dotted_leader_spans_anchor_gap)
        {
            MockLogger logger;
            EntitySystem ecs;
            MasterRenderer renderer;

            ecs.createSystem<PositionComponentSystem>();
            ecs.createSystem<DottedLine2DObjectSystem>(&renderer);

            // A name box on the left and a figure box on the right.
            auto first = ecs.createEntity();
            auto firstPos = ecs.attach<PositionComponent>(first);
            ecs.attach<UiAnchor>(first);
            firstPos->setX(0.0f);
            firstPos->setWidth(40.0f);

            auto second = ecs.createEntity();
            auto secondPos = ecs.attach<PositionComponent>(second);
            ecs.attach<UiAnchor>(second);
            secondPos->setX(200.0f);
            secondPos->setWidth(30.0f);

            // Leader anchored from the first's right edge to the second's left edge.
            auto leader = makeDottedLine2DShape(&ecs, 10.0f, {203.0f, 184.0f, 148.0f, 255.0f});
            auto leaderAnchor = leader.get<UiAnchor>();
            leaderAnchor->setLeftAnchor(PosAnchor{first.id, AnchorType::Right});
            leaderAnchor->setRightAnchor(PosAnchor{second.id, AnchorType::Left});

            for (int i = 0; i < 3; ++i)
                ecs.executeOnce();

            auto leaderPos = leader.get<PositionComponent>();
            EXPECT_FLOAT_EQ(leaderPos->x,     40.0f);
            EXPECT_FLOAT_EQ(leaderPos->width, 160.0f);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(decoratedshapes_test, stroke_material_layout)
        {
            MockLogger logger;
            EntitySystem ecs;
            MasterRenderer renderer;

            ecs.createSystem<PositionComponentSystem>();
            auto* sys = ecs.createSystem<StrokeRect2DObjectSystem>(&renderer);

            auto shape = makeStrokeRect2DShape(&ecs, 160.0f, 90.0f, {128.0f, 106.0f, 74.0f, 255.0f});

            ecs.executeOnce();

            const RenderCall& call = sys->entityRenderCalls.at(shape.id);
            ASSERT_EQ(call.data.size(), 14u);
            EXPECT_FLOAT_EQ(call.data[10], 1.0f);
            EXPECT_FLOAT_EQ(call.data[11], 0.0f);
            EXPECT_FLOAT_EQ(call.data[12], 0.0f);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(decoratedshapes_test, stroke_doubled_flag)
        {
            MockLogger logger;
            EntitySystem ecs;
            MasterRenderer renderer;

            ecs.createSystem<PositionComponentSystem>();
            auto* sys = ecs.createSystem<StrokeRect2DObjectSystem>(&renderer);

            auto shape = makeStrokeRect2DShape(&ecs, 50.0f, 50.0f, {138.0f, 106.0f, 22.0f, 255.0f}, 1.0f, 1.0f, true);

            ecs.executeOnce();

            const RenderCall& call = sys->entityRenderCalls.at(shape.id);
            EXPECT_FLOAT_EQ(call.data[12], 1.0f);
            EXPECT_FLOAT_EQ(call.data[11], 1.0f);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(decoratedshapes_test, stroke_keeps_bounds)
        {
            MockLogger logger;
            EntitySystem ecs;
            MasterRenderer renderer;

            ecs.createSystem<PositionComponentSystem>();
            ecs.createSystem<StrokeRect2DObjectSystem>(&renderer);

            // A 200x100 parent panel.
            auto parent = ecs.createEntity();
            auto parentPos = ecs.attach<PositionComponent>(parent);
            auto parentAnchor = ecs.attach<UiAnchor>(parent);
            parentPos->setWidth(200.0f);
            parentPos->setHeight(100.0f);

            // A stroked rect filling the parent; the stroke is inset, so no size compensation.
            auto stroke = makeStrokeRect2DShape(&ecs, 0.0f, 0.0f, {128.0f, 106.0f, 74.0f, 255.0f});
            stroke.get<UiAnchor>()->fillIn(*parentAnchor);

            for (int i = 0; i < 3; ++i)
                ecs.executeOnce();

            auto strokePos = stroke.get<PositionComponent>();
            EXPECT_FLOAT_EQ(strokePos->width,  200.0f);
            EXPECT_FLOAT_EQ(strokePos->height, 100.0f);
        }
    }
}
