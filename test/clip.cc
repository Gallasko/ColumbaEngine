#include "stdafx.h"

#include <gtest/gtest.h>

#include "2D/position.h"

#include "ECS/entitysystem.h"
#include "Renderer/rendercall.h"

#include "mocklogger.h"

namespace pg
{
    namespace test
    {
        namespace
        {
            EntityRef makeRect(EntitySystem& ecs, float x, float y, float w, float h)
            {
                auto entity = ecs.createEntity();
                auto pos = ecs.attach<PositionComponent>(entity);
                pos->setX(x);
                pos->setY(y);
                pos->setWidth(w);
                pos->setHeight(h);
                return entity;
            }
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(clip_test, single_clip_unchanged)
        {
            MockLogger logger;
            EntitySystem ecs;
            ecs.createSystem<PositionComponentSystem>();

            auto parent = makeRect(ecs, 0, 0, 100, 100);
            auto child = makeRect(ecs, 50, 50, 100, 100);
            ecs.attach<ClippedTo>(child, parent.id);

            ecs.executeOnce();

            EXPECT_TRUE(inClipBound(child, 60, 60));
            EXPECT_FALSE(inClipBound(child, 140, 140));
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(clip_test, nested_intersection)
        {
            MockLogger logger;
            EntitySystem ecs;
            ecs.createSystem<PositionComponentSystem>();

            auto outer = makeRect(ecs, 0, 0, 100, 100);
            auto inner = makeRect(ecs, 50, 0, 100, 100);
            ecs.attach<ClippedTo>(inner, outer.id);
            auto child = makeRect(ecs, 0, 0, 200, 200);
            ecs.attach<ClippedTo>(child, inner.id);

            ecs.executeOnce();

            bool found = false;
            const auto rect = effectiveClipRect(&ecs, inner.id, found);
            ASSERT_TRUE(found);
            EXPECT_FLOAT_EQ(rect.x, 50.0f);
            EXPECT_FLOAT_EQ(rect.y, 0.0f);
            EXPECT_FLOAT_EQ(rect.z, 50.0f);
            EXPECT_FLOAT_EQ(rect.w, 100.0f);

            EXPECT_TRUE(inClipBound(child, 75, 50));
            EXPECT_FALSE(inClipBound(child, 25, 50));
            EXPECT_FALSE(inClipBound(child, 125, 50));
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(clip_test, empty_intersection)
        {
            MockLogger logger;
            EntitySystem ecs;
            ecs.createSystem<PositionComponentSystem>();

            auto outer = makeRect(ecs, 0, 0, 100, 100);
            auto inner = makeRect(ecs, 200, 0, 50, 50);   // entirely right of outer
            ecs.attach<ClippedTo>(inner, outer.id);
            auto child = makeRect(ecs, 0, 0, 300, 300);
            ecs.attach<ClippedTo>(child, inner.id);

            ecs.executeOnce();

            bool found = false;
            const auto rect = effectiveClipRect(&ecs, inner.id, found);
            ASSERT_TRUE(found);
            EXPECT_FLOAT_EQ(rect.z, 0.0f);

            EXPECT_FALSE(inClipBound(child, 210, 10));
            EXPECT_FALSE(inClipBound(child, 50, 50));
            EXPECT_FALSE(inClipBound(child, 0, 0));
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(clip_test, cycle_is_bounded)
        {
            MockLogger logger;
            EntitySystem ecs;
            ecs.createSystem<PositionComponentSystem>();

            auto a = makeRect(ecs, 0, 0, 100, 100);
            auto b = makeRect(ecs, 50, 0, 100, 100);
            ecs.attach<ClippedTo>(a, b.id);
            ecs.attach<ClippedTo>(b, a.id);

            ecs.executeOnce();

            bool found = false;
            const auto rect = effectiveClipRect(&ecs, a.id, found);   // must return, not hang

            ASSERT_TRUE(found);
            EXPECT_FLOAT_EQ(rect.x, 50.0f);
            EXPECT_FLOAT_EQ(rect.y, 0.0f);
            EXPECT_FLOAT_EQ(rect.z, 50.0f);
            EXPECT_FLOAT_EQ(rect.w, 100.0f);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(clip_test, render_call_scissor)
        {
            MockLogger logger;
            EntitySystem ecs;
            ecs.createSystem<PositionComponentSystem>();

            auto outer = makeRect(ecs, 0, 0, 100, 100);
            auto inner = makeRect(ecs, 50, 0, 100, 100);
            ecs.attach<ClippedTo>(inner, outer.id);
            auto child = makeRect(ecs, 0, 0, 200, 200);
            ecs.attach<ClippedTo>(child, inner.id);

            ecs.executeOnce();

            RenderCall call;
            call.processPositionComponent(child->get<PositionComponent>());

            EXPECT_FLOAT_EQ(call.state.scissorBound.x, 50.0f);
            EXPECT_FLOAT_EQ(call.state.scissorBound.y, 0.0f);
            EXPECT_FLOAT_EQ(call.state.scissorBound.z, 50.0f);
            EXPECT_FLOAT_EQ(call.state.scissorBound.w, 100.0f);
        }
    }
}
