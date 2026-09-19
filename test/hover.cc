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
    }
}
