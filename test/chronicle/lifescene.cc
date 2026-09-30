#include "stdafx.h"

#include <cstdio>
#include <fstream>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "Scenes/lifescene.h"
#include "UI/factories.h"
#include "UI/label.h"
#include "UI/mark.h"
#include "UI/statline.h"
#include "UI/lifeclock.h"
#include "UI/windowmeter.h"
#include "UI/resourceledger.h"
#include "UI/eventlog.h"
#include "UI/activityrow.h"
#include "UI/button.h"
#include "UI/tabs.h"
#include "UI/gloss.h"
#include "UI/focusorder.h"
#include "Core/motion.h"
#include "Core/factrouter.h"

#include "ECS/entitysystem.h"
#include "ECS/entitysystem_fwd.h"   // ResizeEvent
#include "Scene/scenemanager.h"
#include "UI/themesystem.h"
#include "UI/ttftext.h"
#include "UI/iconsystem.h"
#include "UI/sizer.h"
#include "UI/prefab.h"
#include "UI/prefabfactory.h"
#include "UI/enginefactories.h"
#include "UI/tooltip.h"
#include "UI/focusable.h"
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
            // A scene with nothing in it, to leave to and to start from
            struct EmptyScene : public Scene
            {
                void init() override {}
            };

            struct LifeFixture
            {
                EntitySystem ecs;
                MasterRenderer renderer;
                IconSystem* icons = nullptr;
                ThemeSystem* theme = nullptr;
                WorldFacts* facts = nullptr;
                FactRouter* router = nullptr;
                SceneElementSystem* scenes = nullptr;

                LifeFixture()
                {
                    Motion::setReduced(true);
                    ecs.createSystem<PositionComponentSystem>();
                    ecs.createSystem<LayoutSystem>();
                    ecs.createSystem<PrefabSystem>();
                    ecs.succeed<PositionComponentSystem, LayoutSystem>();
                    ecs.succeed<PositionComponentSystem, PrefabSystem>();
                    ecs.createSystem<TTFTextSystem>(&renderer);
                    ecs.createSystem<Simple2DObjectSystem>(&renderer);
                    ecs.createSystem<RoundedRect2DObjectSystem>(&renderer);
                    ecs.createSystem<HatchRect2DObjectSystem>(&renderer);
                    ecs.createSystem<DottedLine2DObjectSystem>(&renderer);
                    ecs.createSystem<StrokeRect2DObjectSystem>(&renderer);
                    icons = ecs.createSystem<IconSystem>(&renderer);
                    ecs.createSystem<MouseHoverSystem>();
                    ecs.createSystem<FocusableSystem>();
                    ecs.createSystem<FocusOrderSystem>();
                    auto* tip = ecs.createSystem<TooltipSystem>();
                    ecs.succeed<MouseHoverSystem, TooltipSystem>();
                    ecs.createSystem<TweenSystem>();
                    facts = createTestFacts(&ecs);
                    router = ecs.createSystem<FactRouter>();
                    theme = ecs.createSystem<ThemeSystem>();
                    theme->loadTheme("chronicle/tokens.json", "fonts");
                    tip->setDefaultFont("body-sm");
                    ecs.createSystem<GlossRegistry>();
                    ecs.createSystem<ButtonSystem>();
                    ecs.createSystem<TabsSystem>();
                    ecs.createSystem<ActivitySystem>();
                    installIconEntries();

                    auto* registry = ecs.createSystem<PrefabFactoryRegistry>();
                    registerEnginePrefabFactories(registry);
                    registerChronicleFactories(registry);

                    scenes = ecs.createSystem<SceneElementSystem>();

                    ecs.sendEvent(ResizeEvent{1320.0f, 1020.0f});
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

                void frames(int n)
                {
                    for (int i = 0; i < n; ++i)
                        ecs.executeOnce();
                }

                // Loads the scene and runs until it has started up and its first paths have landed
                template <typename SceneType, typename... Args>
                SceneType* load(const Args&... args)
                {
                    scenes->loadSystemScene<SceneType>(args...);
                    frames(12);

                    return dynamic_cast<SceneType*>(scenes->systemScene);
                }

                LifeScene* life(LifeSceneOptions opt = mockup())
                {
                    return load<LifeScene>(opt);
                }

                static LifeSceneOptions mockup()
                {
                    LifeSceneOptions opt;
                    opt.noSave = true;
                    opt.rulesRoot = "rules";
                    opt.pageFile = "res/chronicle/ui/life.yaml";
                    return opt;
                }

                // Lets the paths just written reach their widgets
                void settle() { frames(4); }

                template <typename Type>
                Type fact(const std::string& path)
                {
                    EXPECT_TRUE(facts->hasFact(path)) << path;
                    return facts->getFact<Type>(path);
                }

                CompRef<PositionComponent> pos(EntityRef e) { return ecs.getEntity(e.id)->get<PositionComponent>(); }
            };

            const char* const Names[] = {
                "title", "subtitle", "age", "ageNote", "tabs",
                "who", "portrait", "name", "profession", "origin",
                "holds", "ledger", "parts", "str", "dex", "int", "vit",
                "clockPanel", "clock", "doors", "window.ruins", "window.tourney",
                "working", "running", "may", "activities",
                "happened", "log", "scribe", "skillsPanel", "skills",
            };

            struct Box
            {
                float left, top, right, bottom;
            };
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(lifescene_test, builds_from_file_at_1320)
        {
            MockLogger logger;
            LifeFixture f;

            LifeScene* life = f.life();
            ASSERT_NE(life, nullptr);
            ASSERT_FALSE(life->page.empty());

            for (const char* name : Names)
                EXPECT_FALSE(life->named(name).empty()) << name;

            auto box = [&](const char* name) {
                auto p = f.pos(life->named(name));
                return Box{p->x, p->y, p->x + p->width, p->y + p->height};
            };

            // Three columns, side by side, panels stacked without overlapping, on a 1020 page
            const std::vector<std::vector<const char*>> columns = {
                {"who", "holds", "parts"},
                {"clockPanel", "doors", "working", "may"},
                {"happened", "scribe", "skillsPanel"},
            };

            float columnRight = 0.0f;

            for (const auto& column : columns)
            {
                float above = 0.0f;
                float right = 0.0f;

                for (const char* name : column)
                {
                    const Box b = box(name);

                    EXPECT_GE(b.left, columnRight) << name;
                    EXPECT_GE(b.top, above) << name;
                    EXPECT_LE(b.bottom, 1020.0f) << name;
                    EXPECT_LE(b.right, 1320.0f - 48.0f + 0.5f) << name;

                    above = b.bottom;
                    right = std::max(right, b.right);
                }

                columnRight = right;
            }

            EXPECT_NEAR(box("who").left, 48.0f, 0.5f);
            EXPECT_NEAR(box("clockPanel").left, 392.0f, 0.5f);
            EXPECT_NEAR(box("happened").left, 972.0f, 0.5f);
            EXPECT_NEAR(box("holds").top, box("who").bottom + 16.0f, 0.5f);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(lifescene_test, start_up_publishes_from_save)
        {
            MockLogger logger;
            LifeFixture f;

            LifeScene* life = f.life();
            ASSERT_NE(life, nullptr);

            EXPECT_FLOAT_EQ(f.fact<float>("life.age"), 17.5f);
            EXPECT_EQ(f.fact<int>("character.parts.str"), 15);
            EXPECT_EQ(f.fact<std::string>("resources.coin.value"), "412");

            ASSERT_NE(life->piece<StatLine>("str"), nullptr);
            EXPECT_EQ(life->piece<StatLine>("str")->figure.spec.text, "15");

            auto ledger = life->piece<ResourceLedger>("ledger");
            ASSERT_NE(ledger, nullptr);
            ASSERT_NE(ledger->row("coin"), nullptr);
            EXPECT_EQ(ledger->row("coin")->figure.spec.text, "412");
            EXPECT_EQ(ledger->groups.size(), 4u);

            // The skills have no heading
            auto skills = life->piece<ResourceLedger>("skills");
            ASSERT_NE(skills, nullptr);
            ASSERT_EQ(skills->groups.size(), 1u);
            EXPECT_FLOAT_EQ(f.pos(skills->groups[0].heading)->height, 0.0f);
            EXPECT_EQ(skills->row("swd")->figure.spec.text, "3");

            // The head and the log, from the save
            EXPECT_EQ(life->piece<Label>("age")->spec.text, "17.5");
            EXPECT_EQ(life->piece<EventLog>("log")->size(), life->save.log.size());
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(lifescene_test, start_up_publishes_from_rules)
        {
            MockLogger logger;
            LifeFixture f;

            LifeScene* life = f.life();
            ASSERT_NE(life, nullptr);

            auto list = life->piece<ActivityList>("activities");
            ASSERT_NE(list, nullptr);
            EXPECT_NE(list->row(&f.ecs, "train.yard"), nullptr);
            EXPECT_NE(list->row(&f.ecs, "adventure.forest"), nullptr);

            // Squiring asks more than he has: the script locks it
            ASSERT_NE(list->row(&f.ecs, "train.squire"), nullptr);
            EXPECT_EQ(list->row(&f.ecs, "train.squire")->spec.state, ActivityState::Locked);

            EXPECT_EQ(f.fact<std::string>("window.ruins.state"), "open");
            EXPECT_EQ(life->piece<WindowMeter>("window.ruins")->spec.state, WindowState::Open);
            EXPECT_EQ(f.fact<int>("life.next.in"), 6);
            EXPECT_EQ(f.fact<std::string>("life.next.label"), "Choose a path");
            EXPECT_EQ(life->piece<LifeClock>("clock")->spec.nextIn, 6);
            EXPECT_EQ(life->piece<Label>("subtitle")->spec.text, "THE SEVENTEENTH YEAR \xC2\xB7 SUMMER \xC2\xB7 BELLMOOR");

            // The Squire's asks at 18 stand on his parts
            EXPECT_EQ(life->piece<StatLine>("str")->spec.threshold, 18);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(lifescene_test, select_runs_forecast)
        {
            MockLogger logger;
            LifeFixture f;

            LifeScene* life = f.life();
            ASSERT_NE(life, nullptr);

            f.ecs.sendEvent(ActivitySelectedEvent{"life.activities", "train.yard"});
            f.settle();

            EXPECT_EQ(f.fact<int>("character.parts.str.projected"), 17);

            auto str = life->piece<StatLine>("str");
            ASSERT_TRUE(str->projected.has_value());
            EXPECT_EQ(str->projected->spec.text, "\xE2\x86\x92 17");

            // A part the activity does not raise has no ghost
            EXPECT_EQ(f.fact<int>("character.parts.int.projected"), -1);
            EXPECT_FALSE(life->piece<StatLine>("int")->projected.has_value());

            f.ecs.sendEvent(ActivitySelectedEvent{"life.activities", ""});
            f.settle();

            EXPECT_EQ(f.fact<int>("character.parts.str.projected"), -1);
            EXPECT_FALSE(str->projected.has_value());
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(lifescene_test, confirm_starts_and_months_tick)
        {
            MockLogger logger;
            LifeFixture f;

            LifeScene* life = f.life();
            ASSERT_NE(life, nullptr);

            auto clock = life->piece<LifeClock>("clock");
            auto running = life->piece<ActivityList>("running");
            ASSERT_NE(clock, nullptr);
            ASSERT_NE(running, nullptr);

            const size_t logRows = life->piece<EventLog>("log")->size();

            f.ecs.sendEvent(ActivityActivatedEvent{"life.activities", "train.yard"});
            f.settle();

            EXPECT_EQ(f.fact<std::string>("activity.running.id"), "train.yard");
            ASSERT_EQ(running->spec.groups.size(), 1u);
            ASSERT_EQ(running->spec.groups[0].rows.size(), 1u);
            EXPECT_NE(running->row(&f.ecs, "train.yard"), nullptr);
            EXPECT_FLOAT_EQ(clock->spec.runningMonths, 6.0f);
            EXPECT_EQ(life->piece<ActivityList>("activities")->row(&f.ecs, "train.yard")->spec.state, ActivityState::Running);

            // The running row is built with its caption and percent: they came in the same update
            ActivityRow* at = running->find(&f.ecs, "train.yard");
            ASSERT_NE(at, nullptr);
            EXPECT_EQ(at->spec.caption, f.fact<std::string>("activity.running.caption"));
            EXPECT_EQ(at->spec.caption.rfind("MONTH 0 OF 6", 0), 0u) << at->spec.caption;
            ASSERT_TRUE(at->progress.has_value());
            ASSERT_TRUE(at->progress->caption.has_value());
            EXPECT_EQ(at->progress->caption->spec.text, at->spec.caption);

            for (int i = 0; i < 3; ++i)
                life->onMonth();
            f.settle();

            EXPECT_FLOAT_EQ(f.fact<float>("activity.running.percent"), 50.0f);
            EXPECT_NEAR(clock->shownAge, 17.75f, 0.001f);
            EXPECT_FLOAT_EQ(clock->spec.runningMonths, 3.0f);
            EXPECT_EQ(running->row(&f.ecs, "train.yard")->spec.percent, 50.0f);

            for (int i = 0; i < 3; ++i)
                life->onMonth();
            f.settle();

            // At term: the row is done, the stat is the script's, the log has the line
            EXPECT_EQ(f.fact<std::string>("activity.running.id"), "");
            EXPECT_TRUE(running->spec.groups.empty());
            EXPECT_EQ(life->save.stats["str"], 17);
            EXPECT_EQ(life->piece<StatLine>("str")->figure.spec.text, "17");
            EXPECT_EQ(life->piece<EventLog>("log")->size(), logRows + 1);
            EXPECT_EQ(life->save.log.back().kind, LogKind::Gain);
            EXPECT_EQ(life->save.log.back().text, "Train at the yard");
            EXPECT_EQ(life->piece<ActivityList>("activities")->row(&f.ecs, "train.yard")->spec.state, ActivityState::Idle);

            // The promise: the lived edge is where the hatch ended
            EXPECT_NEAR(clock->shownAge, 18.0f, 0.001f);
            EXPECT_FLOAT_EQ(clock->spec.runningMonths, 0.0f);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // The guild's errands raise Haggling to 2, which unlocks the Guild's letter: a row going
        // from locked to idle when a term ends.
        TEST(lifescene_test, a_term_that_unlocks_an_activity)
        {
            MockLogger logger;
            LifeFixture f;

            LifeScene* life = f.life();
            ASSERT_NE(life, nullptr);

            auto list = life->piece<ActivityList>("activities");
            ASSERT_NE(list, nullptr);
            ASSERT_EQ(list->row(&f.ecs, "guild.letter")->spec.state, ActivityState::Locked);

            f.ecs.sendEvent(ActivityActivatedEvent{"life.activities", "guild.errands"});
            f.settle();

            for (int i = 0; i < 3; ++i)
            {
                life->onMonth();
                f.settle();
            }

            f.settle();

            EXPECT_EQ(life->save.running, "");
            EXPECT_EQ(life->save.stats["haggle"], 2);
            EXPECT_EQ(list->row(&f.ecs, "guild.letter")->spec.state, ActivityState::Idle);
            EXPECT_EQ(life->piece<ResourceLedger>("skills")->row("haggle")->figure.spec.text, "2");
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(lifescene_test, confirm_refused_while_running)
        {
            MockLogger logger;
            LifeFixture f;

            LifeScene* life = f.life();
            ASSERT_NE(life, nullptr);

            f.ecs.sendEvent(ActivityActivatedEvent{"life.activities", "train.yard"});
            f.settle();

            const size_t logEntries = life->save.log.size();

            f.ecs.sendEvent(ActivityActivatedEvent{"life.activities", "study.letters"});
            f.settle();

            EXPECT_EQ(life->save.running, "train.yard");
            EXPECT_EQ(f.fact<std::string>("activity.running.id"), "train.yard");
            ASSERT_EQ(life->save.log.size(), logEntries + 1);
            EXPECT_EQ(life->save.log.back().text, "Already at work: Train at the yard");
            EXPECT_EQ(life->piece<EventLog>("log")->size(), life->save.log.size());
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(lifescene_test, save_round_trip)
        {
            MockLogger logger;
            LifeFixture f;

            const std::string path = "save/test_life_round_trip.sz";
            std::remove(path.c_str());

            LifeSceneOptions opt = LifeFixture::mockup();
            opt.noSave = false;
            opt.savePath = path;

            LifeScene* life = f.life(opt);
            ASSERT_NE(life, nullptr);

            f.ecs.sendEvent(ActivityActivatedEvent{"life.activities", "train.yard"});
            f.settle();
            life->onMonth();
            f.settle();

            ASSERT_TRUE(life->saveNow());

            const LifeSave written = life->save;
            const size_t writtenLog = written.log.size() - 1;   // saveNow's own line came after

            f.load<EmptyScene>();

            LifeScene* again = f.life(opt);
            ASSERT_NE(again, nullptr);

            EXPECT_NEAR(again->save.age, written.age, 0.0001f);
            EXPECT_EQ(again->save.running, "train.yard");
            EXPECT_EQ(again->save.monthsIn, 1);
            EXPECT_EQ(again->save.stats, written.stats);
            EXPECT_EQ(again->save.log.size(), writtenLog);
            EXPECT_EQ(again->save.resources.size(), written.resources.size());

            EXPECT_NEAR(f.fact<float>("life.age"), written.age, 0.0001f);
            EXPECT_EQ(f.fact<std::string>("activity.running.id"), "train.yard");
            EXPECT_EQ(again->piece<EventLog>("log")->size(), writtenLog);

            std::remove(path.c_str());
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(lifescene_test, on_leave_is_clean)
        {
            MockLogger logger;
            LifeFixture f;

            f.load<EmptyScene>();
            f.frames(6);

            const size_t entities = f.ecs.getNbEntities();
            const size_t subscribers = f.router->count();

            LifeScene* life = f.life();
            ASSERT_NE(life, nullptr);
            EXPECT_GT(f.router->count(), subscribers);
            EXPECT_GT(f.ecs.getNbEntities(), entities + 100);

            f.load<EmptyScene>();
            f.frames(12);

            EXPECT_EQ(f.router->count(), subscribers);
            EXPECT_EQ(f.ecs.getNbEntities(), entities);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(lifescene_test, fresh_life)
        {
            MockLogger logger;
            LifeFixture f;

            LifeSceneOptions opt = LifeFixture::mockup();
            opt.fresh = true;

            LifeScene* life = f.life(opt);
            ASSERT_NE(life, nullptr);

            EXPECT_FLOAT_EQ(f.fact<float>("life.age"), 7.0f);
            EXPECT_EQ(life->piece<Label>("age")->spec.text, "7.0");

            // Nothing earned: nothing in the ledger
            auto ledger = life->piece<ResourceLedger>("ledger");
            ASSERT_NE(ledger, nullptr);
            EXPECT_TRUE(ledger->groups.empty());

            // One year rubric, one milestone row
            auto log = life->piece<EventLog>("log");
            ASSERT_EQ(log->items.size(), 2u);
            EXPECT_TRUE(std::holds_alternative<EventLog::Year>(log->items[0]));
            ASSERT_TRUE(std::holds_alternative<EventLog::Row>(log->items[1]));
            EXPECT_EQ(std::get<EventLog::Row>(log->items[1]).entry.text, "Childhood");
            EXPECT_EQ(std::get<EventLog::Row>(log->items[1]).entry.kind, LogKind::Milestone);

            // Every door is still ahead
            for (const char* id : {"choir", "ruins", "squire", "tourney", "academy"})
                EXPECT_EQ(f.fact<std::string>(std::string("window.") + id + ".state"), "upcoming") << id;
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // The scene computes nothing about the game: a scan of its source (a test in the spirit of
        // the rule, not a proof). No half, no twelfths outside the month step, and no stat, skill
        // or resource name outside wire().
        TEST(lifescene_test, no_scene_arithmetic)
        {
            std::ifstream file(std::string(CHRONICLE_SOURCE_DIR) + "/examples/Chronicle/Scenes/lifescene.cpp");
            ASSERT_TRUE(file.good());

            std::stringstream buffer;
            buffer << file.rdbuf();
            const std::string source = buffer.str();

            EXPECT_EQ(source.find("+ 0.5f"), std::string::npos);
            EXPECT_EQ(source.find("* 12"), std::string::npos);

            // The one month step, and nothing else divided by twelve
            size_t twelfths = 0;

            for (size_t at = source.find("/ 12"); at != std::string::npos; at = source.find("/ 12", at + 1))
                ++twelfths;

            EXPECT_EQ(twelfths, 1u);
            EXPECT_NE(source.find("save.age += 1.0f / 12.0f;"), std::string::npos);

            // Stat names: the page's handles live in wire() only ("coin" is also a log kind, and
            // may be named where kinds are)
            const size_t wireStart = source.find("void LifeScene::wire()");
            const size_t wireEnd = source.find("void LifeScene::rebuild()");
            ASSERT_NE(wireStart, std::string::npos);
            ASSERT_NE(wireEnd, std::string::npos);

            const std::string outside = source.substr(0, wireStart) + source.substr(wireEnd);

            for (const char* stat : {"\"str\"", "\"dex\"", "\"int\"", "\"vit\"", "\"swd\"", "\"ride\"", "\"letters\"", "\"haggle\"", "\"letter\""})
                EXPECT_EQ(outside.find(stat), std::string::npos) << stat;
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // Hovering a part, an activity or a door explains its numbers, with the rules' own.
        TEST(lifescene_test, glosses_explain_the_numbers)
        {
            MockLogger logger;
            LifeFixture f;

            LifeScene* life = f.life();
            ASSERT_NE(life, nullptr);

            auto registry = f.ecs.getSystem<GlossRegistry>();
            ASSERT_NE(registry, nullptr);

            auto rowOf = [](const GlossSpec* g, const std::string& label) -> std::string {
                for (const auto& r : g->rows)
                {
                    if (r.label == label)
                        return r.value;
                }

                return "<none>";
            };

            // A part: where it is, what the next milestone asks of it
            const GlossSpec* str = registry->find("parts/str");
            ASSERT_NE(str, nullptr);
            EXPECT_EQ(str->title, "Strength");
            EXPECT_EQ(rowOf(str, "Now"), "15");
            EXPECT_EQ(rowOf(str, "Choose a path asks"), "18");
            EXPECT_EQ(str->footnote, "IN 6 MO: CHOOSE A PATH");

            // Selected, it says what the activity brings it to
            f.ecs.sendEvent(ActivitySelectedEvent{"life.activities", "train.yard"});
            f.settle();
            EXPECT_EQ(rowOf(registry->find("parts/str"), "Train at the yard brings it to"), "17");

            // An activity: its time, its gains, what it asks
            const GlossSpec* squire = registry->find("activity/train.squire");
            ASSERT_NE(squire, nullptr);
            EXPECT_EQ(squire->title, "Squire at the keep");
            EXPECT_EQ(rowOf(squire, "Time"), "12 mo");
            EXPECT_EQ(rowOf(squire, "Strength"), "15 / 18");
            EXPECT_EQ(squire->footnote, "NOT YET: IT ASKS MORE THAN HE HAS");

            // A door: its note, its ages, what fits
            const GlossSpec* ruins = registry->find("window/ruins");
            ASSERT_NE(ruins, nullptr);
            EXPECT_EQ(ruins->text, f.fact<std::string>("window.ruins.note"));
            EXPECT_EQ(rowOf(ruins, "Opens at"), "15");
            EXPECT_EQ(rowOf(ruins, "Attempts that fit"), "6");

            // And the pieces carry them
            EXPECT_TRUE(life->piece<StatLine>("str")->root->has<TooltipComponent>());
            EXPECT_EQ(life->piece<StatLine>("str")->root->get<TooltipComponent>()->text, "parts/str");
            EXPECT_EQ(life->piece<WindowMeter>("window.ruins")->root->get<TooltipComponent>()->text, "window/ruins");

            ActivityRow* yard = life->piece<ActivityList>("activities")->row(&f.ecs, "train.yard");
            ASSERT_NE(yard, nullptr);
            ASSERT_TRUE(yard->root->has<TooltipComponent>());
            EXPECT_EQ(yard->root->get<TooltipComponent>()->text, "activity/train.yard");
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(lifescene_test, tabs_without_pages_reselect_life)
        {
            MockLogger logger;
            LifeFixture f;

            LifeScene* life = f.life();
            ASSERT_NE(life, nullptr);

            auto tabs = life->piece<Tabs>("tabs");
            ASSERT_NE(tabs, nullptr);

            const size_t logEntries = life->save.log.size();

            f.ecs.sendEvent(TabSelectedEvent{tabs->root.id, "life.tabs", 3});
            f.settle();

            EXPECT_EQ(tabs->active(), 0);
            ASSERT_EQ(life->save.log.size(), logEntries + 1);
            EXPECT_EQ(life->save.log.back().text, "Guild has no page yet");
        }
    }
}
