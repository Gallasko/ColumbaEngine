#include "stdafx.h"

#include <gtest/gtest.h>

#include "UI/tooltip.h"

#include "ECS/entitysystem.h"
#include "Systems/coresystems.h"

#include "mocklogger.h"

namespace pg
{
    namespace test
    {
        namespace
        {
            // Creates the systems and a "fake" style whose tooltip is a 100x40 box (no font needed).
            TooltipSystem* setupTooltip(EntitySystem& ecs, _unique_id* lastRoot)
            {
                ecs.createSystem<PositionComponentSystem>();
                ecs.createSystem<MouseHoverSystem>();
                auto* tip = ecs.createSystem<TooltipSystem>();
                ecs.succeed<MouseHoverSystem, TooltipSystem>();

                tip->registerStyle("fake", [lastRoot](EntitySystem& e, const TooltipComponent&) -> EntityRef {
                    auto ent = e.createEntity();
                    auto pos = e.attach<PositionComponent>(ent);
                    pos->setWidth(100.0f);
                    pos->setHeight(40.0f);
                    *lastRoot = ent.id;
                    return ent;
                });

                return tip;
            }

            _unique_id makeTooltipTarget(EntitySystem& ecs, float x, float y, float w, float h)
            {
                auto ent = ecs.createEntity();
                auto pos = ecs.attach<PositionComponent>(ent);
                pos->setX(x);
                pos->setY(y);
                pos->setWidth(w);
                pos->setHeight(h);
                ecs.attach<TooltipComponent>(ent, "x", "fake");
                return ent.id;
            }

            void pump(EntitySystem& ecs)
            {
                for (int i = 0; i < 3; ++i)
                    ecs.executeOnce();
            }

            void move(EntitySystem& ecs, float x, float y)
            {
                ecs.sendEvent(OnMouseMove{Point2D{x, y}, nullptr});
                pump(ecs);
            }

            void tick(EntitySystem& ecs, float ms)
            {
                ecs.sendEvent(TickEvent{ms});
                pump(ecs);
            }
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(tooltip_test, shows_after_delay)
        {
            MockLogger logger;
            EntitySystem ecs;

            _unique_id lastRoot = 0;
            auto* tip = setupTooltip(ecs, &lastRoot);

            makeTooltipTarget(ecs, 0, 0, 50, 20);

            move(ecs, 5, 5);
            EXPECT_FALSE(tip->isShowing());

            tick(ecs, 100);
            EXPECT_FALSE(tip->isShowing());

            tick(ecs, 100);
            EXPECT_TRUE(tip->isShowing());
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(tooltip_test, leave_cancels_pending)
        {
            MockLogger logger;
            EntitySystem ecs;

            _unique_id lastRoot = 0;
            auto* tip = setupTooltip(ecs, &lastRoot);

            makeTooltipTarget(ecs, 0, 0, 50, 20);

            move(ecs, 5, 5);
            tick(ecs, 100);

            move(ecs, 500, 500);   // leaves the target, cancelling the pending timer
            tick(ecs, 200);

            EXPECT_FALSE(tip->isShowing());
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(tooltip_test, click_hides)
        {
            MockLogger logger;
            EntitySystem ecs;

            _unique_id lastRoot = 0;
            auto* tip = setupTooltip(ecs, &lastRoot);

            makeTooltipTarget(ecs, 0, 0, 50, 20);

            move(ecs, 5, 5);
            tick(ecs, 100);
            tick(ecs, 100);
            ASSERT_TRUE(tip->isShowing());

            ecs.sendEvent(OnMouseClick{Point2D{5, 5}, MouseButton{1}});
            pump(ecs);

            EXPECT_FALSE(tip->isShowing());
            EXPECT_EQ(ecs.getEntity(lastRoot), nullptr);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(tooltip_test, clamped_to_screen)
        {
            MockLogger logger;
            EntitySystem ecs;

            _unique_id lastRoot = 0;
            auto* tip = setupTooltip(ecs, &lastRoot);

            ecs.sendEvent(ResizeEvent{800.0f, 600.0f});
            ecs.executeOnce();

            makeTooltipTarget(ecs, 780, 580, 10, 10);

            move(ecs, 780, 580);
            tick(ecs, 100);
            tick(ecs, 100);
            ASSERT_TRUE(tip->isShowing());

            auto root = ecs.getEntity(lastRoot);
            ASSERT_NE(root, nullptr);
            auto pos = root->get<PositionComponent>();

            EXPECT_LE(pos->x + pos->width,  796.0f);
            EXPECT_LE(pos->y + pos->height, 596.0f);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(tooltip_test, switching_targets)
        {
            MockLogger logger;
            EntitySystem ecs;

            _unique_id lastRoot = 0;
            auto* tip = setupTooltip(ecs, &lastRoot);

            makeTooltipTarget(ecs, 0, 0, 50, 20);
            const _unique_id b = makeTooltipTarget(ecs, 100, 0, 50, 20);

            move(ecs, 5, 5);
            tick(ecs, 100);
            tick(ecs, 100);
            ASSERT_TRUE(tip->isShowing());

            // Move onto B: A's tooltip hides, B becomes pending.
            move(ecs, 105, 5);
            EXPECT_FALSE(tip->isShowing());

            tick(ecs, 100);
            tick(ecs, 100);
            EXPECT_TRUE(tip->isShowing());
            EXPECT_EQ(tip->shownFor, b);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // With showOnPress (a touch screen), a press hides the tooltip and brings back the one of
        // what was pressed: a tap is how a finger asks. Off the target, a press only hides
        TEST(tooltip_test, a_press_shows_the_tooltip_when_asked_to)
        {
            MockLogger logger;
            EntitySystem ecs;

            _unique_id lastRoot = 0;
            auto* tip = setupTooltip(ecs, &lastRoot);

            const _unique_id a = makeTooltipTarget(ecs, 0, 0, 50, 20);

            tip->showOnPress = true;

            move(ecs, 5, 5);
            ecs.sendEvent(OnMouseClick{Point2D{5, 5}, static_cast<MouseButton>(1)});
            pump(ecs);

            EXPECT_FALSE(tip->isShowing());

            tick(ecs, 100);
            tick(ecs, 100);

            ASSERT_TRUE(tip->isShowing());
            EXPECT_EQ(tip->shownFor, a);

            // Pressed again where it is: hidden at once, and back after the delay
            ecs.sendEvent(OnMouseClick{Point2D{5, 5}, static_cast<MouseButton>(1)});
            pump(ecs);

            EXPECT_FALSE(tip->isShowing());

            tick(ecs, 100);
            tick(ecs, 100);

            EXPECT_TRUE(tip->isShowing());

            // Pressed elsewhere: the mouse leaves it first, and nothing comes back
            move(ecs, 200, 200);
            ecs.sendEvent(OnMouseClick{Point2D{200, 200}, static_cast<MouseButton>(1)});
            pump(ecs);
            tick(ecs, 100);
            tick(ecs, 100);

            EXPECT_FALSE(tip->isShowing());
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // What a tooltip is shown for is removed under a mouse that does not move (a list built
        // again): no move tells the tooltip to go, and it must not stay over the page
        TEST(tooltip_test, hides_when_its_target_is_removed)
        {
            MockLogger logger;
            EntitySystem ecs;

            _unique_id lastRoot = 0;
            auto* tip = setupTooltip(ecs, &lastRoot);

            const _unique_id a = makeTooltipTarget(ecs, 0, 0, 50, 20);

            move(ecs, 5, 5);
            tick(ecs, 100);
            tick(ecs, 100);
            ASSERT_TRUE(tip->isShowing());

            ecs.removeEntity(a);
            pump(ecs);
            tick(ecs, 16);

            EXPECT_FALSE(tip->isShowing());
            EXPECT_EQ(ecs.getEntity(lastRoot), nullptr);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // The same for a target that is hidden where it stands (the page it is on is swapped out)
        TEST(tooltip_test, hides_when_its_target_is_hidden)
        {
            MockLogger logger;
            EntitySystem ecs;

            _unique_id lastRoot = 0;
            auto* tip = setupTooltip(ecs, &lastRoot);

            const _unique_id a = makeTooltipTarget(ecs, 0, 0, 50, 20);

            move(ecs, 5, 5);
            tick(ecs, 100);
            tick(ecs, 100);
            ASSERT_TRUE(tip->isShowing());

            ecs.getEntity(a)->get<PositionComponent>()->setVisibility(false);
            pump(ecs);
            tick(ecs, 16);

            EXPECT_FALSE(tip->isShowing());
        }
    }
}
