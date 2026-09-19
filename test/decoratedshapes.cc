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
    }
}
