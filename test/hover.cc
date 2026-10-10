#include "stdafx.h"

#include <gtest/gtest.h>

#include "Input/inputcomponent.h"

#include "ECS/entitysystem.h"
#include "ECS/callable.h"

#include "mocklogger.h"

namespace pg
{
    namespace test
    {
        namespace
        {
            struct EnterFired { _unique_id id = 0; };
            struct LeaveFired { _unique_id id = 0; };

            // Records which entities' enter/leave callbacks fired and every HoverChangedEvent.
            struct HoverRecorder : public System<Listener<EnterFired>, Listener<LeaveFired>, Listener<HoverChangedEvent>, StoragePolicy>
            {
                virtual std::string getSystemName() const override { return "Hover Recorder"; }

                void onEvent(const EnterFired& e) override { entered.push_back(e.id); }
                void onEvent(const LeaveFired& e) override { left.push_back(e.id); }
                void onEvent(const HoverChangedEvent& e) override { hoverEvents.push_back(e); }

                void clear() { entered.clear(); left.clear(); hoverEvents.clear(); }

                std::vector<_unique_id> entered;
                std::vector<_unique_id> left;
                std::vector<HoverChangedEvent> hoverEvents;
            };

            bool contains(const std::vector<_unique_id>& v, _unique_id id)
            {
                return std::find(v.begin(), v.end(), id) != v.end();
            }

            _unique_id makeHoverEntity(EntitySystem& ecs, float x, float y, float z, float w, float h, bool passThrough = false)
            {
                auto entity = ecs.createEntity();

                auto pos = ecs.attach<PositionComponent>(entity);
                pos->setX(x);
                pos->setY(y);
                pos->setZ(z);
                pos->setWidth(w);
                pos->setHeight(h);

                if (passThrough)
                {
                    ecs.attach<MouseEnterComponent>(entity, makeCallable<EnterFired>(EnterFired{entity.id}), true);
                    ecs.attach<MouseLeaveComponent>(entity, makeCallable<LeaveFired>(LeaveFired{entity.id}), true);
                }
                else
                {
                    ecs.attach<MouseEnterComponent>(entity, makeCallable<EnterFired>(EnterFired{entity.id}));
                    ecs.attach<MouseLeaveComponent>(entity, makeCallable<LeaveFired>(LeaveFired{entity.id}));
                }

                return entity.id;
            }

            // Drains the event cascade: move -> hover diff -> enter/leave callbacks -> recorder.
            void pump(EntitySystem& ecs)
            {
                for (int i = 0; i < 3; ++i)
                    ecs.executeOnce();
            }
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(hover_test, top_entity_only)
        {
            MockLogger logger;
            EntitySystem ecs;

            ecs.createSystem<PositionComponentSystem>();
            ecs.createSystem<MouseHoverSystem>();
            auto* rec = ecs.createSystem<HoverRecorder>();

            makeHoverEntity(ecs, 0, 0, 10, 10, 10);
            const _unique_id top = makeHoverEntity(ecs, 0, 0, 20, 10, 10);

            ecs.sendEvent(OnMouseMove{Point2D{5, 5}, nullptr});
            pump(ecs);

            EXPECT_EQ(rec->entered.size(), 1u);
            EXPECT_TRUE(contains(rec->entered, top));
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(hover_test, leave_when_covered)
        {
            MockLogger logger;
            EntitySystem ecs;

            ecs.createSystem<PositionComponentSystem>();
            ecs.createSystem<MouseHoverSystem>();
            auto* rec = ecs.createSystem<HoverRecorder>();

            const _unique_id a = makeHoverEntity(ecs, 0, 0, 10, 10, 10);

            ecs.sendEvent(OnMouseMove{Point2D{5, 5}, nullptr});
            pump(ecs);
            EXPECT_TRUE(contains(rec->entered, a));

            rec->clear();

            const _unique_id b = makeHoverEntity(ecs, 0, 0, 20, 10, 10);

            ecs.sendEvent(OnMouseMove{Point2D{5, 5}, nullptr});
            pump(ecs);

            EXPECT_TRUE(contains(rec->left, a));
            EXPECT_TRUE(contains(rec->entered, b));
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(hover_test, viewport_beats_z)
        {
            MockLogger logger;
            EntitySystem ecs;

            ecs.createSystem<PositionComponentSystem>();
            ecs.createSystem<MouseHoverSystem>();
            auto* rec = ecs.createSystem<HoverRecorder>();
            ecs.registerFlagComponent<ViewportComponent>();

            // A is higher z but on a lower viewport; B wins on viewport.
            auto entityA = ecs.createEntity();
            auto posA = ecs.attach<PositionComponent>(entityA);
            posA->setX(0); posA->setY(0); posA->setZ(50); posA->setWidth(10); posA->setHeight(10);
            ecs.attach<ViewportComponent>(entityA)->viewport = 1;
            ecs.attach<MouseEnterComponent>(entityA, makeCallable<EnterFired>(EnterFired{entityA.id}));
            ecs.attach<MouseLeaveComponent>(entityA, makeCallable<LeaveFired>(LeaveFired{entityA.id}));

            auto entityB = ecs.createEntity();
            auto posB = ecs.attach<PositionComponent>(entityB);
            posB->setX(0); posB->setY(0); posB->setZ(5); posB->setWidth(10); posB->setHeight(10);
            ecs.attach<ViewportComponent>(entityB)->viewport = 2;
            ecs.attach<MouseEnterComponent>(entityB, makeCallable<EnterFired>(EnterFired{entityB.id}));
            ecs.attach<MouseLeaveComponent>(entityB, makeCallable<LeaveFired>(LeaveFired{entityB.id}));

            ecs.sendEvent(OnMouseMove{Point2D{5, 5}, nullptr});
            pump(ecs);

            EXPECT_EQ(rec->entered.size(), 1u);
            EXPECT_TRUE(contains(rec->entered, entityB.id));
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(hover_test, pass_through_does_not_occlude)
        {
            MockLogger logger;
            EntitySystem ecs;

            ecs.createSystem<PositionComponentSystem>();
            ecs.createSystem<MouseHoverSystem>();
            auto* rec = ecs.createSystem<HoverRecorder>();

            const _unique_id a = makeHoverEntity(ecs, 0, 0, 10, 10, 10);
            const _unique_id b = makeHoverEntity(ecs, 0, 0, 20, 10, 10, /*passThrough*/ true);

            ecs.sendEvent(OnMouseMove{Point2D{5, 5}, nullptr});
            pump(ecs);

            EXPECT_TRUE(contains(rec->entered, a));
            EXPECT_TRUE(contains(rec->entered, b));
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(hover_test, hover_changed_event)
        {
            MockLogger logger;
            EntitySystem ecs;

            ecs.createSystem<PositionComponentSystem>();
            ecs.createSystem<MouseHoverSystem>();
            auto* rec = ecs.createSystem<HoverRecorder>();

            const _unique_id b = makeHoverEntity(ecs, 0, 0, 10, 10, 10);

            ecs.sendEvent(OnMouseMove{Point2D{5, 5}, nullptr});
            pump(ecs);

            ASSERT_FALSE(rec->hoverEvents.empty());
            const HoverChangedEvent& first = rec->hoverEvents.front();
            ASSERT_EQ(first.entered.size(), 1u);
            EXPECT_EQ(first.entered.front(), b);
            EXPECT_TRUE(first.left.empty());
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // An entity removed while the mouse is on it has left: nothing else would say so, and
        // what follows the hover would wait for a leave that never comes
        TEST(hover_test, a_removed_entity_is_announced_as_left)
        {
            MockLogger logger;
            EntitySystem ecs;

            ecs.createSystem<PositionComponentSystem>();
            ecs.createSystem<MouseHoverSystem>();
            auto* rec = ecs.createSystem<HoverRecorder>();

            const _unique_id hovered = makeHoverEntity(ecs, 0, 0, 10, 10, 10);
            const _unique_id aside = makeHoverEntity(ecs, 50, 50, 10, 10, 10);

            ecs.sendEvent(OnMouseMove{Point2D{5, 5}, nullptr});
            pump(ecs);

            ASSERT_TRUE(contains(rec->entered, hovered));

            rec->clear();

            // One the mouse is not on: gone without a word
            ecs.removeEntity(aside);
            pump(ecs);

            EXPECT_TRUE(rec->hoverEvents.empty());

            ecs.removeEntity(hovered);
            pump(ecs);

            ASSERT_EQ(rec->hoverEvents.size(), 1u);
            EXPECT_TRUE(rec->hoverEvents.front().entered.empty());
            ASSERT_EQ(rec->hoverEvents.front().left.size(), 1u);
            EXPECT_EQ(rec->hoverEvents.front().left.front(), hovered);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // The hover is computed on a move: an entity made under a mouse that stays still is not
        // hovered until a refresh is asked for
        TEST(hover_test, refresh_hovers_what_was_made_under_a_still_mouse)
        {
            MockLogger logger;
            EntitySystem ecs;

            ecs.createSystem<PositionComponentSystem>();
            ecs.createSystem<MouseHoverSystem>();
            auto* rec = ecs.createSystem<HoverRecorder>();

            // Before a first move there is nowhere to look
            const _unique_id early = makeHoverEntity(ecs, 0, 0, 10, 10, 10);

            ecs.sendEvent(RefreshHoverEvent{});
            pump(ecs);

            EXPECT_TRUE(rec->entered.empty());
            EXPECT_TRUE(rec->hoverEvents.empty());

            ecs.sendEvent(OnMouseMove{Point2D{5, 5}, nullptr});
            pump(ecs);

            EXPECT_TRUE(contains(rec->entered, early));

            rec->clear();

            // Made over the first one, the mouse where it was: nothing hovers it yet
            const _unique_id late = makeHoverEntity(ecs, 0, 0, 20, 10, 10);

            pump(ecs);

            EXPECT_TRUE(rec->entered.empty());

            ecs.sendEvent(RefreshHoverEvent{});
            pump(ecs);

            EXPECT_TRUE(contains(rec->entered, late));
            EXPECT_TRUE(contains(rec->left, early));
            ASSERT_EQ(rec->hoverEvents.size(), 1u);
            EXPECT_EQ(rec->hoverEvents.front().pos.x, 5.0f);

            rec->clear();

            // Nothing changed since: a refresh says nothing
            ecs.sendEvent(RefreshHoverEvent{});
            pump(ecs);

            EXPECT_TRUE(rec->entered.empty());
            EXPECT_TRUE(rec->left.empty());
            EXPECT_TRUE(rec->hoverEvents.empty());
        }
    }
}
