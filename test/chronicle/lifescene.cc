#include "stdafx.h"

#ifdef __linux__
#include <SDL2/SDL.h>
#elif _WIN32
#include <SDL.h>
#endif

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "Scenes/lifescene.h"
#include "UI/factories.h"
#include "UI/label.h"
#include "UI/panel.h"
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
#include "UI/sizer.h"
#include "Core/motion.h"
#include "Core/factrouter.h"
#include "Core/settle.h"

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
#include "Input/sdlevents.h"
#include "Systems/gamefacts.h"
#include "Systems/achievement.h"
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
                AchievementSys* achievements = nullptr;
                SceneElementSystem* scenes = nullptr;
                EntityRef window;

                // What the window does when it is resized: the viewport entity, then the event
                void resize(float width, float height)
                {
                    window->get<PositionComponent>()->setWidth(width);
                    window->get<PositionComponent>()->setHeight(height);
                    ecs.sendEvent(ResizeEvent{width, height});
                    frames(6);
                }

                // `settles`: with the game's settle phase, or pass by pass as an ecs without one
                LifeFixture(bool settles = true)
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
                    achievements = ecs.createSystem<AchievementSys>();
                    ecs.getComponentRegistry()->unregisterSystemSave(achievements->getSystemName());
                    achievements->clear();
                    ecs.succeed<AchievementSys, WorldFacts>();
                    theme = ecs.createSystem<ThemeSystem>();
                    theme->loadTheme("chronicle/tokens.json", "fonts");
                    tip->setDefaultFont("body-sm");
                    ecs.createSystem<GlossRegistry>();
                    ecs.createSystem<ButtonSystem>();
                    ecs.createSystem<TabsSystem>();
                    ecs.createSystem<ActivitySystem>();
                    ecs.succeed<LayoutSystem, ActivitySystem>();
                    installIconEntries();

                    // As the game does
                    if (settles)
                        settleThePage(&ecs);

                    auto* registry = ecs.createSystem<PrefabFactoryRegistry>();
                    registerEnginePrefabFactories(registry);
                    registerChronicleFactories(registry);

                    scenes = ecs.createSystem<SceneElementSystem>();

                    // The viewport the page anchors to, as the window names it
                    ecs.createSystem<EntityNameSystem>();
                    window = ecs.createEntity();
                    auto windowPos = ecs.attach<PositionComponent>(window);
                    windowPos->setWidth(1320.0f);
                    windowPos->setHeight(1020.0f);
                    ecs.attach<UiAnchor>(window);
                    ecs.attach<EntityName>(window, "__MainWindow");

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

                // A first life at its first frame, with its guide. The tests of what every life
                // does ask for a later one (`fresh` with `lives` 2): the list whole, a year of rations
                static LifeSceneOptions first()
                {
                    LifeSceneOptions opt = mockup();
                    opt.fresh = true;
                    return opt;
                }

                // So many milliseconds, a frame's worth at a time: what lasts a moment runs its course
                void pass(float ms, float step = 250.0f)
                {
                    for (float left = ms; left > 0.0f; left -= step)
                    {
                        ecs.sendEvent(TickEvent{std::min(step, left)});
                        frames(2);
                    }

                    frames(4);
                }

                // Lets the paths just written reach their widgets
                void settle() { frames(4); }

                // Where everything on the page stands: every entity's box, and whether it is drawn
                std::string placed()
                {
                    std::ostringstream out;
                    const auto& entities = ecs.view();

                    for (size_t i = 1; i <= ecs.getNbEntities(); ++i)
                    {
                        auto entity = entities[i];

                        if (not entity or not entity->has<PositionComponent>())
                            continue;

                        auto pos = entity->get<PositionComponent>();

                        out << entity->id << ":" << pos->x << "," << pos->y << "," << pos->width << "," << pos->height << "," << (pos->isRenderable() ? 1 : 0) << ";";
                    }

                    return out.str();
                }

                // The passes it takes for the page to stop moving, `most` when it never does
                int passesToRest(int most = 40)
                {
                    std::string last = placed();
                    int still = 0;
                    int passes = 0;

                    // At rest once it has not moved for a few passes: a link of a chain may leave a pass idle
                    while (passes < most and still < 6)
                    {
                        ecs.executeOnce();
                        ++passes;

                        const std::string now = placed();

                        still = now == last ? still + 1 : 0;
                        last = now;
                    }

                    return passes - still;
                }

                template <typename Type>
                Type fact(const std::string& path)
                {
                    EXPECT_TRUE(facts->hasFact(path)) << path;
                    return facts->getFact<Type>(path);
                }

                CompRef<PositionComponent> pos(EntityRef e) { return ecs.getEntity(e.id)->get<PositionComponent>(); }
            };

            // What happened to him, without the town's lore: the lines the tests of a life count
            std::vector<LogEntry> happened(const LifeScene* life)
            {
                std::vector<LogEntry> entries;

                for (const auto& entry : life->save.log)
                {
                    if (entry.kind != LogKind::Lore)
                        entries.push_back(entry);
                }

                return entries;
            }

            // The deeds he reached: not the lore he read, nor what the guide told him
            std::vector<std::string> deedsOf(const LifeScene* life)
            {
                std::vector<std::string> deeds;

                for (const auto& id : life->save.achieved)
                {
                    if (id.rfind("lore.", 0) != 0 and id.rfind("guide.", 0) != 0)
                        deeds.push_back(id);
                }

                return deeds;
            }

            // The guide's leaf is up, under what it is about (over it where the window ends) and
            // inside the window
            void expectNoteBeside(LifeFixture& f, LifeScene* life, EntityRef target)
            {
                ASSERT_FALSE(life->note.empty());
                EXPECT_TRUE(life->noteShown);

                auto note = f.pos(life->note);
                auto where = f.pos(target);

                EXPECT_TRUE(note->visible);
                EXPECT_GE(note->x, 0.0f);
                EXPECT_LE(note->x + note->width, life->windowWidth + 0.5f);
                EXPECT_GE(note->y, 0.0f);
                EXPECT_LE(note->y + note->height, life->windowHeight + 0.5f);
                EXPECT_TRUE(note->y >= where->y + where->height or note->y + note->height <= where->y) << "the leaf covers what it points at";
            }

            bool reached(const LifeScene* life, const std::string& id)
            {
                return std::find(life->save.achieved.begin(), life->save.achieved.end(), id) != life->save.achieved.end();
            }

            const char* const Names[] = {
                "title", "about", "tabs",
                "str", "dex", "int", "vit", "skills", "holds", "ledger",
                "may", "activities",
                "clockPanel", "clock",
                "working", "running", "skip", "happened", "log",
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

            // Three columns, side by side, panels stacked without overlapping, on a 1020 page: what
            // he holds and has learned, the choice alone, the years and the work and the log and
            // his parts
            const std::vector<std::vector<const char*>> columns = {
                {"holds", "learned"},
                {"may"},
                {"clockPanel", "working", "happened"},
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
                    EXPECT_LE(b.right, 1320.0f - 16.0f + 0.5f) << name;

                    above = b.bottom;
                    right = std::max(right, b.right);
                }

                columnRight = right;
            }

            // The page under the panels, each panel's ground under what it holds
            const float pageZ = f.pos(life->named("page"))->z;

            for (const char* name : {"holds", "learned", "may", "clockPanel", "working", "happened"})
            {
                auto panel = life->piece<Panel>(name);
                ASSERT_NE(panel, nullptr) << name;

                EXPECT_GT(f.pos(panel->ground)->z, pageZ) << name;
                EXPECT_GT(f.pos(panel->body)->z, f.pos(panel->ground)->z) << name;
            }

            EXPECT_NEAR(box("holds").left, 16.0f, 0.5f);
            EXPECT_NEAR(box("holds").right, 316.0f, 0.5f);
            EXPECT_NEAR(box("may").left, 328.0f, 0.5f);

            // A head of two lines over columns that start at 70: the title, then the year with what
            // he is after it on the same line
            EXPECT_NEAR(box("holds").top, 70.0f, 0.5f);
            EXPECT_NEAR(box("may").top, 70.0f, 0.5f);
            EXPECT_NEAR(box("clockPanel").top, 70.0f, 0.5f);
            EXPECT_LE(box("title").bottom, box("about").top + 0.5f);
            EXPECT_NEAR(box("about").left, box("title").left, 0.5f);
            EXPECT_LE(box("about").bottom, 70.0f);

            // The world's date at the top right, its month and season under its year; his age is
            // the clock's to say, and the head has no line for it
            ASSERT_FALSE(life->named("date").empty());
            ASSERT_FALSE(life->named("dateNote").empty());
            EXPECT_TRUE(life->named("age").empty());
            EXPECT_TRUE(life->named("ageNote").empty());
            EXPECT_TRUE(life->named("subtitle").empty());
            EXPECT_NEAR(box("date").right, 1304.0f, 0.5f);
            EXPECT_NEAR(box("dateNote").right, 1304.0f, 0.5f);
            EXPECT_LE(box("date").bottom, box("dateNote").top + 0.5f);
            EXPECT_LE(box("dateNote").bottom, 70.0f);
            // The line of who he is ends where the date's room begins: the note's box is wider than what it writes
            EXPECT_LE(box("about").right, 1320.0f - 16.0f - 300.0f + 0.5f);
            EXPECT_EQ(life->piece<Label>("date")->spec.text, "YEAR 10");
            EXPECT_EQ(life->piece<Label>("dateNote")->spec.text, "MONTH 7 \xC2\xB7 SUMMER \xC2\xB7 BELLMOOR");

            // The chapters stand at the head of the choice, in place of a heading, over the list
            auto choice = life->piece<Panel>("may");
            ASSERT_NE(choice, nullptr);
            EXPECT_FALSE(choice->title.has_value());
            EXPECT_NEAR(box("tabs").top, box("may").top + 16.0f, 0.5f);
            EXPECT_NEAR(box("tabs").left, box("may").left + 16.0f, 0.5f);
            EXPECT_NEAR(box("tabs").right, box("may").right - 16.0f, 0.5f);
            EXPECT_NEAR(box("activities").top, box("tabs").bottom + 12.0f, 0.5f);
            EXPECT_LE(box("may").bottom, 1020.0f);
            EXPECT_GT(box("may").bottom, 1020.0f - 32.0f);
            EXPECT_NEAR(box("clockPanel").left, 944.0f, 0.5f);
            EXPECT_NEAR(box("learned").top, box("holds").bottom + 16.0f, 0.5f);
            EXPECT_NEAR(box("working").top, box("clockPanel").bottom + 16.0f, 0.5f);
            EXPECT_NEAR(box("happened").top, box("working").bottom + 16.0f, 0.5f);
            EXPECT_NEAR(box("happened").left, 944.0f, 0.5f);

            // The log closes the right column, down to the bottom of the page: no panel of parts
            // under it any more
            EXPECT_TRUE(life->named("parts").empty());
            EXPECT_LE(box("happened").bottom, 1020.0f);
            EXPECT_GT(box("happened").bottom, 1020.0f - 32.0f);

            // His Vitality stands under the clock's age, in its panel; his other parts stand with
            // what he has learned, over his skills, in the left column; the doors are not on this page
            EXPECT_GE(box("vit").top, box("clock").bottom);
            EXPECT_LE(box("vit").bottom, box("clockPanel").bottom);
            EXPECT_GE(box("str").top, box("learned").top);
            EXPECT_GE(box("dex").top, box("str").bottom);
            EXPECT_GE(box("int").top, box("dex").bottom);
            EXPECT_LE(box("int").bottom, box("skills").top);
            EXPECT_LE(box("skills").bottom, box("learned").bottom + 0.5f);
            EXPECT_NEAR(box("str").left, box("learned").left + 16.0f, 0.5f);
            EXPECT_LE(box("str").right, box("learned").right - 16.0f + 0.5f);
            EXPECT_TRUE(life->named("window.keep").empty());

            // The head: what he is, between the title and the year
            EXPECT_EQ(life->piece<Label>("about")->spec.text, "Sworn man of the Keep at Bellmoor \xC2\xB7 Second son of the miller, born at the mill on the Bell.");

            // One ground for every row of the choice
            for (const auto& group : life->piece<ActivityList>("activities")->spec.groups)
            {
                for (const auto& row : group.rows)
                {
                    ActivityRow* r = life->piece<ActivityList>("activities")->row(&f.ecs, row.id);
                    ASSERT_NE(r, nullptr);
                    EXPECT_FALSE(r->root->get<ActivityRowState>()->stripe) << row.id;
                }
            }
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // The page follows the window: the right column and the age hold to its right edge, the
        // middle column takes the rest of the width, the choice and the log the rest of the height.
        TEST(lifescene_test, page_follows_the_window)
        {
            MockLogger logger;
            LifeFixture f;

            LifeScene* life = f.life();
            ASSERT_NE(life, nullptr);

            auto right = [&](const char* name) { auto p = f.pos(life->named(name)); return p->x + p->width; };
            auto bottom = [&](const char* name) { auto p = f.pos(life->named(name)); return p->y + p->height; };

            // As designed, at 1320 x 1020
            EXPECT_NEAR(f.pos(life->named("may"))->width, 604.0f, 0.5f);
            EXPECT_NEAR(right("clockPanel"), 1304.0f, 0.5f);
            EXPECT_NEAR(right("date"), 1304.0f, 0.5f);

            const float logAt1020 = life->piece<EventLog>("log")->spec.height;

            f.resize(1600.0f, 1100.0f);

            // The middle column takes the new width, the right one moves with the edge
            EXPECT_NEAR(f.pos(life->named("may"))->width, 1600.0f - 32.0f - 300.0f - 360.0f - 24.0f, 0.5f);
            EXPECT_NEAR(f.pos(life->named("may"))->x, 328.0f, 0.5f);
            EXPECT_NEAR(right("clockPanel"), 1584.0f, 0.5f);
            EXPECT_NEAR(right("date"), 1584.0f, 0.5f);
            EXPECT_NEAR(right("dateNote"), 1584.0f, 0.5f);
            EXPECT_NEAR(f.pos(life->named("page"))->width, 1600.0f, 0.5f);

            // The tiles follow the list's width: a wider list holds more to a line (five of 164 in
            // 852, 8 apart); the choice and the log take the new height
            auto list = life->piece<ActivityList>("activities");
            EXPECT_NEAR(list->spec.width, 1600.0f - 32.0f - 300.0f - 360.0f - 24.0f - 32.0f, 0.5f);
            EXPECT_EQ(list->columns(), 5);
            ActivityRow* yard = list->row(&f.ecs, "yard");
            ASSERT_NE(yard, nullptr);
            EXPECT_TRUE(yard->spec.tile);
            EXPECT_NEAR(f.pos(yard->root)->width, 164.0f, 0.5f);
            EXPECT_NEAR(f.pos(yard->root)->width, list->tileSize(), 0.5f);

            EXPECT_LE(bottom("may"), 1100.0f);
            EXPECT_GT(bottom("may"), 1100.0f - 32.0f);
            EXPECT_NEAR(life->piece<EventLog>("log")->spec.height, logAt1020 + 80.0f, 0.5f);
            EXPECT_LE(bottom("happened"), 1100.0f);
            EXPECT_GT(bottom("happened"), 1100.0f - 32.0f);

            // And back
            f.resize(1320.0f, 1020.0f);
            EXPECT_NEAR(f.pos(life->named("may"))->width, 604.0f, 0.5f);
            EXPECT_NEAR(right("clockPanel"), 1304.0f, 0.5f);

            // Three to a line again in 572: the tiles were laid again, the row is another
            EXPECT_EQ(list->columns(), 3);
            yard = list->row(&f.ecs, "yard");
            ASSERT_NE(yard, nullptr);
            EXPECT_NEAR(f.pos(yard->root)->width, (572.0f - 16.0f) / 3.0f, 0.5f);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // Below the three columns' least size (680 wide, their narrowest step) the page is the compact
        // file: the work at hand over the choice, a side column of one panel at a time, and everything
        // the life wrote still there. It swaps back when the window grows.
        TEST(lifescene_test, compact_page_at_640x600)
        {
            MockLogger logger;
            LifeFixture f;

            LifeScene* life = f.life();
            ASSERT_NE(life, nullptr);
            EXPECT_FALSE(life->compact);

            f.resize(640.0f, 600.0f);
            f.settle();

            ASSERT_TRUE(life->compact);

            auto box = [&](const char* name) {
                auto p = f.pos(life->named(name));
                return Box{p->x, p->y, p->x + p->width, p->y + p->height};
            };

            auto shown = [&](const char* name) {
                if (life->named(name).empty())
                {
                    ADD_FAILURE() << "nothing on the page is named " << name;
                    return false;
                }

                return f.pos(life->named(name))->isRenderable();
            };

            // The compact page keeps his age and his year in its head
            for (const char* name : {"subtitle", "age", "ageNote"})
                EXPECT_FALSE(life->named(name).empty()) << name;

            // The same names, but the about line, and the side column's tabs
            for (const char* name : Names)
            {
                if (std::string(name) != "about")
                    EXPECT_FALSE(life->named(name).empty()) << name;
            }

            EXPECT_TRUE(life->named("about").empty());
            ASSERT_NE(life->piece<Tabs>("sideTabs"), nullptr);

            // Two columns inside the window: the work at hand over the choice, the side at the right
            EXPECT_NEAR(box("working").left, 24.0f, 0.5f);
            EXPECT_NEAR(box("working").right, 640.0f - 24.0f - 320.0f - 16.0f, 0.5f);
            EXPECT_NEAR(box("may").top, box("working").bottom + 16.0f, 0.5f);
            EXPECT_NEAR(box("may").right, box("working").right, 0.5f);
            EXPECT_LE(box("may").bottom, 600.0f);
            EXPECT_GT(box("may").bottom, 600.0f - 32.0f);
            EXPECT_NEAR(box("age").right, 616.0f, 0.5f);
            EXPECT_LE(box("title").right, box("age").left);

            for (const char* side : {"parts", "holds", "clockPanel", "happened"})
            {
                EXPECT_NEAR(box(side).right, 616.0f, 0.5f) << side;
                EXPECT_NEAR(box(side).left, 296.0f, 0.5f) << side;
                EXPECT_GE(box(side).top, box("sideTabs").bottom) << side;
            }

            EXPECT_LE(box("happened").bottom, 600.0f);

            // The tiles follow the narrower list: one to a line in 224
            auto list = life->piece<ActivityList>("activities");
            EXPECT_EQ(list->columns(), 1);
            ActivityRow* yard = list->row(&f.ecs, "yard");
            ASSERT_NE(yard, nullptr);
            EXPECT_NEAR(f.pos(yard->root)->width, 256.0f - 32.0f, 0.5f);

            // One side panel at a time: his parts first, the log on its tab
            EXPECT_TRUE(shown("parts"));
            EXPECT_FALSE(shown("holds"));
            EXPECT_FALSE(shown("clockPanel"));
            EXPECT_FALSE(shown("happened"));

            f.ecs.sendEvent(TabSelectedEvent{life->piece<Tabs>("sideTabs")->root.id, "life.side", 3});
            f.settle();

            EXPECT_FALSE(shown("parts"));
            EXPECT_TRUE(shown("happened"));
            EXPECT_FALSE(f.pos(life->named("str"))->isRenderable());
            EXPECT_EQ(life->piece<Tabs>("sideTabs")->active(), 3);

            // What the life wrote reached the new page
            EXPECT_EQ(life->piece<Label>("title")->spec.text, "The Chronicle of " + life->save.name);
            EXPECT_EQ(life->piece<EventLog>("log")->size(), life->save.log.size());
            EXPECT_EQ(life->piece<StatLine>("str")->spec.value, life->save.stats["str"]);
            EXPECT_NE(life->piece<ResourceLedger>("ledger")->row("coin"), nullptr);

            // At work: "At work now" grows and the choice gives it the room
            const float idleList = list->spec.height;

            f.ecs.sendEvent(ActivityActivatedEvent{"life.activities", "yard"});
            f.settle();

            EXPECT_NE(life->piece<ActivityList>("running")->row(&f.ecs, "yard"), nullptr);
            EXPECT_LT(list->spec.height, idleList);
            EXPECT_NEAR(box("may").top, box("working").bottom + 16.0f, 0.5f);
            EXPECT_LE(box("may").bottom, 600.0f);

            // And back to three columns, the running row with it
            f.resize(1320.0f, 1020.0f);
            f.settle();

            EXPECT_FALSE(life->compact);
            EXPECT_FALSE(life->named("about").empty());
            EXPECT_TRUE(life->named("sideTabs").empty());
            EXPECT_NEAR(box("may").right - box("may").left, 604.0f, 0.5f);
            EXPECT_TRUE(life->named("parts").empty());
            EXPECT_TRUE(shown("learned"));
            EXPECT_TRUE(shown("happened"));
            EXPECT_NE(life->piece<ActivityList>("running")->row(&f.ecs, "yard"), nullptr);
            EXPECT_EQ(life->piece<EventLog>("log")->size(), life->save.log.size());
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // A window already small at the start builds the compact page from the first frame
        TEST(lifescene_test, starts_compact_in_a_small_window)
        {
            MockLogger logger;
            LifeFixture f;

            f.window->get<PositionComponent>()->setWidth(640.0f);
            f.window->get<PositionComponent>()->setHeight(600.0f);

            LifeScene* life = f.life();
            ASSERT_NE(life, nullptr);
            EXPECT_TRUE(life->compact);
            EXPECT_TRUE(life->named("about").empty());

            ActivityRow* yard = life->piece<ActivityList>("activities")->row(&f.ecs, "yard");
            ASSERT_NE(yard, nullptr);
            EXPECT_NEAR(f.pos(yard->root)->width, 256.0f - 32.0f, 0.5f);

            auto p = f.pos(life->named("may"));
            EXPECT_LE(p->y + p->height, 600.0f);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // The three columns are the page down to a small window: at 800 x 600 they are built at their
        // narrowest step, the two fixed columns 200 and 280 wide, 6 from the window's edges and 4 apart.
        TEST(lifescene_test, three_columns_at_800x600)
        {
            MockLogger logger;
            LifeFixture f;

            f.window->get<PositionComponent>()->setWidth(800.0f);
            f.window->get<PositionComponent>()->setHeight(600.0f);

            LifeSceneOptions opt = LifeFixture::mockup();
            opt.fresh = true;
            opt.lives = 2;

            LifeScene* life = f.life(opt);
            ASSERT_NE(life, nullptr);

            f.frames(12);

            EXPECT_FALSE(life->compact);
            EXPECT_EQ(life->widthStep, 2);
            EXPECT_EQ(life->heightStep, 2);

            EXPECT_NEAR(f.pos(life->named("holds"))->x, 6.0f, 0.5f);
            EXPECT_NEAR(f.pos(life->named("holds"))->width, 200.0f, 0.5f);
            EXPECT_NEAR(f.pos(life->named("may"))->x, 6.0f + 200.0f + 4.0f, 0.5f);
            EXPECT_NEAR(f.pos(life->named("may"))->width, 800.0f - 12.0f - 200.0f - 280.0f - 8.0f, 0.5f);
            EXPECT_NEAR(f.pos(life->named("clockPanel"))->width, 280.0f, 0.5f);
            EXPECT_NEAR(f.pos(life->named("clockPanel"))->x + 280.0f, 800.0f - 6.0f, 0.5f);

            // Back to the roomiest step when the window is
            f.resize(1320.0f, 1020.0f);
            f.frames(12);

            EXPECT_FALSE(life->compact);
            EXPECT_EQ(life->widthStep, 0);
            EXPECT_NEAR(f.pos(life->named("holds"))->width, 300.0f, 0.5f);
            EXPECT_NEAR(f.pos(life->named("may"))->width, 604.0f, 0.5f);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // The left column has no least height: it grows with what he holds and has learned, and in a
        // window too short for it the column scrolls. The three columns stay.
        TEST(lifescene_test, the_left_column_scrolls_in_a_short_window)
        {
            MockLogger logger;
            LifeFixture f;

            LifeScene* life = f.life();
            ASSERT_NE(life, nullptr);

            f.resize(1320.0f, 700.0f);
            f.frames(12);

            EXPECT_FALSE(life->compact);

            EntityRef left = life->named("left");
            ASSERT_FALSE(left.empty());
            ASSERT_TRUE(left->has<VerticalLayout>());

            // The column is the window's height under the head, and what the mockup's life holds is taller
            EXPECT_NEAR(f.pos(left)->height, 700.0f - 70.0f - 16.0f, 0.5f);
            EXPECT_GT(left->get<VerticalLayout>()->contentHeight, f.pos(left)->height);

            // Its two panels, the second 16 under the first
            auto holds = f.pos(life->named("holds"));
            auto learned = f.pos(life->named("learned"));

            EXPECT_NEAR(learned->y, holds->y + holds->height + 16.0f, 0.5f);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // A new life holds little: a window as short as 700 keeps its three columns.
        TEST(lifescene_test, a_new_life_keeps_three_columns_in_a_short_window)
        {
            MockLogger logger;
            LifeFixture f;

            f.window->get<PositionComponent>()->setWidth(1320.0f);
            f.window->get<PositionComponent>()->setHeight(700.0f);

            LifeSceneOptions opt = LifeFixture::mockup();
            opt.fresh = true;
            opt.lives = 2;

            LifeScene* life = f.life(opt);
            ASSERT_NE(life, nullptr);

            f.frames(12);

            EXPECT_FALSE(life->compact);

            // And nothing to scroll: what he holds fits the column
            EntityRef left = life->named("left");
            ASSERT_FALSE(left.empty());
            EXPECT_LE(left->get<VerticalLayout>()->contentHeight, f.pos(left)->height);
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
            EXPECT_EQ(f.fact<int>("character.parts.str"), 12);
            EXPECT_EQ(f.fact<std::string>("resources.coin.value"), "46");

            ASSERT_NE(life->piece<StatLine>("str"), nullptr);
            EXPECT_EQ(life->piece<StatLine>("str")->figure.spec.text, "12");

            auto ledger = life->piece<ResourceLedger>("ledger");
            ASSERT_NE(ledger, nullptr);
            ASSERT_NE(ledger->row("coin"), nullptr);
            EXPECT_EQ(ledger->row("coin")->figure.spec.text, "46");
            EXPECT_NE(ledger->row("keep_oath"), nullptr);

            // What his works left him besides figures has its rows too, under TIES: whom he is
            // known to, who speaks for him
            EXPECT_EQ(ledger->groups.size(), 5u);
            ASSERT_NE(ledger->row("watch_known"), nullptr);
            ASSERT_NE(ledger->row("edric_support"), nullptr);
            EXPECT_EQ(ledger->row("watch_known")->name.spec.text, "Known to the Watch");

            // A tie or a standing is had or not: its row is its name, with no figure and no leader
            // to one; what is counted keeps both
            EXPECT_EQ(ledger->row("edric_support")->figure.spec.text, "");
            EXPECT_EQ(ledger->row("keep_oath")->figure.spec.text, "");
            EXPECT_FALSE(f.pos(ledger->row("keep_oath")->leader)->isVisible());
            EXPECT_FALSE(f.pos(ledger->row("watch_known")->leader)->isVisible());
            EXPECT_TRUE(f.pos(ledger->row("coin")->leader)->isVisible());
            EXPECT_EQ(ledger->row("mara_student"), nullptr);

            // The skills have no heading; one he has none of is not shown
            auto skills = life->piece<ResourceLedger>("skills");
            ASSERT_NE(skills, nullptr);
            ASSERT_EQ(skills->groups.size(), 1u);
            EXPECT_FLOAT_EQ(f.pos(skills->groups[0].heading)->height, 0.0f);
            ASSERT_NE(skills->row("arms"), nullptr);
            EXPECT_EQ(skills->row("arms")->figure.spec.text, "5");
            EXPECT_EQ(skills->row("lore"), nullptr);

            // The head and the log, from the save
            EXPECT_EQ(f.fact<std::string>("life.headline.ageText"), "17.5");
            EXPECT_EQ(life->piece<Label>("date")->spec.text, "YEAR 10");
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
            EXPECT_NE(list->row(&f.ecs, "yard"), nullptr);
            EXPECT_NE(list->row(&f.ecs, "carters"), nullptr);

            // Sworn: no other path, no childhood door, and the Keep's service not before 18
            for (const char* id : {"keep", "collegium", "hand", "study", "run", "letters", "watch", "serve"})
                EXPECT_EQ(list->row(&f.ecs, id), nullptr) << id;

            EXPECT_EQ(f.fact<std::string>("window.keep.state"), "closed");
            EXPECT_EQ(f.fact<std::string>("window.campaign.state"), "upcoming");
            EXPECT_EQ(f.fact<int>("life.next.in"), 42);
            EXPECT_EQ(f.fact<std::string>("life.next.label"), "The proving");

            // The clock says his age to the month, and nothing of the next milestone: its tick is
            // on the track
            EXPECT_EQ(life->piece<LifeClock>("clock")->age.spec.text, "17");
            EXPECT_EQ(life->piece<LifeClock>("clock")->unit.spec.text, "YEARS 6 MONTHS OLD");
            EXPECT_EQ(life->piece<LifeClock>("clock")->spec.nextLabel, "");
            EXPECT_EQ(life->piece<LifeClock>("clock")->spec.nextIn, -1);
            EXPECT_EQ(life->piece<LifeClock>("clock")->spec.endAge, 30.0f);
            EXPECT_EQ(f.fact<std::string>("life.headline.subtitle"), "THE SEVENTEENTH YEAR \xC2\xB7 SUMMER \xC2\xB7 BELLMOOR");
            EXPECT_EQ(life->piece<Label>("dateNote")->spec.text, "MONTH 7 \xC2\xB7 SUMMER \xC2\xB7 BELLMOOR");

            // The proving asks the Warrior no part: no threshold stands on them
            EXPECT_EQ(life->piece<StatLine>("str")->spec.threshold, 0);
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

            f.ecs.sendEvent(ActivitySelectedEvent{"life.activities", "yard"});
            f.settle();

            EXPECT_EQ(f.fact<int>("character.parts.str.projected"), 13);

            auto str = life->piece<StatLine>("str");
            ASSERT_TRUE(str->projected.has_value());
            EXPECT_EQ(str->projected->spec.text, "\xE2\x86\x92 13");

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
        // A work reaching its term changes the whole page: the tiles, the work at hand, the log, the
        // inventory. With the settle phase all of it is in place after the pass that follows; an ecs
        // without one places it link by link, a pass each.
        TEST(lifescene_test, a_term_is_placed_in_one_pass)
        {
            int passes[2] = {0, 0};
            std::string rest[2];

            for (int settles = 0; settles < 2; ++settles)
            {
                MockLogger logger;
                LifeFixture f(settles == 1);

                LifeSceneOptions opt = LifeFixture::mockup();
                opt.fresh = true;
                opt.lives = 2;

                LifeScene* life = f.life(opt);
                ASSERT_NE(life, nullptr);

                f.frames(20);

                // "Run Messages": three months, his first coin and the deed that goes with it
                f.ecs.sendEvent(ActivityActivatedEvent{"life.activities", "messages"});
                f.frames(20);

                ASSERT_EQ(life->save.running, "messages");

                life->onMonth();
                f.frames(20);
                life->onMonth();
                f.frames(20);

                life->onMonth();

                ASSERT_TRUE(life->save.running.empty());

                passes[settles] = f.passesToRest();
                rest[settles] = f.placed();

                std::cout << "[ life     ] a term is placed in " << passes[settles] << " passes " << (settles == 1 ? "with" : "without") << " the settle phase" << std::endl;
            }

            // The deed is taken in the scene's execute, at the end of the first pass: what it writes is
            // placed by the second
            EXPECT_LE(passes[1], 2);
            EXPECT_GT(passes[0], passes[1]);
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

            // Fed for the whole term: only the term writes in the log
            life->save.stats["rations"] = 24;

            const size_t logRows = life->piece<EventLog>("log")->size();

            f.ecs.sendEvent(ActivityActivatedEvent{"life.activities", "yard"});
            f.settle();

            EXPECT_EQ(f.fact<std::string>("activity.running.id"), "yard");
            ASSERT_EQ(running->spec.groups.size(), 1u);
            ASSERT_EQ(running->spec.groups[0].rows.size(), 1u);
            EXPECT_NE(running->row(&f.ecs, "yard"), nullptr);
            EXPECT_FLOAT_EQ(clock->spec.runningMonths, 6.0f);
            EXPECT_EQ(life->piece<ActivityList>("activities")->row(&f.ecs, "yard")->spec.state, ActivityState::Running);

            // The months run on their own while he works
            EXPECT_FALSE(life->paused);

            // The running row is built with its caption and percent: they came in the same update
            ActivityRow* at = running->find(&f.ecs, "yard");
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
            EXPECT_EQ(running->row(&f.ecs, "yard")->spec.percent, 50.0f);

            // The same row in the choice is told of it too, and draws no rule: the choice's rows are a
            // name, a time and a closing, the rule is "At work now"'s
            ActivityRow* chosen = life->piece<ActivityList>("activities")->row(&f.ecs, "yard");
            ASSERT_NE(chosen, nullptr);
            EXPECT_TRUE(chosen->spec.compact);
            EXPECT_EQ(chosen->spec.percent, 50.0f);
            EXPECT_EQ(chosen->spec.caption, f.fact<std::string>("activity.running.caption"));
            EXPECT_FALSE(chosen->progress.has_value());

            for (int i = 0; i < 3; ++i)
                life->onMonth();
            f.settle();

            // At term: the row is done, the months wait for the next choice
            EXPECT_TRUE(life->paused);

            // The stat is the script's, the log has the line
            EXPECT_EQ(f.fact<std::string>("activity.running.id"), "");
            EXPECT_TRUE(running->spec.groups.empty());
            EXPECT_EQ(life->save.stats["str"], 13);
            EXPECT_EQ(life->piece<StatLine>("str")->figure.spec.text, "13");
            EXPECT_EQ(life->piece<EventLog>("log")->size(), logRows + 1);
            EXPECT_EQ(life->save.log.back().kind, LogKind::Gain);
            EXPECT_EQ(life->save.log.back().text, "Train at the Yard");
            EXPECT_EQ(life->piece<ActivityList>("activities")->row(&f.ecs, "yard")->spec.state, ActivityState::Idle);

            // The promise: the lived edge is where the hatch ended
            EXPECT_NEAR(clock->shownAge, 18.0f, 0.001f);
            EXPECT_FLOAT_EQ(clock->spec.runningMonths, 0.0f);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // Work begun runs a month every opt.monthMs, with no key pressed, and nothing passes while
        // he is at nothing.
        TEST(lifescene_test, work_runs_the_months)
        {
            MockLogger logger;
            LifeFixture f;

            LifeScene* life = f.life();
            ASSERT_NE(life, nullptr);

            life->save.stats["rations"] = 24;

            const float age = life->save.age;
            const int yardBefore = life->save.done["yard"];

            auto working = life->piece<Panel>("working");
            ASSERT_NE(working, nullptr);
            ASSERT_TRUE(working->aside.has_value());

            // At nothing, the time goes by without a month
            EXPECT_TRUE(life->paused);
            EXPECT_EQ(working->aside->spec.text, "IDLE");

            f.ecs.sendEvent(TickEvent{life->opt.monthMs * 2.0f});
            f.settle();

            EXPECT_FLOAT_EQ(life->save.age, age);

            f.ecs.sendEvent(ActivityActivatedEvent{"life.activities", "yard"});
            f.settle();

            ASSERT_EQ(life->save.running, "yard");
            EXPECT_EQ(life->save.monthsIn, 0);
            EXPECT_EQ(working->aside->spec.text, "RUNNING");

            // Both rows head for the first month's figure, in the month's time
            for (const char* name : {"running", "activities"})
            {
                ActivityRow* row = life->piece<ActivityList>(name)->find(&f.ecs, "yard");
                ASSERT_NE(row, nullptr) << name;
                EXPECT_FLOAT_EQ(row->spec.percent, 0.0f) << name;
                EXPECT_NEAR(row->spec.glideTo, 100.0f / 6.0f, 0.01f) << name;
                EXPECT_FLOAT_EQ(row->spec.glideMs, life->opt.monthMs) << name;
            }

            // Just short of a month, then the month
            f.ecs.sendEvent(TickEvent{life->opt.monthMs - 1.0f});
            f.settle();

            EXPECT_EQ(life->save.monthsIn, 0);

            f.ecs.sendEvent(TickEvent{1.0f});
            f.settle();

            EXPECT_EQ(life->save.monthsIn, 1);
            EXPECT_NEAR(f.fact<float>("activity.running.percent"), 100.0f / 6.0f, 0.01f);
            EXPECT_NEAR(f.fact<float>("activity.running.toward"), 200.0f / 6.0f, 0.01f);
            EXPECT_NEAR(life->piece<ActivityList>("running")->find(&f.ecs, "yard")->spec.glideTo, 200.0f / 6.0f, 0.01f);

            // Stopped mid-month: the rule holds, the head says so; going on keeps what the month ran
            f.ecs.sendEvent(TickEvent{life->opt.monthMs / 2.0f});
            f.ecs.sendEvent(OnSDLScanCode{SDL_SCANCODE_SPACE, 0});
            f.settle();

            EXPECT_TRUE(life->paused);
            EXPECT_EQ(working->aside->spec.text, "PAUSED \xC2\xB7 SPACE");
            EXPECT_EQ(working->aside->spec.color, "status-loss");
            EXPECT_FLOAT_EQ(life->piece<ActivityList>("running")->find(&f.ecs, "yard")->spec.glideMs, 0.0f);

            f.ecs.sendEvent(OnSDLScanCode{SDL_SCANCODE_SPACE, 0});
            f.settle();

            EXPECT_FALSE(life->paused);
            EXPECT_EQ(working->aside->spec.text, "RUNNING");
            EXPECT_EQ(working->aside->spec.color, "ink-muted");
            EXPECT_FLOAT_EQ(life->piece<ActivityList>("running")->find(&f.ecs, "yard")->spec.glideMs, life->opt.monthMs / 2.0f);

            // The rest of the term in one long frame: it stops at the term, not past it
            f.ecs.sendEvent(TickEvent{life->opt.monthMs * 10.0f});
            f.settle();

            // One term: the mockup's life had done one at the yard already
            EXPECT_TRUE(life->save.running.empty());
            EXPECT_TRUE(life->paused);
            EXPECT_EQ(life->save.done["yard"], yardBefore + 1);
            EXPECT_NEAR(life->save.age, age + 0.5f, 0.001f);
            EXPECT_EQ(working->aside->spec.text, "IDLE");
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // The work he is at keeps its tile, even once it could no longer be begun: it leaves the
        // list when its term ends, not under him.
        TEST(lifescene_test, the_work_he_is_at_keeps_its_tile)
        {
            MockLogger logger;
            LifeFixture f;

            LifeScene* life = f.life();
            ASSERT_NE(life, nullptr);

            auto list = life->piece<ActivityList>("activities");
            ASSERT_NE(list, nullptr);

            // The last month the carters' three months may be begun: 29 years and 9 months
            life->save.age = 29.75f - 1.0f / 12.0f;
            life->save.stats["rations"] = 60;

            life->onMonth();
            f.settle();

            ASSERT_NE(list->find(&f.ecs, "carters"), nullptr);
            EXPECT_EQ(list->find(&f.ecs, "carters")->spec.until, "CLOSES THIS MONTH");

            f.ecs.sendEvent(ActivityActivatedEvent{"life.activities", "carters"});
            f.settle();

            ASSERT_EQ(life->save.running, "carters");

            // A month in, it could not be begun any more: its tile stays, running
            life->onMonth();
            f.settle();

            ASSERT_EQ(life->save.running, "carters");

            ActivityRow* carters = list->find(&f.ecs, "carters");
            ASSERT_NE(carters, nullptr);
            EXPECT_EQ(carters->spec.state, ActivityState::Running);
            EXPECT_EQ(carters->spec.until, "");

            // Its ground says it is the work he is at; the edge is the selection's alone
            EXPECT_EQ(f.ecs.getEntity(carters->ground.id)->get<ThemeComponent>()->element, "activity.kind.running");
            EXPECT_EQ(f.ecs.getEntity(carters->edge.id)->get<ThemeComponent>()->element, "activity.row.edge");

            // At term it is done, and closed: the tile goes
            life->onMonth();
            life->onMonth();
            f.settle();

            EXPECT_TRUE(life->save.running.empty());
            EXPECT_EQ(list->find(&f.ecs, "carters"), nullptr);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // What a work leaves him besides a figure has its row in the ledger: taught by Mara, he is
        // Mara's pupil there.
        TEST(lifescene_test, a_tie_gets_its_row)
        {
            MockLogger logger;
            LifeFixture f;

            LifeScene* life = f.life();
            ASSERT_NE(life, nullptr);

            auto ledger = life->piece<ResourceLedger>("ledger");
            ASSERT_NE(ledger, nullptr);
            EXPECT_EQ(ledger->row("mara_student"), nullptr);

            // As a term's gains would leave it
            life->save.stats["rations"] = 24;
            life->save.stats["mara_student"] = 1;

            life->onMonth();
            f.settle();

            ASSERT_NE(ledger->row("mara_student"), nullptr);
            EXPECT_EQ(ledger->row("mara_student")->name.spec.text, "Mara's pupil");
            EXPECT_EQ(ledger->row("mara_student")->figure.spec.text, "");
            EXPECT_FALSE(f.pos(ledger->row("mara_student")->leader)->isVisible());

            // Its gloss is its name: nothing held to count, no limit to state
            auto registry = f.ecs.getSystem<GlossRegistry>();
            ASSERT_NE(registry, nullptr);
            ASSERT_NE(registry->find("resource/mara_student"), nullptr);
            EXPECT_EQ(registry->find("resource/mara_student")->title, "Mara's pupil");
            EXPECT_TRUE(registry->find("resource/mara_student")->rows.empty());
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // The world's calendar begins at Year 0 with the first life and never rewinds: a life that
        // ends leaves its months counted, and the next one goes on from that date. It is how long
        // the chronicle has been kept.
        TEST(lifescene_test, the_world_date_outlives_a_life)
        {
            MockLogger logger;
            LifeFixture f;

            LifeSceneOptions opt = LifeFixture::mockup();
            opt.fresh = true;
            opt.lives = 2;

            LifeScene* life = f.life(opt);
            ASSERT_NE(life, nullptr);

            auto date = [&]() { return life->piece<Label>("date")->spec.text; };
            auto note = [&]() { return life->piece<Label>("dateNote")->spec.text; };

            EXPECT_EQ(life->save.world, 0);
            EXPECT_EQ(life->save.lives, 2);
            EXPECT_EQ(date(), "YEAR 0");
            EXPECT_EQ(note(), "MONTH 1 \xC2\xB7 WINTER \xC2\xB7 BELLMOOR");

            // Fourteen months: the third month of Year 1
            life->save.stats["rations"] = 60;

            for (int i = 0; i < 14; ++i)
                life->onMonth();
            f.settle();

            EXPECT_EQ(life->save.world, 14);
            EXPECT_EQ(life->rules.world, 14);
            EXPECT_EQ(date(), "YEAR 1");
            EXPECT_EQ(note(), "MONTH 3 \xC2\xB7 WINTER \xC2\xB7 BELLMOOR");
            EXPECT_EQ(f.fact<std::string>("life.headline.date"), "YEAR 1");

            // He dies the month after: the world has that month too
            life->save.stats["rations"] = 0;
            life->save.stats["vit"] = 1;

            life->onMonth();
            f.settle();

            ASSERT_TRUE(life->ended);
            EXPECT_EQ(life->save.world, 15);

            // The next life begins at 7, on the same calendar
            life->beginAgain();
            f.settle();

            EXPECT_FLOAT_EQ(life->save.age, 7.0f);
            EXPECT_EQ(life->save.world, 15);
            EXPECT_EQ(life->save.lives, 3);
            EXPECT_EQ(life->rules.world, 15);
            EXPECT_EQ(life->rules.lives, 3);
            EXPECT_EQ(date(), "YEAR 1");
            EXPECT_EQ(note(), "MONTH 4 \xC2\xB7 SPRING \xC2\xB7 BELLMOOR");

            life->onMonth();
            f.settle();

            EXPECT_EQ(life->save.world, 16);
            EXPECT_EQ(note(), "MONTH 5 \xC2\xB7 SPRING \xC2\xB7 BELLMOOR");

            // A life begun anew by hand keeps it as well
            life->newLife();
            f.settle();

            EXPECT_FLOAT_EQ(life->save.age, 7.0f);
            EXPECT_EQ(life->save.world, 16);
            EXPECT_EQ(note(), "MONTH 5 \xC2\xB7 SPRING \xC2\xB7 BELLMOOR");
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // A row says when it closes, as the world's date of the last month he may begin it: the date
        // stays as the months pass, and turns urgent. No work leaves the list without having said so.
        // What no age closes says nothing.
        TEST(lifescene_test, a_row_says_when_it_closes)
        {
            MockLogger logger;
            LifeFixture f;

            LifeScene* life = f.life();
            ASSERT_NE(life, nullptr);

            auto list = life->piece<ActivityList>("activities");
            ASSERT_NE(list, nullptr);

            // At 17 his prime is far from its end: the carters say nothing
            ASSERT_NE(list->find(&f.ecs, "carters"), nullptr);
            EXPECT_EQ(list->find(&f.ecs, "carters")->spec.until, "");
            EXPECT_FALSE(list->find(&f.ecs, "carters")->until.has_value());

            // A year and a half from 30, a month at a time: three months of work, begun by 29.75
            life->save.age = 28.5f;
            life->save.stats["rations"] = 60;

            life->onMonth();
            f.settle();

            ActivityRow* carters = list->find(&f.ecs, "carters");
            ASSERT_NE(carters, nullptr);
            EXPECT_EQ(carters->spec.until, "CLOSES 10/11");
            EXPECT_FALSE(carters->spec.urgent);
            ASSERT_TRUE(carters->until.has_value());
            EXPECT_EQ(carters->until->spec.text, "CLOSES 10/11");

            life->onMonth();
            f.settle();

            carters = list->find(&f.ecs, "carters");
            ASSERT_NE(carters, nullptr);
            // A month on, the date it closes on is the same date
            EXPECT_EQ(carters->spec.until, "CLOSES 10/11");

            // Half a year left: urgent
            life->save.age = 29.25f - 1.0f / 12.0f;

            life->onMonth();
            f.settle();

            carters = list->find(&f.ecs, "carters");
            ASSERT_NE(carters, nullptr);
            EXPECT_EQ(carters->spec.until, "CLOSES 4/11");
            EXPECT_TRUE(carters->spec.urgent);

            // An old man's work is listed by now, and is never closed
            ActivityRow* tales = list->find(&f.ecs, "tales");
            ASSERT_NE(tales, nullptr);
            EXPECT_EQ(tales->spec.until, "");
            EXPECT_FALSE(tales->until.has_value());

            // At work, the running row says nothing of its closing
            f.ecs.sendEvent(ActivityActivatedEvent{"life.activities", "carters"});
            f.settle();

            ASSERT_EQ(life->save.running, "carters");
            EXPECT_EQ(list->find(&f.ecs, "carters")->spec.until, "");
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // At nothing, no key makes the months run: a month passes by its button, which stands in "At
        // work now" in the running row's place and leaves when work begins.
        TEST(lifescene_test, a_month_passes_by_its_button)
        {
            MockLogger logger;
            LifeFixture f;

            LifeScene* life = f.life();
            ASSERT_NE(life, nullptr);

            life->save.stats["rations"] = 24;

            auto button = life->piece<Button>("skip");
            ASSERT_NE(button, nullptr);
            ASSERT_FALSE(life->named("skip").empty());

            const float age = life->save.age;

            // Idle: the button is there, under the head of the panel
            EXPECT_TRUE(f.pos(life->named("skip"))->visible);
            EXPECT_FALSE(button->spec.disabled);
            EXPECT_NEAR(f.pos(life->named("working"))->height, 83.0f + 36.0f + 12.0f, 0.5f);

            // SPACE does not start the months
            f.ecs.sendEvent(OnSDLScanCode{SDL_SCANCODE_SPACE, 0});
            f.ecs.sendEvent(TickEvent{life->opt.monthMs * 2.0f});
            f.settle();

            EXPECT_TRUE(life->paused);
            EXPECT_FLOAT_EQ(life->save.age, age);

            // The button passes one month, and the months stay stopped
            f.ecs.sendEvent(ButtonActivatedEvent{button->face.id, "life.skip"});
            f.settle();

            EXPECT_NEAR(life->save.age, age + 1.0f / 12.0f, 0.0001f);
            EXPECT_TRUE(life->paused);
            EXPECT_EQ(life->save.stats["rations"], 23);

            f.ecs.sendEvent(TickEvent{life->opt.monthMs * 2.0f});
            f.settle();

            EXPECT_NEAR(life->save.age, age + 1.0f / 12.0f, 0.0001f);

            // At work: the button leaves, the row takes its place, and it passes nothing
            f.ecs.sendEvent(ActivityActivatedEvent{"life.activities", "yard"});
            f.settle();

            ASSERT_EQ(life->save.running, "yard");
            EXPECT_FALSE(f.pos(life->named("skip"))->visible);
            EXPECT_TRUE(button->spec.disabled);

            // The panel holds the row alone: the button's place in the stack is gone with it
            ActivityRow* row = life->piece<ActivityList>("running")->find(&f.ecs, "yard");
            ASSERT_NE(row, nullptr);
            EXPECT_NEAR(f.pos(life->named("working"))->height, 83.0f + f.pos(row->root)->height, 0.5f);

            f.ecs.sendEvent(ButtonActivatedEvent{button->face.id, "life.skip"});
            f.settle();

            EXPECT_EQ(life->save.monthsIn, 0);

            // The term over, it is back
            for (int i = 0; i < 6; ++i)
                life->onMonth();
            f.settle();

            ASSERT_TRUE(life->save.running.empty());
            EXPECT_TRUE(f.pos(life->named("skip"))->visible);
            EXPECT_FALSE(button->spec.disabled);

            // A life that has worked is not led: no line says what to do
            ASSERT_FALSE(life->named("guide").empty());
            EXPECT_FALSE(f.pos(life->named("guide"))->visible);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // A life that has done nothing yet is led to its first work: the first of the list is chosen
        // for him, Begin is the button of "At work now" under a line that says what to do, and there
        // is no month to pass before a work is begun. The first term done, the page is as it always was.
        TEST(lifescene_test, a_new_life_is_led_to_its_first_work)
        {
            MockLogger logger;
            LifeFixture f;

            LifeSceneOptions opt = LifeFixture::mockup();
            opt.fresh = true;
            opt.lives = 2;

            LifeScene* life = f.life(opt);
            ASSERT_NE(life, nullptr);

            f.frames(12);

            auto list = life->piece<ActivityList>("activities");
            ASSERT_NE(list, nullptr);
            ASSERT_FALSE(life->named("guide").empty());
            ASSERT_FALSE(life->named("begin").empty());

            // The carters feed him: the first row of the table, chosen and lit
            EXPECT_EQ(list->selected(), "carters");
            EXPECT_TRUE(f.pos(life->named("guide"))->visible);
            EXPECT_TRUE(f.pos(life->named("begin"))->visible);
            EXPECT_FALSE(f.pos(life->named("skip"))->visible);
            EXPECT_TRUE(life->piece<Button>("skip")->spec.disabled);

            // The page chose it, not the player
            EXPECT_EQ(life->save.picks, 0);

            // Begin begins it
            f.ecs.sendEvent(ButtonActivatedEvent{life->piece<Button>("begin")->face.id, "life.begin"});
            f.settle();

            ASSERT_EQ(life->save.running, "carters");
            EXPECT_EQ(life->save.begun, 1);
            EXPECT_EQ(list->selected(), "");
            EXPECT_FALSE(f.pos(life->named("guide"))->visible);

            // Its term over, he is at nothing as any life is: a month to pass, nothing chosen, no line
            for (int i = 0; i < 3; ++i)
                life->onMonth();
            f.frames(12);

            ASSERT_TRUE(life->save.running.empty());
            EXPECT_EQ(life->save.done["carters"], 1);
            EXPECT_EQ(list->selected(), "");
            EXPECT_TRUE(f.pos(life->named("skip"))->visible);
            EXPECT_FALSE(f.pos(life->named("begin"))->visible);
            EXPECT_FALSE(f.pos(life->named("guide"))->visible);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // The three ways into a class stand in the list of a boy of 7, locked on his age.
        TEST(lifescene_test, the_classes_show_from_the_first_day)
        {
            MockLogger logger;
            LifeFixture f;

            LifeSceneOptions opt = LifeFixture::mockup();
            opt.fresh = true;
            opt.lives = 2;

            LifeScene* life = f.life(opt);
            ASSERT_NE(life, nullptr);

            f.frames(12);

            auto list = life->piece<ActivityList>("activities");
            ASSERT_NE(list, nullptr);

            for (const char* id : {"keep", "collegium", "hand"})
            {
                ActivityRow* way = list->find(&f.ecs, id);

                ASSERT_NE(way, nullptr) << id;
                EXPECT_EQ(way->spec.state, ActivityState::Locked) << id;
                EXPECT_EQ(way->spec.until, "NEEDS AGE 16") << id;
            }

            // The way left to who takes none of them is not a boy's to see
            EXPECT_EQ(list->find(&f.ecs, "greenwood"), nullptr);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // The month the last class closes on a man of none, the Greenwood comes into his list, and the
        // log says so, once.
        TEST(lifescene_test, the_greenwood_opens_when_the_classes_have_closed)
        {
            MockLogger logger;
            LifeFixture f;

            LifeSceneOptions opt = LifeFixture::mockup();
            opt.fresh = true;
            opt.lives = 2;

            LifeScene* life = f.life(opt);
            ASSERT_NE(life, nullptr);

            f.frames(12);

            auto list = life->piece<ActivityList>("activities");
            ASSERT_NE(list, nullptr);

            auto said = [&]() {
                int lines = 0;

                for (const auto& entry : life->save.log)
                {
                    if (entry.text.find("the Greenwood takes who is left") != std::string::npos)
                    {
                        EXPECT_EQ(entry.kind, LogKind::Milestone);
                        ++lines;
                    }
                }

                return lines;
            };

            // 19 and a half: the Hidden Hand may still be begun this month
            life->save.age = 19.5f - 1.0f / 12.0f;
            life->save.stats["rations"] = 30;
            life->onMonth();
            f.frames(12);

            EXPECT_NE(list->find(&f.ecs, "hand"), nullptr);
            EXPECT_EQ(list->find(&f.ecs, "greenwood"), nullptr);
            EXPECT_EQ(said(), 0);

            // The month after it may not: the Greenwood is his to take
            life->onMonth();
            f.frames(12);

            EXPECT_EQ(list->find(&f.ecs, "hand"), nullptr);
            ASSERT_NE(list->find(&f.ecs, "greenwood"), nullptr);
            EXPECT_EQ(list->find(&f.ecs, "greenwood")->spec.state, ActivityState::Idle);
            EXPECT_EQ(said(), 1);

            // Said once
            life->onMonth();
            f.frames(12);

            EXPECT_EQ(said(), 1);

            // Taken: he is at it, and a year on he is of the Greenwood
            f.ecs.sendEvent(ActivityActivatedEvent{"life.activities", "greenwood"});
            f.settle();

            ASSERT_EQ(life->save.running, "greenwood");

            for (int i = 0; i < 12; ++i)
                life->onMonth();
            f.frames(12);

            ASSERT_TRUE(life->save.running.empty());
            EXPECT_EQ(life->save.aim, "renegade");
            EXPECT_EQ(life->save.stats["renegade"], 1);
            EXPECT_EQ(list->find(&f.ecs, "greenwood"), nullptr);
            EXPECT_NE(list->find(&f.ecs, "poach"), nullptr);
            EXPECT_EQ(said(), 1);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // The save counts what the player pressed in this life, for the analytics: the tiles chosen,
        // the works begun, the things done on the spot and the months passed by the button. A new
        // life counts from nothing.
        TEST(lifescene_test, the_save_counts_what_was_pressed)
        {
            MockLogger logger;
            LifeFixture f;

            LifeScene* life = f.life();
            ASSERT_NE(life, nullptr);

            EXPECT_EQ(life->save.picks, 0);
            EXPECT_EQ(life->save.begun, 0);
            EXPECT_EQ(life->save.atOnce, 0);
            EXPECT_EQ(life->save.skips, 0);

            f.ecs.sendEvent(ButtonActivatedEvent{life->piece<Button>("skip")->face.id, "life.skip"});
            f.settle();

            EXPECT_EQ(life->save.skips, 1);

            // Chosen, then another, then none: two tiles were chosen
            f.ecs.sendEvent(ActivitySelectedEvent{"life.activities", "yard"});
            f.ecs.sendEvent(ActivitySelectedEvent{"life.activities", "carters"});
            f.ecs.sendEvent(ActivitySelectedEvent{"life.activities", ""});
            f.settle();

            EXPECT_EQ(life->save.picks, 2);

            f.ecs.sendEvent(ActivityActivatedEvent{"life.activities", "yard"});
            f.settle();

            ASSERT_EQ(life->save.running, "yard");
            EXPECT_EQ(life->save.begun, 1);

            // Bought while he works: done on the spot, and no work begun for it
            f.ecs.sendEvent(ActivityActivatedEvent{"life.activities", "buy.rations"});
            f.settle();

            EXPECT_EQ(life->save.atOnce, 1);
            EXPECT_EQ(life->save.begun, 1);

            // At work the button passes nothing, and counts nothing
            f.ecs.sendEvent(ButtonActivatedEvent{life->piece<Button>("skip")->face.id, "life.skip"});
            f.settle();

            EXPECT_EQ(life->save.skips, 1);

            life->newLife();
            f.settle();

            EXPECT_EQ(life->save.picks, 0);
            EXPECT_EQ(life->save.begun, 0);
            EXPECT_EQ(life->save.atOnce, 0);
            EXPECT_EQ(life->save.skips, 0);
            EXPECT_EQ(life->save.lives, 2);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // Work that leaves him unfed stops the months at the first month that takes from his
        // Vitality: the head of "At work now" says it is paused, the log says why and which key
        // goes on.
        TEST(lifescene_test, a_failing_part_pauses_the_work)
        {
            MockLogger logger;
            LifeFixture f;

            LifeScene* life = f.life();
            ASSERT_NE(life, nullptr);

            life->save.stats["rations"] = 0;

            f.ecs.sendEvent(ActivityActivatedEvent{"life.activities", "yard"});
            f.settle();

            ASSERT_EQ(life->save.running, "yard");
            ASSERT_FALSE(life->paused);

            const size_t logEntries = life->save.log.size();

            f.ecs.sendEvent(TickEvent{life->opt.monthMs});
            f.settle();

            EXPECT_TRUE(life->paused);
            EXPECT_EQ(life->save.running, "yard");

            auto working = life->piece<Panel>("working");
            ASSERT_NE(working, nullptr);
            ASSERT_TRUE(working->aside.has_value());
            EXPECT_EQ(working->aside->spec.text, "PAUSED \xC2\xB7 SPACE");

            // The month's own lines, then the pause's, last
            ASSERT_GT(life->save.log.size(), logEntries);
            EXPECT_EQ(life->save.log.back().kind, LogKind::Note);
            EXPECT_EQ(life->save.log.back().text, "Vitality is failing: the months stop. SPACE goes on");

            // Stopped, no month passes
            const int monthsIn = life->save.monthsIn;

            f.ecs.sendEvent(TickEvent{life->opt.monthMs * 3.0f});
            f.settle();

            EXPECT_EQ(life->save.monthsIn, monthsIn);

            // SPACE goes on; a second failing month does not stop it again, nor write the line again
            f.ecs.sendEvent(OnSDLScanCode{SDL_SCANCODE_SPACE, 0});
            f.settle();

            EXPECT_FALSE(life->paused);

            const size_t afterPause = life->save.log.size();

            f.ecs.sendEvent(TickEvent{life->opt.monthMs});
            f.settle();

            EXPECT_FALSE(life->paused);
            EXPECT_EQ(life->save.monthsIn, monthsIn + 1);

            for (size_t i = afterPause; i < life->save.log.size(); ++i)
                EXPECT_EQ(life->save.log[i].text.find("SPACE goes on"), std::string::npos);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // His letters bring the chapel: a row that was not listed comes, ready, when a term ends.
        TEST(lifescene_test, a_term_that_unlocks_an_activity)
        {
            MockLogger logger;
            LifeFixture f;

            LifeSceneOptions opt = LifeFixture::mockup();
            opt.fresh = true;
            opt.lives = 2;

            LifeScene* life = f.life(opt);
            ASSERT_NE(life, nullptr);

            // A month short of 9: the month that passes makes him old enough for the chapel
            life->save.age = 9.0f - 1.0f / 12.0f;
            life->onMonth();
            f.settle();

            // Of age, but without his letters the chapel is out of reach
            auto list = life->piece<ActivityList>("activities");
            ASSERT_NE(list, nullptr);
            EXPECT_EQ(list->row(&f.ecs, "chapel"), nullptr);

            f.ecs.sendEvent(ActivityActivatedEvent{"life.activities", "letters"});
            f.settle();

            for (int i = 0; i < 6; ++i)
            {
                life->onMonth();
                f.settle();
            }

            f.settle();

            EXPECT_EQ(life->save.running, "");
            EXPECT_EQ(life->save.stats["letters"], 1);
            ASSERT_NE(list->row(&f.ecs, "chapel"), nullptr);
            EXPECT_EQ(list->row(&f.ecs, "chapel")->spec.state, ActivityState::Idle);
            EXPECT_EQ(life->piece<ResourceLedger>("skills")->row("letters")->figure.spec.text, "1");
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // At 9 the smithy is listed, locked at Strength 6 of 8; the mill raises it to 7, and the row
        // says 7 / 8.
        TEST(lifescene_test, a_locked_row_follows_what_it_asks)
        {
            MockLogger logger;
            LifeFixture f;

            LifeSceneOptions opt = LifeFixture::mockup();
            opt.fresh = true;
            opt.lives = 2;

            LifeScene* life = f.life(opt);
            ASSERT_NE(life, nullptr);

            life->save.age = 9.0f - 1.0f / 12.0f;
            life->onMonth();
            f.settle();

            auto list = life->piece<ActivityList>("activities");
            ASSERT_NE(list, nullptr);
            ASSERT_NE(list->row(&f.ecs, "smithy"), nullptr);
            EXPECT_EQ(list->row(&f.ecs, "smithy")->spec.state, ActivityState::Locked);
            ASSERT_EQ(list->row(&f.ecs, "smithy")->spec.requirements[0].current, 6);

            f.ecs.sendEvent(ActivityActivatedEvent{"life.activities", "mill"});
            f.settle();

            for (int i = 0; i < 6; ++i)
            {
                life->onMonth();
                f.settle();
            }

            f.settle();

            auto row = list->row(&f.ecs, "smithy");
            ASSERT_NE(row, nullptr);

            EXPECT_EQ(life->save.stats["str"], 7);
            EXPECT_EQ(row->spec.state, ActivityState::Locked);
            EXPECT_EQ(row->spec.requirements[0].label, "Strength");
            EXPECT_EQ(row->spec.requirements[0].current, 7);
            EXPECT_EQ(row->spec.requirements[0].needed, 8);

            // The row itself lists nothing: its gloss says what it asks, with what he has now
            EXPECT_FALSE(row->reqs.has_value());

            auto registry = f.ecs.getSystem<GlossRegistry>();
            ASSERT_NE(registry, nullptr);

            const GlossSpec* smithy = registry->find("activity/smithy");
            ASSERT_NE(smithy, nullptr);

            // Under IT ASKS: the smithy brings Strength too, under the same name in its own section
            bool asking = false;
            bool asked = false;

            for (const auto& r : smithy->rows)
            {
                if (r.heading)
                {
                    asking = r.label == "IT ASKS";
                }
                else if (asking and r.label == "Strength")
                {
                    EXPECT_EQ(r.value, "7 / 8");
                    EXPECT_EQ(r.tone, "loss");
                    asked = true;
                }
            }

            EXPECT_TRUE(asked);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // A fresh life shows no skill and holds nothing but rations; the first coin and the first
        // point of a skill each bring their row.
        TEST(lifescene_test, first_earnings_get_their_rows)
        {
            MockLogger logger;
            LifeFixture f;

            LifeSceneOptions opt = LifeFixture::mockup();
            opt.fresh = true;
            opt.lives = 2;

            LifeScene* life = f.life(opt);
            ASSERT_NE(life, nullptr);

            auto ledger = life->piece<ResourceLedger>("ledger");
            auto skills = life->piece<ResourceLedger>("skills");
            ASSERT_NE(ledger, nullptr);
            ASSERT_NE(skills, nullptr);

            EXPECT_EQ(ledger->row("coin"), nullptr);

            for (const char* id : {"arms", "discipline", "letters", "lore", "arcana", "stealth", "guile", "renown"})
                EXPECT_EQ(skills->row(id), nullptr) << id;

            f.ecs.sendEvent(ActivityActivatedEvent{"life.activities", "carters"});
            f.settle();

            for (int i = 0; i < 3; ++i)
            {
                life->onMonth();
                f.settle();
            }

            f.settle();

            ASSERT_NE(ledger->row("coin"), nullptr);
            EXPECT_EQ(ledger->row("coin")->figure.spec.text, "6");

            f.ecs.sendEvent(ActivityActivatedEvent{"life.activities", "letters"});
            f.settle();

            for (int i = 0; i < 6; ++i)
            {
                life->onMonth();
                f.settle();
            }

            f.settle();

            ASSERT_NE(skills->row("letters"), nullptr);
            EXPECT_EQ(skills->row("letters")->figure.spec.text, "1");
            EXPECT_EQ(skills->row("arms"), nullptr);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // The Watch takes a carrier once: its row leaves the list when the term ends.
        TEST(lifescene_test, a_spent_activity_leaves_the_list)
        {
            MockLogger logger;
            LifeFixture f;

            LifeSceneOptions opt = LifeFixture::mockup();
            opt.fresh = true;
            opt.lives = 2;

            LifeScene* life = f.life(opt);
            ASSERT_NE(life, nullptr);

            life->save.age = 10.0f - 1.0f / 12.0f;
            life->save.stats["str"] = 8;
            life->save.stats["rations"] = 24;
            life->onMonth();
            f.settle();

            auto list = life->piece<ActivityList>("activities");
            ASSERT_NE(list, nullptr);
            ASSERT_NE(list->find(&f.ecs, "watch"), nullptr);

            f.ecs.sendEvent(ActivityActivatedEvent{"life.activities", "watch"});
            f.settle();

            for (int i = 0; i < 12; ++i)
            {
                life->onMonth();
                f.settle();
            }

            f.settle();

            EXPECT_EQ(life->save.running, "");
            EXPECT_EQ(life->save.done["watch"], 1);
            EXPECT_EQ(list->find(&f.ecs, "watch"), nullptr);
            EXPECT_NE(list->find(&f.ecs, "carters"), nullptr);
            EXPECT_EQ(f.fact<int>("done.watch"), 1);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // Two terms at the yard: the third takes nine months, and once three are done the row asks
        // twelve.
        TEST(lifescene_test, repetition_upgrades_an_activity)
        {
            MockLogger logger;
            LifeFixture f;

            LifeScene* life = f.life();
            ASSERT_NE(life, nullptr);

            life->save.stats["rations"] = 24;

            auto list = life->piece<ActivityList>("activities");
            ASSERT_NE(list, nullptr);
            ASSERT_EQ(list->row(&f.ecs, "yard")->spec.months, 6);

            life->save.done["yard"] = 2;
            life->rules.done = life->save.terms();

            const size_t logEntries = life->save.log.size();

            f.ecs.sendEvent(ActivityActivatedEvent{"life.activities", "yard"});
            f.settle();

            EXPECT_FLOAT_EQ(f.fact<float>("activity.running.months"), 9.0f);

            for (int i = 0; i < 9; ++i)
            {
                life->onMonth();
                f.settle();
            }

            f.settle();

            auto row = list->row(&f.ecs, "yard");
            ASSERT_NE(row, nullptr);

            EXPECT_EQ(life->save.running, "");
            EXPECT_EQ(life->save.done["yard"], 3);
            EXPECT_EQ(row->spec.months, 12);

            // The term's own line
            EXPECT_EQ(life->save.log.size(), logEntries + 1);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // Work that feeds him leaves his rations be, and the ledger says so; idle, he eats again.
        TEST(lifescene_test, work_with_meals_spares_the_rations)
        {
            MockLogger logger;
            LifeFixture f;

            LifeScene* life = f.life();
            ASSERT_NE(life, nullptr);

            auto ledger = life->piece<ResourceLedger>("ledger");
            ASSERT_NE(ledger, nullptr);
            ASSERT_NE(ledger->row("rations"), nullptr);

            f.ecs.sendEvent(ActivityActivatedEvent{"life.activities", "carters"});
            f.settle();

            EXPECT_EQ(ledger->row("rations")->spec.rate, "");

            for (int i = 0; i < 3; ++i)
            {
                life->onMonth();
                f.settle();
            }

            EXPECT_EQ(life->save.running, "");
            EXPECT_EQ(life->save.stats["rations"], 3);
            EXPECT_EQ(life->save.stats["coin"], 46 + 6);
            EXPECT_EQ(ledger->row("rations")->spec.rate, "\xE2\x88\x92" "1 / mo");

            life->onMonth();
            f.settle();

            EXPECT_EQ(life->save.stats["rations"], 2);
            EXPECT_EQ(ledger->row("rations")->figure.spec.text, "2/60");
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // Rations have a limit, and a purchase begun with room left may carry him past it. Under
        // it, at it and past it the row that sells them goes through every state it has (it asks
        // for room, is locked, is chosen and confirmed all the same) and the months pass over it.
        TEST(lifescene_test, a_holding_at_its_limit)
        {
            for (int settles = 0; settles < 2; ++settles)
            {
                MockLogger logger;
                LifeFixture f(settles == 1);

                LifeScene* life = f.life();
                ASSERT_NE(life, nullptr);

                auto list = life->piece<ActivityList>("activities");
                auto ledger = life->piece<ResourceLedger>("ledger");
                ASSERT_NE(list, nullptr);
                ASSERT_NE(ledger, nullptr);

                life->save.stats["coin"] = 500;

                for (int rations : {50, 53, 54, 55, 56, 58, 59, 60, 61, 62, 66, 120, 60, 54, 1})
                {
                    // A month tells the page of it (and eats one)
                    life->save.stats["rations"] = rations;
                    life->onMonth();
                    f.settle();

                    // Under the mouse, chosen, begun twice, and a month over it
                    f.ecs.sendEvent(ActivityHoveredEvent{"life.activities", "buy.rations"});
                    f.settle();
                    f.ecs.sendEvent(ActivitySelectedEvent{"life.activities", "buy.rations"});
                    f.settle();

                    // What it costs is its gloss's to say: his purse shows what he has
                    ASSERT_NE(ledger->row("coin"), nullptr);
                    EXPECT_EQ(ledger->row("coin")->spec.value, std::to_string(life->save.stats["coin"]));

                    f.ecs.sendEvent(ActivityActivatedEvent{"life.activities", "buy.rations"});
                    f.settle();
                    f.ecs.sendEvent(ActivityActivatedEvent{"life.activities", "buy.rations"});
                    f.settle();
                    f.ecs.sendEvent(ActivityHoveredEvent{"life.activities", ""});
                    life->onMonth();
                    f.settle();

                    // Begun with any room left it brings all it brings, past the limit: full, it is refused
                    EXPECT_LE(life->save.stats["rations"], std::max(59 + 6, rations)) << rations;
                    ASSERT_NE(ledger->row("rations"), nullptr);
                    ASSERT_NE(list->find(&f.ecs, "buy.rations"), nullptr) << rations;

                    // In the column, over the most he can hold, past it too
                    EXPECT_EQ(ledger->row("rations")->figure.spec.text, std::to_string(life->save.stats["rations"]) + "/60");
                }
            }
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // Rations go down a month at a time; out of them, his Vitality pays.
        TEST(lifescene_test, a_holding_runs_out)
        {
            MockLogger logger;
            LifeFixture f;

            LifeScene* life = f.life();
            ASSERT_NE(life, nullptr);

            auto ledger = life->piece<ResourceLedger>("ledger");
            ASSERT_NE(ledger, nullptr);
            ASSERT_NE(ledger->row("rations"), nullptr);

            life->save.stats["rations"] = 2;

            const int vit = life->save.stats["vit"];
            const size_t logEntries = life->save.log.size();

            life->onMonth();
            f.settle();

            EXPECT_EQ(life->save.stats["rations"], 1);
            EXPECT_EQ(ledger->row("rations")->figure.spec.text, "1/60");
            EXPECT_EQ(life->save.log.size(), logEntries);

            life->onMonth();
            f.settle();

            EXPECT_EQ(life->save.stats["rations"], 0);
            EXPECT_EQ(life->save.stats["vit"], vit);
            ASSERT_EQ(life->save.log.size(), logEntries + 1);
            EXPECT_EQ(life->save.log.back().kind, LogKind::Loss);

            life->onMonth();
            f.settle();

            EXPECT_EQ(life->save.stats["vit"], vit - 1);
            EXPECT_EQ(life->save.log.size(), logEntries + 1);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // A month that takes from his Vitality stops the months and turns it red for a moment. With
        // none left the life is lost: a new one begins at 7 with its rations, stopped, and says why.
        TEST(lifescene_test, a_life_ends_when_vitality_is_spent)
        {
            MockLogger logger;
            LifeFixture f;

            LifeScene* life = f.life();
            ASSERT_NE(life, nullptr);

            life->paused = false;
            life->save.stats["rations"] = 0;
            life->save.stats["vit"] = 2;

            life->onMonth();
            f.settle();

            // Still his life, a month older, and in danger: the months stopped, his life in red
            EXPECT_EQ(life->save.stats["vit"], 1);
            EXPECT_GT(life->save.age, 17.5f);
            EXPECT_TRUE(life->paused);

            auto vit = life->piece<StatLine>("vit");
            ASSERT_NE(vit, nullptr);
            EXPECT_TRUE(vit->spec.alert);
            EXPECT_EQ(vit->figure.spec.color, "status-loss");
            EXPECT_FALSE(life->piece<StatLine>("str")->spec.alert);

            // The red stays past the flash: with nothing to eat the month to come takes from it again
            f.ecs.sendEvent(TickEvent{2000.0f});
            f.settle();

            EXPECT_TRUE(life->piece<StatLine>("vit")->spec.alert);
            EXPECT_EQ(life->piece<StatLine>("vit")->figure.spec.color, "status-loss");
            EXPECT_TRUE(life->paused);

            life->paused = false;

            life->onMonth();
            f.settle();

            // Over: its ending is up, and the next life waits for it to be left
            EXPECT_TRUE(life->ended);
            EXPECT_GT(life->save.age, 17.5f);

            life->beginAgain();
            f.settle();

            EXPECT_FALSE(life->ended);
            EXPECT_FLOAT_EQ(life->save.age, 7.0f);
            EXPECT_FLOAT_EQ(f.fact<float>("life.age"), 7.0f);
            EXPECT_EQ(life->save.stats["vit"], 8);
            EXPECT_TRUE(life->save.running.empty());
            EXPECT_EQ(life->save.stats["rations"], 12);
            EXPECT_TRUE(life->paused);
            EXPECT_FALSE(life->piece<StatLine>("vit")->spec.alert);

            // Childhood, then what became of the last life
            ASSERT_EQ(happened(life).size(), 2u);
            EXPECT_EQ(happened(life).back().kind, LogKind::Loss);
            EXPECT_NE(happened(life).back().text.find("life ended"), std::string::npos);

            auto ledger = life->piece<ResourceLedger>("ledger");
            ASSERT_NE(ledger, nullptr);
            ASSERT_NE(ledger->row("rations"), nullptr);
            EXPECT_EQ(ledger->row("rations")->figure.spec.text, "12/60");
            EXPECT_EQ(ledger->row("coin"), nullptr);

            // The new life goes on as any other
            life->onMonth();
            f.settle();

            EXPECT_GT(life->save.age, 7.0f);
            EXPECT_EQ(life->save.stats["vit"], 8);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // What was taken from his Vitality comes back, one every two months, up to the most it can
        // be: the hatch on its line is what is still to mend.
        TEST(lifescene_test, vitality_mends_toward_its_most)
        {
            MockLogger logger;
            LifeFixture f;

            LifeScene* life = f.life();
            ASSERT_NE(life, nullptr);

            life->save.stats["rations"] = 24;

            auto vit = [&]() { return life->piece<StatLine>("vit"); };
            ASSERT_NE(vit(), nullptr);

            // Whole: no ghost
            EXPECT_EQ(f.fact<int>("character.parts.vit.projected"), -1);
            EXPECT_FALSE(vit()->projected.has_value());

            life->save.stats["vit"] = 8;

            life->onMonth();
            f.settle();

            EXPECT_EQ(life->save.stats["vit"], 8);
            EXPECT_EQ(vit()->spec.value, 8);
            EXPECT_EQ(f.fact<int>("character.parts.vit.projected"), 10);
            ASSERT_TRUE(vit()->projected.has_value());
            EXPECT_EQ(vit()->spec.projected, 10);
            EXPECT_FALSE(vit()->spec.alert);

            life->onMonth();
            f.settle();

            EXPECT_EQ(life->save.stats["vit"], 9);
            EXPECT_EQ(vit()->spec.value, 9);
            EXPECT_EQ(vit()->spec.projected, 10);

            life->onMonth();
            life->onMonth();
            f.settle();

            // Whole again: the most it can be did not move, and it stops there
            EXPECT_EQ(life->save.stats["vit"], 10);
            EXPECT_EQ(life->save.stats["vitmax"], 10);
            EXPECT_EQ(f.fact<int>("character.parts.vit.projected"), -1);
            EXPECT_FALSE(vit()->projected.has_value());

            life->onMonth();
            life->onMonth();
            f.settle();

            EXPECT_EQ(life->save.stats["vit"], 10);

            // The gloss says both
            auto registry = f.ecs.getSystem<GlossRegistry>();
            ASSERT_NE(registry, nullptr);

            const GlossSpec* gloss = registry->find("parts/vit");
            ASSERT_NE(gloss, nullptr);

            bool most = false;

            for (const auto& r : gloss->rows)
            {
                if (r.label == "At most")
                {
                    EXPECT_EQ(r.value, "10");
                    most = true;
                }
            }

            EXPECT_TRUE(most);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // Hovering what he holds says what it is for: its limit, what it uses, what it brings.
        TEST(lifescene_test, holdings_explain_themselves)
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

            const GlossSpec* rations = registry->find("resource/rations");
            ASSERT_NE(rations, nullptr);
            EXPECT_EQ(rations->title, "Rations");
            EXPECT_FALSE(rations->text.empty());
            // What he holds over the most he can, on one row
            EXPECT_EQ(rowOf(rations, "Held"), "3/60");
            EXPECT_EQ(rowOf(rations, "Limit"), "<none>");
            EXPECT_EQ(rowOf(rations, "Lasts"), "3 mo");

            const GlossSpec* coin = registry->find("resource/coin");
            ASSERT_NE(coin, nullptr);
            EXPECT_EQ(rowOf(coin, "Held"), "46");
            EXPECT_EQ(rowOf(coin, "Limit"), "<none>");

            // What the rules do not follow still says what he holds of it
            const GlossSpec* iron = registry->find("resource/iron");
            ASSERT_NE(iron, nullptr);
            EXPECT_EQ(iron->title, "Iron");
            EXPECT_EQ(rowOf(iron, "Held"), "6");

            // The rows carry them, and follow the months
            auto ledger = life->piece<ResourceLedger>("ledger");
            ASSERT_NE(ledger, nullptr);
            ASSERT_NE(ledger->row("rations"), nullptr);
            ASSERT_TRUE(ledger->row("rations")->line->has<TooltipComponent>());
            EXPECT_EQ(ledger->row("rations")->line->get<TooltipComponent>()->text, "resource/rations");

            life->onMonth();
            f.settle();

            EXPECT_EQ(rowOf(registry->find("resource/rations"), "Held"), "2/60");

            // A holding earned later gets its gloss with its row
            life->save.stats["favors"] = 2;

            life->onMonth();
            f.settle();

            ASSERT_NE(ledger->row("favors"), nullptr);

            const GlossSpec* favors = registry->find("resource/favors");
            ASSERT_NE(favors, nullptr);
            EXPECT_EQ(favors->title, "Favors");
            EXPECT_EQ(rowOf(favors, "Held"), "2");
            EXPECT_FALSE(favors->text.empty());
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // A row says how often it was done, and what is left of a limited one; doing it counts.
        TEST(lifescene_test, a_row_counts_its_uses)
        {
            MockLogger logger;
            LifeFixture f;

            LifeSceneOptions opt = LifeFixture::mockup();
            opt.fresh = true;
            opt.lives = 2;

            LifeScene* life = f.life(opt);
            ASSERT_NE(life, nullptr);

            // At 8, with coin for the market, which he knows
            life->save.age = 8.0f - 1.0f / 12.0f;
            life->save.stats["coin"] = 10;
            life->save.stats["market_known"] = 1;
            life->onMonth();
            f.settle();

            auto list = life->piece<ActivityList>("activities");
            ASSERT_NE(list, nullptr);

            auto registry = f.ecs.getSystem<GlossRegistry>();
            ASSERT_NE(registry, nullptr);

            // The row keeps the count and writes none: it stands in its gloss, beside the name
            ActivityRow* roam = list->row(&f.ecs, "roam");
            ASSERT_NE(roam, nullptr);
            EXPECT_EQ(roam->spec.count, "DONE 0 \xC2\xB7 2 LEFT");
            EXPECT_FALSE(roam->rank.has_value());

            ASSERT_NE(registry->find("activity/roam"), nullptr);
            EXPECT_EQ(registry->find("activity/roam")->aside, "0/2");

            ActivityRow* buy = list->row(&f.ecs, "buy.rations");
            ASSERT_NE(buy, nullptr);
            EXPECT_EQ(buy->spec.count, "DONE 0");
            EXPECT_FALSE(buy->rank.has_value());
            EXPECT_EQ(registry->find("activity/buy.rations")->aside, "0");

            // Done at once: the same row, one more
            f.ecs.sendEvent(ActivityActivatedEvent{"life.activities", "buy.rations"});
            f.settle();

            buy = list->row(&f.ecs, "buy.rations");
            ASSERT_NE(buy, nullptr);
            EXPECT_EQ(buy->spec.count, "DONE 1");
            EXPECT_EQ(f.fact<std::string>("activity.buy.rations.count"), "DONE 1");
            EXPECT_EQ(registry->find("activity/buy.rations")->aside, "1");
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // The first coin of a fresh life is a deed: one line in the log, once, and it is kept.
        TEST(lifescene_test, a_deed_is_reached_once)
        {
            MockLogger logger;
            LifeFixture f;

            LifeSceneOptions opt = LifeFixture::mockup();
            opt.fresh = true;
            opt.lives = 2;

            LifeScene* life = f.life(opt);
            ASSERT_NE(life, nullptr);

            EXPECT_TRUE(deedsOf(life).empty());

            f.ecs.sendEvent(ActivityActivatedEvent{"life.activities", "carters"});
            f.settle();

            for (int i = 0; i < 3; ++i)
            {
                life->onMonth();
                f.settle();
            }

            f.frames(8);

            ASSERT_EQ(deedsOf(life).size(), 1u);
            EXPECT_EQ(deedsOf(life)[0], "first.coin");

            size_t lines = 0;

            for (const auto& entry : life->save.log)
            {
                if (entry.text == "Held a coin of his own for the first time")
                    ++lines;
            }

            EXPECT_EQ(lines, 1u);
            EXPECT_EQ(life->piece<EventLog>("log")->size(), life->save.log.size());

            // More coin: nothing more to reach
            f.ecs.sendEvent(ActivityActivatedEvent{"life.activities", "carters"});
            f.settle();

            for (int i = 0; i < 3; ++i)
            {
                life->onMonth();
                f.settle();
            }

            f.frames(8);

            EXPECT_EQ(deedsOf(life).size(), 1u);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // A deed reached is in the save: the life loaded again does not reach it a second time.
        TEST(lifescene_test, a_deed_survives_the_save)
        {
            MockLogger logger;
            LifeFixture f;

            const std::string path = "save/test_life_deeds.sz";
            std::remove(path.c_str());

            LifeSceneOptions opt = LifeFixture::mockup();
            opt.fresh = true;
            opt.lives = 2;
            opt.noSave = false;
            opt.savePath = path;

            LifeScene* life = f.life(opt);
            ASSERT_NE(life, nullptr);

            f.ecs.sendEvent(ActivityActivatedEvent{"life.activities", "carters"});
            f.settle();

            for (int i = 0; i < 3; ++i)
            {
                life->onMonth();
                f.settle();
            }

            f.frames(8);

            ASSERT_EQ(deedsOf(life).size(), 1u);
            ASSERT_TRUE(life->saveNow());

            const size_t kept = life->save.achieved.size();

            const size_t writtenLog = life->save.log.size() - 1;   // saveNow's own line came after
            const int coin = life->save.stats["coin"];

            f.load<EmptyScene>();

            opt.fresh = false;

            LifeScene* again = f.life(opt);
            ASSERT_NE(again, nullptr);

            f.frames(8);

            EXPECT_EQ(deedsOf(again).size(), 1u);
            EXPECT_EQ(again->save.achieved.size(), kept);
            EXPECT_EQ(again->save.done["carters"], 1);
            EXPECT_EQ(again->save.stats["coin"], coin);
            EXPECT_EQ(again->save.log.size(), writtenLog);

            std::remove(path.c_str());
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // Buying rations takes coin and no time: done on confirm, even while he is at work.
        TEST(lifescene_test, an_activity_done_at_once)
        {
            MockLogger logger;
            LifeFixture f;

            LifeScene* life = f.life();
            ASSERT_NE(life, nullptr);

            auto list = life->piece<ActivityList>("activities");
            ASSERT_NE(list, nullptr);
            ASSERT_NE(list->row(&f.ecs, "buy.rations"), nullptr);
            // A tile done at once leaves its time line empty
            EXPECT_EQ(list->row(&f.ecs, "buy.rations")->cost.spec.text, "");

            f.ecs.sendEvent(ActivityActivatedEvent{"life.activities", "yard"});
            f.settle();

            const float age = life->save.age;
            const int coin = life->save.stats["coin"];
            const int rations = life->save.stats["rations"];
            const size_t logEntries = life->save.log.size();

            f.ecs.sendEvent(ActivityActivatedEvent{"life.activities", "buy.rations"});
            f.settle();

            EXPECT_FLOAT_EQ(life->save.age, age);
            EXPECT_EQ(life->save.running, "yard");
            EXPECT_EQ(life->save.stats["coin"], coin - 5);
            EXPECT_EQ(life->save.stats["rations"], rations + 6);
            EXPECT_EQ(life->save.done["buy.rations"], 1);
            EXPECT_EQ(life->save.log.size(), logEntries + 1);

            auto ledger = life->piece<ResourceLedger>("ledger");
            ASSERT_NE(ledger, nullptr);
            EXPECT_EQ(ledger->row("coin")->figure.spec.text, std::to_string(coin - 5));
            EXPECT_EQ(ledger->row("rations")->figure.spec.text, std::to_string(rations + 6) + "/60");

            // Still there to be done again
            EXPECT_NE(list->find(&f.ecs, "buy.rations"), nullptr);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // Without the coin it asks, the row is locked and confirming it does nothing.
        TEST(lifescene_test, an_activity_he_cannot_pay)
        {
            MockLogger logger;
            LifeFixture f;

            LifeSceneOptions opt = LifeFixture::mockup();
            opt.fresh = true;
            opt.lives = 2;

            LifeScene* life = f.life(opt);
            ASSERT_NE(life, nullptr);

            auto list = life->piece<ActivityList>("activities");
            ASSERT_NE(list, nullptr);

            // He knows the market, and a month tells the page so (and eats a ration)
            life->save.stats["market_known"] = 1;
            life->onMonth();
            f.settle();

            auto row = list->row(&f.ecs, "buy.rations");
            ASSERT_NE(row, nullptr);
            EXPECT_EQ(row->spec.state, ActivityState::Locked);

            f.ecs.sendEvent(ActivityActivatedEvent{"life.activities", "buy.rations"});
            f.settle();

            // Nothing taken, nothing brought: the rations he had, untouched
            EXPECT_EQ(life->save.stats["coin"], 0);
            EXPECT_EQ(life->save.stats["rations"], 11);
            EXPECT_EQ(life->save.done.count("buy.rations"), 0u);
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

            f.ecs.sendEvent(ActivityActivatedEvent{"life.activities", "yard"});
            f.settle();

            const size_t logEntries = life->save.log.size();

            f.ecs.sendEvent(ActivityActivatedEvent{"life.activities", "carters"});
            f.settle();

            EXPECT_EQ(life->save.running, "yard");
            EXPECT_EQ(f.fact<std::string>("activity.running.id"), "yard");
            ASSERT_EQ(life->save.log.size(), logEntries + 1);
            EXPECT_EQ(life->save.log.back().text, "Already at work: Train at the Yard");
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

            f.ecs.sendEvent(ActivityActivatedEvent{"life.activities", "yard"});
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
            EXPECT_EQ(again->save.world, 127);
            EXPECT_EQ(again->save.world, written.world);
            EXPECT_EQ(again->save.running, "yard");
            EXPECT_EQ(again->save.monthsIn, 1);
            EXPECT_EQ(again->save.stats, written.stats);
            EXPECT_EQ(again->save.log.size(), writtenLog);
            EXPECT_EQ(again->save.resources.size(), written.resources.size());

            EXPECT_NEAR(f.fact<float>("life.age"), written.age, 0.0001f);
            EXPECT_EQ(f.fact<std::string>("activity.running.id"), "yard");
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
            opt.lives = 2;

            LifeScene* life = f.life(opt);
            ASSERT_NE(life, nullptr);

            EXPECT_FLOAT_EQ(f.fact<float>("life.age"), 7.0f);
            EXPECT_EQ(f.fact<std::string>("life.headline.ageText"), "7.0");

            // The world begins with him: Year 0, its first month
            EXPECT_EQ(life->save.world, 0);
            EXPECT_EQ(life->piece<Label>("date")->spec.text, "YEAR 0");
            EXPECT_EQ(life->piece<Label>("dateNote")->spec.text, "MONTH 1 \xC2\xB7 WINTER \xC2\xB7 BELLMOOR");

            // Nothing earned: only the rations he was sent off with
            auto ledger = life->piece<ResourceLedger>("ledger");
            ASSERT_NE(ledger, nullptr);
            ASSERT_EQ(ledger->groups.size(), 1u);
            ASSERT_NE(ledger->row("rations"), nullptr);
            EXPECT_EQ(ledger->row("rations")->figure.spec.text, "12/60");
            EXPECT_EQ(ledger->row("coin"), nullptr);

            // One milestone row, and no year rubric over it: the Life page's log is its lines alone
            auto log = life->piece<EventLog>("log");
            ASSERT_EQ(log->items.size(), 1u);
            ASSERT_TRUE(std::holds_alternative<EventLog::Row>(log->items[0]));
            EXPECT_EQ(std::get<EventLog::Row>(log->items[0]).entry.text, "Childhood");
            EXPECT_EQ(std::get<EventLog::Row>(log->items[0]).entry.kind, LogKind::Milestone);

            // Every door is still ahead
            for (const char* id : {"choir", "watch", "boys", "keep", "collegium", "hand"})
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

            for (const char* stat : {"\"str\"", "\"dex\"", "\"int\"", "\"vit\"", "\"arms\"", "\"discipline\"", "\"letters\"", "\"lore\"", "\"arcana\"", "\"stealth\"", "\"guile\"", "\"renown\"", "\"rations\"", "\"favors\"", "\"reagents\"", "\"keep_oath\""})
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

            // A part: where it is, and when the next milestone comes
            const GlossSpec* str = registry->find("parts/str");
            ASSERT_NE(str, nullptr);
            EXPECT_EQ(str->title, "Strength");
            EXPECT_EQ(rowOf(str, "Now"), "12");
            EXPECT_EQ(str->footnote, "IN 42 MO: THE PROVING");

            // Selected, it says what the activity brings it to
            f.ecs.sendEvent(ActivitySelectedEvent{"life.activities", "yard"});
            f.settle();
            EXPECT_EQ(rowOf(registry->find("parts/str"), "Train at the Yard brings it to"), "13");

            // An activity: its time, what it brings and costs, what it asks, whether it feeds him
            const GlossSpec* campaign = registry->find("activity/campaign");
            ASSERT_NE(campaign, nullptr);
            EXPECT_EQ(campaign->title, "Join the Border Campaign");
            EXPECT_EQ(rowOf(campaign, "Time"), "24 mo");
            EXPECT_EQ(rowOf(campaign, "Meals"), "Provided");

            // Locked, and no footnote to say so: what it still asks is in red under IT ASKS
            EXPECT_EQ(campaign->footnote, "");

            // In sections, under the stat's full name: a gain and a thing asked may share it, the
            // section says which. Each figure in the colour of the way it goes
            auto in = [](const GlossSpec* g, const std::string& section, const std::string& label) -> std::pair<std::string, std::string> {
                bool inside = false;

                for (const auto& r : g->rows)
                {
                    if (r.heading)
                        inside = r.label == section;
                    else if (inside and r.label == label)
                        return {r.value, r.tone};
                }

                return {"<none>", "<none>"};
            };

            auto toneOf = [](const GlossSpec* g, const std::string& label) -> std::string {
                for (const auto& r : g->rows)
                {
                    if (r.label == label)
                        return r.heading ? "<heading>" : r.tone;
                }

                return "<none>";
            };

            EXPECT_EQ(toneOf(campaign, "Time"), "time");
            EXPECT_EQ(toneOf(campaign, "Meals"), "gain");
            EXPECT_EQ(toneOf(campaign, "IT BRINGS"), "<heading>");
            EXPECT_EQ(toneOf(campaign, "IT TAKES"), "<heading>");
            EXPECT_EQ(toneOf(campaign, "IT ASKS"), "<heading>");

            EXPECT_EQ(in(campaign, "IT BRINGS", "Coin").first, "+50");
            EXPECT_EQ(in(campaign, "IT BRINGS", "Coin").second, "gain");
            EXPECT_EQ(in(campaign, "IT TAKES", "Coin").first, "\xE2\x88\x92" "10");
            EXPECT_EQ(in(campaign, "IT TAKES", "Coin").second, "loss");
            EXPECT_EQ(in(campaign, "IT ASKS", "Age").first, "17 / 21");
            EXPECT_EQ(in(campaign, "IT ASKS", "Age").second, "loss");
            EXPECT_EQ(in(campaign, "IT ASKS", "Arms").first, "5 / 6");
            EXPECT_EQ(in(campaign, "IT ASKS", "Arms").second, "loss");
            EXPECT_EQ(in(campaign, "IT ASKS", "Discipline").second, "loss");

            // No short name with "at term" any more, no age it is done by, and nothing said of how
            // to begin what he can begin
            for (const auto& r : campaign->rows)
            {
                EXPECT_EQ(r.label.find("at term"), std::string::npos) << r.label;
                EXPECT_EQ(r.label.find("to begin"), std::string::npos) << r.label;
                EXPECT_NE(r.label, "Done by");
            }

            ASSERT_NE(registry->find("activity/yard"), nullptr);
            EXPECT_EQ(registry->find("activity/yard")->footnote, "");
            EXPECT_EQ(in(registry->find("activity/yard"), "IT BRINGS", "Strength").first, "+1");

            // Its head: how often it was done beside the name, the kind of thing it is under it,
            // and a rule before the first figures
            EXPECT_EQ(campaign->aside, "0/1");
            EXPECT_TRUE(campaign->inlineValues);
            EXPECT_EQ(campaign->text, "The Keep");
            ASSERT_FALSE(campaign->rows.empty());
            EXPECT_TRUE(campaign->rows[0].heading);
            EXPECT_EQ(campaign->rows[0].label, "");
            EXPECT_EQ(toneOf(campaign, "Done"), "<none>");

            // What a holding's month does to it comes coloured from the rules
            EXPECT_EQ(toneOf(registry->find("resource/rations"), "A month uses"), "loss");
            EXPECT_EQ(toneOf(registry->find("resource/rations"), "Lasts"), "time");

            EXPECT_EQ(rowOf(registry->find("activity/yard"), "Meals"), "His own rations");

            // A door: its note, its ages, what fits
            const GlossSpec* door = registry->find("window/campaign");
            ASSERT_NE(door, nullptr);
            EXPECT_EQ(door->text, f.fact<std::string>("window.campaign.note"));
            EXPECT_EQ(rowOf(door, "Opens at"), "21");
            EXPECT_EQ(rowOf(door, "Closes at"), "26");
            EXPECT_EQ(rowOf(door, "Attempts that fit"), "2");

            // And the pieces carry them
            EXPECT_TRUE(life->piece<StatLine>("str")->root->has<TooltipComponent>());
            EXPECT_EQ(life->piece<StatLine>("str")->root->get<TooltipComponent>()->text, "parts/str");

            ActivityRow* yard = life->piece<ActivityList>("activities")->row(&f.ecs, "yard");
            ASSERT_NE(yard, nullptr);
            ASSERT_TRUE(yard->root->has<TooltipComponent>());
            EXPECT_EQ(yard->root->get<TooltipComponent>()->text, "activity/yard");
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // A cost is taken the month it begins; the way into a path, done, makes him one of it: the
        // other ways close, his path's rows come, its asks stand on his parts.
        TEST(lifescene_test, a_path_is_entered)
        {
            MockLogger logger;
            LifeFixture f;

            LifeSceneOptions opt = LifeFixture::mockup();
            opt.fresh = true;
            opt.lives = 2;

            LifeScene* life = f.life(opt);
            ASSERT_NE(life, nullptr);

            life->save.age = 16.0f - 1.0f / 12.0f;
            life->save.aim = "";
            life->save.stats["str"] = 12;
            life->save.stats["arms"] = 3;
            life->save.stats["discipline"] = 2;
            life->save.stats["coin"] = 30;
            life->onMonth();
            f.settle();

            auto list = life->piece<ActivityList>("activities");
            ASSERT_NE(list, nullptr);
            ASSERT_NE(list->row(&f.ecs, "keep"), nullptr);
            EXPECT_EQ(list->row(&f.ecs, "keep")->spec.state, ActivityState::Idle);
            EXPECT_EQ(list->row(&f.ecs, "serve"), nullptr);

            // A strong boy: the Collegium is far from him, and listed all the same, locked: a way into
            // a path shows whatever he has
            ASSERT_NE(list->row(&f.ecs, "collegium"), nullptr);
            EXPECT_EQ(list->row(&f.ecs, "collegium")->spec.state, ActivityState::Locked);

            const int rations = life->save.stats["rations"];

            f.ecs.sendEvent(ActivityActivatedEvent{"life.activities", "keep"});
            f.settle();

            // Paid as it begins
            EXPECT_EQ(life->save.running, "keep");
            EXPECT_EQ(life->save.stats["coin"], 20);
            EXPECT_EQ(life->piece<ResourceLedger>("ledger")->row("coin")->figure.spec.text, "20");

            for (int i = 0; i < 18; ++i)
            {
                life->onMonth();
                f.settle();
            }

            f.frames(8);

            // Fed by the Keep: not a ration eaten, and not paid twice
            EXPECT_EQ(life->save.running, "");
            EXPECT_EQ(life->save.stats["rations"], rations);
            EXPECT_EQ(life->save.stats["coin"], 20);
            EXPECT_EQ(life->save.stats["keep_oath"], 1);
            EXPECT_EQ(life->save.aim, "warrior");

            EXPECT_EQ(list->row(&f.ecs, "hand"), nullptr);

            EXPECT_NE(life->piece<ResourceLedger>("ledger")->row("keep_oath"), nullptr);
            EXPECT_EQ(f.fact<std::string>("window.keep.state"), "closed");
            EXPECT_NE(std::find(life->save.achieved.begin(), life->save.achieved.end(), "sworn"), life->save.achieved.end());

            // The Keep's service is his at 18
            EXPECT_EQ(list->row(&f.ecs, "serve"), nullptr);

            for (int i = 0; i < 6; ++i)
            {
                life->onMonth();
                f.settle();
            }

            ASSERT_NE(list->row(&f.ecs, "serve"), nullptr);
            EXPECT_EQ(list->row(&f.ecs, "serve")->spec.state, ActivityState::Idle);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // A life with nothing left ends before the term pays: the mill's Vitality comes too late.
        TEST(lifescene_test, death_comes_before_the_term)
        {
            MockLogger logger;
            LifeFixture f;

            LifeSceneOptions opt = LifeFixture::mockup();
            opt.fresh = true;
            opt.lives = 2;

            LifeScene* life = f.life(opt);
            ASSERT_NE(life, nullptr);

            f.ecs.sendEvent(ActivityActivatedEvent{"life.activities", "mill"});
            f.settle();

            life->save.monthsIn = 5;
            life->save.stats["rations"] = 0;
            life->save.stats["vit"] = 1;

            life->onMonth();
            f.settle();

            // Over, with the mill still under way
            EXPECT_TRUE(life->ended);
            EXPECT_EQ(life->save.running, "mill");

            life->beginAgain();
            f.settle();

            // A new life, and the mill's term never came
            EXPECT_FLOAT_EQ(life->save.age, 7.0f);
            EXPECT_TRUE(life->save.running.empty());
            EXPECT_EQ(life->save.done.count("mill"), 0u);
            ASSERT_EQ(happened(life).size(), 2u);
            EXPECT_NE(happened(life).back().text.find("life ended"), std::string::npos);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // The last milestone ends his prime, not his life: its line, the work of an old man in place
        // of every other, and from there his Vitality leaves him, a little every two months, with
        // no red and no stop, until the life ends in its old age.
        TEST(lifescene_test, old_age_begins_at_thirty)
        {
            MockLogger logger;
            LifeFixture f;

            LifeScene* life = f.life();
            ASSERT_NE(life, nullptr);

            life->save.age = 30.0f - 1.0f / 12.0f;
            life->save.stats["rations"] = 24;
            life->save.stats["vit"] = 20;
            life->save.stats["vitmax"] = 20;

            life->onMonth();
            f.settle();

            EXPECT_EQ(f.fact<std::string>("life.headline.ageText"), "30.0");
            EXPECT_EQ(f.fact<std::string>("life.next.label"), "");
            EXPECT_EQ(life->save.stats["vit"], 20);

            bool written = false;

            for (const auto& entry : life->save.log)
            {
                if (entry.kind == LogKind::Milestone and entry.text.find("old age begins") != std::string::npos)
                    written = true;
            }

            EXPECT_TRUE(written);

            // The work of his prime is closed, an old man's is there, and he can still buy his rations
            auto list = life->piece<ActivityList>("activities");
            ASSERT_NE(list, nullptr);

            EXPECT_EQ(list->find(&f.ecs, "carters"), nullptr);
            EXPECT_NE(list->find(&f.ecs, "buy.rations"), nullptr);
            EXPECT_NE(list->find(&f.ecs, "tales"), nullptr);
            EXPECT_NE(list->find(&f.ecs, "garden"), nullptr);
            EXPECT_NE(list->find(&f.ecs, "teach"), nullptr);

            // The months go on: he tells his tales, fed, and the second month takes from his Vitality
            const int coin = life->save.stats["coin"];
            const int rations = life->save.stats["rations"];

            f.ecs.sendEvent(ActivityActivatedEvent{"life.activities", "tales"});
            f.settle();

            ASSERT_EQ(life->save.running, "tales");

            life->onMonth();
            f.settle();

            EXPECT_EQ(life->save.stats["vit"], 20);
            EXPECT_FALSE(life->paused);

            life->onMonth();
            f.settle();

            EXPECT_EQ(life->save.stats["vit"], 19);
            EXPECT_EQ(life->save.stats["vitmax"], 19);
            EXPECT_EQ(life->save.done["tales"], 1);
            EXPECT_EQ(life->save.stats["coin"], coin + 2);
            EXPECT_EQ(life->save.stats["rations"], rations);

            // The years took it: no part in red
            EXPECT_FALSE(life->piece<StatLine>("vit")->spec.alert);
            EXPECT_EQ(life->piece<StatLine>("vit")->figure.spec.text, "19");

            // A year into his old age: the clock's figure goes past the end of its track
            for (int i = 0; i < 10; ++i)
                life->onMonth();
            f.settle();

            EXPECT_EQ(f.fact<std::string>("life.headline.ageText"), "31.0");
            EXPECT_EQ(life->piece<LifeClock>("clock")->age.spec.text, "31");
            EXPECT_EQ(life->save.stats["vit"], 14);
            EXPECT_NE(list->find(&f.ecs, "tales"), nullptr);

            // The last of it: the life ends, and the next one says it ended old
            life->save.stats["vit"] = 1;
            life->save.stats["vitmax"] = 1;

            life->onMonth();
            f.settle();

            EXPECT_GT(life->save.age, 31.0f);

            life->onMonth();
            f.settle();

            EXPECT_TRUE(life->ended);
            EXPECT_NE(life->epitaph.cause.find("old man"), std::string::npos) << life->epitaph.cause;

            life->beginAgain();
            f.settle();

            EXPECT_FLOAT_EQ(life->save.age, 7.0f);
            EXPECT_TRUE(life->paused);
            EXPECT_NE(happened(life).back().text.find("old age"), std::string::npos) << happened(life).back().text;
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // A life that ends is not replaced at once: its ending comes up over the page, a leaf in the
        // middle of the window saying who he was, how it ended and what the chronicle keeps of him,
        // on a veil that takes the mouse. Nothing passes until its button begins the next life.
        TEST(lifescene_test, a_life_ends_on_its_ending)
        {
            MockLogger logger;
            LifeFixture f;

            LifeScene* life = f.life();
            ASSERT_NE(life, nullptr);

            EXPECT_FALSE(life->ended);
            EXPECT_TRUE(life->ending.empty());

            // The mockup's life (23 terms, the carters' most of all, sworn to the Keep, three deeds),
            // with nothing left to eat
            life->save.stats["rations"] = 0;
            life->save.stats["vit"] = 1;

            life->onMonth();
            f.settle();

            ASSERT_TRUE(life->ended);
            ASSERT_FALSE(life->ending.empty());
            EXPECT_TRUE(life->paused);

            // The page under it is the life as it ended
            EXPECT_EQ(life->save.stats["vit"], 0);
            EXPECT_EQ(life->piece<StatLine>("vit")->figure.spec.text, "0");

            auto part = [&](const char* name) { return life->ending->get<Prefab>()->findEntity(name); };
            auto said = [&](const char* name) { return part(name)->get<Label>()->spec.text; };

            for (const char* name : {"veil", "leaf", "endName", "endCause", "endStory", "endTally", "again"})
                ASSERT_FALSE(part(name).empty()) << name;

            // What it says is the save's and the rules'
            EXPECT_EQ(said("endName"), life->save.name);
            EXPECT_EQ(said("endCause"), life->epitaph.cause);
            EXPECT_NE(said("endCause").find("His strength gave out"), std::string::npos) << said("endCause");
            EXPECT_EQ(said("endStory"), life->epitaph.text);
            EXPECT_NE(said("endStory").find("He swore himself to the Keep"), std::string::npos) << said("endStory");
            EXPECT_NE(said("endStory").find("Carry for the Carters"), std::string::npos) << said("endStory");
            EXPECT_NE(said("endStory").find("The carters' road"), std::string::npos) << said("endStory");
            EXPECT_EQ(said("endTally"), life->epitaph.tally);
            EXPECT_EQ(said("endTally").rfind("AGE 17 \xC2\xB7 WORKS 23 \xC2\xB7 COIN ", 0), 0u) << said("endTally");

            // The veil covers the window, over the page, and takes the mouse; the leaf stands in its
            // middle, over the veil
            auto veil = f.pos(part("veil"));
            auto leaf = f.pos(part("leaf"));

            EXPECT_NEAR(veil->x, 0.0f, 0.5f);
            EXPECT_NEAR(veil->y, 0.0f, 0.5f);
            EXPECT_NEAR(veil->width, 1320.0f, 0.5f);
            EXPECT_NEAR(veil->height, 1020.0f, 0.5f);

            // A larger window: the veil is the window's size still, the leaf in its middle
            f.resize(1600.0f, 1100.0f);

            EXPECT_NEAR(veil->x, 0.0f, 0.5f);
            EXPECT_NEAR(veil->y, 0.0f, 0.5f);
            EXPECT_NEAR(veil->width, 1600.0f, 0.5f);
            EXPECT_NEAR(veil->height, 1100.0f, 0.5f);
            EXPECT_NEAR(leaf->x + leaf->width / 2.0f, 800.0f, 1.0f);
            EXPECT_NEAR(leaf->y + leaf->height / 2.0f, 550.0f, 1.0f);

            f.resize(1320.0f, 1020.0f);

            EXPECT_NEAR(veil->width, 1320.0f, 0.5f);
            EXPECT_NEAR(veil->height, 1020.0f, 0.5f);
            EXPECT_TRUE(part("veil")->has<MouseEnterComponent>());
            EXPECT_GT(veil->z, f.pos(life->named("may"))->z + 50.0f);
            EXPECT_GT(leaf->z, veil->z);
            EXPECT_NEAR(leaf->x + leaf->width / 2.0f, 660.0f, 1.0f);
            EXPECT_NEAR(leaf->y + leaf->height / 2.0f, 510.0f, 1.0f);
            EXPECT_GT(leaf->height, 150.0f);

            // Nothing passes: no month, no work, no key
            const float age = life->save.age;

            life->onMonth();
            f.ecs.sendEvent(ActivityActivatedEvent{"life.activities", "yard"});
            f.ecs.sendEvent(ButtonActivatedEvent{life->piece<Button>("skip")->face.id, "life.skip"});
            f.ecs.sendEvent(OnSDLScanCode{SDL_SCANCODE_SPACE, 0});
            f.ecs.sendEvent(TickEvent{life->opt.monthMs * 3.0f});
            f.settle();

            EXPECT_TRUE(life->ended);
            EXPECT_FLOAT_EQ(life->save.age, age);
            EXPECT_TRUE(life->save.running.empty());

            // Its button begins the next life, which says how the last one ended
            Button again = *part("again")->get<Button>().component;

            f.ecs.sendEvent(ButtonActivatedEvent{again.face.id, "life.again"});
            f.settle();

            EXPECT_FALSE(life->ended);
            EXPECT_TRUE(life->ending.empty());
            EXPECT_FLOAT_EQ(life->save.age, 7.0f);
            EXPECT_TRUE(life->save.done.empty());
            ASSERT_EQ(happened(life).size(), 2u);
            EXPECT_EQ(happened(life).back().kind, LogKind::Loss);
            EXPECT_NE(happened(life).back().text.find("life ended"), std::string::npos);

            // And the page takes work again
            f.ecs.sendEvent(ButtonActivatedEvent{life->piece<Button>("skip")->face.id, "life.skip"});
            f.settle();

            EXPECT_GT(life->save.age, 7.0f);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // What the next milestone asks of the path he is headed for stands on his parts.
        TEST(lifescene_test, the_next_milestone_stands_on_his_parts)
        {
            MockLogger logger;
            LifeFixture f;

            LifeSceneOptions opt = LifeFixture::mockup();
            opt.fresh = true;
            opt.lives = 2;

            LifeScene* life = f.life(opt);
            ASSERT_NE(life, nullptr);

            // A boy: the apprenticeship asks nothing
            EXPECT_EQ(life->piece<StatLine>("str")->spec.threshold, 0);

            life->save.age = 15.5f - 1.0f / 12.0f;
            life->onMonth();
            f.settle();

            // Choosing a path at 16 asks the Warrior Strength 12
            EXPECT_EQ(f.fact<std::string>("life.next.label"), "Choose a class");
            EXPECT_EQ(life->piece<StatLine>("str")->spec.threshold, 12);
            EXPECT_EQ(life->piece<StatLine>("int")->spec.threshold, 0);

            auto registry = f.ecs.getSystem<GlossRegistry>();
            ASSERT_NE(registry, nullptr);

            bool asked = false;

            for (const auto& r : registry->find("parts/str")->rows)
            {
                if (r.label == "Choose a class asks")
                {
                    EXPECT_EQ(r.value, "12");
                    asked = true;
                }
            }

            EXPECT_TRUE(asked);
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

            // Only Life for now: the other chapters come back as their pages are built
            ASSERT_EQ(tabs->tabs.size(), 1u);
            EXPECT_EQ(tabs->tabs[0].label.spec.text, "Life");

            // A chapter with no page yet, should one be selected, says so and Life stays open
            f.ecs.sendEvent(TabSelectedEvent{tabs->root.id, "life.tabs", 3});
            f.settle();

            EXPECT_EQ(tabs->active(), 0);
            ASSERT_EQ(life->save.log.size(), logEntries + 1);
            EXPECT_EQ(life->save.log.back().text, "That page has no page yet");
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // The first life of a world opens a month before 7 with nothing held and one task in its
        // list. The guide says what the game is on a leaf under the task, its hand beside it; once
        // he has chosen it, hand and leaf go to the button that begins it, and the leaf says a
        // double click does too.
        TEST(lifescene_test, a_first_life_opens_on_one_tile)
        {
            MockLogger logger;
            LifeFixture f;

            LifeScene* life = f.life(LifeFixture::first());
            ASSERT_NE(life, nullptr);

            f.frames(12);

            EXPECT_NEAR(life->save.age, 6.0f + 11.0f / 12.0f, 0.0001f);
            EXPECT_EQ(life->save.lives, 1);
            EXPECT_EQ(life->save.guide, 0);
            EXPECT_TRUE(life->guiding());

            // His age as the head writes it: the clock's figure, then the months of the year
            EXPECT_EQ(life->piece<LifeClock>("clock")->age.spec.text, "6");
            EXPECT_EQ(f.fact<std::string>("life.headline.ageUnit"), "YEARS 11 MONTHS OLD");

            // Nothing held: no rations to eat, no row for them
            auto ledger = life->piece<ResourceLedger>("ledger");
            ASSERT_NE(ledger, nullptr);
            EXPECT_TRUE(ledger->groups.empty());
            EXPECT_EQ(life->save.stats.count("rations"), 0u);
            EXPECT_FALSE(f.facts->hasFact("stat.rations"));

            // One row, and nothing chosen for him: the guide has him choose it
            auto list = life->piece<ActivityList>("activities");
            ASSERT_NE(list, nullptr);
            ASSERT_EQ(list->spec.groups.size(), 1u);
            ASSERT_EQ(list->spec.groups[0].rows.size(), 1u);
            EXPECT_EQ(list->spec.groups[0].rows[0].id, "helping");
            EXPECT_EQ(list->selected(), "");
            EXPECT_EQ(life->save.picks, 0);

            // No button yet: neither the one that begins, nor the one that passes a month
            auto begin = life->piece<Button>("begin");
            ASSERT_NE(begin, nullptr);
            EXPECT_FALSE(f.pos(life->named("begin"))->visible);
            EXPECT_FALSE(f.pos(life->named("skip"))->visible);

            // The guide's first step: said on its own leaf, not on the line of "At work now"
            EXPECT_TRUE(life->step.active);
            EXPECT_EQ(life->step.id, "guide.begin");
            EXPECT_EQ(life->step.order, 1);
            EXPECT_FALSE(f.pos(life->named("guide"))->visible);
            EXPECT_FALSE(life->step.say.empty());
            EXPECT_EQ(life->noteWords.spec.text, life->step.say);
            EXPECT_EQ(life->noteWords.spec.style, "body");

            // The leaf has the button that leaves the guide; none to pass a step that waits for him
            EXPECT_FALSE(life->noteSkip.spec.disabled);
            EXPECT_FALSE(life->nextShown);
            EXPECT_TRUE(life->noteNext.spec.disabled);
            EXPECT_FALSE(f.pos(life->noteNext.root)->visible);

            // Its hand beside the task, and the rest of the page under a shade
            ActivityRow* helping = list->find(&f.ecs, "helping");
            ASSERT_NE(helping, nullptr);
            ASSERT_FALSE(life->hand.empty());
            EXPECT_EQ(life->handOn, helping->root.id);
            EXPECT_TRUE(f.pos(life->hand)->visible);
            EXPECT_LE(f.pos(life->hand)->x + f.pos(life->hand)->width, f.pos(helping->root)->x + 0.5f);
            EXPECT_TRUE(life->veiled);
            EXPECT_FALSE(life->veil.empty());
            expectNoteBeside(f, life, helping->root);

            // The task chosen: the button that begins it, the month it takes, and the hand beside it.
            // The line says what to press, and that a double click on a task begins it as well
            f.ecs.sendEvent(ActivitySelectedEvent{"life.activities", "helping"});
            f.settle();

            EXPECT_EQ(life->save.picks, 1);
            EXPECT_TRUE(f.pos(life->named("begin"))->visible);
            EXPECT_EQ(begin->spec.label, "Begin");
            EXPECT_EQ(begin->spec.months, 1);
            EXPECT_TRUE(life->step.active);
            EXPECT_NE(life->step.sayChosen, life->step.say);
            EXPECT_EQ(life->noteWords.spec.text, life->step.sayChosen);
            EXPECT_NE(life->noteWords.spec.text.find("double-click"), std::string::npos) << life->noteWords.spec.text;
            EXPECT_EQ(life->handOn, begin->root.id);
            expectNoteBeside(f, life, begin->root);
            EXPECT_TRUE(f.pos(life->hand)->visible);
            EXPECT_LE(f.pos(life->hand)->x + f.pos(life->hand)->width, f.pos(begin->root)->x + 0.5f);
            EXPECT_NEAR(f.pos(life->hand)->y + f.pos(life->hand)->height / 2.0f, f.pos(begin->root)->y + f.pos(begin->root)->height / 2.0f, 1.0f);
            EXPECT_TRUE(life->veiled);

            // Let go of: the hand is on the task again
            f.ecs.sendEvent(ActivitySelectedEvent{"life.activities", ""});
            f.settle();

            EXPECT_FALSE(f.pos(life->named("begin"))->visible);
            EXPECT_EQ(life->handOn, helping->root.id);
            EXPECT_EQ(life->noteWords.spec.text, life->step.say);
            EXPECT_EQ(life->save.guide, 0);

            // What the guide and the lore watch
            EXPECT_EQ(f.fact<int>("life.terms"), 0);
            EXPECT_EQ(f.fact<int>("life.working"), 0);
            EXPECT_EQ(f.fact<int>("life.guide"), 0);
            EXPECT_EQ(f.fact<int>("life.lives"), 1);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // The first task takes a month and brings him to 7 with his first rations: the guide says
        // what they are with its hand beside them, then shows the next work without choosing it.
        TEST(lifescene_test, the_first_term_brings_the_rations_and_moves_the_hand)
        {
            MockLogger logger;
            LifeFixture f;

            LifeScene* life = f.life(LifeFixture::first());
            ASSERT_NE(life, nullptr);

            f.frames(12);

            auto ledger = life->piece<ResourceLedger>("ledger");
            auto list = life->piece<ActivityList>("activities");
            ASSERT_NE(ledger, nullptr);
            ASSERT_NE(list, nullptr);
            ASSERT_TRUE(life->step.active);

            // The task, then the button
            f.ecs.sendEvent(ActivitySelectedEvent{"life.activities", "helping"});
            f.settle();
            f.ecs.sendEvent(ButtonActivatedEvent{life->piece<Button>("begin")->face.id, "life.begin"});
            f.settle();

            // Begun: the first step ends with it, and the hand leaves
            ASSERT_EQ(life->save.running, "helping");
            EXPECT_EQ(life->save.guide, 1);
            EXPECT_FALSE(life->step.active);
            EXPECT_EQ(life->handOn, 0u);
            EXPECT_FALSE(life->veiled);
            EXPECT_FALSE(life->noteShown);
            EXPECT_FALSE(f.pos(life->note)->visible);
            EXPECT_EQ(f.fact<int>("life.working"), 1);

            life->onMonth();
            f.frames(12);

            EXPECT_NEAR(life->save.age, 7.0f, 0.0001f);
            EXPECT_TRUE(life->save.running.empty());
            EXPECT_EQ(life->save.done["helping"], 1);
            EXPECT_EQ(life->save.stats["rations"], 12);
            EXPECT_EQ(f.fact<int>("life.terms"), 1);
            EXPECT_EQ(f.fact<int>("life.working"), 0);

            // His first holding has its row; the opening task is gone and the carters have come
            ASSERT_NE(ledger->row("rations"), nullptr);
            EXPECT_EQ(ledger->row("rations")->figure.spec.text, "12/60");
            EXPECT_EQ(list->find(&f.ecs, "helping"), nullptr);
            ASSERT_NE(list->find(&f.ecs, "carters"), nullptr);
            EXPECT_EQ(list->find(&f.ecs, "messages"), nullptr);

            // The second step: what he holds, the hand beside it
            ASSERT_TRUE(life->step.active);
            EXPECT_EQ(life->step.id, "guide.rations");
            EXPECT_FALSE(life->step.say.empty());
            EXPECT_EQ(life->noteWords.spec.text, life->step.say);
            EXPECT_FALSE(f.pos(life->named("guide"))->visible);
            EXPECT_EQ(life->handOn, ledger->rowEntity("rations").id);
            expectNoteBeside(f, life, ledger->rowEntity("rations"));

            // Said for a time: it can be passed
            EXPECT_TRUE(life->nextShown);
            EXPECT_FALSE(life->noteNext.spec.disabled);
            EXPECT_TRUE(f.pos(life->noteNext.root)->visible);
            EXPECT_TRUE(life->veiled);
            EXPECT_EQ(life->save.guide, 1);

            // Said for five seconds; then the third shows the carters' tile, and chooses nothing
            f.pass(5200.0f);

            EXPECT_EQ(life->save.guide, 2);
            EXPECT_EQ(f.fact<int>("life.guide"), 2);
            ASSERT_TRUE(life->step.active);
            EXPECT_EQ(life->step.id, "guide.carters");
            EXPECT_EQ(life->noteWords.spec.text, life->step.say);

            ActivityRow* carters = list->find(&f.ecs, "carters");
            ASSERT_NE(carters, nullptr);
            EXPECT_EQ(life->handOn, carters->root.id);
            expectNoteBeside(f, life, carters->root);
            EXPECT_FALSE(life->nextShown);
            EXPECT_EQ(list->selected(), "");
            EXPECT_TRUE(life->save.running.empty());

            // Chosen, the hand goes to the button that begins it
            f.ecs.sendEvent(ActivitySelectedEvent{"life.activities", "carters"});
            f.settle();

            EXPECT_EQ(life->handOn, life->piece<Button>("begin")->root.id);
            EXPECT_EQ(life->noteWords.spec.text, life->step.sayChosen);
            EXPECT_EQ(life->save.guide, 2);

            // And a double click on the task begins it just the same: the step ends
            f.ecs.sendEvent(ActivityActivatedEvent{"life.activities", "carters"});
            f.settle();

            EXPECT_EQ(life->save.running, "carters");
            EXPECT_EQ(life->save.guide, 3);
            EXPECT_FALSE(life->step.active);
            EXPECT_EQ(life->handOn, 0u);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // Six works, each said its sentence: the list grows a task at a time, his age and the classes
        // are said with the fourth term, and after its last step nothing of the guide is left on
        // the page.
        TEST(lifescene_test, the_guide_ends_after_the_sixth_work)
        {
            MockLogger logger;
            LifeFixture f;

            LifeScene* life = f.life(LifeFixture::first());
            ASSERT_NE(life, nullptr);

            f.frames(12);

            auto list = life->piece<ActivityList>("activities");
            ASSERT_NE(list, nullptr);

            // A work begun, and its months
            auto work = [&](const std::string& id, int months) {
                f.ecs.sendEvent(ActivityActivatedEvent{"life.activities", id});
                f.settle();

                ASSERT_EQ(life->save.running, id);

                for (int i = 0; i < months; ++i)
                {
                    life->onMonth();
                    f.settle();
                }

                f.frames(12);

                ASSERT_TRUE(life->save.running.empty()) << id;
            };

            // "Next" on the leaf: the step said for a time ends now
            auto next = [&]() {
                ASSERT_TRUE(life->step.active);
                ASSERT_TRUE(life->nextShown) << life->step.id;

                f.ecs.sendEvent(ButtonActivatedEvent{life->noteNext.face.id, "life.guide.next"});
                f.frames(12);
            };

            work("helping", 1);
            EXPECT_EQ(life->step.id, "guide.rations");
            next();
            EXPECT_EQ(life->save.guide, 2);
            EXPECT_EQ(life->step.id, "guide.carters");

            work("carters", 3);
            EXPECT_EQ(life->step.id, "guide.coin");
            next();
            EXPECT_EQ(life->save.guide, 4);
            EXPECT_EQ(life->step.id, "guide.messages");
            ASSERT_NE(list->find(&f.ecs, "messages"), nullptr);
            EXPECT_EQ(list->find(&f.ecs, "kitchen"), nullptr);

            work("messages", 3);
            EXPECT_EQ(life->step.id, "guide.dex");
            EXPECT_EQ(life->handOn, life->piece<StatLine>("dex")->root.id);
            next();
            EXPECT_EQ(life->save.guide, 6);
            EXPECT_EQ(life->step.id, "guide.kitchen");
            ASSERT_NE(list->find(&f.ecs, "kitchen"), nullptr);
            EXPECT_EQ(life->handOn, list->find(&f.ecs, "kitchen")->root.id);
            EXPECT_EQ(list->find(&f.ecs, "keep"), nullptr);

            // The fourth work: his age and what the game is about, then the classes, which came
            // into the list with this term
            work("kitchen", 3);
            EXPECT_EQ(life->step.id, "guide.clock");
            EXPECT_EQ(life->handOn, life->piece<LifeClock>("clock")->root.id);
            EXPECT_NE(life->noteWords.spec.text.find("30"), std::string::npos) << life->noteWords.spec.text;

            for (const char* id : {"keep", "collegium", "hand"})
                EXPECT_NE(list->find(&f.ecs, id), nullptr) << id;

            next();
            EXPECT_EQ(life->save.guide, 8);
            EXPECT_EQ(life->step.id, "guide.doors");
            EXPECT_EQ(life->handOn, list->find(&f.ecs, "keep")->root.id);
            EXPECT_NE(life->noteWords.spec.text.find("classes"), std::string::npos) << life->noteWords.spec.text;

            next();
            EXPECT_EQ(life->save.guide, 9);
            EXPECT_EQ(life->step.id, "guide.rest");
            EXPECT_EQ(life->handOn, 0u);
            EXPECT_TRUE(life->noteShown);

            next();
            EXPECT_EQ(life->save.guide, 10);
            EXPECT_FALSE(life->step.active);

            // Two more works: his rations run low at his letters, and the word kept for that is said
            work("mill", 6);
            EXPECT_FALSE(life->step.active);

            work("letters", 6);
            EXPECT_EQ(life->step.id, "guide.rations_low");
            EXPECT_EQ(life->step.order, 0);
            next();

            EXPECT_EQ(life->save.guide, 10);
            EXPECT_EQ(f.fact<int>("life.guide"), 10);
            EXPECT_FALSE(life->guiding());
            EXPECT_FALSE(life->step.active);
            EXPECT_TRUE(life->stepsDue.empty());

            // Nothing of it remains: no leaf, no line, no hand, no shade
            EXPECT_FALSE(life->noteShown);
            EXPECT_FALSE(f.pos(life->note)->visible);
            EXPECT_TRUE(life->noteSkip.spec.disabled);
            EXPECT_FALSE(f.pos(life->named("guide"))->visible);
            EXPECT_EQ(life->handOn, 0u);
            ASSERT_FALSE(life->hand.empty());
            EXPECT_FALSE(f.pos(life->hand)->visible);
            EXPECT_FALSE(life->veiled);

            for (const auto& strip : life->veil)
                EXPECT_FALSE(f.pos(strip)->visible);

            // Two lines of the town's story at the least, among what happened
            EXPECT_TRUE(reached(life, "lore.bell"));
            EXPECT_TRUE(reached(life, "lore.gates"));
            EXPECT_TRUE(reached(life, "lore.classes"));
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // The guide's place is in the save: opened again, a first life is said the step it was at,
        // not the first.
        TEST(lifescene_test, the_guide_resumes_after_a_reload)
        {
            MockLogger logger;
            LifeFixture f;

            const std::string path = "save/test_life_guide.sz";
            std::remove(path.c_str());

            LifeSceneOptions opt = LifeFixture::first();
            opt.noSave = false;
            opt.savePath = path;

            LifeScene* life = f.life(opt);
            ASSERT_NE(life, nullptr);

            f.frames(12);

            // The opening task, what the guide says of it, then the carters' term: the fourth step
            f.ecs.sendEvent(ActivityActivatedEvent{"life.activities", "helping"});
            f.settle();
            life->onMonth();
            f.frames(12);
            f.pass(5200.0f);

            ASSERT_EQ(life->step.id, "guide.carters");

            f.ecs.sendEvent(ActivityActivatedEvent{"life.activities", "carters"});
            f.settle();

            for (int i = 0; i < 3; ++i)
            {
                life->onMonth();
                f.settle();
            }

            f.frames(12);

            ASSERT_TRUE(life->step.active);
            ASSERT_EQ(life->step.id, "guide.coin");
            ASSERT_EQ(life->save.guide, 3);
            ASSERT_TRUE(life->saveNow());

            f.load<EmptyScene>();

            opt.fresh = false;

            LifeScene* again = f.life(opt);
            ASSERT_NE(again, nullptr);

            f.frames(12);

            EXPECT_EQ(again->save.lives, 1);
            EXPECT_EQ(again->save.guide, 3);
            EXPECT_EQ(f.fact<int>("life.guide"), 3);

            ASSERT_TRUE(again->step.active);
            EXPECT_EQ(again->step.id, "guide.coin");
            EXPECT_FALSE(again->step.say.empty());
            EXPECT_EQ(again->noteWords.spec.text, again->step.say);
            EXPECT_TRUE(again->noteShown);

            auto ledger = again->piece<ResourceLedger>("ledger");
            ASSERT_NE(ledger, nullptr);
            ASSERT_NE(ledger->row("coin"), nullptr);
            EXPECT_EQ(again->handOn, ledger->rowEntity("coin").id);

            // And it goes on from there
            f.pass(5200.0f);

            EXPECT_EQ(again->save.guide, 4);
            EXPECT_EQ(again->step.id, "guide.messages");

            std::remove(path.c_str());
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // The guide is a first life's: the next one opens as every life did, at 7 with a year of
        // rations and its list whole, led to its first work by one line and no hand.
        TEST(lifescene_test, a_second_life_has_no_guide_and_the_old_opening)
        {
            MockLogger logger;
            LifeFixture f;

            LifeScene* life = f.life(LifeFixture::first());
            ASSERT_NE(life, nullptr);

            f.frames(12);

            ASSERT_TRUE(life->step.active);
            ASSERT_NE(life->handOn, 0u);

            life->newLife();
            f.frames(12);

            EXPECT_FLOAT_EQ(life->save.age, 7.0f);
            EXPECT_EQ(life->save.lives, 2);
            EXPECT_EQ(life->rules.lives, 2);
            EXPECT_EQ(life->save.guide, 0);
            EXPECT_EQ(life->save.stats["rations"], 12);
            EXPECT_FALSE(life->guiding());

            auto ledger = life->piece<ResourceLedger>("ledger");
            ASSERT_NE(ledger, nullptr);
            ASSERT_NE(ledger->row("rations"), nullptr);
            EXPECT_EQ(ledger->row("rations")->figure.spec.text, "12/60");

            // The list of a first day, whole, and the carters chosen
            auto list = life->piece<ActivityList>("activities");
            ASSERT_NE(list, nullptr);

            for (const char* id : {"carters", "mill", "messages", "kitchen", "letters", "keep", "collegium", "hand"})
                EXPECT_NE(list->find(&f.ecs, id), nullptr) << id;

            EXPECT_EQ(list->find(&f.ecs, "helping"), nullptr);
            EXPECT_EQ(list->selected(), "carters");

            // The line of a life led to its first work, in its own words; no step, no hand, no shade
            EXPECT_TRUE(f.pos(life->named("guide"))->visible);
            EXPECT_EQ(life->piece<Label>("guide")->spec.text, "Choose a work, then press Begin");
            EXPECT_FALSE(life->step.active);
            EXPECT_TRUE(life->stepsDue.empty());
            EXPECT_EQ(life->handOn, 0u);
            EXPECT_FALSE(life->veiled);
            EXPECT_FALSE(life->noteShown);

            // Nor does one come with time, or with its first work
            f.pass(1000.0f);

            f.ecs.sendEvent(ButtonActivatedEvent{life->piece<Button>("begin")->face.id, "life.begin"});
            f.settle();

            ASSERT_EQ(life->save.running, "carters");

            for (int i = 0; i < 3; ++i)
            {
                life->onMonth();
                f.settle();
            }

            f.frames(12);

            EXPECT_FALSE(life->step.active);
            EXPECT_TRUE(life->stepsDue.empty());
            EXPECT_EQ(life->save.guide, 0);
            EXPECT_EQ(life->handOn, 0u);
            EXPECT_FALSE(f.pos(life->named("guide"))->visible);

            // A later life reads the town's story as the first did
            EXPECT_TRUE(reached(life, "lore.hand"));
            EXPECT_TRUE(reached(life, "lore.bell"));
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // A line of lore is a line in the log, in its own kind, and nothing else: no toast, and
        // never twice in a life.
        TEST(lifescene_test, lore_is_written_not_toasted)
        {
            MockLogger logger;
            LifeFixture f;

            LifeScene* life = f.life(LifeFixture::first());
            ASSERT_NE(life, nullptr);

            f.frames(12);

            // The page has arrived: from here a toast would be shown
            f.pass(800.0f);

            auto bell = [&]() {
                size_t lines = 0;

                for (const auto& entry : life->save.log)
                {
                    if (entry.kind == LogKind::Lore and entry.text.find("Bellmoor sits where the Bell meets the North Road") == 0)
                        ++lines;
                }

                return lines;
            };

            EXPECT_EQ(bell(), 0u);
            EXPECT_FALSE(reached(life, "lore.bell"));

            f.ecs.sendEvent(ActivityActivatedEvent{"life.activities", "helping"});
            f.settle();

            life->onMonth();

            // What the month itself toasts (the work that came into his list) is up already
            const int toasts = life->toastsUp();

            f.frames(12);

            EXPECT_EQ(bell(), 1u);
            EXPECT_TRUE(reached(life, "lore.bell"));
            EXPECT_EQ(life->toastsUp(), toasts);

            // In the log as its own kind, with the quill and no figure
            auto log = life->piece<EventLog>("log");
            ASSERT_NE(log, nullptr);
            EXPECT_EQ(log->size(), life->save.log.size());

            bool shown = false;

            for (const auto& item : log->items)
            {
                if (not std::holds_alternative<EventLog::Row>(item))
                    continue;

                const auto& row = std::get<EventLog::Row>(item);

                if (row.entry.kind != LogKind::Lore)
                    continue;

                shown = true;

                EXPECT_EQ(row.text.spec.style, "gloss");
                EXPECT_EQ(row.mark.spec.name, "quill");
                EXPECT_FALSE(row.figure.has_value());
            }

            EXPECT_TRUE(shown);

            // It is no deed, and the guide's steps are not kept with the deeds either
            EXPECT_TRUE(deedsOf(life).empty());
            EXPECT_FALSE(reached(life, "guide.begin"));

            // Another term: its ask holds still, and it is not written again
            f.pass(5200.0f);

            f.ecs.sendEvent(ActivityActivatedEvent{"life.activities", "carters"});
            f.settle();

            for (int i = 0; i < 3; ++i)
            {
                life->onMonth();
                f.settle();
            }

            f.frames(12);

            EXPECT_EQ(life->save.done["carters"], 1);
            EXPECT_EQ(bell(), 1u);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // The leaf's two buttons: "Next" passes a step that is said for a time without waiting for
        // it, and "Skip tutorial" ends the guide for good, in this session and in the save.
        TEST(lifescene_test, the_guide_is_passed_or_skipped)
        {
            MockLogger logger;
            LifeFixture f;

            const std::string path = "save/test_life_skip.sz";
            std::remove(path.c_str());

            LifeSceneOptions opt = LifeFixture::first();
            opt.noSave = false;
            opt.savePath = path;

            LifeScene* life = f.life(opt);
            ASSERT_NE(life, nullptr);

            f.frames(12);

            ASSERT_TRUE(life->step.active);
            ASSERT_FALSE(life->note.empty());

            // A step that waits for him cannot be passed: the button's event does nothing
            f.ecs.sendEvent(ButtonActivatedEvent{life->noteNext.face.id, "life.guide.next"});
            f.settle();

            EXPECT_EQ(life->save.guide, 0);
            EXPECT_EQ(life->step.id, "guide.begin");

            f.ecs.sendEvent(ActivityActivatedEvent{"life.activities", "helping"});
            f.settle();
            life->onMonth();
            f.frames(12);

            // What he holds, said for five seconds: "Next" passes it at once, no tick needed
            ASSERT_EQ(life->step.id, "guide.rations");
            ASSERT_TRUE(life->nextShown);

            f.ecs.sendEvent(ButtonActivatedEvent{life->noteNext.face.id, "life.guide.next"});
            f.frames(12);

            EXPECT_EQ(life->save.guide, 2);
            ASSERT_TRUE(life->step.active);
            EXPECT_EQ(life->step.id, "guide.carters");
            EXPECT_EQ(life->save.guideSkipped, 0);

            // "Skip tutorial": no step, no leaf, no hand, no shade, and the guide past its last step
            f.ecs.sendEvent(ButtonActivatedEvent{life->noteSkip.face.id, "life.guide.skip"});
            f.frames(12);

            EXPECT_EQ(life->save.guide, 10);
            EXPECT_EQ(life->save.guideSkipped, 3);
            EXPECT_EQ(f.fact<int>("life.guide"), 10);
            EXPECT_FALSE(life->guiding());
            EXPECT_FALSE(life->step.active);
            EXPECT_TRUE(life->stepsDue.empty());
            EXPECT_FALSE(life->noteShown);
            EXPECT_FALSE(f.pos(life->note)->visible);
            EXPECT_EQ(life->handOn, 0u);
            EXPECT_FALSE(life->veiled);

            // The word it kept for later is taken as said
            EXPECT_TRUE(reached(life, "guide.rations_low"));

            // The life goes on without it: the carters' term brings no step
            f.ecs.sendEvent(ActivityActivatedEvent{"life.activities", "carters"});
            f.settle();

            for (int i = 0; i < 3; ++i)
            {
                life->onMonth();
                f.settle();
            }

            f.frames(12);
            f.pass(1000.0f);

            EXPECT_EQ(life->save.done["carters"], 1);
            EXPECT_FALSE(life->step.active);
            EXPECT_FALSE(life->noteShown);

            // Nor does it come back with the save
            f.load<EmptyScene>();

            opt.fresh = false;

            LifeScene* again = f.life(opt);
            ASSERT_NE(again, nullptr);

            f.frames(12);

            EXPECT_EQ(again->save.guide, 10);
            EXPECT_EQ(again->save.guideSkipped, 3);
            EXPECT_FALSE(again->step.active);
            EXPECT_FALSE(again->noteShown);

            std::remove(path.c_str());
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // Skipped before anything was done, a first life is led as a later one is: its first task
        // chosen for it, Begin on the page, and the line of "At work now" in its own words.
        TEST(lifescene_test, a_guide_skipped_at_once_leaves_the_lead)
        {
            MockLogger logger;
            LifeFixture f;

            LifeScene* life = f.life(LifeFixture::first());
            ASSERT_NE(life, nullptr);

            f.frames(12);

            ASSERT_TRUE(life->step.active);
            ASSERT_FALSE(f.pos(life->named("begin"))->visible);

            f.ecs.sendEvent(ButtonActivatedEvent{life->noteSkip.face.id, "life.guide.skip"});
            f.frames(12);

            EXPECT_EQ(life->save.guide, 10);
            EXPECT_EQ(life->save.guideSkipped, 1);
            EXPECT_FALSE(life->step.active);
            EXPECT_FALSE(life->noteShown);

            auto list = life->piece<ActivityList>("activities");
            ASSERT_NE(list, nullptr);

            EXPECT_EQ(list->selected(), "helping");
            EXPECT_TRUE(f.pos(life->named("begin"))->visible);
            EXPECT_TRUE(f.pos(life->named("guide"))->visible);
            EXPECT_EQ(life->piece<Label>("guide")->spec.text, "Choose a work, then press Begin");
            EXPECT_EQ(life->save.picks, 0);

            // Still his first life: the list grows a task at a time
            f.ecs.sendEvent(ButtonActivatedEvent{life->piece<Button>("begin")->face.id, "life.begin"});
            f.settle();

            ASSERT_EQ(life->save.running, "helping");

            life->onMonth();
            f.frames(12);

            EXPECT_NE(list->find(&f.ecs, "carters"), nullptr);
            EXPECT_EQ(list->find(&f.ecs, "mill"), nullptr);
            EXPECT_FALSE(life->step.active);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // A page built again (another step of the three columns) has other tiles: the hand goes to
        // the one the step points at, and the leaf with the step's words goes with it.
        TEST(lifescene_test, the_hand_follows_a_rebuild)
        {
            MockLogger logger;
            LifeFixture f;

            LifeScene* life = f.life(LifeFixture::first());
            ASSERT_NE(life, nullptr);

            f.frames(12);

            f.ecs.sendEvent(ActivityActivatedEvent{"life.activities", "helping"});
            f.settle();
            life->onMonth();
            f.frames(12);
            f.pass(5200.0f);

            ASSERT_TRUE(life->step.active);
            ASSERT_EQ(life->step.id, "guide.carters");

            auto list = life->piece<ActivityList>("activities");
            ASSERT_NE(list, nullptr);
            ASSERT_NE(list->find(&f.ecs, "carters"), nullptr);

            const _unique_id before = life->handOn;

            ASSERT_EQ(before, list->find(&f.ecs, "carters")->root.id);
            ASSERT_EQ(life->widthStep, 0);

            // Narrower: the three columns at their second step, a page built again
            f.resize(1000.0f, 1020.0f);
            f.frames(12);

            EXPECT_FALSE(life->compact);
            ASSERT_EQ(life->widthStep, 1);

            list = life->piece<ActivityList>("activities");
            ASSERT_NE(list, nullptr);

            ActivityRow* carters = list->find(&f.ecs, "carters");
            ASSERT_NE(carters, nullptr);

            EXPECT_NE(carters->root.id, before);
            EXPECT_EQ(life->handOn, carters->root.id);
            EXPECT_TRUE(f.pos(life->hand)->visible);
            EXPECT_LE(f.pos(life->hand)->x + f.pos(life->hand)->width, f.pos(carters->root)->x + 0.5f);
            EXPECT_TRUE(life->veiled);

            EXPECT_TRUE(life->step.active);
            EXPECT_EQ(life->step.id, "guide.carters");
            EXPECT_EQ(life->noteWords.spec.text, life->step.say);
            expectNoteBeside(f, life, carters->root);
        }
    }
}
