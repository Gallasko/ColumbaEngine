#include "stdafx.h"

#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "UI/lifeclock.h"
#include "Core/motion.h"

#include "ECS/entitysystem.h"
#include "UI/themesystem.h"
#include "ECS/entitysystem_fwd.h"   // ResizeEvent
#include "UI/ttftext.h"
#include "UI/iconsystem.h"
#include "UI/sizer.h"
#include "UI/prefab.h"
#include "Systems/gamefacts.h"
#include "Systems/tween.h"
#include "Systems/coresystems.h"
#include "2D/simple2dobject.h"
#include "2D/decoratedshapes.h"

#include "mocklogger.h"
#include "factfeed.h"

using namespace chronicle;

namespace pg
{
    namespace test
    {
        namespace
        {
            struct LifeClockFixture
            {
                EntitySystem ecs;
                MasterRenderer renderer;
                TTFTextSystem* ttf = nullptr;
                IconSystem* icons = nullptr;
                ThemeSystem* theme = nullptr;
                WorldFacts* facts = nullptr;
                FactFeed* feed = nullptr;

                LifeClockFixture()
                {
                    Motion::setReduced(true);   // The animation test flips this back
                    ecs.createSystem<PositionComponentSystem>();
                    ecs.createSystem<LayoutSystem>();
                    ecs.succeed<PositionComponentSystem, LayoutSystem>();
                    ttf = ecs.createSystem<TTFTextSystem>(&renderer);
                    ecs.createSystem<Simple2DObjectSystem>(&renderer);
                    ecs.createSystem<HatchRect2DObjectSystem>(&renderer);
                    ecs.createSystem<DottedLine2DObjectSystem>(&renderer);
                    ecs.createSystem<StrokeRect2DObjectSystem>(&renderer);
                    icons = ecs.createSystem<IconSystem>(&renderer);
                    ecs.createSystem<TweenSystem>();
                    facts = createTestFacts(&ecs);
                    feed = ecs.createSystem<FactFeed>();
                    theme = ecs.createSystem<ThemeSystem>();
                    theme->loadTheme("chronicle/tokens.json", "fonts");
                    installIconEntries();
                    ecs.sendEvent(ResizeEvent{1320.0f, 860.0f});
                }

                void installIconEntries()
                {
                    std::vector<IconEntry> marks;
                    const int sizes[5] = {14, 16, 18, 24, 48};

                    for (const auto& name : markNames())
                    {
                        for (int s : sizes)
                            marks.push_back({name, s, {s, s}, {0.0f, 0.0f}, {0.1f, 0.1f}});
                    }

                    icons->registerEntriesForTest("chronicle", marks, 1024, 1024);
                    renderer.registerTexture("IconAtlas_chronicle", OpenGLTexture{});
                }

                void settle() { ecs.executeOnce(); ecs.executeOnce(); ecs.executeOnce(); }

                void tick(float ms)
                {
                    ecs.sendEvent(TickEvent{ms});
                    settle();
                }

                LifeClock make(const LifeClockSpec& spec, float x = 100.0f, float y = 100.0f)
                {
                    LifeClock clock = makeLifeClock(&ecs, spec);
                    clock.root->get<PositionComponent>()->setX(x);
                    clock.root->get<PositionComponent>()->setY(y);
                    settle();

                    return clock;
                }

                float asc(const std::string& style)
                {
                    const TextStyle& s = theme->style(style);

                    return ttf->measureText(s.fontAlias, "H", 1.0f, 0.0f, 0.0f, s.letterSpacingPx).ascender;
                }

                constant::Vector4D color(const std::string& token, const std::string& id) { return theme->theme().color(token, id); }

                CompRef<PositionComponent> pos(EntityRef e) { return ecs.getEntity(e.id)->get<PositionComponent>(); }

                float left(EntityRef e, const LifeClock& clock) { return pos(e)->x - pos(clock.root)->x; }

                float right(EntityRef e, const LifeClock& clock) { return left(e, clock) + pos(e)->width; }

                std::string element(EntityRef e) { return ecs.getEntity(e.id)->get<ThemeComponent>()->element; }

                bool visible(EntityRef e) { return pos(e)->isVisible(); }
            };

            LifeClockSpec lifeSpec(float age, float runningMonths = 0.0f)
            {
                LifeClockSpec spec;
                spec.width = 640.0f;
                spec.startAge = 7.0f;
                spec.endAge = 43.0f;
                spec.age = age;
                spec.runningMonths = runningMonths;

                return spec;
            }

            std::vector<ClockMilestone> planMilestones()
            {
                return {{7.0f, ""}, {14.0f, ""}, {18.0f, ""}, {25.0f, ""}, {40.0f, ""}, {43.0f, ""}};
            }
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(lifeclock_test, geometry_and_elements)
        {
            MockLogger logger;
            LifeClockFixture s;

            LifeClock clock = s.make(lifeSpec(17.4f));

            EXPECT_FLOAT_EQ(s.pos(clock.root)->width, 640.0f);
            EXPECT_FLOAT_EQ(s.pos(clock.root)->height, 76.0f);

            EXPECT_NEAR(s.pos(clock.track)->y - s.pos(clock.root)->y, 42.0f, 0.01f);
            EXPECT_FLOAT_EQ(s.pos(clock.track)->width, 640.0f);
            EXPECT_FLOAT_EQ(s.pos(clock.track)->height, 14.0f);
            EXPECT_EQ(s.element(clock.track), "clock.track");
            EXPECT_EQ(s.element(clock.frame), "clock.frame");

            EXPECT_NEAR(s.pos(clock.lived)->width, clock.xFor(17.4f) - 1.0f, 0.01f);
            EXPECT_NEAR(s.pos(clock.lived)->width, 638.0f * 10.4f / 36.0f, 0.01f);
            EXPECT_EQ(s.element(clock.lived), "clock.lived");

            EXPECT_EQ(clock.age.spec.text, "17");
            EXPECT_EQ(s.element(clock.age.entity), "clock.age");
            EXPECT_EQ(clock.unit.spec.text, "YEARS");
            EXPECT_EQ(s.element(clock.unit.entity), "clock.unit");

            const float ageBaseline = s.pos(clock.age.entity)->y + s.asc("figure-xl");
            const float unitBaseline = s.pos(clock.unit.entity)->y + s.asc("label");
            EXPECT_NEAR(ageBaseline, unitBaseline, 0.5f);
            EXPECT_NEAR(s.left(clock.unit.entity, clock), s.right(clock.age.entity, clock) + 8.0f, 0.5f);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(lifeclock_test, x_for_clamps)
        {
            MockLogger logger;
            LifeClockFixture s;

            LifeClock clock = s.make(lifeSpec(17.4f));

            EXPECT_FLOAT_EQ(clock.xFor(5.0f), 1.0f);
            EXPECT_FLOAT_EQ(clock.xFor(50.0f), 639.0f);
            EXPECT_FLOAT_EQ(clock.xFor(25.0f), 1.0f + 638.0f * 0.5f);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(lifeclock_test, running_segment)
        {
            MockLogger logger;
            LifeClockFixture s;

            LifeClock clock = s.make(lifeSpec(17.4f, 9.0f));

            EXPECT_TRUE(s.visible(clock.running));
            EXPECT_NEAR(s.left(clock.running, clock), s.right(clock.lived, clock), 0.01f);
            EXPECT_NEAR(s.pos(clock.running)->width, 638.0f * 0.75f / 36.0f, 0.01f);
            EXPECT_EQ(s.element(clock.running), "clock.running");

            EXPECT_TRUE(s.visible(clock.runningEdge));
            EXPECT_NEAR(s.left(clock.runningEdge, clock), s.left(clock.running, clock), 0.01f);
            EXPECT_FLOAT_EQ(s.pos(clock.runningEdge)->width, 1.0f);
            EXPECT_FLOAT_EQ(s.pos(clock.runningEdge)->height, 12.0f);
            EXPECT_EQ(s.element(clock.runningEdge), "clock.running.edge");

            clock.setRunning(&s.ecs, 0.0f);
            s.settle();

            EXPECT_FALSE(s.visible(clock.running));
            EXPECT_FALSE(s.visible(clock.runningEdge));

            LifeClock old = s.make(lifeSpec(42.5f, 12.0f), 100.0f, 300.0f);

            EXPECT_NEAR(s.pos(old.running)->width, 638.0f * 0.5f / 36.0f, 0.01f);
            EXPECT_NEAR(s.right(old.running, old), old.xFor(43.0f), 0.01f);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(lifeclock_test, window_band)
        {
            MockLogger logger;
            LifeClockFixture s;

            LifeClockSpec spec = lifeSpec(17.4f);
            spec.windows = {{16.0f, 22.0f, "Squire"}};
            LifeClock clock = s.make(spec);

            ASSERT_EQ(clock.windows.size(), 1u);
            const auto& band = clock.windows[0];

            EXPECT_NEAR(s.left(band.wash, clock), clock.xFor(16.0f), 0.01f);
            EXPECT_NEAR(s.pos(band.wash)->width, 638.0f * 6.0f / 36.0f, 0.01f);
            EXPECT_EQ(s.element(band.wash), "clock.window.wash");

            EXPECT_EQ(s.element(band.left), "clock.window.edge");
            EXPECT_EQ(s.element(band.right), "clock.window.edge");
            EXPECT_FLOAT_EQ(s.pos(band.left)->width, 2.0f);
            EXPECT_FLOAT_EQ(s.pos(band.left)->height, 12.0f);
            EXPECT_FLOAT_EQ(s.pos(band.right)->width, 2.0f);
            EXPECT_FLOAT_EQ(s.pos(band.right)->height, 12.0f);
            EXPECT_NEAR(s.left(band.left, clock), s.left(band.wash, clock), 0.01f);
            EXPECT_NEAR(s.right(band.right, clock), s.right(band.wash, clock), 0.01f);

            clock.setWindowClosed(&s.ecs, 0, true);
            s.settle();

            EXPECT_EQ(s.element(band.left), "clock.window.edge.closed");
            EXPECT_EQ(s.element(band.right), "clock.window.edge.closed");
            EXPECT_EQ(s.element(band.wash), "clock.window.wash");
            EXPECT_TRUE(clock.spec.windows[0].closed);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(lifeclock_test, ticks_land_on_ages)
        {
            MockLogger logger;
            LifeClockFixture s;

            LifeClockSpec spec = lifeSpec(17.4f);
            spec.milestones = planMilestones();
            LifeClock clock = s.make(spec);

            ASSERT_EQ(clock.ticks.size(), 6u);

            const std::vector<std::string> texts = {"7", "14", "18", "25", "40", "43"};

            for (size_t i = 0; i < clock.ticks.size(); ++i)
            {
                const auto& tick = clock.ticks[i];

                EXPECT_NEAR(s.left(tick.mark, clock), clock.xFor(tick.age) - 0.5f, 0.01f);
                EXPECT_FLOAT_EQ(s.pos(tick.mark)->width, 1.0f);
                EXPECT_FLOAT_EQ(s.pos(tick.mark)->height, 5.0f);
                EXPECT_NEAR(s.pos(tick.mark)->y - s.pos(clock.root)->y, 57.0f, 0.01f);
                EXPECT_EQ(s.element(tick.mark), "clock.tick.mark");

                EXPECT_EQ(tick.label.spec.text, texts[i]);
                EXPECT_NEAR(s.pos(tick.label.entity)->y - s.pos(clock.root)->y, 62.0f, 0.01f);

                // The interior labels are centred on their tick; the ends are nudged inward.
                if (i > 0 and i + 1 < clock.ticks.size())
                {
                    const float centre = s.left(tick.label.entity, clock) + s.pos(tick.label.entity)->width / 2.0f;
                    EXPECT_NEAR(centre, clock.xFor(tick.age), 0.5f);
                }
            }

            EXPECT_GE(s.left(clock.ticks.front().label.entity, clock), 0.0f);
            EXPECT_LE(s.right(clock.ticks.back().label.entity, clock), 640.0f);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(lifeclock_test, ticks_past_and_future)
        {
            MockLogger logger;
            LifeClockFixture s;

            LifeClockSpec spec = lifeSpec(17.4f);
            spec.milestones = planMilestones();
            LifeClock clock = s.make(spec);

            ASSERT_EQ(clock.ticks.size(), 6u);

            EXPECT_EQ(s.element(clock.ticks[0].label.entity), "clock.tick.label.past");
            EXPECT_EQ(s.element(clock.ticks[1].label.entity), "clock.tick.label.past");
            EXPECT_EQ(s.element(clock.ticks[2].label.entity), "clock.tick.label");
            EXPECT_EQ(s.element(clock.ticks[3].label.entity), "clock.tick.label");
            EXPECT_EQ(s.element(clock.ticks[4].label.entity), "clock.tick.label");
            EXPECT_EQ(s.element(clock.ticks[5].label.entity), "clock.tick.label");

            clock.setAge(&s.ecs, 26.0f);
            s.settle();

            EXPECT_EQ(s.element(clock.ticks[2].label.entity), "clock.tick.label.past");
            EXPECT_EQ(s.element(clock.ticks[3].label.entity), "clock.tick.label.past");
            EXPECT_EQ(s.element(clock.ticks[4].label.entity), "clock.tick.label");
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(lifeclock_test, next_line)
        {
            MockLogger logger;
            LifeClockFixture s;

            LifeClockSpec spec = lifeSpec(17.4f);
            spec.nextLabel = "Choose a path";
            spec.nextIn = 14;
            LifeClock clock = s.make(spec);

            EXPECT_EQ(clock.nextMark.spec.name, "time");
            EXPECT_EQ(clock.nextMark.spec.size, MarkSize::S14);
            EXPECT_EQ(s.element(clock.nextMark.entity), "clock.next.mark");
            EXPECT_EQ(clock.next.spec.text, "Choose a path");
            EXPECT_EQ(s.element(clock.next.entity), "clock.next");
            EXPECT_EQ(clock.nextIn.spec.text, "14 mo");
            EXPECT_EQ(s.element(clock.nextIn.entity), "clock.next.in");

            EXPECT_NEAR(s.right(clock.nextIn.entity, clock), 640.0f, 0.5f);
            EXPECT_NEAR(s.right(clock.next.entity, clock), s.left(clock.nextIn.entity, clock) - 8.0f, 0.5f);
            EXPECT_NEAR(s.right(clock.nextMark.entity, clock), s.left(clock.next.entity, clock) - 8.0f, 0.5f);

            EXPECT_TRUE(s.visible(clock.nextMark.entity));
            EXPECT_TRUE(s.visible(clock.next.entity));
            EXPECT_TRUE(s.visible(clock.nextIn.entity));

            clock.setNext(&s.ecs, "", -1);
            s.settle();

            EXPECT_FALSE(s.visible(clock.nextMark.entity));
            EXPECT_FALSE(s.visible(clock.next.entity));
            EXPECT_FALSE(s.visible(clock.nextIn.entity));
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(lifeclock_test, age_animates_and_running_follows)
        {
            MockLogger logger;
            LifeClockFixture s;
            Motion::setReduced(false);

            LifeClock clock = s.make(lifeSpec(17.0f, 12.0f));
            clock.setAge(&s.ecs, 18.0f);

            ASSERT_TRUE(s.ecs.getEntity(clock.lived.id)->has<TweenComponent>());
            EXPECT_EQ(clock.age.spec.text, "18");   // The figure is the fact, at once

            // 600 ms per 36 years: one year takes 16.67 ms.
            const float duration = 600.0f / 36.0f;
            EXPECT_NEAR(s.ecs.getEntity(clock.lived.id)->get<TweenComponent>()->duration, duration, 0.01f);

            // TweenSystem accumulates whole milliseconds, so tick a whole number and read the
            // progress the tween really made: 8 of 16.67 ms is 0.48 of a year.
            s.tick(8.0f);
            const float shown = 17.0f + 8.0f / duration;

            EXPECT_NEAR(clock.shownAge, shown, 0.001f);
            EXPECT_NEAR(s.right(clock.lived, clock), clock.xFor(shown), 0.01f);
            EXPECT_NEAR(s.left(clock.running, clock), s.right(clock.lived, clock), 0.01f);
            EXPECT_NEAR(s.right(clock.running, clock), clock.xFor(shown + 1.0f), 0.01f);

            s.tick(20.0f);

            EXPECT_FLOAT_EQ(clock.shownAge, 18.0f);
            EXPECT_NEAR(s.right(clock.lived, clock), clock.xFor(18.0f), 0.01f);

            Motion::setReduced(true);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(lifeclock_test, promise_kept)
        {
            MockLogger logger;
            LifeClockFixture s;

            LifeClock clock = s.make(lifeSpec(17.0f, 6.0f));
            const float promised = s.right(clock.running, clock);

            // What the scene does when the activity completes.
            clock.setRunning(&s.ecs, 0.0f);
            clock.setAge(&s.ecs, 17.5f, false);
            s.settle();

            EXPECT_NEAR(s.right(clock.lived, clock), promised, 0.01f);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(lifeclock_test, outside_span_dropped)
        {
            MockLogger logger;
            LifeClockFixture s;

            LifeClockSpec spec = lifeSpec(17.4f);
            spec.milestones = {{50.0f, ""}};
            spec.windows = {{45.0f, 48.0f, "Too late"}};

            // A first clock loads the fonts, so the count below is only the dropped entries.
            s.make(lifeSpec(17.4f), 100.0f, 300.0f);

            const unsigned int before = logger.getNbError();
            LifeClock clock = s.make(spec);

            EXPECT_TRUE(clock.ticks.empty());
            EXPECT_TRUE(clock.windows.empty());
            EXPECT_TRUE(clock.spec.milestones.empty());
            EXPECT_TRUE(clock.spec.windows.empty());
            EXPECT_EQ(logger.getNbError() - before, 2u);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(lifeclock_test, fed_from_worldfacts)
        {
            MockLogger logger;
            LifeClockFixture s;

            LifeClockSpec spec = lifeSpec(17.0f);
            spec.nextLabel = "Choose a path";
            spec.nextIn = 7;
            LifeClock clock = s.make(spec);

            s.feed->onFact = [&](const std::string& name, const ElementType& v) {
                if (name == "life.age")
                    clock.setAge(&s.ecs, v.get<float>());
                else if (name == "activity.running.months")
                    clock.setRunning(&s.ecs, v.get<float>());
                else if (name == "life.next.label")
                    clock.setNext(&s.ecs, v.get<std::string>(), clock.spec.nextIn);
                else if (name == "life.next.in")
                    clock.setNext(&s.ecs, clock.spec.nextLabel, v.get<int>());
            };

            s.facts->setFact("life.age", 18.25f);
            s.facts->setFact("activity.running.months", 3.0f);
            s.facts->setFact("life.next.label", std::string("Squire"));
            s.facts->setFact("life.next.in", 9);

            // The update leaves on the first pass and is delivered on the next one
            s.settle();
            s.settle();

            EXPECT_FLOAT_EQ(clock.shownAge, 18.25f);
            EXPECT_EQ(clock.age.spec.text, "18");
            EXPECT_NEAR(s.right(clock.lived, clock), clock.xFor(18.25f), 0.01f);
            EXPECT_NEAR(s.right(clock.running, clock), clock.xFor(18.5f), 0.01f);
            EXPECT_EQ(clock.next.spec.text, "Squire");
            EXPECT_EQ(clock.nextIn.spec.text, "9 mo");
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(lifeclock_test, theme_switch)
        {
            MockLogger logger;
            LifeClockFixture s;

            LifeClockSpec spec = lifeSpec(17.4f);
            spec.windows = {{16.0f, 22.0f, "Squire"}};
            LifeClock clock = s.make(spec);

            s.theme->setTheme("candle");
            s.settle();

            const auto lived = s.ecs.getEntity(clock.lived.id)->get<Simple2DObject>()->colors;
            EXPECT_FLOAT_EQ(lived.x, s.color("progress-ink", "candle").x);
            EXPECT_FLOAT_EQ(lived.y, s.color("progress-ink", "candle").y);
            EXPECT_FLOAT_EQ(lived.z, s.color("progress-ink", "candle").z);

            const auto edge = s.ecs.getEntity(clock.windows[0].left.id)->get<Simple2DObject>()->colors;
            EXPECT_FLOAT_EQ(edge.x, s.color("ochre", "candle").x);
            EXPECT_FLOAT_EQ(edge.y, s.color("ochre", "candle").y);
            EXPECT_FLOAT_EQ(edge.z, s.color("ochre", "candle").z);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(lifeclock_test, z_bands)
        {
            MockLogger logger;
            LifeClockFixture s;

            LifeClockSpec spec = lifeSpec(17.4f, 9.0f);
            spec.windows = {{16.0f, 22.0f, "Squire"}};
            spec.milestones = planMilestones();
            spec.nextLabel = "Choose a path";
            spec.nextIn = 7;
            spec.z = 20;
            LifeClock clock = s.make(spec);

            EXPECT_FLOAT_EQ(s.pos(clock.root)->z, 20.0f);

            EXPECT_FLOAT_EQ(s.pos(clock.age.entity)->z, 22.0f);
            EXPECT_FLOAT_EQ(s.pos(clock.unit.entity)->z, 22.0f);
            EXPECT_FLOAT_EQ(s.pos(clock.nextMark.entity)->z, 21.0f);
            EXPECT_FLOAT_EQ(s.pos(clock.next.entity)->z, 22.0f);
            EXPECT_FLOAT_EQ(s.pos(clock.nextIn.entity)->z, 22.0f);

            EXPECT_FLOAT_EQ(s.pos(clock.track)->z, 21.0f);
            EXPECT_FLOAT_EQ(s.pos(clock.lived)->z, 22.0f);
            EXPECT_FLOAT_EQ(s.pos(clock.windows[0].wash)->z, 23.0f);
            EXPECT_FLOAT_EQ(s.pos(clock.running)->z, 24.0f);
            EXPECT_FLOAT_EQ(s.pos(clock.runningEdge)->z, 25.0f);
            EXPECT_FLOAT_EQ(s.pos(clock.windows[0].left)->z, 25.0f);
            EXPECT_FLOAT_EQ(s.pos(clock.windows[0].right)->z, 25.0f);
            EXPECT_FLOAT_EQ(s.pos(clock.frame)->z, 26.0f);

            EXPECT_FLOAT_EQ(s.pos(clock.ticks[0].mark)->z, 21.0f);
            EXPECT_FLOAT_EQ(s.pos(clock.ticks[0].label.entity)->z, 22.0f);
        }
    }
}
