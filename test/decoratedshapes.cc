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
    }
}
