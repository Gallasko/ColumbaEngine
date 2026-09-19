#include "stdafx.h"

#include <gtest/gtest.h>

#include "UI/iconsystem.h"

#include "ECS/entitysystem.h"

#include "mocklogger.h"

namespace pg
{
    namespace test
    {
        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(iconsystem, render_call_data)
        {
            MockLogger logger;
            EntitySystem ecs;
            MasterRenderer renderer;

            ecs.createSystem<PositionComponentSystem>();
            auto sys = ecs.createSystem<IconSystem>(&renderer);

            sys->registerEntriesForTest("t", {{"time", 24, {24, 24}, {0.0f, 0.0f}, {0.5f, 0.5f}}}, 1024, 1024);

            auto icon = makeIcon(&ecs, "t", "time", 24, {255.0f, 0.0f, 0.0f, 255.0f});

            ecs.executeOnce();

            const RenderCall* call = sys->getRenderCall(icon.id);
            ASSERT_NE(call, nullptr);
            ASSERT_EQ(call->data.size(), 15u);

            // Colours normalised to 0-1: alpha and red full, green and blue zero.
            EXPECT_FLOAT_EQ(call->data[6], 1.0f);
            EXPECT_FLOAT_EQ(call->data[7], 1.0f);
            EXPECT_FLOAT_EQ(call->data[8], 0.0f);
            EXPECT_FLOAT_EQ(call->data[9], 0.0f);

            // UV rectangle of the picked entry.
            EXPECT_FLOAT_EQ(call->data[11], 0.0f);
            EXPECT_FLOAT_EQ(call->data[12], 0.0f);
            EXPECT_FLOAT_EQ(call->data[13], 0.5f);
            EXPECT_FLOAT_EQ(call->data[14], 0.5f);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(iconsystem, picks_nearest_size)
        {
            MockLogger logger;
            EntitySystem ecs;
            MasterRenderer renderer;

            ecs.createSystem<PositionComponentSystem>();
            auto sys = ecs.createSystem<IconSystem>(&renderer);

            // Distinct UVs per size so the render call reveals which was picked.
            std::vector<IconEntry> entries = {
                {"time", 24, {24, 24}, {0.0f,  0.0f}, {0.25f, 0.25f}},
                {"time", 48, {48, 48}, {0.25f, 0.0f}, {0.75f, 0.5f}}};
            sys->registerEntriesForTest("t", entries, 1024, 1024);

            auto wide   = makeIcon(&ecs, "t", "time", 30); // -> 48
            auto exact  = makeIcon(&ecs, "t", "time", 24); // -> 24
            auto bigger = makeIcon(&ecs, "t", "time", 64); // -> 48 (largest fallback)

            ecs.executeOnce();

            const RenderCall* wideCall   = sys->getRenderCall(wide.id);
            const RenderCall* exactCall  = sys->getRenderCall(exact.id);
            const RenderCall* biggerCall = sys->getRenderCall(bigger.id);

            ASSERT_NE(wideCall,   nullptr);
            ASSERT_NE(exactCall,  nullptr);
            ASSERT_NE(biggerCall, nullptr);

            // data[11] is uvTopLeft.x: 0.0 for the 24px entry, 0.25 for the 48px one.
            EXPECT_FLOAT_EQ(wideCall->data[11],   0.25f);
            EXPECT_FLOAT_EQ(exactCall->data[11],  0.0f);
            EXPECT_FLOAT_EQ(biggerCall->data[11], 0.25f);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(iconsystem, set_icon_name_updates_uvs)
        {
            MockLogger logger;
            EntitySystem ecs;
            MasterRenderer renderer;

            ecs.createSystem<PositionComponentSystem>();
            auto sys = ecs.createSystem<IconSystem>(&renderer);

            std::vector<IconEntry> entries = {
                {"time",  24, {24, 24}, {0.0f, 0.0f}, {0.25f, 0.25f}},
                {"cross", 24, {24, 24}, {0.5f, 0.5f}, {0.75f, 0.75f}}};
            sys->registerEntriesForTest("t", entries, 1024, 1024);

            auto icon = makeIcon(&ecs, "t", "time", 24);
            ecs.executeOnce();

            const RenderCall* call = sys->getRenderCall(icon.id);
            ASSERT_NE(call, nullptr);
            EXPECT_FLOAT_EQ(call->data[11], 0.0f);
            EXPECT_FLOAT_EQ(call->data[13], 0.25f);

            icon.get<IconComponent>()->setIconName("cross");
            ecs.executeOnce();

            call = sys->getRenderCall(icon.id);
            ASSERT_NE(call, nullptr);
            EXPECT_FLOAT_EQ(call->data[11], 0.5f);
            EXPECT_FLOAT_EQ(call->data[13], 0.75f);
        }
    }
}
