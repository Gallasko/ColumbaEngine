#include <map>
#include <set>
#include "stdafx.h"

#ifdef __linux__
#include <SDL2/SDL.h>
#elif _WIN32
#include <SDL.h>
#endif

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "compiledrules.h"

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
#include "UI/placetile.h"
#include "UI/button.h"
#include "UI/tabs.h"
#include "UI/switch.h"
#include "UI/radiocard.h"
#include "UI/wordfield.h"
#include "UI/gloss.h"
#include "UI/focusorder.h"
#include "UI/sizer.h"
#include "Core/motion.h"
#include "Core/factrouter.h"
#include "Core/settle.h"
#include "Core/bugreport.h"
#include "Core/screenshot.h"

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
                    ecs.createSystem<SwitchSystem>();
                    ecs.createSystem<RadioCardSystem>();
                    ecs.createSystem<WordFieldSystem>();
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
                    opt.rulesRoot = compiledRules();
                    opt.pageFile = "res/chronicle/ui/life.yaml";
                    return opt;
                }

                // A first life at its first frame, with its guide. The tests of what every life
                // does ask for a later one past its sixth year (`fresh` with `lives` 2 and `age` 7):
                // the list whole, a year of rations
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

            // A row of what is bought on the spot: at the market, on the Town page, for who knows the town
            ActivityRow* stall(LifeFixture& f, LifeScene* life, const std::string& id)
            {
                auto market = life->piece<ActivityList>("marketRows");

                // Quietly: a row that is not there is nothing to log
                return market ? market->find(&f.ecs, id) : nullptr;
            }

            // A field of what the rules answered: a number as an int, a flag as true or false
            int fieldInt(const ElementMap& map, const std::string& key)
            {
                auto it = map.find(key);

                if (it == map.end())
                    return 0;

                switch (it->second.type)
                {
                case UnionType::INT:    return it->second.get<int>();
                case UnionType::FLOAT:  return static_cast<int>(it->second.get<float>());
                case UnionType::DOUBLE: return static_cast<int>(it->second.get<double>());
                case UnionType::BOOL:   return it->second.get<bool>() ? 1 : 0;
                default:                return 0;
                }
            }

            bool fieldFlag(const ElementMap& map, const std::string& key)
            {
                return fieldInt(map, key) != 0;
            }

            // A work that raises a place has no row: it is the next level of a place on the Town
            // page, begun by a click on its tile. That place, or nullptr while the work is no place's next
            const RulePlace* work(LifeFixture&, LifeScene* life, const std::string& id)
            {
                if (not life->save.townKnown)
                    return nullptr;

                for (const auto& place : life->town.places)
                {
                    auto next = place.fields.find("nextWork");

                    if (fieldFlag(place.fields, "shown") and next != place.fields.end() and next->second.toString() == id)
                        return &place;
                }

                return nullptr;
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
        // Narrower than 600, a phone held upright, the page is one column: the work at hand, one
        // row of tabs, and under it one thing at a time, inside the window. A tab's panel takes the
        // choice's place and the Life tab gives it back; wider again, the page is the compact one.
        TEST(lifescene_test, phone_page_at_390x844)
        {
            MockLogger logger;
            LifeFixture f;

            LifeScene* life = f.life();
            ASSERT_NE(life, nullptr);
            EXPECT_FALSE(life->phone);

            f.resize(390.0f, 844.0f);
            f.frames(12);

            ASSERT_TRUE(life->phone);
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

            // One row of tabs for everything, and none for a side column
            auto tabs = life->piece<Tabs>("tabs");
            ASSERT_NE(tabs, nullptr);
            ASSERT_EQ(tabs->spec.items.size(), 7u);
            EXPECT_TRUE(life->named("sideTabs").empty());
            EXPECT_EQ(tabs->spec.items[0].label, "Life");
            EXPECT_EQ(tabs->spec.items[5].label, "Town");
            EXPECT_EQ(tabs->spec.items[6].label, "Walks");

            // Seven of them hold in the column: the last one ends inside it
            ASSERT_EQ(tabs->tabs.size(), 7u);
            EXPECT_LE(f.pos(tabs->tabs[6].face)->x + f.pos(tabs->tabs[6].face)->width, 378.5f);

            // One column, 12 from each side of the window, and everything in it
            for (const char* name : {"working", "may", "parts", "holds", "clockPanel", "happened"})
            {
                EXPECT_NEAR(box(name).left, 12.0f, 0.5f) << name;
                EXPECT_NEAR(box(name).right, 378.0f, 0.5f) << name;
            }

            // The title ends before his age as it is drawn (right-aligned in its box), and the bug
            // report's button stands between them: the title takes the room up to the button, not
            // cut while the whole of it fits there
            const float ageDrawn = life->named("age")->get<TTFText>()->textWidth;
            ASSERT_GT(ageDrawn, 0.0f);
            EXPECT_NEAR(box("age").right, 378.0f, 0.5f);
            EXPECT_LE(box("title").right, box("age").right - ageDrawn + 0.5f);

            ASSERT_FALSE(life->reportButton.root.empty());
            auto report = f.pos(life->reportButton.root);
            EXPECT_LE(box("title").right, report->x + 0.5f);
            EXPECT_LE(report->x + report->width, box("age").right - ageDrawn + 0.5f);
            EXPECT_GT(box("title").right, report->x - 20.0f);

            // The note on his age is a line of its own: it does not begin with the "years" that
            // goes on from the figure beside it on a wider page
            const std::string ageNote = life->piece<Label>("ageNote")->spec.text;
            EXPECT_NE(ageNote.rfind("years", 0), 0u) << ageNote;
            EXPECT_NE(ageNote.find(" mo to the "), std::string::npos) << ageNote;

            EXPECT_GE(box("tabs").top, box("working").bottom);
            EXPECT_GE(box("may").top, box("tabs").bottom - 0.5f);
            EXPECT_LE(box("may").bottom, 844.0f);
            EXPECT_GT(box("may").bottom, 844.0f - 32.0f);

            // The choice under the tabs, two tiles to a line, and no panel with it
            EXPECT_TRUE(shown("may"));

            for (const char* side : {"parts", "holds", "clockPanel", "happened"})
                EXPECT_FALSE(shown(side)) << side;

            auto list = life->piece<ActivityList>("activities");
            ASSERT_NE(list, nullptr);
            EXPECT_EQ(list->columns(), 2);

            // The Stats tab: his parts in the choice's place, under the same tabs
            f.ecs.sendEvent(TabSelectedEvent{tabs->root.id, "life.tabs", 1});
            f.frames(12);

            EXPECT_EQ(life->phonePanel, "parts");
            EXPECT_TRUE(shown("parts"));
            EXPECT_FALSE(shown("may"));
            EXPECT_FALSE(shown("happened"));
            EXPECT_GE(box("parts").top, box("tabs").bottom - 0.5f);
            EXPECT_FALSE(life->townShown);

            // The log, to the bottom of the window
            f.ecs.sendEvent(TabSelectedEvent{tabs->root.id, "life.tabs", 4});
            f.frames(12);

            EXPECT_TRUE(shown("happened"));
            EXPECT_FALSE(shown("parts"));
            EXPECT_LE(box("happened").bottom, 844.0f);

            // The town, which he knows: the choice's panel again, with the town's page in it
            f.ecs.sendEvent(TabSelectedEvent{tabs->root.id, "life.tabs", 5});
            f.frames(12);

            EXPECT_TRUE(life->phonePanel.empty());
            EXPECT_TRUE(life->townShown);
            EXPECT_TRUE(shown("may"));
            EXPECT_TRUE(shown("town"));
            EXPECT_FALSE(shown("happened"));

            auto grid = life->piece<PlaceGrid>("places");
            ASSERT_NE(grid, nullptr);
            EXPECT_EQ(grid->columns(), 2);

            // And the Life tab gives the choice back
            f.ecs.sendEvent(TabSelectedEvent{tabs->root.id, "life.tabs", 0});
            f.frames(12);

            EXPECT_FALSE(life->townShown);
            EXPECT_TRUE(shown("may"));
            EXPECT_TRUE(shown("activities"));
            EXPECT_FALSE(shown("town"));

            // Held sideways the window is wide enough for the compact page, and back
            f.resize(844.0f, 390.0f);
            f.frames(12);

            EXPECT_FALSE(life->phone);
            EXPECT_TRUE(life->compact);
            EXPECT_FALSE(life->named("sideTabs").empty());

            f.resize(390.0f, 844.0f);
            f.frames(12);

            EXPECT_TRUE(life->phone);
            EXPECT_NEAR(box("working").right, 378.0f, 0.5f);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // On the phone page a first life's panel keeps the room of its button before a task is
        // chosen: Begin waits there greyed, and choosing a task moves nothing under the panel, so a
        // second tap on the task finds it where the first did.
        TEST(lifescene_test, phone_begin_waits_in_its_place)
        {
            MockLogger logger;
            LifeFixture f;

            LifeScene* life = f.life(LifeFixture::first());
            ASSERT_NE(life, nullptr);

            f.resize(390.0f, 844.0f);
            f.frames(12);

            ASSERT_TRUE(life->phone);

            auto list = life->piece<ActivityList>("activities");
            auto begin = life->piece<Button>("begin");
            ASSERT_NE(list, nullptr);
            ASSERT_NE(begin, nullptr);
            ASSERT_NE(list->find(&f.ecs, "glean"), nullptr);

            // Nothing chosen: the button is there, and takes no press
            EXPECT_TRUE(f.pos(life->named("begin"))->visible);
            EXPECT_TRUE(f.ecs.getEntity(begin->face.id)->get<ButtonState>()->disabled);

            const float workingBottom = f.pos(life->named("working"))->y + f.pos(life->named("working"))->height;
            const float tileY = f.pos(list->find(&f.ecs, "glean")->root)->y;

            f.ecs.sendEvent(ActivitySelectedEvent{"life.activities", "glean"});
            f.frames(12);

            // Chosen: the same button, live, and the task where it was
            EXPECT_TRUE(f.pos(life->named("begin"))->visible);
            EXPECT_FALSE(f.ecs.getEntity(begin->face.id)->get<ButtonState>()->disabled);
            EXPECT_NEAR(f.pos(life->named("working"))->y + f.pos(life->named("working"))->height, workingBottom, 0.5f);
            ASSERT_NE(list->find(&f.ecs, "glean"), nullptr);
            EXPECT_NEAR(f.pos(list->find(&f.ecs, "glean")->root)->y, tileY, 0.5f);
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
            opt.age = 7.0f;

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
            opt.age = 7.0f;

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
                opt.age = 7.5f;   // (the messages are run from seven and four months)

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
        // A child's months are shorter: half opt.monthMs in his sixth year, three quarters of it
        // until he is ten, the whole of it from there.
        TEST(lifescene_test, a_childs_months_are_shorter)
        {
            MockLogger logger;
            LifeFixture f;

            LifeScene* life = f.life(LifeFixture::first());
            ASSERT_NE(life, nullptr);

            f.frames(12);

            const float whole = life->opt.monthMs;

            ASSERT_FLOAT_EQ(life->save.age, 6.0f);
            EXPECT_FLOAT_EQ(life->monthMs(), whole * 0.5f);

            // The opening task is a month: its rule fills in half the time, and it ends there
            f.ecs.sendEvent(ActivityActivatedEvent{"life.activities", "glean"});
            f.settle();

            ASSERT_EQ(life->save.running, "glean");
            EXPECT_FLOAT_EQ(f.fact<float>("activity.running.glideMs"), whole * 0.5f);

            f.ecs.sendEvent(TickEvent{whole * 0.5f - 1.0f});
            f.settle();

            EXPECT_EQ(life->save.running, "glean");

            f.ecs.sendEvent(TickEvent{1.0f});
            f.settle();

            EXPECT_TRUE(life->save.running.empty());
            EXPECT_EQ(life->save.done["glean"], 1);
            EXPECT_NEAR(life->save.age, 6.0f + 1.0f / 12.0f, 0.0001f);

            // Seven and ten as the months reach them, one twelfth of a year at a time
            life->save.age = 6.0f;

            for (int i = 0; i < 11; ++i)
                life->save.age += 1.0f / 12.0f;

            EXPECT_FLOAT_EQ(life->monthMs(), whole * 0.5f);

            life->save.age += 1.0f / 12.0f;

            EXPECT_FLOAT_EQ(life->monthMs(), whole * 0.75f);

            for (int i = 0; i < 35; ++i)
                life->save.age += 1.0f / 12.0f;

            EXPECT_FLOAT_EQ(life->monthMs(), whole * 0.75f);

            life->save.age += 1.0f / 12.0f;

            EXPECT_FLOAT_EQ(life->monthMs(), whole);

            life->save.age = 17.5f;

            EXPECT_FLOAT_EQ(life->monthMs(), whole);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // Written time runs the months two or three times as fast: a button at the right of "At
        // work now", on the line of its last button, goes through the speeds while any is held; a
        // second played at xN takes N - 1 of it, and the months are back at their own pace the
        // frame it runs out.
        TEST(lifescene_test, written_time_runs_the_months_faster)
        {
            MockLogger logger;
            LifeFixture f;

            LifeScene* life = f.life();
            ASSERT_NE(life, nullptr);

            // A later life: a first one is walked through its guide with nothing else under its works
            life->save.lives = 2;

            const float whole = life->opt.monthMs;
            const std::string months = "Months \xC3\x97";

            // None held: its row is in the Inventory all the same, and there is no button
            EXPECT_EQ(life->written(), 0);
            EXPECT_EQ(life->haste, 1);
            EXPECT_FALSE(f.pos(life->named("haste"))->visible);

            auto ledger = life->piece<ResourceLedger>("ledger");
            ASSERT_NE(ledger, nullptr);
            ASSERT_NE(ledger->row("written"), nullptr);
            EXPECT_EQ(ledger->row("written")->figure.spec.text, "0/3600");

            const float height = f.pos(life->named("working"))->height;

            // A press with none does nothing
            life->pressHaste();
            EXPECT_EQ(life->haste, 1);

            life->save.stats["rations"] = 24;
            life->save.stats["written"] = 14;
            life->setHaste(1);
            f.frames(12);

            auto fast = life->piece<Button>("haste");
            ASSERT_NE(fast, nullptr);
            EXPECT_TRUE(f.pos(life->named("haste"))->visible);
            EXPECT_EQ(fast->spec.label, months + "1");
            EXPECT_FALSE(fast->spec.disabled);

            // At nothing it shares the line of Pass a month, at the panel's right: the panel is as tall as it was
            auto working = [&]() { return f.pos(life->named("working")); };
            auto onLine = [&](const char* name) {
                auto button = f.pos(life->named("haste"));
                auto other = f.pos(life->named(name));
                EXPECT_NEAR(button->y + button->height, other->y + other->height, 0.5f) << name;
                EXPECT_NEAR(button->x + button->width, working()->x + working()->width - 16.0f, 0.5f) << name;
            };

            EXPECT_NEAR(working()->height, height, 0.5f);
            onLine("skip");

            f.ecs.sendEvent(ActivityActivatedEvent{"life.activities", "yard"});
            f.settle();

            ASSERT_EQ(life->save.running, "yard");
            EXPECT_FLOAT_EQ(f.fact<float>("activity.running.glideMs"), whole);

            // Under a running row the panel keeps it a line of its own
            f.frames(12);
            EXPECT_TRUE(f.pos(life->named("hasteRow"))->visible);
            onLine("hasteRow");

            // Twice as fast: the month under way ends in half the time, and its length is the same
            f.ecs.sendEvent(ButtonActivatedEvent{fast->face.id, "life.haste"});
            f.settle();

            EXPECT_EQ(life->haste, 2);
            EXPECT_EQ(fast->spec.label, months + "2");
            EXPECT_FLOAT_EQ(life->monthMs(), whole);
            EXPECT_FLOAT_EQ(f.fact<float>("activity.running.glideMs"), whole / 2.0f);

            f.ecs.sendEvent(TickEvent{whole / 2.0f - 1.0f});
            f.settle();

            EXPECT_EQ(life->save.monthsIn, 0);

            f.ecs.sendEvent(TickEvent{1.0f});
            f.settle();

            // A month of his in a second of the clock, and a week of written time for it: seven seconds
            EXPECT_EQ(life->save.monthsIn, 1);
            EXPECT_EQ(life->written(), 7);

            // Held with his coin, in seconds, and its row says how fast it goes
            EXPECT_EQ(ledger->row("written")->figure.spec.text, "7/3600");
            EXPECT_EQ(ledger->row("written")->spec.rate, "\xC3\x97" "2");

            // Stopped, nothing of it is spent
            f.ecs.sendEvent(OnSDLScanCode{SDL_SCANCODE_SPACE, 0});
            f.ecs.sendEvent(TickEvent{whole * 3.0f});
            f.settle();

            EXPECT_EQ(life->save.monthsIn, 1);
            EXPECT_EQ(life->written(), 7);

            // Go on stands on its line: the room kept for it goes
            f.frames(12);
            EXPECT_FALSE(f.pos(life->named("hasteRow"))->visible);
            onLine("begin");

            f.ecs.sendEvent(OnSDLScanCode{SDL_SCANCODE_SPACE, 0});
            f.settle();

            // Three times as fast, and a month costs the same week: two months' worth of the clock
            // would take fourteen, and the seven left pay for half of what it adds. It ran a month
            // and a third; the months are back at their own pace, with two thirds of a month to run
            f.ecs.sendEvent(ButtonActivatedEvent{fast->face.id, "life.haste"});
            f.settle();

            EXPECT_EQ(life->haste, 3);
            EXPECT_EQ(fast->spec.label, months + "3");

            f.ecs.sendEvent(TickEvent{whole * 2.0f / 3.0f});
            f.settle();

            EXPECT_EQ(life->save.monthsIn, 2);
            EXPECT_EQ(life->written(), 0);
            EXPECT_EQ(life->haste, 1);
            EXPECT_EQ(fast->spec.label, months + "1");
            EXPECT_FALSE(f.pos(life->named("haste"))->visible);
            EXPECT_EQ(ledger->row("written")->figure.spec.text, "0/3600");
            EXPECT_NEAR(f.fact<float>("activity.running.glideMs"), whole * 2.0f / 3.0f, 0.5f);

            // And a month is a month again
            f.ecs.sendEvent(TickEvent{whole * 2.0f / 3.0f - 1.0f});
            f.settle();

            EXPECT_EQ(life->save.monthsIn, 2);

            f.ecs.sendEvent(TickEvent{2.0f});
            f.settle();

            EXPECT_EQ(life->save.monthsIn, 3);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // Written time multiplies the pace of his age: it does not replace it.
        TEST(lifescene_test, written_time_multiplies_a_childs_pace)
        {
            MockLogger logger;
            LifeFixture f;

            LifeScene* life = f.life(LifeFixture::first());
            ASSERT_NE(life, nullptr);

            f.frames(12);

            const float whole = life->opt.monthMs;

            f.ecs.sendEvent(ActivityActivatedEvent{"life.activities", "glean"});
            f.settle();

            ASSERT_EQ(life->save.running, "glean");

            life->save.stats["written"] = 10;
            life->setHaste(2);
            f.settle();

            // A first life has its guide under "At work now", not this button: the speed is his all the same
            EXPECT_FALSE(f.pos(life->named("haste"))->visible);
            EXPECT_EQ(life->haste, 2);
            EXPECT_FLOAT_EQ(life->monthMs(), whole * 0.5f);
            EXPECT_FLOAT_EQ(f.fact<float>("activity.running.glideMs"), whole * 0.25f);

            f.ecs.sendEvent(TickEvent{whole * 0.25f - 1.0f});
            f.settle();

            EXPECT_EQ(life->save.running, "glean");

            f.ecs.sendEvent(TickEvent{1.0f});
            f.settle();

            EXPECT_TRUE(life->save.running.empty());
            EXPECT_EQ(life->save.done["glean"], 1);

            // Asked for more than there is: the fastest it goes
            life->setHaste(9);
            EXPECT_EQ(life->haste, 3);

            life->setHaste(1);
            EXPECT_FLOAT_EQ(life->monthMs(), whole * 0.5f);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // Months that stand still with nobody at the game write time too: a minute of them with
        // no input, and it is written from its start, a second a second after, up to what can be
        // held. An input begins the count again; months that run are play.
        TEST(lifescene_test, idle_time_is_written)
        {
            MockLogger logger;
            LifeFixture f;

            LifeScene* life = f.life();
            ASSERT_NE(life, nullptr);

            ASSERT_TRUE(life->paused);
            ASSERT_EQ(life->written(), 0);

            // Less than a minute: nothing
            f.pass(59000.0f, 1000.0f);
            EXPECT_EQ(life->written(), 0);
            EXPECT_FALSE(f.pos(life->named("haste"))->visible);

            // The minute: all of it, and the button comes with it
            f.pass(1000.0f, 1000.0f);
            EXPECT_EQ(life->written(), 60);
            EXPECT_TRUE(f.pos(life->named("haste"))->visible);
            EXPECT_EQ(life->piece<ResourceLedger>("ledger")->row("written")->figure.spec.text, "60/3600");

            f.pass(5000.0f, 500.0f);
            EXPECT_EQ(life->written(), 65);

            // A key: somebody is there, and the count begins again
            f.ecs.sendEvent(OnSDLScanCode{SDL_SCANCODE_Q, 0});
            f.pass(30000.0f, 1000.0f);
            EXPECT_EQ(life->written(), 65);

            // The mouse too
            f.pass(29000.0f, 1000.0f);
            f.ecs.sendEvent(OnSDLMouseWheel{0, 1});
            f.pass(59000.0f, 1000.0f);
            EXPECT_EQ(life->written(), 65);

            // Months that run write nothing, however long nobody touches the game
            life->save.stats["rations"] = 24;
            f.ecs.sendEvent(ActivityActivatedEvent{"life.activities", "yard"});
            f.settle();

            ASSERT_EQ(life->save.running, "yard");

            f.pass(70000.0f, 70000.0f);
            EXPECT_EQ(life->written(), 65);

            // No more than can be held
            ASSERT_TRUE(life->paused);
            life->save.stats["written"] = 3590;
            f.pass(120000.0f, 1000.0f);
            EXPECT_EQ(life->written(), 3600);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // The time the chronicle stays shut is written time: a second for a second from the hour
        // the save was written, an hour of it at most, and none for a page loaded again. It is
        // the player's: a new life has what the last one held.
        TEST(lifescene_test, time_away_is_written)
        {
            MockLogger logger;
            LifeFixture f;

            const std::string path = "save/test_life_written.sz";
            std::remove(path.c_str());

            LifeSceneOptions opt = LifeFixture::mockup();
            opt.noSave = false;
            opt.savePath = path;
            opt.now = 1000000;

            LifeScene* life = f.life(opt);
            ASSERT_NE(life, nullptr);

            // A later life, kept by the save: a first one shows no button while its guide is said
            life->save.lives = 2;

            EXPECT_EQ(life->written(), 0);
            ASSERT_TRUE(life->saveNow());
            EXPECT_EQ(life->save.savedAt, 1000000u);

            f.load<EmptyScene>();

            // Half a minute: a page loaded again
            opt.now += 30;

            LifeScene* soon = f.life(opt);
            ASSERT_NE(soon, nullptr);

            EXPECT_EQ(soon->written(), 0);
            EXPECT_FALSE(f.pos(soon->named("haste"))->visible);
            ASSERT_TRUE(soon->saveNow());

            f.load<EmptyScene>();

            // Ten minutes
            opt.now += 600;

            LifeScene* later = f.life(opt);
            ASSERT_NE(later, nullptr);

            f.frames(12);

            EXPECT_EQ(later->written(), 600);
            EXPECT_EQ(f.fact<int>("stat.written"), 600);

            auto ledger = later->piece<ResourceLedger>("ledger");
            ASSERT_NE(ledger, nullptr);
            ASSERT_NE(ledger->row("written"), nullptr);
            EXPECT_EQ(ledger->row("written")->figure.spec.text, "600/3600");
            EXPECT_EQ(ledger->row("written")->name.spec.text, "Written time");

            ASSERT_NE(later->piece<Button>("haste"), nullptr);
            EXPECT_TRUE(f.pos(later->named("haste"))->visible);
            EXPECT_FALSE(later->piece<Button>("haste")->spec.disabled);
            EXPECT_EQ(later->haste, 1);

            bool said = false;

            for (const auto& entry : later->save.log)
                said = said or entry.text.rfind("600 seconds were written", 0) == 0;

            EXPECT_TRUE(said);

            // Its gloss says what it stops at, what a month takes of it, and how many months it runs
            // faster: 600 seconds at 7 a month
            auto registry = f.ecs.getSystem<GlossRegistry>();
            ASSERT_NE(registry, nullptr);

            const GlossSpec* gloss = registry->find("resource/written");
            ASSERT_NE(gloss, nullptr);
            ASSERT_GE(gloss->rows.size(), 3u);
            EXPECT_EQ(gloss->rows[0].value, "600/3600");
            EXPECT_EQ(gloss->rows[1].value, "7 s");
            EXPECT_EQ(gloss->rows[2].value, "85 mo");

            ASSERT_TRUE(later->saveNow());

            f.load<EmptyScene>();

            // A day: an hour is all that is held
            opt.now += 86400;

            LifeScene* last = f.life(opt);
            ASSERT_NE(last, nullptr);

            EXPECT_EQ(last->written(), 3600);

            // Opened once more at the same hour: counted once
            ASSERT_TRUE(last->saveNow());

            last->save.savedAt = static_cast<size_t>(opt.now);
            last->creditAway();

            EXPECT_EQ(last->written(), 3600);

            // The next life is born with nothing of his, and with this
            last->newLife();
            f.frames(12);

            EXPECT_FLOAT_EQ(last->save.age, 6.0f);
            EXPECT_EQ(last->save.lives, 3);
            EXPECT_EQ(last->save.stats["coin"], 0);
            EXPECT_EQ(last->written(), 3600);
            ASSERT_NE(last->piece<ResourceLedger>("ledger")->row("written"), nullptr);
            EXPECT_EQ(last->piece<ResourceLedger>("ledger")->row("written")->figure.spec.text, "3600/3600");

            std::remove(path.c_str());
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
            // (not strong enough to carry for them in two: Strength 9 shortens it)
            life->save.stats["str"] = 8;

            life->onMonth();
            f.settle();

            ASSERT_NE(list->find(&f.ecs, "carters"), nullptr);
            EXPECT_EQ(list->find(&f.ecs, "carters")->spec.closes, 0);

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
        // A familiar bound has its row under FAMILIARS, and makes its berries from the next month on
        TEST(lifescene_test, a_bound_familiar_has_its_row)
        {
            MockLogger logger;
            LifeFixture f;

            LifeSceneOptions opt = LifeFixture::mockup();
            opt.fresh = true;
            opt.lives = 2;
            opt.age = 8.0f;

            LifeScene* life = f.life(opt);
            ASSERT_NE(life, nullptr);

            auto ledger = life->piece<ResourceLedger>("ledger");
            ASSERT_NE(ledger, nullptr);
            EXPECT_EQ(ledger->row("fam.sprite"), nullptr);

            // He has scavenged twice and holds what binding a sprite asks
            life->save.stats["rations"] = 24;
            life->save.stats["scavenged"] = 2;
            life->save.stats["berries"] = 6;

            life->onMonth();
            f.settle();

            ASSERT_NE(life->piece<ActivityList>("activities")->find(&f.ecs, "bind.sprite"), nullptr);

            f.ecs.sendEvent(ActivitySelectedEvent{"life.activities", "bind.sprite"});
            f.settle();
            f.ecs.sendEvent(ActivityActivatedEvent{"life.activities", "bind.sprite"});
            f.settle();

            ASSERT_EQ(life->save.running, "bind.sprite");

            life->onMonth();
            f.frames(12);

            EXPECT_EQ(life->save.stats["fam.sprite"], 1);
            ASSERT_NE(ledger->row("fam.sprite"), nullptr);
            EXPECT_EQ(ledger->row("fam.sprite")->name.spec.text, "A hedge sprite");

            // Fed, it makes its berries a month
            const int berries = life->save.stats["berries"];

            life->onMonth();
            f.frames(12);

            EXPECT_EQ(life->save.stats["berries"], berries + 4);
        }

        // With no slot free there is no bind row on the page; when a slot comes the row he has seen
        // before comes back without being told as new
        TEST(lifescene_test, bind_rows_wait_for_a_free_slot)
        {
            MockLogger logger;
            LifeFixture f;

            LifeSceneOptions opt = LifeFixture::mockup();
            opt.fresh = true;
            opt.lives = 2;
            opt.age = 8.0f;

            LifeScene* life = f.life(opt);
            ASSERT_NE(life, nullptr);

            auto list = life->piece<ActivityList>("activities");
            ASSERT_NE(list, nullptr);

            // The page has arrived: from here a toast would be shown
            f.pass(800.0f);

            life->save.stats["rations"] = 24;
            life->save.stats["scavenged"] = 2;
            life->save.stats["berries"] = 12;
            life->onMonth();
            f.settle();

            // The first time it comes, it is news
            ASSERT_NE(list->find(&f.ecs, "bind.sprite"), nullptr);
            const std::string news = "New: Bind a hedge sprite";
            EXPECT_TRUE(life->toastUp(news));

            // His one slot filled: no bind row, and the lesson of the one he has is there
            life->save.stats["fam.sprite"] = 1;
            life->onMonth();
            f.settle();

            EXPECT_EQ(list->find(&f.ecs, "bind.sprite"), nullptr);
            EXPECT_NE(list->find(&f.ecs, "train.sprite"), nullptr);

            // What was toasted runs out
            f.pass(5000.0f);
            ASSERT_FALSE(life->toastUp(news));

            // A slot with his standing: the row is back, and it is not news
            life->save.stats["keep_oath"] = 1;
            life->onMonth();
            f.settle();

            EXPECT_NE(list->find(&f.ecs, "bind.sprite"), nullptr);
            EXPECT_FALSE(life->toastUp(news));
        }

        // What he gathers by hand is counted, but the count is the rules' own: the tile, its gloss
        // and the log say the berries alone
        TEST(lifescene_test, gathering_shows_no_count_of_it)
        {
            MockLogger logger;
            LifeFixture f;

            LifeSceneOptions opt = LifeFixture::mockup();
            opt.fresh = true;
            opt.lives = 2;
            opt.age = 8.0f;

            LifeScene* life = f.life(opt);
            ASSERT_NE(life, nullptr);

            life->save.stats["rations"] = 24;
            life->onMonth();
            f.settle();

            auto tile = life->piece<ActivityList>("activities")->find(&f.ecs, "scavenge.berries");
            ASSERT_NE(tile, nullptr);
            ASSERT_EQ(tile->spec.gains.size(), 1u);
            EXPECT_EQ(tile->spec.gains[0].amount, 6);

            auto registry = f.ecs.getSystem<GlossRegistry>();
            ASSERT_NE(registry, nullptr);
            const GlossSpec* gloss = registry->find("activity/scavenge.berries");
            ASSERT_NE(gloss, nullptr);

            for (const auto& row : gloss->rows)
                EXPECT_EQ(row.label.find("gathered by hand"), std::string::npos) << row.label;

            f.ecs.sendEvent(ActivitySelectedEvent{"life.activities", "scavenge.berries"});
            f.settle();
            f.ecs.sendEvent(ActivityActivatedEvent{"life.activities", "scavenge.berries"});
            f.settle();
            ASSERT_EQ(life->save.running, "scavenge.berries");

            const size_t before = life->save.log.size();
            life->onMonth();
            f.frames(12);

            EXPECT_EQ(life->save.stats["scavenged"], 1);
            ASSERT_GT(life->save.log.size(), before);

            bool berries = false;

            for (size_t i = before; i < life->save.log.size(); ++i)
            {
                const std::string& figure = life->save.log[i].figure;
                EXPECT_EQ(figure.find("scavenged"), std::string::npos) << figure;

                if (figure.find("berries") != std::string::npos)
                    berries = true;
            }

            EXPECT_TRUE(berries);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // A gloss says what a part or a skill of his would make of a work, before he has it, and
        // marks it once he has.
        TEST(lifescene_test, a_gloss_says_what_a_stat_would_bring)
        {
            MockLogger logger;
            LifeFixture f;

            LifeSceneOptions opt = LifeFixture::mockup();
            opt.fresh = true;
            opt.lives = 2;
            opt.age = 8.0f;

            LifeScene* life = f.life(opt);
            ASSERT_NE(life, nullptr);

            life->save.stats["rations"] = 24;
            life->save.stats["dex"] = 8;
            life->onMonth();
            f.settle();

            auto registry = f.ecs.getSystem<GlossRegistry>();
            ASSERT_NE(registry, nullptr);

            auto section = [](const GlossSpec* g, const std::string& heading) {
                std::vector<GlossRow> rows;
                bool inside = false;

                for (const auto& r : g->rows)
                {
                    if (r.heading)
                        inside = r.label == heading;
                    else if (inside)
                        rows.push_back(r);
                }

                return rows;
            };

            // The berries: what Dexterity 8 makes of them, which he has, and what 12 would
            const GlossSpec* picking = registry->find("activity/scavenge.berries");
            ASSERT_NE(picking, nullptr);

            auto steps = section(picking, "BONUS");
            ASSERT_EQ(steps.size(), 2u);
            EXPECT_EQ(steps[0].label, "With Dexterity 8: +3 berries");
            EXPECT_EQ(steps[0].tone, "gain");
            EXPECT_EQ(steps[1].label, "With Dexterity 12: +6 berries");
            EXPECT_EQ(steps[1].tone, "");

            auto reward = section(picking, "REWARD");
            ASSERT_EQ(reward.size(), 1u);
            EXPECT_EQ(reward[0].value, "+9");
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // A gloss says once what a work takes, with what he holds of it, and asks for none of it
        // among its requirements. A work he cannot pay for stays locked.
        TEST(lifescene_test, a_gloss_says_a_cost_once)
        {
            MockLogger logger;
            LifeFixture f;

            LifeSceneOptions opt = LifeFixture::mockup();
            opt.fresh = true;
            opt.lives = 2;
            opt.age = 8.0f;

            LifeScene* life = f.life(opt);
            ASSERT_NE(life, nullptr);

            life->save.stats["rations"] = 24;
            life->save.stats["berries"] = 4;
            life->onMonth();
            f.settle();

            auto registry = f.ecs.getSystem<GlossRegistry>();
            ASSERT_NE(registry, nullptr);

            auto section = [](const GlossSpec* g, const std::string& heading) {
                std::vector<GlossRow> rows;
                bool inside = false;

                for (const auto& r : g->rows)
                {
                    if (r.heading)
                        inside = r.label == heading;
                    else if (inside)
                        rows.push_back(r);
                }

                return rows;
            };

            // Drying berries takes three, and he has four: nothing asked, the cost says what he has
            const GlossSpec* drying = registry->find("activity/dry.berries");
            ASSERT_NE(drying, nullptr);

            for (const auto& r : drying->rows)
                EXPECT_NE(r.label, "REQUIREMENTS");

            auto dried = section(drying, "COST");
            ASSERT_EQ(dried.size(), 1u);
            EXPECT_EQ(dried[0].label, "Berries");
            EXPECT_EQ(dried[0].value, "\xE2\x88\x92" "3 (has 4)");
            EXPECT_EQ(dried[0].tone, "");

            // The pies take six: he is short, in the loss's colour, and the row stays locked
            const GlossSpec* pies = registry->find("activity/pies");
            ASSERT_NE(pies, nullptr);

            for (const auto& r : section(pies, "REQUIREMENTS"))
                EXPECT_NE(r.label, "Berries");

            auto baked = section(pies, "COST");
            ASSERT_EQ(baked.size(), 1u);
            EXPECT_EQ(baked[0].label, "Berries");
            EXPECT_EQ(baked[0].value, "\xE2\x88\x92" "6 (has 4)");
            EXPECT_EQ(baked[0].tone, "loss");

            auto list = life->piece<ActivityList>("activities");
            ASSERT_NE(list, nullptr);
            ASSERT_NE(list->find(&f.ecs, "pies"), nullptr);
            EXPECT_EQ(list->find(&f.ecs, "pies")->spec.state, ActivityState::Locked);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // A kind learned from a study row is kept by the world: the next life can bind it.
        TEST(lifescene_test, a_kind_learned_is_kept_by_the_world)
        {
            MockLogger logger;
            LifeFixture f;

            LifeSceneOptions opt = LifeFixture::mockup();
            opt.fresh = true;
            opt.lives = 2;
            opt.age = 9.5f;

            LifeScene* life = f.life(opt);
            ASSERT_NE(life, nullptr);

            life->save.stats["rations"] = 24;
            life->save.stats["letters"] = 1;

            life->onMonth();
            f.settle();

            ASSERT_NE(life->piece<ActivityList>("activities")->find(&f.ecs, "research.imp"), nullptr);

            f.ecs.sendEvent(ActivitySelectedEvent{"life.activities", "research.imp"});
            f.settle();
            f.ecs.sendEvent(ActivityActivatedEvent{"life.activities", "research.imp"});
            f.settle();

            ASSERT_EQ(life->save.running, "research.imp");

            for (int month = 0; month < 3; ++month)
            {
                life->onMonth();
                f.frames(12);
            }

            EXPECT_EQ(life->save.knowledge["fam.imp"], 1);

            life->newLife();
            f.settle();

            EXPECT_EQ(life->save.knowledge["fam.imp"], 1);

            life->save.age = 8.0f;
            life->save.stats["scavenged"] = 2;
            life->save.stats["rations"] = 24;
            life->onMonth();
            f.settle();

            EXPECT_NE(life->piece<ActivityList>("activities")->find(&f.ecs, "bind.imp"), nullptr);
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
            opt.age = 7.0f;

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

            // The next life begins at 6, on the same calendar
            life->beginAgain();
            f.settle();

            EXPECT_FLOAT_EQ(life->save.age, 6.0f);
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

            EXPECT_FLOAT_EQ(life->save.age, 6.0f);
            EXPECT_EQ(life->save.world, 16);
            EXPECT_EQ(note(), "MONTH 5 \xC2\xB7 SPRING \xC2\xB7 BELLMOOR");
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // An event of his life does not go by under a long work. The month it comes the months stop
        // and say so, its answers are in red with a mark before their names, and an answer may be
        // begun at once: the work is set aside, and goes on where it stood once the answer is done.
        TEST(lifescene_test, an_event_stops_the_months_and_sets_a_work_aside)
        {
            MockLogger logger;
            LifeFixture f;

            LifeSceneOptions opt = LifeFixture::mockup();
            opt.fresh = true;
            opt.lives = 2;
            opt.age = 10.75f;

            LifeScene* life = f.life(opt);
            ASSERT_NE(life, nullptr);

            auto said = [&](const std::string& text) {
                for (const auto& entry : life->save.log)
                {
                    if (entry.text.find(text) != std::string::npos)
                        return true;
                }

                return false;
            };

            life->save.stats["rations"] = 60;

            // A year at the Watch, begun three months before the Flood
            life->save.running = "watch";
            life->save.monthsIn = 0;
            life->rules.running = "watch";
            life->paused = false;

            life->onMonth();
            life->onMonth();
            f.settle();

            EXPECT_FALSE(life->paused);
            EXPECT_FALSE(said("The Flood!"));

            // Eleven: the Flood, and the months stop on it
            life->onMonth();
            f.frames(12);

            EXPECT_EQ(life->save.monthsIn, 3);
            EXPECT_TRUE(life->paused);
            EXPECT_EQ(life->save.running, "watch");
            EXPECT_TRUE(said("The Flood! Bellmoor calls on him"));

            auto list = life->piece<ActivityList>("activities");
            ASSERT_NE(list, nullptr);

            // On top of the list, framed in red, with the mark on its corner
            ASSERT_FALSE(list->spec.groups.empty());
            EXPECT_EQ(list->spec.groups[0].label, "The Flood");

            ActivityRow* salvage = list->find(&f.ecs, "flood.salvage");
            ASSERT_NE(salvage, nullptr);
            EXPECT_EQ(salvage->spec.name, "Salvage the Wreck");
            EXPECT_TRUE(salvage->spec.urgent);
            EXPECT_EQ(salvage->spec.badge, "alert");
            ASSERT_TRUE(salvage->badge.has_value());
            EXPECT_EQ(salvage->badge->spec.name, "alert");

            // (the works are under it)
            ActivityRow* watch = list->find(&f.ecs, "watch");
            ASSERT_NE(watch, nullptr);
            EXPECT_TRUE(watch->spec.badge.empty());
            EXPECT_FALSE(watch->badge.has_value());

            // Chosen, the button under the stopped work answers it instead of going on
            f.ecs.sendEvent(ActivitySelectedEvent{"life.activities", "flood.salvage"});
            f.frames(12);

            EXPECT_EQ(life->piece<Button>("begin")->spec.label, "Answer now");

            f.ecs.sendEvent(ButtonActivatedEvent{life->piece<Button>("begin")->face.id, "life.begin"});
            f.settle();

            EXPECT_EQ(life->save.running, "flood.salvage");
            EXPECT_EQ(life->save.setAside, "watch");
            EXPECT_EQ(life->save.setAsideMonths, 3);
            EXPECT_FALSE(life->paused);
            EXPECT_TRUE(said("Carry for the Watch is set aside for Salvage the Wreck"));

            // A month later the answer is given, and the Watch goes on from its third month
            life->onMonth();
            f.settle();

            EXPECT_EQ(life->save.done["flood.salvage"], 1);
            EXPECT_EQ(life->save.running, "watch");
            EXPECT_EQ(life->save.monthsIn, 3);
            EXPECT_TRUE(life->save.setAside.empty());
            EXPECT_FALSE(life->paused);
            EXPECT_TRUE(said("Back to Carry for the Watch"));

            // Answered: the months do not stop for it again, and it is never stood by
            for (int i = 0; i < 12 and not life->save.running.empty(); ++i)
            {
                life->onMonth();
                EXPECT_FALSE(life->paused and not life->save.running.empty()) << i;
            }

            EXPECT_EQ(life->save.stats.count("stood_flood"), 0u);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // Left unanswered, an event stops the months once more, the last month an answer can be
        // begun. A new life has its events still to come.
        TEST(lifescene_test, an_event_unanswered_stops_the_months_at_its_last_month)
        {
            MockLogger logger;
            LifeFixture f;

            LifeSceneOptions opt = LifeFixture::mockup();
            opt.fresh = true;
            opt.lives = 2;
            opt.age = 11.0f - 1.0f / 12.0f;

            LifeScene* life = f.life(opt);
            ASSERT_NE(life, nullptr);

            life->save.stats["rations"] = 60;

            auto calls = [&]() {
                int n = 0;

                for (const auto& entry : life->save.log)
                {
                    if (entry.text.find("The Flood!") != std::string::npos)
                        ++n;
                }

                return n;
            };

            auto work = [&]() {
                life->save.running = "watch";
                life->save.monthsIn = 0;
                life->rules.running = "watch";
                life->paused = false;
            };

            work();

            // Eleven: the first stop
            life->onMonth();
            f.settle();

            EXPECT_TRUE(life->paused);
            EXPECT_EQ(calls(), 1);

            // Gone on with: no stop until the last month to begin a one-month answer (11y10)
            life->paused = false;

            for (int i = 0; i < 9; ++i)
            {
                life->onMonth();
                EXPECT_FALSE(life->paused) << i;
            }

            life->onMonth();
            f.settle();

            EXPECT_TRUE(life->paused);
            EXPECT_EQ(calls(), 2);
            EXPECT_NE(life->save.log.back().text.find("last month to answer it"), std::string::npos);

            // A new life forgets what the last one was told
            life->newLife();
            f.settle();

            EXPECT_TRUE(life->eventsCalled.empty());
            EXPECT_TRUE(life->eventsLastCalled.empty());
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
            EXPECT_EQ(list->find(&f.ecs, "carters")->spec.closes, -1);
            ASSERT_TRUE(list->find(&f.ecs, "carters")->status.has_value());
            EXPECT_EQ(list->find(&f.ecs, "carters")->status->spec.text, "");

            // A year and a half from 30, a month at a time: three months of work, begun by 29.75
            life->save.age = 28.5f;
            life->save.stats["rations"] = 60;
            // (not strong enough to carry for them in two: Strength 9 shortens it)
            life->save.stats["str"] = 8;

            life->onMonth();
            f.settle();

            ActivityRow* carters = list->find(&f.ecs, "carters");
            ASSERT_NE(carters, nullptr);

            // Fourteen months to begin it, counted on its tile
            EXPECT_EQ(carters->spec.closes, 14);
            EXPECT_FALSE(carters->spec.urgent);
            ASSERT_TRUE(carters->status.has_value());
            EXPECT_EQ(carters->status->spec.text, "14 mo");
            EXPECT_TRUE(f.pos(carters->status->entity)->visible);
            EXPECT_FALSE(carters->until.has_value());

            life->onMonth();
            f.settle();

            carters = list->find(&f.ecs, "carters");
            ASSERT_NE(carters, nullptr);
            // A month on, a month fewer
            EXPECT_EQ(carters->spec.closes, 13);
            EXPECT_EQ(carters->status->spec.text, "13 mo");

            // Half a year left: urgent
            life->save.age = 29.25f - 1.0f / 12.0f;

            life->onMonth();
            f.settle();

            carters = list->find(&f.ecs, "carters");
            ASSERT_NE(carters, nullptr);
            EXPECT_EQ(carters->spec.closes, 6);
            EXPECT_TRUE(carters->spec.urgent);
            EXPECT_EQ(carters->status->spec.text, "6 mo");
            EXPECT_EQ(f.ecs.getEntity(carters->frame.id)->get<ThemeComponent>()->element, "activity.tile.frame.urgent");

            // An old man's work is listed by now, and is never closed
            ActivityRow* tales = list->find(&f.ecs, "tales");
            ASSERT_NE(tales, nullptr);
            EXPECT_EQ(tales->spec.closes, -1);
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

            // What he gleans feeds him: the first row of the table, chosen and lit
            EXPECT_EQ(list->selected(), "glean");
            EXPECT_TRUE(f.pos(life->named("guide"))->visible);
            EXPECT_TRUE(f.pos(life->named("begin"))->visible);
            EXPECT_FALSE(f.pos(life->named("skip"))->visible);
            EXPECT_TRUE(life->piece<Button>("skip")->spec.disabled);

            // The page chose it, not the player
            EXPECT_EQ(life->save.picks, 0);

            // Begin begins it
            f.ecs.sendEvent(ButtonActivatedEvent{life->piece<Button>("begin")->face.id, "life.begin"});
            f.settle();

            ASSERT_EQ(life->save.running, "glean");
            EXPECT_EQ(life->save.begun, 1);
            EXPECT_EQ(list->selected(), "");
            EXPECT_FALSE(f.pos(life->named("guide"))->visible);

            // Its term over, he is at nothing as any life is: a month to pass, nothing chosen, no line
            life->onMonth();
            f.frames(12);

            ASSERT_TRUE(life->save.running.empty());
            EXPECT_EQ(life->save.done["glean"], 1);
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
            opt.age = 7.0f;

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

                // What it asks is its gloss's: the tile says nothing of it
                EXPECT_EQ(way->spec.until, "") << id;
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
            opt.age = 7.0f;

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
        // The works the digest names one by one once they are done: his sixth year's (which of the
        // two he took, which two of the three), the ways into a class and what opens at their age
        // or later. Not the town's work of a boy, a walk, nor what is bought.
        TEST(lifescene_test, the_digest_names_the_works_of_his_sixth_year_and_of_the_end_of_a_life)
        {
            MockLogger logger;
            LifeFixture f;

            LifeScene* life = f.life(LifeFixture::mockup());
            ASSERT_NE(life, nullptr);

            const auto named = life->namedWorks();
            auto has = [&](const std::string& id) { return std::find(named.begin(), named.end(), id) != named.end(); };

            for (const char* id : {"glean", "flour", "wheel", "scribe", "tome", "knots", "sacks", "sing", "tomarket", "harvest", "helping"})
                EXPECT_TRUE(has(id)) << id;

            for (const char* id : {"keep", "collegium", "hand", "greenwood", "serve", "campaign", "captain", "ritual", "vault", "silence", "woodking", "tales", "garden", "teach"})
                EXPECT_TRUE(has(id)) << id;

            for (const char* id : {"carters", "yard", "mara", "buy.rations", "buy.reagents", "walk.hedgerows"})
                EXPECT_FALSE(has(id)) << id;

            // A work of his class done, one of his sixth year, and one that is neither
            life->save.done["serve"] = 2;
            life->save.done["scribe"] = 1;
            life->save.done["yard"] = 3;

            const std::string digest = life->save.digest(named);

            EXPECT_NE(digest.find("\"did.scribe\":1"), std::string::npos);
            EXPECT_NE(digest.find("\"did.serve\":2"), std::string::npos);
            EXPECT_EQ(digest.find("did.yard"), std::string::npos);
            EXPECT_EQ(digest.find("did.wheel"), std::string::npos);
            EXPECT_NE(digest.find("\"v\":\"0.6\""), std::string::npos);
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
        // Vitality: the head of "At work now" says it is paused, the log says why and what goes on:
        // a button under the work, for a screen with no key, or SPACE.
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
            EXPECT_EQ(life->save.log.back().text, "Vitality is failing: the months stop. Press Go on, or SPACE");

            // The button that goes on is under the work, live
            auto begin = life->piece<Button>("begin");
            ASSERT_NE(begin, nullptr);
            EXPECT_TRUE(f.pos(life->named("begin"))->visible);
            EXPECT_EQ(begin->spec.label, "Go on");
            EXPECT_FALSE(f.ecs.getEntity(begin->face.id)->get<ButtonState>()->disabled);

            // Stopped, no month passes
            const int monthsIn = life->save.monthsIn;

            f.ecs.sendEvent(TickEvent{life->opt.monthMs * 3.0f});
            f.settle();

            EXPECT_EQ(life->save.monthsIn, monthsIn);

            // SPACE goes on, and the button leaves; SPACE again stops the work, and the button is back
            f.ecs.sendEvent(OnSDLScanCode{SDL_SCANCODE_SPACE, 0});
            f.settle();

            EXPECT_FALSE(life->paused);
            EXPECT_FALSE(f.pos(life->named("begin"))->visible);

            f.ecs.sendEvent(OnSDLScanCode{SDL_SCANCODE_SPACE, 0});
            f.settle();

            ASSERT_TRUE(life->paused);
            EXPECT_TRUE(f.pos(life->named("begin"))->visible);

            // Pressed, it goes on as the key does: a second failing month does not stop it again,
            // nor write the line again
            f.ecs.sendEvent(ButtonActivatedEvent{begin->face.id, "life.begin"});
            f.settle();

            EXPECT_FALSE(life->paused);
            EXPECT_EQ(life->save.running, "yard");
            EXPECT_FALSE(f.pos(life->named("begin"))->visible);

            const size_t afterPause = life->save.log.size();

            f.ecs.sendEvent(TickEvent{life->opt.monthMs});
            f.settle();

            EXPECT_FALSE(life->paused);
            EXPECT_EQ(life->save.monthsIn, monthsIn + 1);

            for (size_t i = afterPause; i < life->save.log.size(); ++i)
                EXPECT_EQ(life->save.log[i].text.find("the months stop"), std::string::npos);
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
            opt.age = 7.0f;

            LifeScene* life = f.life(opt);
            ASSERT_NE(life, nullptr);

            // The letters burn a little firewood for the light to read by
            life->save.stats["firewood"] = 2;

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
            opt.age = 7.0f;

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

            // Under REQUIREMENTS: the smithy brings Strength too, under the same name in its own section
            bool asking = false;
            bool asked = false;

            for (const auto& r : smithy->rows)
            {
                if (r.heading)
                {
                    asking = r.label == "REQUIREMENTS";
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
            opt.age = 7.0f;

            LifeScene* life = f.life(opt);
            ASSERT_NE(life, nullptr);

            auto ledger = life->piece<ResourceLedger>("ledger");
            auto skills = life->piece<ResourceLedger>("skills");
            ASSERT_NE(ledger, nullptr);
            ASSERT_NE(skills, nullptr);

            EXPECT_EQ(ledger->row("coin"), nullptr);

            for (const char* id : {"arms", "discipline", "letters", "lore", "arcana", "stealth", "guile", "renown"})
                EXPECT_EQ(skills->row(id), nullptr) << id;

            // The letters burn a little firewood for the light to read by
            life->save.stats["firewood"] = 2;

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
            opt.age = 7.0f;

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
        // Work that feeds him gives the rations of its months as he begins it, and the log says so;
        // he eats them a month at a time as he eats any, and ends the work with what he had.
        TEST(lifescene_test, work_that_feeds_him_gives_rations_up_front)
        {
            MockLogger logger;
            LifeFixture f;

            LifeScene* life = f.life();
            ASSERT_NE(life, nullptr);

            auto ledger = life->piece<ResourceLedger>("ledger");
            ASSERT_NE(ledger, nullptr);
            ASSERT_NE(ledger->row("rations"), nullptr);
            // (not strong enough to carry for them in two: Strength 9 shortens it)
            life->save.stats["str"] = 8;

            ASSERT_EQ(life->save.stats["rations"], 3);

            const size_t before = life->save.log.size();

            f.ecs.sendEvent(ActivityActivatedEvent{"life.activities", "carters"});
            f.settle();

            ASSERT_EQ(life->save.running, "carters");
            EXPECT_EQ(life->save.stats["rations"], 6);
            EXPECT_EQ(ledger->row("rations")->spec.rate, "\xE2\x88\x92" "1 / mo");

            bool said = false;

            for (size_t i = before; i < life->save.log.size(); ++i)
            {
                if (life->save.log[i].text == "Carry for the Carters gives its rations up front")
                {
                    said = true;
                    EXPECT_EQ(life->save.log[i].figure.rfind("+3", 0), 0u) << life->save.log[i].figure;
                }
            }

            EXPECT_TRUE(said);

            for (int i = 0; i < 3; ++i)
            {
                life->onMonth();
                f.settle();

                EXPECT_EQ(life->save.stats["rations"], 5 - i);
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
        // A work that feeds him, set aside for an answer, closed before he could go back to it: he
        // left it before its term, and the rations it gave for the months he never worked there go
        // back, never more than he holds.
        TEST(lifescene_test, a_fed_work_left_early_gives_back_its_rations)
        {
            for (int held : {20, 5})
            {
                MockLogger logger;
                LifeFixture f;

                LifeSceneOptions opt = LifeFixture::mockup();
                opt.fresh = true;
                opt.lives = 2;
                opt.age = 19.0f;

                LifeScene* life = f.life(opt);
                ASSERT_NE(life, nullptr);

                // Six months into the Keep's eighteen, set aside; the carters' last month under way
                life->save.stats["rations"] = held;
                life->save.setAside = "keep";
                life->save.setAsideMonths = 6;
                life->save.running = "carters";
                life->save.monthsIn = 2;
                life->rules.running = "carters";

                const size_t before = life->save.log.size();

                life->onMonth();
                f.settle();

                // At nineteen the Keep can no longer be served out by twenty: it is not taken up
                EXPECT_TRUE(life->save.setAside.empty());
                EXPECT_EQ(life->save.running, "");

                bool closed = false;
                bool back = false;

                for (size_t i = before; i < life->save.log.size(); ++i)
                {
                    const LogEntry& entry = life->save.log[i];

                    if (entry.text.find("is closed to him now") != std::string::npos)
                        closed = true;

                    if (entry.text == "Swear Service to the Keep, left before its term: the rations of the months not worked go back")
                    {
                        back = true;
                        EXPECT_EQ(entry.figure.rfind("\xE2\x88\x92" + std::to_string(std::min(12, held - 1)), 0), 0u) << entry.figure;
                    }
                }

                EXPECT_TRUE(closed);
                EXPECT_TRUE(back);

                // He ate his one; the twelve months he never worked there take back twelve, or what is left
                EXPECT_EQ(life->save.stats["rations"], std::max(0, held - 1 - 12)) << held;
            }
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // A save from before the rations were given up front, written in the middle of a work that
        // feeds him: that work never gave them, and the months it has left must not starve him. It
        // is given them once, as it is opened.
        TEST(lifescene_test, an_old_save_at_a_fed_work_is_given_its_rations)
        {
            MockLogger logger;
            LifeFixture f;

            const std::string path = "save/test_life_up_front.sz";
            std::remove(path.c_str());

            LifeScene* life = f.life();
            ASSERT_NE(life, nullptr);

            // A life begun today has them up front already
            EXPECT_TRUE(life->save.boardUpFront);

            // As it was written before: a month into the carters' three, nothing given for them
            LifeSave old = life->save;
            old.running = "carters";
            old.monthsIn = 1;
            old.stats["rations"] = 3;
            old.stats["str"] = 8;
            old.boardUpFront = false;
            ASSERT_TRUE(old.save(path));

            LifeSave read;
            ASSERT_TRUE(read.load(path));
            EXPECT_FALSE(read.boardUpFront);

            f.load<EmptyScene>();

            LifeSceneOptions opt = LifeFixture::mockup();
            opt.noSave = false;
            opt.savePath = path;

            LifeScene* again = f.life(opt);
            ASSERT_NE(again, nullptr);

            // The two months it has left
            EXPECT_EQ(again->save.running, "carters");
            EXPECT_EQ(again->save.monthsIn, 1);
            EXPECT_EQ(again->save.stats["rations"], 5);
            EXPECT_TRUE(again->save.boardUpFront);

            // Written and opened again, they are not given twice
            ASSERT_TRUE(again->saveNow());
            f.load<EmptyScene>();

            LifeScene* third = f.life(opt);
            ASSERT_NE(third, nullptr);
            EXPECT_EQ(third->save.stats["rations"], 5);

            // A save of today at the same work had them as it began: nothing more
            LifeSave today = third->save;
            today.stats["rations"] = 3;
            ASSERT_TRUE(today.boardUpFront);
            ASSERT_TRUE(today.save(path));
            f.load<EmptyScene>();

            LifeScene* fourth = f.life(opt);
            ASSERT_NE(fourth, nullptr);
            EXPECT_EQ(fourth->save.stats["rations"], 3);

            std::remove(path.c_str());
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // What an old save is given is written at once: opened again before a month has passed, and
        // with nothing saved by hand, it is not given twice. A fed work set aside for an answer is
        // given the months it has left as well
        TEST(lifescene_test, an_old_saves_rations_are_given_once_and_kept)
        {
            MockLogger logger;
            LifeFixture f;

            const std::string path = "save/test_life_up_front_kept.sz";
            std::remove(path.c_str());

            LifeScene* life = f.life();
            ASSERT_NE(life, nullptr);

            LifeSceneOptions opt = LifeFixture::mockup();
            opt.noSave = false;
            opt.savePath = path;

            for (const bool aside : {false, true})
            {
                // A month into the carters' three, at work or set aside, nothing given for them
                LifeSave old = life->save;
                old.running = aside ? "" : "carters";
                old.monthsIn = aside ? 0 : 1;
                old.setAside = aside ? "carters" : "";
                old.setAsideMonths = aside ? 1 : 0;
                old.stats["rations"] = 3;
                old.stats["str"] = 8;
                old.boardUpFront = false;
                ASSERT_TRUE(old.save(path));

                f.load<EmptyScene>();

                LifeScene* opened = f.life(opt);
                ASSERT_NE(opened, nullptr);
                EXPECT_EQ(opened->save.stats["rations"], 5) << aside;
                EXPECT_TRUE(opened->save.boardUpFront);

                // The file says so already, before any month or any save by hand
                LifeSave read;
                ASSERT_TRUE(read.load(path));
                EXPECT_TRUE(read.boardUpFront) << aside;
                EXPECT_EQ(read.stats["rations"], 5) << aside;

                f.load<EmptyScene>();

                LifeScene* again = f.life(opt);
                ASSERT_NE(again, nullptr);
                EXPECT_EQ(again->save.stats["rations"], 5) << aside;

                f.load<EmptyScene>();
                life = f.life();
                ASSERT_NE(life, nullptr);
            }

            std::remove(path.c_str());
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
                    ASSERT_NE(stall(f, life, "buy.rations"), nullptr) << rations;

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

            EXPECT_EQ(life->save.stats["vit"], vit - 5);
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
            life->save.stats["vit"] = 10;

            life->onMonth();
            f.settle();

            // Still his life, a month older, and in danger: the months stopped, his life in red
            EXPECT_EQ(life->save.stats["vit"], 5);
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
            EXPECT_FLOAT_EQ(life->save.age, 6.0f);
            EXPECT_FLOAT_EQ(f.fact<float>("life.age"), 6.0f);
            EXPECT_EQ(life->save.stats["vit"], 35);
            EXPECT_EQ(life->save.stats["str"], 4);
            EXPECT_TRUE(life->save.running.empty());
            EXPECT_EQ(life->save.stats.count("rations"), 0u);
            EXPECT_TRUE(life->paused);
            EXPECT_FALSE(life->piece<StatLine>("vit")->spec.alert);

            // The town is not his yet: every boy is taken to its market in his sixth year
            EXPECT_FALSE(life->save.townKnown);

            // Childhood, then what became of the last life (the inn a second life finds in the town
            // waits for him to know the town)
            ASSERT_EQ(happened(life).size(), 2u);
            EXPECT_EQ(happened(life)[0].kind, LogKind::Milestone);
            EXPECT_EQ(happened(life).back().kind, LogKind::Loss);
            EXPECT_NE(happened(life).back().text.find("life ended"), std::string::npos);

            auto ledger = life->piece<ResourceLedger>("ledger");
            ASSERT_NE(ledger, nullptr);
            // He holds nothing yet: his first work brings his first rations
            EXPECT_EQ(ledger->row("rations"), nullptr);
            EXPECT_EQ(ledger->row("coin"), nullptr);

            // The new life goes on as any other
            life->onMonth();
            f.settle();

            // (he holds no rations yet, and is not out of what he never had)
            EXPECT_GT(life->save.age, 6.0f);
            EXPECT_EQ(life->save.stats["vit"], 35);
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

            life->save.stats["vit"] = 40;

            life->onMonth();
            f.settle();

            EXPECT_EQ(life->save.stats["vit"], 40);
            EXPECT_EQ(vit()->spec.value, 40);
            EXPECT_EQ(f.fact<int>("character.parts.vit.projected"), 50);
            ASSERT_TRUE(vit()->projected.has_value());
            EXPECT_EQ(vit()->spec.projected, 50);
            EXPECT_FALSE(vit()->spec.alert);

            life->onMonth();
            f.settle();

            EXPECT_EQ(life->save.stats["vit"], 45);
            EXPECT_EQ(vit()->spec.value, 45);
            EXPECT_EQ(vit()->spec.projected, 50);

            life->onMonth();
            life->onMonth();
            f.settle();

            // Whole again: the most it can be did not move, and it stops there
            EXPECT_EQ(life->save.stats["vit"], 50);
            EXPECT_EQ(life->save.stats["vitmax"], 50);
            EXPECT_EQ(f.fact<int>("character.parts.vit.projected"), -1);
            EXPECT_FALSE(vit()->projected.has_value());

            life->onMonth();
            life->onMonth();
            f.settle();

            EXPECT_EQ(life->save.stats["vit"], 50);

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
                    EXPECT_EQ(r.value, "50");
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
            opt.age = 7.0f;

            LifeScene* life = f.life(opt);
            ASSERT_NE(life, nullptr);

            // At 8, with coin for the market, which he knows
            life->save.age = 8.0f - 1.0f / 12.0f;
            life->save.stats["coin"] = 10;
            life->save.townKnown = true;
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

            ActivityRow* buy = stall(f, life, "buy.rations");
            ASSERT_NE(buy, nullptr);
            EXPECT_EQ(buy->spec.count, "DONE 0");
            EXPECT_FALSE(buy->rank.has_value());
            EXPECT_EQ(registry->find("activity/buy.rations")->aside, "0");

            // Done at once: the same row, one more
            f.ecs.sendEvent(ActivityActivatedEvent{"life.activities", "buy.rations"});
            f.settle();

            buy = stall(f, life, "buy.rations");
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
            opt.age = 7.0f;

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
            opt.age = 7.0f;
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
            ASSERT_NE(stall(f, life, "buy.rations"), nullptr);
            // A tile done at once writes its price where the others write their months
            EXPECT_EQ(stall(f, life, "buy.rations")->cost.spec.text, "5 coin");

            // It is the market's: the Life page has no tile for it, nor one that goes there
            EXPECT_EQ(list->find(&f.ecs, "buy.rations"), nullptr);
            EXPECT_EQ(list->find(&f.ecs, "goto.market"), nullptr);

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
            EXPECT_NE(stall(f, life, "buy.rations"), nullptr);
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
            opt.age = 7.0f;

            LifeScene* life = f.life(opt);
            ASSERT_NE(life, nullptr);

            auto list = life->piece<ActivityList>("activities");
            ASSERT_NE(list, nullptr);

            // He knows the town, and a month tells the page so (and eats a ration)
            life->save.townKnown = true;
            life->onMonth();
            f.settle();

            auto row = stall(f, life, "buy.rations");
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
        // Vitality is counted in the points of a fight: a boy has 40, at home as on a walk. A save
        // written when he had 8 (and five times that on a walk, less the scratches the rounding
        // left him) is brought to the same count when it is read, once.
        TEST(lifescene_test, a_save_from_before_has_its_vitality_in_points)
        {
            MockLogger logger;
            LifeFixture f;

            const std::string path = "save/test_life_vitality.sz";
            std::remove(path.c_str());

            LifeScene* life = f.life();
            ASSERT_NE(life, nullptr);

            // A life begun today is in points already
            EXPECT_EQ(life->save.vitalityIn, LifeSave::VitalityPoints);
            EXPECT_EQ(life->save.stats["vit"], 50);
            EXPECT_EQ(life->save.stats["vitmax"], 50);

            // As it was written before: 8 of 10, three scratches from his walks
            LifeSave old = life->save;
            old.vitalityIn = 1;
            old.stats["vit"] = 8;
            old.stats["vitmax"] = 10;
            old.stats["scratches"] = 3;

            ASSERT_TRUE(old.save(path));

            LifeSave read;
            ASSERT_TRUE(read.load(path));

            EXPECT_EQ(read.vitalityIn, LifeSave::VitalityPoints);
            EXPECT_EQ(read.stats["vit"], 37);
            EXPECT_EQ(read.stats["vitmax"], 50);
            EXPECT_EQ(read.stats.count("scratches"), 0u);

            // Written again and read again, it is not counted twice
            ASSERT_TRUE(read.save(path));

            LifeSave again;
            ASSERT_TRUE(again.load(path));

            EXPECT_EQ(again.stats["vit"], 37);
            EXPECT_EQ(again.stats["vitmax"], 50);

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
            opt.age = 7.0f;

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

            // An activity: its time, what it brings and costs, what it asks, the rations it gives up front
            const GlossSpec* campaign = registry->find("activity/campaign");
            ASSERT_NE(campaign, nullptr);
            EXPECT_EQ(campaign->title, "Join the Border Campaign");
            EXPECT_EQ(rowOf(campaign, "Time"), "24 mo");
            EXPECT_EQ(rowOf(campaign, "Rations"), "24 up front");

            // Locked, and no footnote to say so: what it still asks is in red under REQUIREMENTS
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
            EXPECT_EQ(toneOf(campaign, "Rations"), "gain");
            EXPECT_EQ(toneOf(campaign, "REWARD"), "<heading>");
            EXPECT_EQ(toneOf(campaign, "COST"), "<heading>");
            EXPECT_EQ(toneOf(campaign, "REQUIREMENTS"), "<heading>");

            EXPECT_EQ(in(campaign, "REWARD", "Coin").first, "+50");
            EXPECT_EQ(in(campaign, "REWARD", "Coin").second, "gain");
            // (what it takes says what he holds of it, plain while he has enough, and is not asked again)
            EXPECT_EQ(in(campaign, "COST", "Coin").first, "\xE2\x88\x92" "10 (has 46)");
            EXPECT_EQ(in(campaign, "COST", "Coin").second, "");
            EXPECT_EQ(in(campaign, "REQUIREMENTS", "Coin").first, "<none>");
            EXPECT_EQ(in(campaign, "REQUIREMENTS", "Age").first, "17 / 21");
            EXPECT_EQ(in(campaign, "REQUIREMENTS", "Age").second, "loss");
            EXPECT_EQ(in(campaign, "REQUIREMENTS", "Arms").first, "5 / 6");
            EXPECT_EQ(in(campaign, "REQUIREMENTS", "Arms").second, "loss");
            EXPECT_EQ(in(campaign, "REQUIREMENTS", "Discipline").second, "loss");

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
            EXPECT_EQ(in(registry->find("activity/yard"), "REWARD", "Strength").first, "+1");

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

            // What does not feed him says nothing of rations
            EXPECT_EQ(rowOf(registry->find("activity/yard"), "Rations"), "<none>");

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
            opt.age = 7.0f;

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

            // A class is a tile apart, open or not: framed, and marked with its glyph
            for (const char* id : {"keep", "collegium"})
            {
                EXPECT_TRUE(list->row(&f.ecs, id)->spec.major) << id;
                EXPECT_FALSE(list->row(&f.ecs, id)->frame.empty()) << id;
                EXPECT_FALSE(list->row(&f.ecs, id)->mark.entity.empty()) << id;
            }

            EXPECT_FLOAT_EQ(f.pos(list->row(&f.ecs, "keep")->frame)->width, f.pos(list->row(&f.ecs, "keep")->root)->width);
            EXPECT_EQ(list->row(&f.ecs, "keep")->frame->get<ThemeComponent>()->element, "activity.major.frame");
            EXPECT_EQ(list->row(&f.ecs, "collegium")->frame->get<ThemeComponent>()->element, "activity.major.frame.locked");
            EXPECT_NE(f.ecs.getSystem<GlossRegistry>()->find("activity/keep")->text.find("the other classes close"), std::string::npos);

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
            opt.age = 7.0f;

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
            EXPECT_FLOAT_EQ(life->save.age, 6.0f);
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
            life->save.stats["vit"] = 100;
            life->save.stats["vitmax"] = 100;

            life->onMonth();
            f.settle();

            EXPECT_EQ(f.fact<std::string>("life.headline.ageText"), "30.0");
            EXPECT_EQ(f.fact<std::string>("life.next.label"), "");
            EXPECT_EQ(life->save.stats["vit"], 100);

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
            EXPECT_NE(stall(f, life, "buy.rations"), nullptr);
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

            EXPECT_EQ(life->save.stats["vit"], 100);
            EXPECT_FALSE(life->paused);

            life->onMonth();
            f.settle();

            EXPECT_EQ(life->save.stats["vit"], 95);
            EXPECT_EQ(life->save.stats["vitmax"], 95);
            EXPECT_EQ(life->save.done["tales"], 1);
            EXPECT_EQ(life->save.stats["coin"], coin + 2);
            EXPECT_EQ(life->save.stats["rations"], rations);

            // The years took it: no part in red
            EXPECT_FALSE(life->piece<StatLine>("vit")->spec.alert);
            EXPECT_EQ(life->piece<StatLine>("vit")->figure.spec.text, "95");

            // A year into his old age: the clock's figure goes past the end of its track
            for (int i = 0; i < 10; ++i)
                life->onMonth();
            f.settle();

            EXPECT_EQ(f.fact<std::string>("life.headline.ageText"), "31.0");
            EXPECT_EQ(life->piece<LifeClock>("clock")->age.spec.text, "31");
            EXPECT_EQ(life->save.stats["vit"], 70);
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

            // A life lived to its end: under its ending, a word is asked of who played it
            ASSERT_FALSE(life->ending.empty());

            EntityRef ask = life->ending->get<Prefab>()->findEntity("endAsk");

            ASSERT_FALSE(ask.empty());
            EXPECT_FALSE(life->epitaph.ask.empty());
            EXPECT_EQ(ask->get<Label>()->spec.text, life->epitaph.ask);
            EXPECT_TRUE(f.pos(ask)->visible);

            life->beginAgain();
            f.settle();

            EXPECT_FLOAT_EQ(life->save.age, 6.0f);
            EXPECT_TRUE(life->paused);
            EXPECT_NE(happened(life).back().text.find("old age"), std::string::npos) << happened(life).back().text;
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // An Inn raised past his thirtieth year does not give him back his prime: the old age that
        // began at thirty goes on, the works it closed stay closed, and he dies of it long before
        // fifty. The next life, born with the Inn standing, has its prime to fifty.
        TEST(lifescene_test, an_inn_raised_past_thirty_does_not_stop_his_old_age)
        {
            MockLogger logger;
            LifeFixture f;

            LifeScene* life = f.life();
            ASSERT_NE(life, nullptr);

            // A second life of a world, who knows the town, a month from thirty, and no Inn
            life->save.lives = 2;
            life->rules.lives = 2;
            life->save.townKnown = true;
            life->save.age = 30.0f - 1.0f / 12.0f;
            life->save.stats["rations"] = 200;
            life->save.stats["vit"] = 100;
            life->save.stats["vitmax"] = 100;
            life->save.stats["coin"] = 50;
            life->save.stats["firewood"] = 20;
            life->save.stats["berries"] = 20;

            life->onMonth();
            f.settle();

            EXPECT_EQ(life->save.stats["prime_end"], 30);

            auto list = life->piece<ActivityList>("activities");
            ASSERT_NE(list, nullptr);
            EXPECT_EQ(list->find(&f.ecs, "carters"), nullptr);

            // The chair, raised at thirty and a month
            f.ecs.sendEvent(PlaceSelectedEvent{"inn"});
            f.settle();
            f.ecs.sendEvent(PlaceSelectedEvent{"inn"});
            f.settle();

            ASSERT_EQ(life->save.running, "raise.inn.1");

            for (int i = 0; i < 6; ++i)
            {
                life->onMonth();
                f.settle();
            }

            ASSERT_EQ(life->save.town["inn"], 1);
            EXPECT_EQ(life->save.stats["prime_end"], 30);

            // Old age goes on: no milestone ahead of him, and the works of his prime stay closed
            EXPECT_EQ(f.fact<std::string>("life.next.label"), "");
            EXPECT_EQ(list->find(&f.ecs, "carters"), nullptr);
            EXPECT_NE(list->find(&f.ecs, "tales"), nullptr);

            const int most = life->save.stats["vitmax"];

            for (int i = 0; i < 4; ++i)
                life->onMonth();
            f.settle();

            EXPECT_EQ(life->save.stats["vitmax"], most - 10);
            EXPECT_EQ(list->find(&f.ecs, "carters"), nullptr);

            // And it ends him, years before fifty
            for (int i = 0; i < 120 and not life->ended; ++i)
                life->onMonth();
            f.settle();

            ASSERT_TRUE(life->ended);
            EXPECT_LT(life->save.age, 35.0f);
            EXPECT_NE(life->epitaph.cause.find("old man"), std::string::npos) << life->epitaph.cause;

            // The next life is born with the Inn standing: nothing settled yet, and at thirty his
            // prime runs on to fifty
            life->beginAgain();
            f.settle();

            EXPECT_EQ(life->save.stats.count("prime_end"), 0u);

            life->save.townKnown = true;
            life->save.age = 30.0f - 1.0f / 12.0f;
            life->save.stats["rations"] = 200;
            life->save.stats["vit"] = 100;
            life->save.stats["vitmax"] = 100;

            for (int i = 0; i < 13; ++i)
                life->onMonth();
            f.settle();

            EXPECT_EQ(life->save.stats["prime_end"], 50);
            EXPECT_EQ(life->save.stats["vitmax"], 100);
            EXPECT_EQ(life->save.stats["vit"], 100);
            EXPECT_FALSE(life->ended);
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

            // The mockup's life (24 terms, the wooden blade's practice one of them, the carters' most
            // of all, sworn to the Keep, three deeds),
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
            EXPECT_EQ(said("endTally").rfind("AGE 17 \xC2\xB7 WORKS 24 \xC2\xB7 COIN ", 0), 0u) << said("endTally");

            // He did not live his life out: nothing is asked of who played it
            ASSERT_FALSE(part("endAsk").empty());
            EXPECT_TRUE(life->epitaph.ask.empty());
            EXPECT_FALSE(f.pos(part("endAsk"))->visible);

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
            EXPECT_FLOAT_EQ(life->save.age, 6.0f);
            EXPECT_TRUE(life->save.done.empty());

            // Childhood, and the last life's end
            ASSERT_EQ(happened(life).size(), 2u);
            EXPECT_EQ(happened(life).back().kind, LogKind::Loss);
            EXPECT_NE(happened(life).back().text.find("life ended"), std::string::npos);

            // And the page takes work again
            f.ecs.sendEvent(ButtonActivatedEvent{life->piece<Button>("skip")->face.id, "life.skip"});
            f.settle();

            EXPECT_GT(life->save.age, 6.0f);
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
            opt.age = 7.0f;

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

            // Life, the town, his walks and the options for now: the other chapters come back as their pages are built
            ASSERT_EQ(tabs->tabs.size(), 4u);
            EXPECT_EQ(tabs->tabs[0].label.spec.text, "Life");
            EXPECT_EQ(tabs->tabs[1].label.spec.text, "Town");
            EXPECT_EQ(tabs->tabs[2].label.spec.text, "Adventure");
            EXPECT_EQ(tabs->tabs[3].label.spec.text, "Options");

            // A chapter with no page yet, should one be selected, says so and Life stays open
            f.ecs.sendEvent(TabSelectedEvent{tabs->root.id, "life.tabs", 4});
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

            EXPECT_NEAR(life->save.age, 6.0f, 0.0001f);
            EXPECT_EQ(life->save.lives, 1);
            EXPECT_EQ(life->save.guide, 0);
            EXPECT_TRUE(life->guiding());

            // His age as the head writes it: the clock's figure, then the months of the year
            EXPECT_EQ(life->piece<LifeClock>("clock")->age.spec.text, "6");
            EXPECT_EQ(f.fact<std::string>("life.headline.ageUnit"), "YEARS OLD");

            // Nothing held: no rations to eat, no row for them. Written time alone has its row
            // from the first, at nothing
            auto ledger = life->piece<ResourceLedger>("ledger");
            ASSERT_NE(ledger, nullptr);
            ASSERT_EQ(ledger->groups.size(), 1u);
            ASSERT_EQ(ledger->groups[0].rows.size(), 1u);
            EXPECT_NE(ledger->row("written"), nullptr);
            EXPECT_EQ(life->save.stats.count("rations"), 0u);
            EXPECT_FALSE(f.facts->hasFact("stat.rations"));

            // One row, and nothing chosen for him: the guide has him choose it
            auto list = life->piece<ActivityList>("activities");
            ASSERT_NE(list, nullptr);
            ASSERT_EQ(list->spec.groups.size(), 1u);
            ASSERT_EQ(list->spec.groups[0].rows.size(), 1u);
            EXPECT_EQ(list->spec.groups[0].rows[0].id, "glean");
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
            ActivityRow* glean = list->find(&f.ecs, "glean");
            ASSERT_NE(glean, nullptr);
            ASSERT_FALSE(life->hand.empty());
            EXPECT_EQ(life->handOn, glean->root.id);
            EXPECT_TRUE(f.pos(life->hand)->visible);
            EXPECT_LE(f.pos(life->hand)->x + f.pos(life->hand)->width, f.pos(glean->root)->x + 0.5f);
            EXPECT_TRUE(life->veiled);
            EXPECT_FALSE(life->veil.empty());
            expectNoteBeside(f, life, glean->root);

            // The task chosen: the button that begins it, the month it takes, and the hand beside it.
            // The line says what to press, and that a double click on a task begins it as well
            f.ecs.sendEvent(ActivitySelectedEvent{"life.activities", "glean"});
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
            EXPECT_EQ(life->handOn, glean->root.id);
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
        // The first task takes a month and brings his first rations: the guide says what they are
        // with its hand beside them, then shows the next work without choosing it.
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
            f.ecs.sendEvent(ActivitySelectedEvent{"life.activities", "glean"});
            f.settle();
            f.ecs.sendEvent(ButtonActivatedEvent{life->piece<Button>("begin")->face.id, "life.begin"});
            f.settle();

            // Begun: the first step ends with it, and the hand leaves
            ASSERT_EQ(life->save.running, "glean");
            EXPECT_EQ(life->save.guide, 1);
            EXPECT_FALSE(life->step.active);
            EXPECT_EQ(life->handOn, 0u);
            EXPECT_FALSE(life->veiled);
            EXPECT_FALSE(life->noteShown);
            EXPECT_FALSE(f.pos(life->note)->visible);
            EXPECT_EQ(f.fact<int>("life.working"), 1);

            life->onMonth();
            f.frames(12);

            EXPECT_NEAR(life->save.age, 6.0f + 1.0f / 12.0f, 0.0001f);
            EXPECT_TRUE(life->save.running.empty());
            EXPECT_EQ(life->save.done["glean"], 1);
            EXPECT_EQ(life->save.stats["rations"], 10);
            EXPECT_EQ(f.fact<int>("life.terms"), 1);
            EXPECT_EQ(f.fact<int>("life.working"), 0);

            // His first holding has its row; the opening task is gone and the baker's flour has come
            ASSERT_NE(ledger->row("rations"), nullptr);
            EXPECT_EQ(ledger->row("rations")->figure.spec.text, "10/60");
            EXPECT_EQ(list->find(&f.ecs, "glean"), nullptr);
            ASSERT_NE(list->find(&f.ecs, "flour"), nullptr);
            EXPECT_EQ(list->find(&f.ecs, "wheel"), nullptr);

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

            // Said for a few seconds; then the third shows the flour's tile, and chooses nothing
            f.pass(5200.0f);

            EXPECT_EQ(life->save.guide, 2);
            EXPECT_EQ(f.fact<int>("life.guide"), 2);
            ASSERT_TRUE(life->step.active);
            EXPECT_EQ(life->step.id, "guide.flour");
            EXPECT_EQ(life->noteWords.spec.text, life->step.say);

            ActivityRow* flour = list->find(&f.ecs, "flour");
            ASSERT_NE(flour, nullptr);
            EXPECT_EQ(life->handOn, flour->root.id);
            expectNoteBeside(f, life, flour->root);
            EXPECT_FALSE(life->nextShown);
            EXPECT_EQ(list->selected(), "");
            EXPECT_TRUE(life->save.running.empty());

            // Chosen, the hand goes to the button that begins it
            f.ecs.sendEvent(ActivitySelectedEvent{"life.activities", "flour"});
            f.settle();

            EXPECT_EQ(life->handOn, life->piece<Button>("begin")->root.id);
            EXPECT_EQ(life->noteWords.spec.text, life->step.sayChosen);
            EXPECT_EQ(life->save.guide, 2);

            // And a double click on the task begins it just the same: the step ends
            f.ecs.sendEvent(ActivityActivatedEvent{"life.activities", "flour"});
            f.settle();

            EXPECT_EQ(life->save.running, "flour");
            EXPECT_EQ(life->save.guide, 3);
            EXPECT_FALSE(life->step.active);
            EXPECT_EQ(life->handOn, 0u);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // His sixth year under the guide: a work at a time, each said its sentence; one of the two with
        // both lit, two of the three, the market and what it sells, the harvest, his age, and the
        // classes the day he is seven. After its last step nothing of the guide is left on the page.
        TEST(lifescene_test, the_guide_walks_him_through_his_sixth_year)
        {
            MockLogger logger;
            LifeFixture f;

            LifeScene* life = f.life(LifeFixture::first());
            ASSERT_NE(life, nullptr);

            f.frames(12);

            auto list = life->piece<ActivityList>("activities");
            auto tabs = life->piece<Tabs>("tabs");
            ASSERT_NE(list, nullptr);
            ASSERT_NE(tabs, nullptr);

            // A work begun, and its months
            auto work = [&](const std::string& id, int months) {
                // Letters and the kitchen burn firewood, and the kitchen draws water: he has them
                life->save.stats["firewood"] = 6;
                life->save.stats["water"] = 3;
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

            auto months = [&]() { return static_cast<int>(std::lround((life->save.age - 6.0f) * 12.0f)); };

            work("glean", 1);
            EXPECT_EQ(life->step.id, "guide.rations");
            next();
            EXPECT_EQ(life->save.guide, 2);
            EXPECT_EQ(life->step.id, "guide.flour");

            work("flour", 1);
            EXPECT_EQ(life->step.id, "guide.coin");
            next();
            EXPECT_EQ(life->save.guide, 4);

            // One or the other: the hand on the first, both lit, and Begin whichever he takes
            EXPECT_EQ(life->step.id, "guide.choice");
            ASSERT_NE(list->find(&f.ecs, "wheel"), nullptr);
            ASSERT_NE(list->find(&f.ecs, "scribe"), nullptr);
            EXPECT_EQ(life->handOn, list->find(&f.ecs, "wheel")->root.id);
            EXPECT_EQ(life->step.light, (std::vector<std::string>{"tile:scribe"}));
            EXPECT_TRUE(life->veiled);

            // The gloss of each says what taking it gives up; a task that closes nothing says none
            {
                auto registry = f.ecs.getSystem<GlossRegistry>();

                ASSERT_NE(registry->find("activity/scribe"), nullptr);
                EXPECT_NE(registry->find("activity/scribe")->caution.find("Climb the Mill Wheel"), std::string::npos);
                EXPECT_NE(registry->find("activity/wheel")->caution.find("Sit with the Old Scribe"), std::string::npos);
                EXPECT_EQ(registry->find("activity/glean")->caution, "");
            }

            f.ecs.sendEvent(ActivitySelectedEvent{"life.activities", "scribe"});
            f.settle();

            EXPECT_EQ(life->handOn, life->piece<Button>("begin")->root.id);
            EXPECT_EQ(life->noteWords.spec.text, life->step.sayChosen);

            // The scribe's bench: his mind, and his Vitality; the wheel has no row any more, and
            // the log says it went, as a loss
            work("scribe", 1);
            EXPECT_EQ(list->find(&f.ecs, "wheel"), nullptr);

            {
                bool written = false;

                for (const auto& line : life->save.log)
                {
                    if (line.text == "One or the other: Climb the Mill Wheel is closed to him")
                    {
                        written = true;
                        EXPECT_EQ(line.kind, LogKind::Loss);
                    }
                }

                EXPECT_TRUE(written);
            }
            EXPECT_EQ(life->save.stats["int"], 5);
            EXPECT_EQ(life->save.stats["vitmax"], 40);
            EXPECT_EQ(life->save.stats["str"], 4);

            EXPECT_EQ(life->step.id, "guide.stats");
            EXPECT_EQ(life->handOn, life->piece<StatLine>("int")->root.id);
            next();
            EXPECT_EQ(life->save.guide, 6);
            EXPECT_EQ(life->step.id, "guide.tome");

            // The keepsake: his, in the ledger
            work("tome", 1);
            EXPECT_EQ(life->save.stats["tome"], 1);
            EXPECT_NE(life->piece<ResourceLedger>("ledger")->row("tome"), nullptr);
            EXPECT_EQ(life->save.stats["coin"], 10);
            EXPECT_EQ(months(), 4);

            // Three works, and time for two
            EXPECT_EQ(life->step.id, "guide.pick");
            EXPECT_EQ(life->handOn, list->find(&f.ecs, "knots")->root.id);
            EXPECT_EQ(life->step.light.size(), 2u);
            EXPECT_FALSE(life->nextShown);

            work("knots", 2);
            EXPECT_EQ(life->save.guide, 8);
            EXPECT_FALSE(life->step.active);

            // The sacks take from his Vitality as they begin, and the guide says so while he hauls
            f.ecs.sendEvent(ActivityActivatedEvent{"life.activities", "sacks"});
            f.settle();
            f.frames(12);

            ASSERT_EQ(life->save.running, "sacks");
            EXPECT_EQ(life->save.stats["vit"], 35);
            EXPECT_EQ(f.fact<int>("working.sacks"), 1);
            ASSERT_TRUE(life->step.active);
            EXPECT_EQ(life->step.id, "guide.vitality");
            EXPECT_EQ(life->handOn, life->piece<StatLine>("vit")->root.id);
            next();

            for (int i = 0; i < 2; ++i)
            {
                life->onMonth();
                f.settle();
            }

            f.frames(12);

            ASSERT_TRUE(life->save.running.empty());
            EXPECT_EQ(f.fact<int>("working.sacks"), 0);
            EXPECT_EQ(months(), 8);

            // The third has closed, and the market is all there is: the guide sends him there
            EXPECT_EQ(list->find(&f.ecs, "sing"), nullptr);
            EXPECT_EQ(life->step.id, "guide.tomarket");
            ASSERT_NE(list->find(&f.ecs, "tomarket"), nullptr);
            EXPECT_EQ(life->handOn, list->find(&f.ecs, "tomarket")->root.id);
            EXPECT_EQ(life->save.stats["rations"], 3);

            work("tomarket", 1);
            EXPECT_EQ(life->save.guide, 9);
            EXPECT_TRUE(life->save.townKnown);

            // The town's own steps: its tab, then its market, where the step waits for him to buy
            tabs = life->piece<Tabs>("tabs");
            ASSERT_NE(tabs, nullptr);

            EXPECT_EQ(life->step.id, "guide.town");
            EXPECT_EQ(life->handOn, tabs->tabs[1].face.id);

            f.ecs.sendEvent(TabSelectedEvent{tabs->root.id, "life.tabs", 1});
            f.frames(12);

            EXPECT_EQ(life->step.id, "guide.market");
            ASSERT_NE(stall(f, life, "buy.rations"), nullptr);
            EXPECT_EQ(life->handOn, stall(f, life, "buy.rations")->root.id);
            EXPECT_FALSE(life->nextShown);

            f.ecs.sendEvent(ActivitySelectedEvent{"town.market", "buy.rations"});
            f.settle();

            EXPECT_EQ(life->handOn, life->piece<Button>("begin")->root.id);
            EXPECT_EQ(life->noteWords.spec.text, life->step.sayChosen);

            f.ecs.sendEvent(ActivityActivatedEvent{"town.market", "buy.rations"});
            f.settle();
            f.frames(12);

            EXPECT_EQ(life->save.stats["rations"], 2 + 6);
            EXPECT_EQ(life->save.stats["coin"], 5);
            EXPECT_EQ(life->step.id, "guide.places");
            next();

            // The harvest is on the Life page: the hand shows the way back, then the tile
            EXPECT_EQ(life->save.guide, 9);
            EXPECT_EQ(life->step.id, "guide.harvest");
            EXPECT_TRUE(life->townShown);
            EXPECT_EQ(life->handOn, tabs->tabs[0].face.id);
            EXPECT_NE(life->noteWords.spec.text, life->step.say);
            EXPECT_NE(life->noteWords.spec.text.find("Life tab"), std::string::npos) << life->noteWords.spec.text;

            f.ecs.sendEvent(TabSelectedEvent{tabs->root.id, "life.tabs", 0});
            f.frames(12);

            list = life->piece<ActivityList>("activities");
            ASSERT_NE(list, nullptr);
            ASSERT_NE(list->find(&f.ecs, "harvest"), nullptr);
            EXPECT_EQ(life->handOn, list->find(&f.ecs, "harvest")->root.id);
            EXPECT_EQ(life->noteWords.spec.text, life->step.say);

            // The harvest: his age and what the game is about
            work("harvest", 2);
            EXPECT_EQ(life->save.guide, 10);
            EXPECT_EQ(life->step.id, "guide.clock");
            EXPECT_EQ(life->handOn, life->piece<LifeClock>("clock")->root.id);
            EXPECT_NE(life->noteWords.spec.text.find("30"), std::string::npos) << life->noteWords.spec.text;
            next();

            // A month at the mill, with nothing said: the classes wait for him to be seven
            EXPECT_EQ(life->save.guide, 11);
            EXPECT_FALSE(life->step.active);
            EXPECT_EQ(list->find(&f.ecs, "keep"), nullptr);
            ASSERT_NE(list->find(&f.ecs, "helping"), nullptr);

            const int toasts = life->toastsUp();

            work("helping", 1);
            EXPECT_NEAR(life->save.age, 7.0f, 0.0001f);

            // The town's work comes all at once, and is counted in one toast, not named in nine
            EXPECT_LE(life->toastsUp(), toasts + 2);

            for (const char* id : {"keep", "collegium", "hand", "carters", "mill", "letters"})
                EXPECT_NE(list->find(&f.ecs, id), nullptr) << id;

            // The rest of a boy's work comes over his seventh year, not with it
            for (const char* id : {"messages", "kitchen"})
                EXPECT_EQ(list->find(&f.ecs, id), nullptr) << id;

            // What the year made of him: the mind's road, the knots and the sacks
            EXPECT_EQ(life->save.stats["str"], 6);
            EXPECT_EQ(life->save.stats["dex"], 6);
            EXPECT_EQ(life->save.stats["int"], 7);
            EXPECT_EQ(life->save.stats["vitmax"], 40);
            EXPECT_EQ(life->save.stats["rations"], 8 - 2 + 6 - 1);

            EXPECT_EQ(life->step.id, "guide.doors");
            EXPECT_EQ(life->handOn, list->find(&f.ecs, "keep")->root.id);
            EXPECT_NE(life->noteWords.spec.text.find("classes"), std::string::npos) << life->noteWords.spec.text;

            next();
            EXPECT_EQ(life->save.guide, 12);
            EXPECT_EQ(life->step.id, "guide.rest");
            EXPECT_EQ(life->handOn, 0u);
            EXPECT_TRUE(life->noteShown);

            next();
            EXPECT_EQ(life->save.guide, 13);
            EXPECT_FALSE(life->step.active);

            // Two more works: his rations run low at his letters, and the word kept for that is said
            work("mill", 6);
            EXPECT_FALSE(life->step.active);

            work("letters", 6);
            EXPECT_EQ(life->step.id, "guide.rations_market");
            EXPECT_EQ(life->step.order, 0);
            next();

            EXPECT_EQ(life->save.guide, 13);
            EXPECT_EQ(f.fact<int>("life.guide"), 13);
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

            // Lines of the town's story, among what happened
            EXPECT_TRUE(reached(life, "lore.bell"));
            EXPECT_TRUE(reached(life, "lore.classes"));
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // The guided first year of a boy who buys nothing at the market: his rations run out once,
        // at the harvest, and the mill's month that brings him to seven gives its six as he begins
        // it. Nothing then says they are running low, and they do not run out a second time.
        TEST(lifescene_test, the_first_year_runs_out_of_rations_once)
        {
            MockLogger logger;
            LifeFixture f;

            LifeScene* life = f.life(LifeFixture::first());
            ASSERT_NE(life, nullptr);

            f.frames(12);

            // Whatever the guide says for a time is read, and passed with its Next
            auto through = [&]() {
                for (int guard = 0; guard < 4 and life->step.active and life->nextShown; ++guard)
                {
                    f.ecs.sendEvent(ButtonActivatedEvent{life->noteNext.face.id, "life.guide.next"});
                    f.frames(12);
                }
            };

            auto work = [&](const std::string& id, int months) {
                through();

                life->save.stats["firewood"] = 6;
                life->save.stats["water"] = 3;
                f.ecs.sendEvent(ActivityActivatedEvent{"life.activities", id});
                f.settle();

                ASSERT_EQ(life->save.running, id);

                for (int i = 0; i < months; ++i)
                {
                    through();
                    life->onMonth();
                    f.settle();
                }

                f.frames(12);

                ASSERT_TRUE(life->save.running.empty()) << id;
            };

            // The times the log says so, from its entry `from` on
            auto ranOut = [&](size_t from) {
                int n = 0;

                for (size_t i = from; i < life->save.log.size(); ++i)
                {
                    if (life->save.log[i].text == "The rations ran out")
                        ++n;
                }

                return n;
            };

            work("glean", 1);
            work("flour", 1);
            work("scribe", 1);
            work("tome", 1);
            work("knots", 2);
            work("sacks", 2);
            work("tomarket", 1);
            ASSERT_EQ(life->save.stats["rations"], 2);

            // No market: the harvest eats the last two
            work("harvest", 2);
            EXPECT_EQ(life->save.stats["rations"], 0);
            EXPECT_EQ(ranOut(0), 1);

            // The mill's month: six as he begins it, and nothing said of running low
            const float eleven = 6.0f + 11.0f / 12.0f;
            ASSERT_NEAR(life->save.age, eleven, 0.001f);

            through();
            const size_t atMill = life->save.log.size();
            f.ecs.sendEvent(ActivityActivatedEvent{"life.activities", "helping"});
            f.settle();

            ASSERT_EQ(life->save.running, "helping");
            EXPECT_EQ(life->save.stats["rations"], 6);
            EXPECT_EQ(f.fact<std::string>("life.warning").find("RATIONS"), std::string::npos) << f.fact<std::string>("life.warning");

            life->onMonth();
            f.settle();
            f.frames(12);

            EXPECT_NEAR(life->save.age, 7.0f, 0.0001f);
            EXPECT_TRUE(life->save.running.empty());
            EXPECT_EQ(life->save.stats["rations"], 5);
            EXPECT_EQ(f.fact<std::string>("life.warning").find("RATIONS"), std::string::npos) << f.fact<std::string>("life.warning");

            EXPECT_EQ(ranOut(atMill), 0);
            EXPECT_EQ(ranOut(0), 1);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // A fed work's tooltip says how long its rations last when his familiars eat them before its
        // term: the rules count it, the tooltip only says it
        TEST(lifescene_test, a_fed_works_tooltip_says_how_long_its_rations_last)
        {
            MockLogger logger;
            LifeFixture f;

            LifeScene* life = f.life();
            ASSERT_NE(life, nullptr);

            auto registry = f.ecs.getSystem<GlossRegistry>();
            ASSERT_NE(registry, nullptr);

            auto rations = [&]() -> std::string {
                const GlossSpec* carters = registry->find("activity/carters");

                if (not carters)
                    return "<no gloss>";

                for (const auto& r : carters->rows)
                {
                    if (r.label == "Rations")
                        return r.value;
                }

                return "<none>";
            };

            // (not strong enough to carry for them in two: Strength 9 shortens it)
            life->save.stats["str"] = 8;
            life->onMonth();
            f.settle();

            EXPECT_EQ(rations(), "3 up front");

            // Two sprites and nothing held: he and they eat three a month, the three last one
            life->save.stats["fam.sprite"] = 2;
            life->save.stats["rations"] = 1;
            life->onMonth();
            f.settle();

            ASSERT_EQ(life->save.stats["rations"], 0);
            EXPECT_EQ(rations(), "3 up front \xC2\xB7 lasts 1 mo");

            // With nine held the three months are fed: nothing more is said
            life->save.stats["rations"] = 12;
            life->onMonth();
            f.settle();

            ASSERT_EQ(life->save.stats["rations"], 9);
            EXPECT_EQ(rations(), "3 up front");
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // A first life that is seven already (a boy who let the year go by, a save from before the
        // year) is not told of works that are closed to him: the guide passes over the steps of his
        // sixth year unsaid, and says his age and the classes.
        TEST(lifescene_test, a_boy_of_seven_is_not_told_of_his_sixth_year)
        {
            MockLogger logger;
            LifeFixture f;

            LifeSceneOptions opt = LifeFixture::first();
            opt.age = 7.0f;

            LifeScene* life = f.life(opt);
            ASSERT_NE(life, nullptr);

            f.frames(12);

            auto list = life->piece<ActivityList>("activities");
            ASSERT_NE(list, nullptr);

            EXPECT_EQ(life->save.lives, 1);
            EXPECT_TRUE(life->guiding());
            EXPECT_EQ(life->save.guide, 10);
            EXPECT_EQ(f.fact<int>("life.guide"), 10);
            EXPECT_EQ(list->find(&f.ecs, "glean"), nullptr);

            ASSERT_TRUE(life->step.active);
            EXPECT_EQ(life->step.id, "guide.clock");
            EXPECT_EQ(life->handOn, life->piece<LifeClock>("clock")->root.id);

            f.ecs.sendEvent(ButtonActivatedEvent{life->noteNext.face.id, "life.guide.next"});
            f.frames(12);

            ASSERT_TRUE(life->step.active);
            EXPECT_EQ(life->step.id, "guide.doors");
            ASSERT_NE(list->find(&f.ecs, "keep"), nullptr);
            EXPECT_EQ(life->handOn, list->find(&f.ecs, "keep")->root.id);

            f.ecs.sendEvent(ButtonActivatedEvent{life->noteNext.face.id, "life.guide.next"});
            f.frames(12);
            f.ecs.sendEvent(ButtonActivatedEvent{life->noteNext.face.id, "life.guide.next"});
            f.frames(12);

            EXPECT_EQ(life->save.guide, 13);
            EXPECT_FALSE(life->guiding());
            EXPECT_FALSE(life->step.active);
            EXPECT_TRUE(life->stepsDue.empty());
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

            // The opening task, what the guide says of it, then the flour's month: the fourth step
            f.ecs.sendEvent(ActivityActivatedEvent{"life.activities", "glean"});
            f.settle();
            life->onMonth();
            f.frames(12);
            f.pass(5200.0f);

            ASSERT_EQ(life->step.id, "guide.flour");

            f.ecs.sendEvent(ActivityActivatedEvent{"life.activities", "flour"});
            f.settle();

            life->onMonth();
            f.settle();

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
            EXPECT_EQ(again->step.id, "guide.choice");

            std::remove(path.c_str());
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // A later life has no guide: his sixth year comes a work at a time, in the order a first
        // life met it, and the first of its works is chosen for him.
        TEST(lifescene_test, a_second_life_has_no_guide_and_his_sixth_year_in_order)
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

            EXPECT_FLOAT_EQ(life->save.age, 6.0f);
            EXPECT_EQ(life->save.lives, 2);
            EXPECT_EQ(life->rules.lives, 2);
            EXPECT_EQ(life->save.guide, 0);
            EXPECT_EQ(life->save.stats.count("rations"), 0u);
            EXPECT_EQ(life->save.stats["str"], 4);
            EXPECT_EQ(life->save.stats["vit"], 35);
            EXPECT_FALSE(life->guiding());

            auto ledger = life->piece<ResourceLedger>("ledger");
            ASSERT_NE(ledger, nullptr);
            EXPECT_EQ(ledger->row("rations"), nullptr);

            // The first work of the year alone, as a first life has it, and chosen for him
            auto list = life->piece<ActivityList>("activities");
            ASSERT_NE(list, nullptr);

            EXPECT_NE(list->find(&f.ecs, "glean"), nullptr);

            // (the next ones come one at a time; the three wait for his fourth month, the market
            // for his eighth, the town's work for seven)
            for (const char* id : {"flour", "wheel", "scribe", "tome", "knots", "sacks", "sing", "tomarket", "harvest", "helping", "carters", "keep"})
                EXPECT_EQ(list->find(&f.ecs, id), nullptr) << id;

            EXPECT_EQ(list->selected(), "glean");

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

            ASSERT_EQ(life->save.running, "glean");

            life->onMonth();
            f.settle();

            f.frames(12);

            EXPECT_FALSE(life->step.active);
            EXPECT_TRUE(life->stepsDue.empty());
            EXPECT_EQ(life->save.guide, 0);
            EXPECT_EQ(life->handOn, 0u);
            EXPECT_FALSE(f.pos(life->named("guide"))->visible);

            // In the order of a first life: the flour, then one of the two, and the other is gone
            EXPECT_NE(list->find(&f.ecs, "flour"), nullptr);
            EXPECT_EQ(list->find(&f.ecs, "scribe"), nullptr);

            f.ecs.sendEvent(ActivityActivatedEvent{"life.activities", "flour"});
            f.settle();

            ASSERT_EQ(life->save.running, "flour");

            life->onMonth();
            f.settle();
            f.frames(12);

            EXPECT_NE(list->find(&f.ecs, "wheel"), nullptr);

            f.ecs.sendEvent(ActivityActivatedEvent{"life.activities", "scribe"});
            f.settle();

            ASSERT_EQ(life->save.running, "scribe");

            life->onMonth();
            f.settle();
            f.frames(12);

            EXPECT_EQ(list->find(&f.ecs, "wheel"), nullptr);
            EXPECT_NE(list->find(&f.ecs, "tome"), nullptr);

            // Nothing of the guide for the work that costs him either
            EXPECT_FALSE(life->step.active);

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

            f.ecs.sendEvent(ActivityActivatedEvent{"life.activities", "glean"});
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

            f.ecs.sendEvent(ActivityActivatedEvent{"life.activities", "flour"});
            f.settle();

            life->onMonth();
            f.settle();

            f.frames(12);

            EXPECT_EQ(life->save.done["flour"], 1);
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

            f.ecs.sendEvent(ActivityActivatedEvent{"life.activities", "glean"});
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
            EXPECT_EQ(life->step.id, "guide.flour");
            EXPECT_EQ(life->save.guideSkipped, 0);

            // "Skip tutorial": no step, no leaf, no hand, no shade, and the guide past its last step
            f.ecs.sendEvent(ButtonActivatedEvent{life->noteSkip.face.id, "life.guide.skip"});
            f.frames(12);

            EXPECT_EQ(life->save.guide, 13);
            EXPECT_EQ(life->save.guideSkipped, 3);
            EXPECT_EQ(f.fact<int>("life.guide"), 13);
            EXPECT_FALSE(life->guiding());
            EXPECT_FALSE(life->step.active);
            EXPECT_TRUE(life->stepsDue.empty());
            EXPECT_FALSE(life->noteShown);
            EXPECT_FALSE(f.pos(life->note)->visible);
            EXPECT_EQ(life->handOn, 0u);
            EXPECT_FALSE(life->veiled);

            // The word it kept for later is taken as said
            EXPECT_TRUE(reached(life, "guide.rations_low"));

            // The life goes on without it: the flour's month brings no step
            f.ecs.sendEvent(ActivityActivatedEvent{"life.activities", "flour"});
            f.settle();

            life->onMonth();
            f.settle();

            f.frames(12);
            f.pass(1000.0f);

            EXPECT_EQ(life->save.done["flour"], 1);
            EXPECT_FALSE(life->step.active);
            EXPECT_FALSE(life->noteShown);

            // Nor does it come back with the save
            f.load<EmptyScene>();

            opt.fresh = false;

            LifeScene* again = f.life(opt);
            ASSERT_NE(again, nullptr);

            f.frames(12);

            EXPECT_EQ(again->save.guide, 13);
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

            EXPECT_EQ(life->save.guide, 13);
            EXPECT_EQ(life->save.guideSkipped, 1);
            EXPECT_FALSE(life->step.active);
            EXPECT_FALSE(life->noteShown);

            auto list = life->piece<ActivityList>("activities");
            ASSERT_NE(list, nullptr);

            EXPECT_EQ(list->selected(), "glean");
            EXPECT_TRUE(f.pos(life->named("begin"))->visible);
            EXPECT_TRUE(f.pos(life->named("guide"))->visible);
            EXPECT_EQ(life->piece<Label>("guide")->spec.text, "Choose a work, then press Begin");
            EXPECT_EQ(life->save.picks, 0);

            // Still his first life: the list grows a task at a time
            f.ecs.sendEvent(ButtonActivatedEvent{life->piece<Button>("begin")->face.id, "life.begin"});
            f.settle();

            ASSERT_EQ(life->save.running, "glean");

            life->onMonth();
            f.frames(12);

            EXPECT_NE(list->find(&f.ecs, "flour"), nullptr);
            EXPECT_EQ(list->find(&f.ecs, "wheel"), nullptr);
            EXPECT_FALSE(life->step.active);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // The town's tab is not on the page at all until a life has explored the town.
        TEST(lifescene_test, the_town_tab_is_hidden_until_he_has_been_to_the_market)
        {
            MockLogger logger;
            LifeFixture f;

            LifeSceneOptions opt = LifeFixture::mockup();
            opt.fresh = true;
            opt.lives = 2;
            opt.age = 7.0f;

            LifeScene* life = f.life(opt);
            ASSERT_NE(life, nullptr);

            auto tabs = life->piece<Tabs>("tabs");
            ASSERT_NE(tabs, nullptr);
            ASSERT_EQ(tabs->tabs.size(), 4u);

            EXPECT_FALSE(life->save.townKnown);
            EXPECT_TRUE(tabs->enabled(0));
            EXPECT_FALSE(tabs->enabled(1));
            EXPECT_TRUE(tabs->tabs[1].face->get<TabState>()->disabled);
            EXPECT_TRUE(tabs->tabs[1].face->get<TabState>()->hidden);
            EXPECT_FALSE(f.pos(tabs->tabs[1].face)->visible);
            EXPECT_FALSE(f.pos(tabs->tabs[1].label.entity)->visible);
            EXPECT_TRUE(f.pos(tabs->tabs[0].face)->visible);
            EXPECT_EQ(tabs->tabs[1].notice, 0);
            EXPECT_EQ(f.fact<int>("stat.town_known"), 0);
            EXPECT_EQ(f.fact<int>("life.town"), 0);

            // Selected all the same (a key, a script): the choice stays where it is
            f.ecs.sendEvent(TabSelectedEvent{tabs->root.id, "life.tabs", 1});
            f.settle();

            EXPECT_FALSE(life->townShown);
            EXPECT_EQ(tabs->active(), 0);
            EXPECT_TRUE(f.pos(life->named("activities"))->visible);
            EXPECT_FALSE(f.pos(life->named("town"))->visible);

            // No work for the town, nothing bought, nowhere to go: the town is not known
            auto list = life->piece<ActivityList>("activities");
            ASSERT_NE(list, nullptr);

            EXPECT_NE(list->find(&f.ecs, "tomarket"), nullptr);
            EXPECT_EQ(list->find(&f.ecs, "goto.market"), nullptr);
            EXPECT_EQ(work(f, life, "raise.market.1"), nullptr);
            EXPECT_EQ(stall(f, life, "buy.rations"), nullptr);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // A month at the market: the town is his from then on, its tab opens, the log and a toast
        // say so, and a first life is shown where its page is.
        TEST(lifescene_test, going_to_the_market_opens_the_town)
        {
            MockLogger logger;
            LifeFixture f;

            // A first life whose guide has said everything, a boy of seven who has not been yet
            LifeSceneOptions opt = LifeFixture::first();
            opt.guide = 13;
            opt.age = 7.0f;

            LifeScene* life = f.life(opt);
            ASSERT_NE(life, nullptr);

            f.frames(12);

            life->save.done = {{"carters", 3}};
            life->rules.done = life->save.terms();
            life->save.stats["rations"] = 24;
            life->save.stats["coin"] = 10;
            life->onMonth();
            f.frames(12);
            f.pass(800.0f);

            auto tabs = life->piece<Tabs>("tabs");
            auto list = life->piece<ActivityList>("activities");
            ASSERT_NE(tabs, nullptr);
            ASSERT_NE(list, nullptr);
            ASSERT_NE(list->find(&f.ecs, "tomarket"), nullptr);
            ASSERT_FALSE(tabs->enabled(1));
            ASSERT_FALSE(f.pos(tabs->tabs[1].face)->visible);

            // The tile that goes there stands out from the others: the page wants it seen. At seven
            // it was the first work open to a life that had done nothing, so the page chose it for
            // him, and it stands on the ground of what is chosen...
            auto groundOf = [&](const char* id) { return f.ecs.getEntity(list->find(&f.ecs, id)->root->get<ActivityRowState>()->ground)->get<ThemeComponent>()->element; };

            EXPECT_TRUE(list->find(&f.ecs, "tomarket")->spec.featured);
            EXPECT_TRUE(list->find(&f.ecs, "tomarket")->root->get<ActivityRowState>()->featured);
            EXPECT_TRUE(list->find(&f.ecs, "tomarket")->root->get<ActivityRowState>()->selected);
            EXPECT_EQ(groundOf("tomarket"), "activity.kind.selected");
            EXPECT_FALSE(list->find(&f.ecs, "carters")->spec.featured);

            // ...and once nothing is, on its own
            list->select(&f.ecs, "");
            f.settle();

            EXPECT_EQ(groundOf("tomarket").find("activity.kind.featured"), 0u);

            // And it is one of the tiles that turn a life: framed in gold, its mark in gold,
            // and its gloss says what it does in the place of its group
            EXPECT_TRUE(list->find(&f.ecs, "tomarket")->spec.major);
            EXPECT_FALSE(list->find(&f.ecs, "tomarket")->frame.empty());
            EXPECT_FALSE(list->find(&f.ecs, "tomarket")->mark.entity.empty());
            EXPECT_FALSE(list->find(&f.ecs, "carters")->spec.major);
            EXPECT_EQ(f.ecs.getEntity(list->find(&f.ecs, "carters")->frame.id)->get<ThemeComponent>()->element, "activity.tile.frame");
            EXPECT_EQ(f.ecs.getEntity(list->find(&f.ecs, "tomarket")->frame.id)->get<ThemeComponent>()->element, "activity.major.frame");

            auto glosses = f.ecs.getSystem<GlossRegistry>();
            ASSERT_NE(glosses, nullptr);
            ASSERT_NE(glosses->find("activity/tomarket"), nullptr);
            EXPECT_EQ(glosses->find("activity/tomarket")->text.find("Opens the Town page"), 0u);
            EXPECT_EQ(glosses->find("activity/carters")->text, "Work");

            auto grid = life->piece<PlaceGrid>("places");
            ASSERT_NE(grid, nullptr);

            f.ecs.sendEvent(ActivityActivatedEvent{"life.activities", "tomarket"});
            f.settle();

            ASSERT_EQ(life->save.running, "tomarket");

            life->onMonth();
            f.settle();

            f.frames(12);

            // Known: the world's, and no stat of his
            EXPECT_TRUE(life->save.running.empty());
            EXPECT_TRUE(life->save.townKnown);
            EXPECT_EQ(life->save.stats.count("town_known"), 0u);
            EXPECT_EQ(f.fact<int>("stat.town_known"), 1);
            EXPECT_TRUE(tabs->enabled(1));
            EXPECT_TRUE(life->toastUp(life->town.opened));

            bool said = false;

            for (const auto& entry : life->save.log)
                said = said or (entry.kind == LogKind::Milestone and entry.text == life->town.opened);

            EXPECT_TRUE(said);
            EXPECT_FALSE(life->town.opened.empty());

            // The row that goes there is gone; the works for the town and the market's stalls have
            // come, on the town's page: the Life page has no tile that goes there
            EXPECT_EQ(list->find(&f.ecs, "tomarket"), nullptr);
            EXPECT_EQ(list->find(&f.ecs, "goto.market"), nullptr);
            EXPECT_NE(work(f, life, "raise.market.1"), nullptr);
            EXPECT_NE(stall(f, life, "buy.rations"), nullptr);

            // A first look at the town is two places, the Market and the Mill, and their two works:
            // the rest comes with what he does
            EXPECT_EQ(grid->tiles.size(), 2u);
            EXPECT_EQ(life->save.seen.size(), 2u);
            EXPECT_NE(grid->tile("market"), nullptr);
            EXPECT_NE(grid->tile("mill"), nullptr);
            EXPECT_NE(work(f, life, "raise.mill.1"), nullptr);
            EXPECT_EQ(list->find(&f.ecs, "raise.market.1"), nullptr);
            EXPECT_EQ(list->find(&f.ecs, "raise.mill.1"), nullptr);
            EXPECT_EQ(work(f, life, "raise.smithy.1"), nullptr);
            EXPECT_EQ(work(f, life, "raise.chapel.1"), nullptr);
            EXPECT_EQ(work(f, life, "raise.gate.1"), nullptr);

            // Its tab has come onto the page, with a notice of what is behind it: its two works
            EXPECT_TRUE(f.pos(tabs->tabs[1].face)->visible);
            EXPECT_FALSE(tabs->tabs[1].face->get<TabState>()->hidden);
            EXPECT_GE(tabs->tabs[1].notice, 2);
            ASSERT_FALSE(tabs->tabs[1].noticeGround.empty());
            EXPECT_TRUE(f.pos(tabs->tabs[1].noticeGround)->visible);
            EXPECT_EQ(tabs->tabs[0].notice, 0);

            // And the guide's word for it, its hand on the tab, until he opens it
            ASSERT_TRUE(life->step.active);
            EXPECT_EQ(life->step.id, "guide.town");
            EXPECT_EQ(life->handOn, tabs->tabs[1].face.id);
            EXPECT_FALSE(life->nextShown);
            EXPECT_TRUE(life->save.told.empty());

            f.ecs.sendEvent(TabSelectedEvent{tabs->root.id, "life.tabs", 1});
            f.frames(12);

            // Opened: the notice is gone, the step is said, and the next shows the market, where his
            // coin buys rations
            EXPECT_TRUE(life->townShown);
            EXPECT_EQ(f.fact<int>("life.town"), 1);
            EXPECT_EQ(tabs->tabs[1].notice, 0);
            ASSERT_EQ(life->save.told.size(), 1u);
            EXPECT_EQ(life->save.told[0], "guide.town");

            ASSERT_TRUE(life->step.active);
            EXPECT_EQ(life->step.id, "guide.market");
            ASSERT_NE(stall(f, life, "buy.rations"), nullptr);
            EXPECT_EQ(life->handOn, stall(f, life, "buy.rations")->root.id);
            EXPECT_NE(life->noteWords.spec.text.find("rations"), std::string::npos) << life->noteWords.spec.text;

            // It waits for him to buy some: no button passes it, and the hand goes to the one that
            // buys once the stall is chosen
            EXPECT_FALSE(life->nextShown);

            f.ecs.sendEvent(ActivitySelectedEvent{"town.market", "buy.rations"});
            f.settle();

            EXPECT_EQ(life->handOn, life->piece<Button>("begin")->root.id);
            EXPECT_NE(life->noteWords.spec.text.find("press Buy"), std::string::npos) << life->noteWords.spec.text;

            const int held = life->save.stats["rations"];

            f.ecs.sendEvent(ActivityActivatedEvent{"town.market", "buy.rations"});
            f.settle();
            f.frames(12);

            EXPECT_EQ(life->save.stats["rations"], held + 6);

            // Then the places
            ASSERT_TRUE(life->step.active);
            EXPECT_EQ(life->step.id, "guide.places");
            EXPECT_EQ(life->handOn, grid->tile("market")->root.id);

            f.ecs.sendEvent(ButtonActivatedEvent{life->noteNext.face.id, "life.guide.next"});
            f.frames(12);

            EXPECT_FALSE(life->step.active);
            EXPECT_EQ(life->save.told.size(), 3u);

            // The next life does not have the town's page: every boy is taken to the market once
            life->newLife();
            f.frames(12);
            f.pass(1000.0f);

            tabs = life->piece<Tabs>("tabs");
            ASSERT_NE(tabs, nullptr);

            EXPECT_FALSE(life->save.townKnown);
            EXPECT_FALSE(tabs->enabled(1));
            EXPECT_FALSE(f.pos(tabs->tabs[1].face)->visible);

            // His own month there, in his sixth year: the page is his, and the guide's words for
            // it, said once in a world, are not said again
            life->save.age = 6.0f + 7.0f / 12.0f;
            life->save.stats["rations"] = 6;
            life->onMonth();
            f.settle();

            f.ecs.sendEvent(ActivityActivatedEvent{"life.activities", "tomarket"});
            f.settle();

            ASSERT_EQ(life->save.running, "tomarket");

            life->onMonth();
            f.settle();
            f.pass(1000.0f);

            tabs = life->piece<Tabs>("tabs");
            ASSERT_NE(tabs, nullptr);

            EXPECT_TRUE(life->save.townKnown);
            EXPECT_EQ(life->save.told.size(), 3u);
            EXPECT_FALSE(life->step.active);
            EXPECT_TRUE(life->stepsDue.empty());
            EXPECT_TRUE(f.pos(tabs->tabs[1].face)->visible);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // A tile does not say what it asks: a work he cannot pay for, a purchase he has no coin for, a
        // class he is too young for are locked without a line of their own. Their gloss says it.
        TEST(lifescene_test, a_tile_does_not_say_what_it_asks)
        {
            MockLogger logger;
            LifeFixture f;

            LifeSceneOptions opt = LifeFixture::mockup();
            opt.fresh = true;
            opt.lives = 2;
            opt.age = 7.0f;

            LifeScene* life = f.life(opt);
            ASSERT_NE(life, nullptr);

            f.frames(12);

            life->save.townKnown = true;
            life->save.stats["rations"] = 40;
            life->save.stats["coin"] = 0;
            life->onMonth();
            f.frames(12);

            ActivityRow* rations = stall(f, life, "buy.rations");
            ASSERT_NE(rations, nullptr);
            EXPECT_EQ(rations->spec.state, ActivityState::Locked);
            EXPECT_EQ(rations->spec.until, "");
            EXPECT_FALSE(rations->until.has_value());

            // A work for the town has no tile of its own: its place's tile begins it, and does not
            // while he cannot pay for it
            for (const char* id : {"raise.market.1", "raise.mill.1"})
            {
                ASSERT_NE(work(f, life, id), nullptr) << id;
                EXPECT_TRUE(fieldFlag(work(f, life, id)->fields, "nextLocked")) << id;
            }

            f.ecs.sendEvent(PlaceSelectedEvent{"market"});
            f.settle();
            EXPECT_TRUE(life->save.running.empty());
            EXPECT_EQ(life->save.stats["coin"], 0);

            // Nor is its tile lit
            ASSERT_NE(life->piece<PlaceGrid>("places"), nullptr);

            for (const char* id : {"market", "mill"})
            {
                ASSERT_NE(life->piece<PlaceGrid>("places")->tile(id), nullptr) << id;
                EXPECT_FALSE(life->piece<PlaceGrid>("places")->tile(id)->spec.ready) << id;
            }

            // The place's gloss keeps it: the coin and the berries it takes, with what he has of
            // them, in the loss's colour. Each once: its time line says the time alone
            auto registry = f.ecs.getSystem<GlossRegistry>();
            ASSERT_NE(registry, nullptr);

            const GlossSpec* gloss = registry->find("place/market");
            ASSERT_NE(gloss, nullptr);

            int coin = 0;
            int berries = 0;

            for (const auto& row : gloss->rows)
            {
                if (row.label == "Coin")
                {
                    ++coin;
                    EXPECT_EQ(row.value, "\xE2\x88\x92" "5 (has 0)");
                    EXPECT_EQ(row.tone, "loss");
                }

                if (row.label == "Berries")
                {
                    ++berries;
                    EXPECT_EQ(row.value, "\xE2\x88\x92" "20 (has 0)");
                    EXPECT_EQ(row.tone, "loss");
                }

                if (row.label == "It takes")
                {
                    EXPECT_EQ(row.value.find("coin"), std::string::npos) << row.value;
                    EXPECT_EQ(row.value.find("Berries"), std::string::npos) << row.value;
                }
            }

            EXPECT_EQ(coin, 1);
            EXPECT_EQ(berries, 1);

            // Nor an age, nor a stat: the Keep is years away and says nothing of it, and its gloss does
            auto list = life->piece<ActivityList>("activities");
            ASSERT_NE(list, nullptr);
            ASSERT_NE(list->find(&f.ecs, "keep"), nullptr);
            EXPECT_EQ(list->find(&f.ecs, "keep")->spec.state, ActivityState::Locked);
            EXPECT_EQ(list->find(&f.ecs, "keep")->spec.until, "");
            EXPECT_FALSE(list->find(&f.ecs, "keep")->until.has_value());

            const GlossSpec* keep = registry->find("activity/keep");
            ASSERT_NE(keep, nullptr);

            bool age = false;

            for (const auto& row : keep->rows)
                age = age or (row.value.find("/ 16") != std::string::npos and row.tone == "loss");

            EXPECT_TRUE(age);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // The facts outlive a world: what a world before was told of the town stands among them when
        // this one opens. A first life is not told of the market for it, a month into its first work.
        TEST(lifescene_test, the_town_guide_waits_for_this_world)
        {
            MockLogger logger;
            LifeFixture f;

            // Left by a world before, as the engine's save keeps them
            f.facts->factMap["said.guide.town"] = ElementType{1};
            f.facts->factMap["said.guide.market"] = ElementType{1};

            LifeScene* life = f.life(LifeFixture::first());
            ASSERT_NE(life, nullptr);

            f.frames(12);
            f.pass(1000.0f);

            auto townSteps = [&]() {
                size_t steps = life->step.active and (life->step.id == "guide.town" or life->step.id == "guide.market" or life->step.id == "guide.places") ? 1 : 0;

                for (const auto& due : life->stepsDue)
                {
                    if (due.id == "guide.town" or due.id == "guide.market" or due.id == "guide.places")
                        ++steps;
                }

                return steps;
            };

            // This world has been told nothing, and its facts say so
            EXPECT_TRUE(life->save.told.empty());
            EXPECT_EQ(f.fact<int>("said.guide.town"), 0);
            EXPECT_EQ(f.fact<int>("said.guide.market"), 0);
            EXPECT_EQ(townSteps(), 0u);

            // His first work, and the months after it: the guide goes on with its own steps
            f.ecs.sendEvent(ActivityActivatedEvent{"life.activities", "helping"});
            f.settle();

            for (int i = 0; i < 3; ++i)
            {
                life->onMonth();
                f.settle();
            }

            f.frames(12);
            f.pass(1000.0f);

            EXPECT_FALSE(life->save.townKnown);
            EXPECT_TRUE(life->save.told.empty());
            EXPECT_EQ(townSteps(), 0u);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // What comes onto the page that is not the one in view is said at its tab's corner, and no
        // longer once that page is opened.
        TEST(lifescene_test, a_new_task_on_the_other_page_marks_its_tab)
        {
            MockLogger logger;
            LifeFixture f;

            LifeSceneOptions opt = LifeFixture::mockup();
            opt.fresh = true;
            opt.lives = 2;
            opt.age = 7.0f;

            LifeScene* life = f.life(opt);
            ASSERT_NE(life, nullptr);

            f.frames(12);

            auto tabs = life->piece<Tabs>("tabs");
            ASSERT_NE(tabs, nullptr);

            // He knows the town, with coin for it: on the Life page, what the town has is new behind
            // its tab; what came into the choice is in view, and no news
            life->save.townKnown = true;
            life->save.stats["rations"] = 40;
            life->save.stats["coin"] = 200;
            life->onMonth();
            f.frames(12);

            // The rations, water, firewood and stone it sells, and the three works for its first three
            // places (the market, the mill, and the inn of a second life)
            ASSERT_NE(stall(f, life, "buy.rations"), nullptr);
            EXPECT_EQ(tabs->tabs[1].notice, 7);
            EXPECT_EQ(life->newInTown, 7);
            EXPECT_EQ(tabs->tabs[0].notice, 0);
            ASSERT_FALSE(tabs->tabs[1].noticeGround.empty());
            EXPECT_TRUE(f.pos(tabs->tabs[1].noticeGround)->visible);

            // The Town page opened: seen
            f.ecs.sendEvent(TabSelectedEvent{tabs->root.id, "life.tabs", 1});
            f.frames(12);

            ASSERT_TRUE(life->townShown);
            EXPECT_EQ(tabs->tabs[1].notice, 0);
            EXPECT_EQ(life->newInTown, 0);
            EXPECT_FALSE(f.pos(tabs->tabs[1].noticeGround)->visible);

            // Eight years old, on the Town page: what came over his seventh year (the messages, the
            // cutpurses, the kitchen), the market's work and the streets come into the choice, behind
            // the Life tab. The wooden blade has come to the market, which is the page in view: no
            // news, and its practice has no row before he holds one. (Mill Pond has no row: he has
            // not walked the Hedgerows)
            life->save.age = 8.0f - 1.0f / 12.0f;
            life->onMonth();
            f.frames(12);

            EXPECT_NE(stall(f, life, "buy.blade"), nullptr);
            EXPECT_EQ(life->piece<ActivityList>("activities")->find(&f.ecs, "blades"), nullptr);
            EXPECT_EQ(tabs->tabs[0].notice, 6);
            EXPECT_EQ(life->newInLife, 6);
            EXPECT_EQ(life->piece<ActivityList>("activities")->find(&f.ecs, "walk.millpond"), nullptr);
            EXPECT_EQ(tabs->tabs[1].notice, 0);
            ASSERT_FALSE(tabs->tabs[0].noticeGround.empty());
            EXPECT_TRUE(f.pos(tabs->tabs[0].noticeGround)->visible);

            // Back to the Life page: seen too
            f.ecs.sendEvent(TabSelectedEvent{tabs->root.id, "life.tabs", 0});
            f.frames(12);

            ASSERT_FALSE(life->townShown);
            EXPECT_EQ(tabs->tabs[0].notice, 0);
            EXPECT_FALSE(f.pos(tabs->tabs[0].noticeGround)->visible);

            // The market's charter: a sword and a horse are sold there, behind the town's tab again
            life->save.town["market"] = 3;
            life->onMonth();
            f.frames(12);

            EXPECT_EQ(tabs->tabs[1].notice, 2);
            EXPECT_NE(stall(f, life, "buy.sword"), nullptr);

            // Out of coin, then in coin again: what the market sells was shut and is open, and
            // none of it is new. Nothing more is said at the tab
            life->save.stats["coin"] = 0;
            life->onMonth();
            f.frames(12);

            ASSERT_NE(stall(f, life, "buy.rations"), nullptr);
            EXPECT_EQ(stall(f, life, "buy.rations")->spec.state, ActivityState::Locked);
            EXPECT_EQ(tabs->tabs[1].notice, 2);

            life->save.stats["coin"] = 200;
            life->onMonth();
            f.frames(12);

            EXPECT_EQ(stall(f, life, "buy.rations")->spec.state, ActivityState::Idle);
            EXPECT_EQ(tabs->tabs[1].notice, 2);
            EXPECT_EQ(life->newInTown, 2);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // What a page gains while the other is in view is not drawn over it: the places made again
        // (one was raised, one was found), a line a tile did not have a month ago.
        TEST(lifescene_test, what_the_hidden_page_gains_is_not_drawn)
        {
            MockLogger logger;
            LifeFixture f;

            LifeScene* life = f.life();
            ASSERT_NE(life, nullptr);

            f.frames(12);

            ASSERT_TRUE(life->save.townKnown);
            ASSERT_FALSE(life->townShown);

            auto grid = life->piece<PlaceGrid>("places");
            ASSERT_NE(grid, nullptr);
            ASSERT_FALSE(grid->tiles.empty());

            // The Life page is in view, and the town's tiles are made again under it, as the scene
            // does when a place is raised or found
            std::vector<PlaceTileSpec> places;

            for (const auto& tile : grid->tiles)
                places.push_back(tile.spec);

            grid->setPlaces(&f.ecs, places);
            f.frames(6);

            for (const auto& tile : grid->tiles)
            {
                EXPECT_FALSE(f.pos(tile.root)->isRenderable()) << tile.spec.id;
                EXPECT_FALSE(f.pos(tile.ground)->isRenderable()) << tile.spec.id;
                EXPECT_FALSE(f.pos(tile.name.entity)->isRenderable()) << tile.spec.id;
                EXPECT_FALSE(f.pos(tile.line.entity)->isRenderable()) << tile.spec.id;

                for (const auto& seal : tile.seals)
                    EXPECT_FALSE(f.pos(seal.entity)->isRenderable()) << tile.spec.id;
            }

            // A row of the market is given a line it did not have
            ActivityRow* raise = stall(f, life, "buy.rations");
            ASSERT_NE(raise, nullptr);

            raise->setUntil(&f.ecs, "");
            f.frames(4);
            ASSERT_FALSE(raise->until);

            raise->setUntil(&f.ecs, "ASKS COIN 20");
            f.frames(6);

            ASSERT_TRUE(raise->until);
            EXPECT_FALSE(f.pos(raise->root)->isRenderable());
            EXPECT_FALSE(f.pos(raise->until->entity)->isRenderable());

            // The Town page in view: all of it is drawn, the new line with the rest
            life->showTown(true);
            f.frames(12);

            EXPECT_TRUE(f.pos(grid->tiles[0].root)->isRenderable());
            EXPECT_TRUE(f.pos(grid->tiles[0].name.entity)->isRenderable());
            EXPECT_TRUE(f.pos(raise->until->entity)->isRenderable());

            // And it is the choice's turn to gain a line out of sight
            auto list = life->piece<ActivityList>("activities");
            ASSERT_NE(list, nullptr);
            ASSERT_FALSE(list->spec.groups.empty());
            ASSERT_FALSE(list->spec.groups[0].rows.empty());

            ActivityRow* first = list->find(&f.ecs, list->spec.groups[0].rows[0].id);
            ASSERT_NE(first, nullptr);

            first->setUntil(&f.ecs, "");
            f.frames(4);
            first->setUntil(&f.ecs, "CLOSES 1/2", true);
            f.frames(6);

            ASSERT_TRUE(first->until);
            EXPECT_FALSE(f.pos(first->root)->isRenderable());
            EXPECT_FALSE(f.pos(first->until->entity)->isRenderable());

            life->showTown(false);
            f.frames(12);

            EXPECT_TRUE(f.pos(first->until->entity)->isRenderable());
            EXPECT_FALSE(f.pos(grid->tiles[0].name.entity)->isRenderable());
            EXPECT_FALSE(f.pos(raise->until->entity)->isRenderable());
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // The Town page takes the choice's place in the middle column, and gives it back: the places
        // in a grid, what is bought on the spot under them, and the two other columns as they were.
        TEST(lifescene_test, the_town_page_swaps_the_middle_column)
        {
            MockLogger logger;
            LifeFixture f;

            LifeScene* life = f.life();
            ASSERT_NE(life, nullptr);

            f.frames(12);

            // What the market's stall asks besides its coin, so that its tile begins it: the page
            // reads it the next month
            life->save.stats["berries"] = 20;
            life->save.stats["firewood"] = 20;
            life->onMonth();
            f.frames(12);

            auto tabs = life->piece<Tabs>("tabs");
            ASSERT_NE(tabs, nullptr);
            ASSERT_TRUE(life->save.townKnown);
            ASSERT_TRUE(tabs->enabled(1));

            // The Life page first
            EXPECT_FALSE(life->townShown);
            EXPECT_TRUE(f.pos(life->named("activities"))->visible);
            EXPECT_FALSE(f.pos(life->named("town"))->visible);

            const float holdsX = f.pos(life->named("holds"))->x;
            const float clockX = f.pos(life->named("clockPanel"))->x;
            const float mayX = f.pos(life->named("may"))->x;
            const float mayWidth = f.pos(life->named("may"))->width;

            f.ecs.sendEvent(TabSelectedEvent{tabs->root.id, "life.tabs", 1});
            f.frames(12);

            EXPECT_TRUE(life->townShown);
            EXPECT_FALSE(f.pos(life->named("activities"))->visible);
            EXPECT_TRUE(f.pos(life->named("town"))->visible);

            // The places on his page, as the rules list them, none raised yet: the two every life
            // starts with. He has sworn to the Keep and done nothing there yet that brings a place:
            // the others are a veteran's, a captain's, a scholar's, a smith's, or a later life's
            auto grid = life->piece<PlaceGrid>("places");
            ASSERT_NE(grid, nullptr);
            ASSERT_EQ(life->town.places.size(), 11u);
            ASSERT_EQ(grid->tiles.size(), 2u);

            const char* const onHisPage[] = {"market", "mill"};

            for (size_t i = 0; i < grid->tiles.size(); ++i)
                EXPECT_EQ(grid->tiles[i].spec.id, onHisPage[i]) << i;

            for (const char* id : {"smithy", "yard", "gate", "chapel", "collegium", "harrow", "inn", "storehouse"})
                EXPECT_EQ(grid->tile(id), nullptr) << id;

            EXPECT_EQ(life->save.seen.size(), 2u);
            EXPECT_EQ(grid->columns(), 3);

            for (const auto& tile : grid->tiles)
            {
                EXPECT_EQ(tile.spec.level, 0) << tile.spec.id;
                EXPECT_EQ(tile.spec.of, 3) << tile.spec.id;
                EXPECT_EQ(tile.spec.line, "NOT YET BUILT") << tile.spec.id;
            }

            // Each explains itself: what its first level asks, and the work that raises it
            auto registry = f.ecs.getSystem<GlossRegistry>();
            ASSERT_NE(registry, nullptr);
            ASSERT_NE(registry->find("place/market"), nullptr);
            EXPECT_EQ(registry->find("place/market")->title, "The Market");
            EXPECT_EQ(registry->find("place/market")->aside, "0/3");
            EXPECT_NE(registry->find("place/market")->footnote.find("ENDOW THE MARKET STALL"), std::string::npos);

            // And every level it has, what each gives, none of them raised yet: all in the faint ink
            std::vector<const GlossRow*> tiers;
            bool listing = false;

            for (const auto& row : registry->find("place/market")->rows)
            {
                if (row.heading)
                    listing = row.label == "UPGRADES";
                else if (listing)
                    tiers.push_back(&row);
            }

            ASSERT_EQ(tiers.size(), 3u);
            EXPECT_EQ(tiers[0]->label, "1 - The market stall: Buy rations brings 8 rations, not 6");
            EXPECT_EQ(tiers[2]->label, "3 - The market charter: A sword and a horse are sold at the market");

            for (const GlossRow* tier : tiers)
            {
                EXPECT_FALSE(tier->strong) << tier->label;
                EXPECT_TRUE(tier->faint) << tier->label;
            }

            // What is bought on the spot is here, and no longer in the choice
            EXPECT_NE(stall(f, life, "buy.rations"), nullptr);
            EXPECT_EQ(life->piece<ActivityList>("activities")->find(&f.ecs, "buy.rations"), nullptr);

            // The works that raise a place have no row, on this page or in the choice: the next
            // level of each place is begun from its tile
            EXPECT_NE(work(f, life, "raise.market.1"), nullptr);
            EXPECT_NE(work(f, life, "raise.mill.1"), nullptr);
            EXPECT_EQ(life->piece<ActivityList>("activities")->find(&f.ecs, "raise.market.1"), nullptr);
            EXPECT_EQ(life->piece<ActivityList>("marketRows")->find(&f.ecs, "raise.market.1"), nullptr);

            // A place clicked chooses it, as a tile of the lists; the button begins it: its coin
            // taken, its months running
            ASSERT_TRUE(life->save.running.empty());

            const int coin = life->save.stats["coin"];
            ASSERT_GE(coin, 5);

            // While a click would raise it, its tile is lit
            EXPECT_TRUE(grid->tile("market")->spec.ready);

            f.ecs.sendEvent(PlaceSelectedEvent{"market"});
            f.settle();
            EXPECT_TRUE(life->save.running.empty());
            EXPECT_EQ(life->save.stats["coin"], coin);
            EXPECT_EQ(grid->selected(), "market");

            // Clicked again, but not soon: chosen still, and nothing begun
            f.ecs.sendEvent(TickEvent{500.0f});
            f.settle();
            f.ecs.sendEvent(PlaceSelectedEvent{"market"});
            f.settle();
            EXPECT_TRUE(life->save.running.empty());
            EXPECT_EQ(grid->selected(), "market");

            f.ecs.sendEvent(ButtonActivatedEvent{life->piece<Button>("begin")->face.id, "life.begin"});
            f.settle();
            EXPECT_EQ(life->save.running, "raise.market.1");
            EXPECT_EQ(life->save.stats["coin"], coin - 5);
            EXPECT_EQ(life->save.stats["berries"], 0);
            EXPECT_EQ(life->save.stats["firewood"], 0);
            EXPECT_EQ(grid->selected(), "");

            // At work, no tile is: a click would begin nothing
            for (const auto& tile : grid->tiles)
                EXPECT_FALSE(tile.spec.ready) << tile.spec.id;

            // Another clicked while he is at it begins nothing
            f.ecs.sendEvent(PlaceSelectedEvent{"mill"});
            f.settle();
            EXPECT_EQ(life->save.running, "raise.market.1");
            EXPECT_EQ(life->save.stats["coin"], coin - 5);

            // The two other columns have not moved, nor has the middle one's panel
            EXPECT_FLOAT_EQ(f.pos(life->named("holds"))->x, holdsX);
            EXPECT_FLOAT_EQ(f.pos(life->named("clockPanel"))->x, clockX);
            EXPECT_FLOAT_EQ(f.pos(life->named("may"))->x, mayX);
            EXPECT_FLOAT_EQ(f.pos(life->named("may"))->width, mayWidth);

            // And back
            f.ecs.sendEvent(TabSelectedEvent{tabs->root.id, "life.tabs", 0});
            f.frames(12);

            EXPECT_FALSE(life->townShown);
            EXPECT_TRUE(f.pos(life->named("activities"))->visible);
            EXPECT_FALSE(f.pos(life->named("town"))->visible);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // Toasts in a crowd: four up at most, the oldest pushed out; a line said again kept once;
        // one over another, with no room left where one has gone; a line wider than the window
        // wrapped inside it.
        TEST(lifescene_test, toasts_in_a_crowd)
        {
            MockLogger logger;
            LifeFixture f;

            LifeScene* life = f.life(LifeFixture::first());
            ASSERT_NE(life, nullptr);

            f.frames(12);

            // Past the page's arrival, when nothing is toasted, and what it toasted gone
            f.pass(5000.0f);

            for (const char* line : {"One", "Two", "Three"})
                life->toast(line, 60000.0f);

            life->toast("Four", 500.0f);
            life->toast("Five", 60000.0f);
            life->toast("Six", 60000.0f);
            f.frames(6);

            EXPECT_EQ(life->toastsUp(), 4);
            EXPECT_FALSE(life->toastUp("One"));
            EXPECT_FALSE(life->toastUp("Two"));
            EXPECT_TRUE(life->toastUp("Three"));
            EXPECT_TRUE(life->toastUp("Four"));
            EXPECT_TRUE(life->toastUp("Six"));

            life->toast("Five");
            EXPECT_EQ(life->toastsUp(), 4);

            // Each over the one under it, 8 apart, none over another
            auto stacked = [&]() {
                for (size_t i = 1; i < life->shownToasts.size(); ++i)
                {
                    auto lower = f.ecs.getEntity(life->shownToasts[i - 1].groundId)->get<PositionComponent>();
                    auto upper = f.ecs.getEntity(life->shownToasts[i].groundId)->get<PositionComponent>();

                    EXPECT_NEAR(upper->y + upper->height + 8.0f, lower->y, 1.0f) << life->shownToasts[i].text;
                }
            };

            stacked();

            // The short one goes, and the ones over it come down into its room
            f.pass(1000.0f);

            EXPECT_FALSE(life->toastUp("Four"));
            EXPECT_TRUE(life->toastUp("Six"));
            stacked();

            // A line wider than the window wraps inside it, its slip as tall as its lines
            std::string longLine;

            for (int i = 0; i < 40; ++i)
                longLine += "No rations left, buy some at the market ";

            life->toast(longLine);
            f.frames(6);

            ASSERT_FALSE(life->shownToasts.empty());
            ASSERT_EQ(life->shownToasts.back().text, longLine);

            auto window = f.ecs.getEntity("__MainWindow")->get<PositionComponent>();
            auto slip = f.ecs.getEntity(life->shownToasts.back().groundId)->get<PositionComponent>();

            EXPECT_GE(slip->x, window->x);
            EXPECT_LE(slip->x + slip->width, window->x + window->width);
            EXPECT_GT(slip->height, 36.0f);
            stacked();
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // The market is reached by the Town tab, which opens its page and does nothing else: no
        // term, no month, no line. No tile of the Life page goes there.
        TEST(lifescene_test, the_market_is_reached_by_its_tab)
        {
            MockLogger logger;
            LifeFixture f;

            LifeScene* life = f.life();
            ASSERT_NE(life, nullptr);

            f.frames(12);

            auto tabs = life->piece<Tabs>("tabs");
            auto list = life->piece<ActivityList>("activities");
            ASSERT_NE(tabs, nullptr);
            ASSERT_NE(list, nullptr);
            EXPECT_EQ(list->find(&f.ecs, "goto.market"), nullptr);

            for (const auto& group : list->spec.groups)
                EXPECT_NE(group.label, "Market");

            const float age = life->save.age;
            const int world = life->save.world;
            const size_t logEntries = life->save.log.size();
            const int atOnce = life->save.atOnce;

            f.ecs.sendEvent(TabSelectedEvent{tabs->root.id, "life.tabs", 1});
            f.frames(12);

            EXPECT_TRUE(life->townShown);
            EXPECT_EQ(tabs->active(), 1);
            EXPECT_TRUE(f.pos(life->named("town"))->visible);

            EXPECT_FLOAT_EQ(life->save.age, age);
            EXPECT_EQ(life->save.world, world);
            EXPECT_TRUE(life->save.running.empty());
            EXPECT_EQ(life->save.done.count("goto.market"), 0u);
            EXPECT_EQ(life->save.atOnce, atOnce);
            EXPECT_EQ(life->save.log.size(), logEntries);

            // What is bought there is bought from the market's own list
            const int rations = life->save.stats["rations"];

            f.ecs.sendEvent(ActivityActivatedEvent{"town.market", "buy.rations"});
            f.settle();

            EXPECT_EQ(life->save.stats["rations"], rations + 6);
            EXPECT_TRUE(life->townShown);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // A work for the town: its coin taken as it begins, its months, and at its term the place is
        // the world's at that level. The log and its lore say so, its tile has a seal, and what
        // the level gives is given on the spot.
        TEST(lifescene_test, raising_a_place)
        {
            MockLogger logger;
            LifeFixture f;

            LifeScene* life = f.life();
            ASSERT_NE(life, nullptr);

            f.frames(12);

            // What the market's stall asks besides its coin: the page reads it the next month
            life->save.stats["rations"] = 24;
            life->save.stats["berries"] = 20;
            life->save.stats["firewood"] = 20;
            life->onMonth();
            f.frames(12);

            auto list = life->piece<ActivityList>("activities");
            auto grid = life->piece<PlaceGrid>("places");
            ASSERT_NE(list, nullptr);
            ASSERT_NE(grid, nullptr);
            ASSERT_NE(work(f, life, "raise.market.1"), nullptr);
            EXPECT_EQ(work(f, life, "raise.market.2"), nullptr);

            const int coin = life->save.stats["coin"];
            const int millMonths = list->find(&f.ecs, "mill") ? list->find(&f.ecs, "mill")->spec.months : -1;

            ASSERT_GE(coin, 5);

            // A double click on the place begins it, as on a row
            f.ecs.sendEvent(PlaceSelectedEvent{"market"});
            f.settle();
            f.ecs.sendEvent(PlaceSelectedEvent{"market"});
            f.settle();

            ASSERT_EQ(life->save.running, "raise.market.1");
            EXPECT_EQ(life->save.stats["coin"], coin - 5);
            EXPECT_EQ(life->save.stats["berries"], 0);
            EXPECT_EQ(life->save.stats["firewood"], 0);
            EXPECT_EQ(life->save.town.count("market"), 0u);

            for (int i = 0; i < 6; ++i)
            {
                life->onMonth();
                f.settle();
            }

            f.frames(12);

            EXPECT_TRUE(life->save.running.empty());
            EXPECT_EQ(life->save.done["raise.market.1"], 1);
            EXPECT_EQ(life->save.town["market"], 1);
            EXPECT_EQ(life->save.fund.count("market"), 0u);
            EXPECT_EQ(f.fact<int>("stat.town.market"), 1);

            // Who raised it, and in which year of the world
            ASSERT_EQ(life->save.raisedBy.count("market"), 1u);
            EXPECT_EQ(life->save.raisedBy["market"].find(life->save.name), 0u);
            EXPECT_NE(life->save.raisedBy["market"].find("YEAR"), std::string::npos);

            // The log: what stands there now, then the line the place keeps of it
            bool built = false;
            bool lore = false;

            for (const auto& entry : life->save.log)
            {
                built = built or (entry.kind == LogKind::Milestone and entry.text.find("The market stall") == 0);
                lore = lore or (entry.kind == LogKind::Lore and entry.text.find("A stall of his own name") == 0);
            }

            EXPECT_TRUE(built);
            EXPECT_TRUE(lore);

            // Its tile: one seal earned, and what it gives now
            PlaceTile* market = grid->tile("market");
            ASSERT_NE(market, nullptr);
            EXPECT_EQ(market->spec.level, 1);
            ASSERT_EQ(market->seals.size(), 3u);
            EXPECT_EQ(f.ecs.getEntity(market->seals[0].entity.id)->get<ThemeComponent>()->element, "place.seal.earned");
            EXPECT_EQ(f.ecs.getEntity(market->seals[1].entity.id)->get<ThemeComponent>()->element, "place.seal.empty");
            EXPECT_NE(market->spec.line, "NOT YET BUILT");

            auto raisedLabel = life->piece<Label>("raised");
            ASSERT_NE(raisedLabel, nullptr);
            EXPECT_NE(raisedLabel->spec.text.find("THE MARKET"), std::string::npos);

            // The next work for it has taken the first one's place
            EXPECT_EQ(work(f, life, "raise.market.1"), nullptr);
            EXPECT_NE(work(f, life, "raise.market.2"), nullptr);

            // What it gives is given on the spot: rations are 8 for 5 coin now, and nothing else moved
            const int rations = life->save.stats["rations"];

            life->save.stats["coin"] = 5;
            life->onMonth();
            f.settle();

            f.ecs.sendEvent(ActivityActivatedEvent{"town.market", "buy.rations"});
            f.settle();

            EXPECT_EQ(life->save.stats["rations"], rations - 1 + 8);
            EXPECT_EQ(life->save.stats["coin"], 0);

            if (millMonths >= 0 and list->find(&f.ecs, "mill"))
                EXPECT_EQ(list->find(&f.ecs, "mill")->spec.months, millMonths);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // A work that costs years of his life: the clock jumps as it begins, his and the world's.
        TEST(lifescene_test, years_cost_moves_the_clock)
        {
            MockLogger logger;
            LifeFixture f;

            LifeScene* life = f.life();
            ASSERT_NE(life, nullptr);

            f.frames(12);

            // The chapel at its second level, and what its almshouse asks: a little coin, and resources
            life->save.town["chapel"] = 2;
            life->save.stats["coin"] = 100;
            life->save.stats["water"] = 80;
            life->save.stats["clay"] = 20;
            life->save.stats["gems"] = 4;
            life->save.stats["antler"] = 1;
            life->save.stats["rations"] = 24;
            life->onMonth();
            f.frames(12);

            auto list = life->piece<ActivityList>("activities");
            ASSERT_NE(list, nullptr);

            const RulePlace* almshouse = work(f, life, "raise.chapel.3");
            ASSERT_NE(almshouse, nullptr);
            EXPECT_EQ(fieldInt(almshouse->fields, "nextYears"), 2);
            EXPECT_EQ(fieldInt(almshouse->fields, "nextMonths"), 0);

            const float age = life->save.age;
            const int world = life->save.world;

            f.ecs.sendEvent(PlaceSelectedEvent{"chapel"});
            f.settle();
            f.ecs.sendEvent(ButtonActivatedEvent{life->piece<Button>("begin")->face.id, "life.begin"});
            f.frames(12);

            // Done at once, and two years older for it
            EXPECT_TRUE(life->save.running.empty());
            EXPECT_NEAR(life->save.age, age + 2.0f, 0.001f);
            EXPECT_EQ(life->save.world, world + 24);
            EXPECT_EQ(life->rules.world, world + 24);
            EXPECT_EQ(life->save.stats["coin"], 85);
            EXPECT_EQ(life->save.stats["water"], 0);
            EXPECT_EQ(life->save.stats["antler"], 0);
            EXPECT_EQ(life->save.town["chapel"], 3);
            EXPECT_EQ(life->save.done["raise.chapel.3"], 1);
            EXPECT_NEAR(f.fact<float>("life.age"), age + 2.0f, 0.001f);
            EXPECT_EQ(work(f, life, "raise.chapel.3"), nullptr);

            // Its gloss says all the chapel gives, its three levels and not the last alone, all in bold
            auto registry = f.ecs.getSystem<GlossRegistry>();
            ASSERT_NE(registry, nullptr);

            const GlossSpec* chapel = registry->find("place/chapel");
            ASSERT_NE(chapel, nullptr);
            EXPECT_EQ(chapel->aside, "3/3");

            std::vector<std::string> gives;
            bool giving = false;

            for (const auto& row : chapel->rows)
            {
                if (row.heading)
                    giving = row.label == "UPGRADES";
                else if (giving and row.strong)
                    gives.push_back(row.label);
            }

            ASSERT_EQ(gives.size(), 3u);
            EXPECT_EQ(gives[0], "1 - The chapel windows: Learn Your Letters takes 4 months, not 6");
            EXPECT_EQ(gives[1], "2 - The chapel primer: A new life starts with Letters 1, and a well imp can be bound");
            EXPECT_EQ(gives[2], "3 - The almshouse: Teach the Young pays 4 more coin");
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // What a life gave the town outlives it: the next one is born knowing the town, into its
        // places as they stand, and with what they give a life at its birth.
        TEST(lifescene_test, the_town_survives_death)
        {
            MockLogger logger;
            LifeFixture f;

            LifeScene* life = f.life();
            ASSERT_NE(life, nullptr);

            f.frames(12);

            life->save.town = {{"mill", 2}, {"chapel", 2}, {"gate", 1}};
            life->save.raisedBy = {{"mill", "Aldren of Bellmoor \xC2\xB7 YEAR 10"}};
            life->save.fund = {{"yard", 7}};

            // His last month
            life->save.stats["rations"] = 0;
            life->save.stats["vit"] = 1;
            life->onMonth();
            f.settle();

            ASSERT_TRUE(life->ended);

            const int world = life->save.world;

            life->beginAgain();
            f.frames(12);

            EXPECT_FALSE(life->ended);
            EXPECT_EQ(life->save.lives, 2);
            EXPECT_EQ(life->save.world, world);

            // The town, as he left it; its page is not his yet
            EXPECT_FALSE(life->save.townKnown);
            EXPECT_EQ(life->save.town["mill"], 2);
            EXPECT_EQ(life->save.town["chapel"], 2);
            EXPECT_EQ(life->save.town["gate"], 1);
            EXPECT_EQ(life->save.raisedBy["mill"], "Aldren of Bellmoor \xC2\xB7 YEAR 10");
            EXPECT_EQ(life->save.fund["yard"], 7);

            // A new life's opening: 6 years old, a boy's parts, and what the town gives on top (the
            // mill loft gives a familiar slot, not rations: he is born with none)
            EXPECT_FLOAT_EQ(life->save.age, 6.0f);
            EXPECT_EQ(life->save.stats["rations"], 0);
            EXPECT_EQ(life->save.stats["letters"], 1);
            EXPECT_EQ(life->save.stats["watch_known"], 1);
            EXPECT_EQ(life->save.stats["str"], 4);
            EXPECT_EQ(life->save.stats.count("town_known"), 0u);

            // On the page from its first frame: his letters among his skills, his tie in the ledger
            auto ledger = life->piece<ResourceLedger>("ledger");
            auto skills = life->piece<ResourceLedger>("skills");
            ASSERT_NE(ledger, nullptr);
            ASSERT_NE(skills, nullptr);
            EXPECT_NE(ledger->row("watch_known"), nullptr);
            EXPECT_NE(skills->row("letters"), nullptr);

            // The page opens on the choice, and the town's tab is not there: nothing of the town
            // to work at before he has been to its market
            auto tabs = life->piece<Tabs>("tabs");
            auto list = life->piece<ActivityList>("activities");
            ASSERT_NE(tabs, nullptr);
            ASSERT_NE(list, nullptr);

            EXPECT_FALSE(life->townShown);
            EXPECT_FALSE(tabs->enabled(1));
            EXPECT_EQ(list->find(&f.ecs, "explore"), nullptr);
            EXPECT_EQ(list->find(&f.ecs, "goto.market"), nullptr);
            EXPECT_EQ(work(f, life, "raise.mill.3"), nullptr);

            // His month at the market, in his sixth year: the page is his, with the places as the
            // last life left them and the works that raise them further
            life->save.age = 6.0f + 7.0f / 12.0f;
            life->onMonth();
            f.settle();

            f.ecs.sendEvent(ActivityActivatedEvent{"life.activities", "tomarket"});
            f.settle();

            ASSERT_EQ(life->save.running, "tomarket");

            life->onMonth();
            f.settle();
            f.frames(12);

            tabs = life->piece<Tabs>("tabs");
            ASSERT_NE(tabs, nullptr);

            EXPECT_TRUE(life->save.townKnown);
            EXPECT_TRUE(tabs->enabled(1));
            EXPECT_NE(work(f, life, "raise.mill.3"), nullptr);

            // What the chapel's first level gives, in this life as in the last: his letters in 4
            // months, the day he is seven
            life->save.age = 7.0f - 1.0f / 12.0f;
            life->onMonth();
            f.settle();

            list = life->piece<ActivityList>("activities");
            ASSERT_NE(list, nullptr);
            ASSERT_NE(list->row(&f.ecs, "letters"), nullptr);
            EXPECT_EQ(list->row(&f.ecs, "letters")->spec.months, 4);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // At his death the coin he holds can be left to a place: it is counted toward the place's
        // next level, for whoever raises it. A gift is his to make or not.
        TEST(lifescene_test, gift_at_death_funds_a_place)
        {
            MockLogger logger;
            LifeFixture f;

            LifeScene* life = f.life();
            ASSERT_NE(life, nullptr);

            f.frames(12);

            // The chapel is a place a life before him found: it is on his page with his own two
            life->save.stats["coin"] = 84;
            life->save.town = {{"mill", 3}};
            life->save.seen.push_back("chapel");
            life->save.stats["rations"] = 0;
            life->save.stats["vit"] = 1;
            life->onMonth();
            f.frames(12);

            ASSERT_TRUE(life->ended);
            ASSERT_FALSE(life->ending.empty());

            auto prefab = life->ending->get<Prefab>();

            // The offer: his coin, and a button a place that can still be raised
            EntityRef line = prefab->findEntity("giftLabel");
            ASSERT_FALSE(line.empty());
            EXPECT_TRUE(f.pos(line)->visible);
            EXPECT_EQ(line->get<Label>()->spec.text, "LEAVE HIS 84 COIN TO");

            // The two on his page that are not at their last level, the market and the chapel: the
            // mill is, and the others are not on his page
            ASSERT_EQ(life->gifts.size(), 2u);

            for (const auto& [id, button] : life->gifts)
                EXPECT_TRUE(id == "market" or id == "chapel") << id;

            EXPECT_TRUE(life->gift.empty());

            Button again = *prefab->findEntity("again")->get<Button>().component;
            EXPECT_EQ(again.spec.label, "Begin a new life");

            // The chapel chosen: the others step back, and the button says what it will do
            Button chapel;

            for (const auto& [id, button] : life->gifts)
            {
                if (id == "chapel")
                    chapel = button;
            }

            ASSERT_FALSE(chapel.face.empty());

            f.ecs.sendEvent(ButtonActivatedEvent{chapel.face.id, "life.gift.chapel"});
            f.settle();

            EXPECT_EQ(life->gift, "chapel");

            for (const auto& [id, button] : life->gifts)
                EXPECT_EQ(f.ecs.getEntity(button.face.id)->get<ButtonState>()->disabled, id != "chapel") << id;

            EXPECT_EQ(prefab->findEntity("again")->get<Button>()->spec.label, "Leave it, and begin a new life");

            // Chosen again it is let go of, then taken up again
            f.ecs.sendEvent(ButtonActivatedEvent{chapel.face.id, "life.gift.chapel"});
            f.settle();
            EXPECT_TRUE(life->gift.empty());
            EXPECT_EQ(prefab->findEntity("again")->get<Button>()->spec.label, "Begin a new life");

            f.ecs.sendEvent(ButtonActivatedEvent{chapel.face.id, "life.gift.chapel"});
            f.settle();
            ASSERT_EQ(life->gift, "chapel");

            f.ecs.sendEvent(ButtonActivatedEvent{again.face.id, "life.again"});
            f.frames(12);

            // The next life: the fund is the chapel's, its first level asks no coin, and its gloss says why
            EXPECT_FALSE(life->ended);
            EXPECT_EQ(life->save.fund["chapel"], 84);
            EXPECT_EQ(life->save.stats["coin"], 0);
            EXPECT_TRUE(life->gifts.empty());

            for (const auto& place : life->town.places)
            {
                const auto id = place.fields.find("id");

                if (id != place.fields.end() and id->second.toString() == "chapel")
                {
                    EXPECT_EQ(place.fields.at("nextCoin").get<int>(), 0);
                    EXPECT_EQ(place.fields.at("fund").get<int>(), 84);
                }
            }

            auto list = life->piece<ActivityList>("activities");
            ASSERT_NE(list, nullptr);

            // (the town's page is his once he has been to its market: taken there here), and what the
            // windows ask besides coin
            life->save.townKnown = true;
            life->save.stats["water"] = 20;
            life->save.stats["clay"] = 20;
            life->onMonth();
            f.frames(12);

            const RulePlace* glaze = work(f, life, "raise.chapel.1");
            ASSERT_NE(glaze, nullptr);
            EXPECT_FALSE(fieldFlag(glaze->fields, "nextLocked"));

            // Raised, the fund pays the windows' 5 coin, and keeps the rest toward the primer
            f.ecs.sendEvent(PlaceSelectedEvent{"chapel"});
            f.settle();
            f.ecs.sendEvent(ButtonActivatedEvent{life->piece<Button>("begin")->face.id, "life.begin"});
            f.settle();

            ASSERT_EQ(life->save.running, "raise.chapel.1");

            for (int i = 0; i < 6; ++i)
            {
                life->onMonth();
                f.settle();
            }

            f.frames(12);

            EXPECT_EQ(life->save.town["chapel"], 1);
            EXPECT_EQ(life->save.fund["chapel"], 84 - 5);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // The town grows with what he does: two places at first, the smithy with a term at the
        // forge, the yard and the gate with the Keep's border and its chain, the inn with the second
        // life of a world. A place that comes later is said; one that has been on the page stays on
        // it, in this life and in the next.
        TEST(lifescene_test, the_town_grows_with_the_life)
        {
            MockLogger logger;
            LifeFixture f;

            LifeSceneOptions opt = LifeFixture::mockup();
            opt.fresh = true;
            opt.lives = 2;
            opt.age = 7.0f;

            LifeScene* life = f.life(opt);
            ASSERT_NE(life, nullptr);

            f.frames(12);

            // He knows the town, at 7: a month tells the page so
            life->save.townKnown = true;
            life->save.stats["rations"] = 40;
            life->onMonth();
            f.frames(12);

            auto grid = life->piece<PlaceGrid>("places");
            auto list = life->piece<ActivityList>("activities");
            ASSERT_NE(grid, nullptr);
            ASSERT_NE(list, nullptr);

            auto found = [&]() {
                size_t lines = 0;

                for (const auto& entry : life->save.log)
                {
                    if (entry.kind == LogKind::Milestone and entry.text.find(life->town.foundLine) != std::string::npos)
                        ++lines;
                }

                return lines;
            };

            // The works for the town: a place on the page with a next level to raise
            auto works = [&]() {
                size_t next = 0;

                for (const auto& place : life->town.places)
                {
                    auto id = place.fields.find("nextWork");

                    if (fieldFlag(place.fields, "shown") and id != place.fields.end() and not id->second.toString().empty())
                        ++next;
                }

                return next;
            };

            ASSERT_FALSE(life->town.foundLine.empty());

            // The first look, in a second life: the market and the mill, and the inn, which a world
            // has from its second life. Their three works, and no line
            ASSERT_EQ(grid->tiles.size(), 3u);
            EXPECT_EQ(grid->tiles[0].spec.id, "market");
            EXPECT_EQ(grid->tiles[1].spec.id, "mill");
            EXPECT_EQ(grid->tiles[2].spec.id, "inn");
            EXPECT_EQ(life->save.seen.size(), 3u);
            EXPECT_EQ(works(), 3u);
            EXPECT_EQ(found(), 0u);

            // A term at the forge: the smithy comes onto the page, with its work and a line. The
            // rules are handed the terms when one ends: written here by hand, handed by hand
            life->save.done["smithy"] = 1;
            life->rules.done = life->save.terms();
            life->onMonth();
            f.frames(12);

            ASSERT_EQ(grid->tiles.size(), 4u);
            EXPECT_EQ(grid->tiles[2].spec.id, "smithy");
            EXPECT_EQ(life->save.seen.size(), 4u);
            EXPECT_EQ(works(), 4u);
            EXPECT_EQ(found(), 1u);
            EXPECT_NE(work(f, life, "raise.smithy.1"), nullptr);

            auto registry = f.ecs.getSystem<GlossRegistry>();
            ASSERT_NE(registry, nullptr);
            EXPECT_NE(registry->find("place/smithy"), nullptr);

            // Sworn to the Keep: the oath alone brings no place
            life->save.stats["keep_oath"] = 1;
            life->onMonth();
            f.frames(12);

            EXPECT_EQ(grid->tiles.size(), 4u);
            EXPECT_EQ(found(), 1u);

            // A veteran of the border: the yard is his to build
            life->save.stats["veteran"] = 1;
            life->onMonth();
            f.frames(12);

            ASSERT_EQ(grid->tiles.size(), 5u);
            EXPECT_EQ(grid->tiles[3].spec.id, "yard");
            EXPECT_EQ(found(), 2u);
            EXPECT_NE(work(f, life, "raise.yard.1"), nullptr);
            EXPECT_EQ(grid->tile("gate"), nullptr);

            // Its captain: the Watch Gate
            life->save.stats["captain"] = 1;
            life->onMonth();
            f.frames(12);

            ASSERT_EQ(grid->tiles.size(), 6u);
            EXPECT_EQ(grid->tiles[4].spec.id, "gate");
            EXPECT_EQ(found(), 3u);
            EXPECT_EQ(grid->tile("chapel"), nullptr);
            EXPECT_EQ(grid->tile("collegium"), nullptr);
            EXPECT_EQ(grid->tile("harrow"), nullptr);

            // Nothing is said twice
            life->onMonth();
            f.frames(12);
            EXPECT_EQ(found(), 3u);

            // The next life, the third of its world, finds the town as it was seen once he has been
            // to its market: six places, of no class
            life->newLife();
            f.frames(12);

            EXPECT_FLOAT_EQ(life->save.age, 6.0f);
            EXPECT_FALSE(life->save.townKnown);

            life->save.townKnown = true;
            life->onMonth();
            f.frames(12);

            grid = life->piece<PlaceGrid>("places");
            list = life->piece<ActivityList>("activities");
            ASSERT_NE(grid, nullptr);
            ASSERT_NE(list, nullptr);

            EXPECT_EQ(life->save.stats.count("keep_oath"), 0u);
            EXPECT_EQ(life->save.stats.count("captain"), 0u);
            EXPECT_EQ(life->save.seen.size(), 6u);
            ASSERT_EQ(grid->tiles.size(), 6u);
            EXPECT_NE(grid->tile("smithy"), nullptr);
            EXPECT_NE(grid->tile("yard"), nullptr);
            EXPECT_NE(grid->tile("gate"), nullptr);
            EXPECT_EQ(works(), 6u);
            EXPECT_EQ(found(), 0u);
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

            f.ecs.sendEvent(ActivityActivatedEvent{"life.activities", "glean"});
            f.settle();
            life->onMonth();
            f.frames(12);
            f.pass(5200.0f);

            ASSERT_TRUE(life->step.active);
            ASSERT_EQ(life->step.id, "guide.flour");

            auto list = life->piece<ActivityList>("activities");
            ASSERT_NE(list, nullptr);
            ASSERT_NE(list->find(&f.ecs, "flour"), nullptr);

            const _unique_id before = life->handOn;

            ASSERT_EQ(before, list->find(&f.ecs, "flour")->root.id);
            ASSERT_EQ(life->widthStep, 0);

            // Narrower: the three columns at their second step, a page built again
            f.resize(1000.0f, 1020.0f);
            f.frames(12);

            EXPECT_FALSE(life->compact);
            ASSERT_EQ(life->widthStep, 1);

            list = life->piece<ActivityList>("activities");
            ASSERT_NE(list, nullptr);

            ActivityRow* flour = list->find(&f.ecs, "flour");
            ASSERT_NE(flour, nullptr);

            EXPECT_NE(flour->root.id, before);
            EXPECT_EQ(life->handOn, flour->root.id);
            EXPECT_TRUE(f.pos(life->hand)->visible);
            EXPECT_LE(f.pos(life->hand)->x + f.pos(life->hand)->width, f.pos(flour->root)->x + 0.5f);
            EXPECT_TRUE(life->veiled);

            EXPECT_TRUE(life->step.active);
            EXPECT_EQ(life->step.id, "guide.flour");
            EXPECT_EQ(life->noteWords.spec.text, life->step.say);
            expectNoteBeside(f, life, flour->root);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // The walks outside the town (Scenes/lifewalk.cpp): a row of the Adventure group begun
        // opens its screen over the page, and what he did there is kept as a record.
        namespace
        {
            // A later life: the Hedgerows are open to it from its first day. Its dice are its own
            LifeScene* walker(LifeFixture& f, int seed = 40)
            {
                LifeSceneOptions opt = LifeFixture::mockup();
                opt.fresh = true;
                opt.lives = 2;
                opt.age = 7.0f;
                opt.walkSeed = seed;

                LifeScene* life = f.life(opt);

                if (not life)
                    return nullptr;

                return life;
            }

            // An event that comes up on the way is answered with the first option he can take
            void answerEvents(LifeFixture& f, LifeScene* life)
            {
                for (int guard = 0; guard < 4 and life->onWalk() and life->walk.site.count("screen") > 0 and life->walk.site.at("screen").toString() == "event"; ++guard)
                {
                    for (size_t i = 0; i < life->walk.options.size(); ++i)
                    {
                        auto locked = life->walk.options[i].find("locked");

                        if (locked == life->walk.options[i].end() or locked->second.get<int>() == 0)
                        {
                            f.ecs.sendEvent(ButtonActivatedEvent{0, "walk.option." + std::to_string(i)});
                            break;
                        }
                    }

                    f.settle();
                }
            }

            EntityRef walkPart(LifeScene* life, const std::string& name)
            {
                if (life->walkPage.empty() or not life->walkPage->has<Prefab>())
                    return EntityRef{};

                return life->walkPage->get<Prefab>()->findEntity(name);
            }
        }

        TEST(lifescene_test, a_walk_opens_its_screen_and_keeps_a_record)
        {
            MockLogger logger;
            LifeFixture f;

            LifeScene* life = walker(f);
            ASSERT_NE(life, nullptr);

            ASSERT_TRUE(life->save.running.empty());

            // Its tile says it takes him out of the town, with nothing to do before it; the
            // Adventure page has no tab while he has never been on a walk
            auto first = life->piece<ActivityList>("activities")->find(&f.ecs, "walk.hedgerows");

            ASSERT_NE(first, nullptr);
            EXPECT_TRUE(first->spec.adventure);
            EXPECT_EQ(first->spec.state, ActivityState::Idle);
            EXPECT_EQ(first->ground->get<ThemeComponent>()->element, "activity.kind.adventure");
            ASSERT_FALSE(first->frame.empty());
            EXPECT_EQ(first->frame->get<ThemeComponent>()->element, "activity.adventure.frame");
            EXPECT_EQ(first->mark.spec.name, "adventure");
            EXPECT_EQ(life->piece<ActivityList>("activities")->find(&f.ecs, "hedges"), nullptr);
            EXPECT_FALSE(life->piece<ActivityList>("activities")->find(&f.ecs, "carters")->spec.adventure);
            EXPECT_FALSE(life->piece<Tabs>("tabs")->tabs[2].shown);

            // Its group stands right under his work, where the market's tile was, and the next
            // walk has no tile until this one is walked
            {
                const auto& groups = life->piece<ActivityList>("activities")->spec.groups;

                // (over them, the market a boy of seven has not been to)
                ASSERT_GE(groups.size(), 4u);
                EXPECT_EQ(groups[0].label, "The sixth year");
                EXPECT_EQ(groups[1].label, "Work");
                EXPECT_EQ(groups[2].label, "Scavenge");
                EXPECT_EQ(groups[3].label, "Adventure");

                for (const auto& group : groups)
                    EXPECT_NE(group.label, "Market");
            }

            EXPECT_EQ(life->piece<ActivityList>("activities")->find(&f.ecs, "walk.millpond"), nullptr);

            // Begun as any work is: he is there, and no month runs while the days are chosen
            f.ecs.sendEvent(ActivityActivatedEvent{"life.activities", "walk.hedgerows"});
            f.frames(12);

            ASSERT_TRUE(life->onWalk());
            ASSERT_FALSE(life->walkPage.empty());
            EXPECT_EQ(life->walkScreen, "site");
            EXPECT_TRUE(life->save.running.empty());
            EXPECT_TRUE(life->paused);

            const float age = life->save.age;

            life->onMonth();   // The dev key's month is another matter: the tick's months do not run
            f.pass(5000.0f);
            EXPECT_NEAR(life->save.age, age + 1.0f / 12.0f, 0.0001f);

            // The screen says what the rules say
            EntityRef title = walkPart(life, "zoneTitle");
            ASSERT_FALSE(title.empty());
            EXPECT_EQ(title->get<Label>()->spec.text, "The Hedgerows");
            EXPECT_EQ(walkPart(life, "daysLeft")->get<Label>()->spec.text, "7");

            // No beast lives there: the panel that seeks them is out of the column
            EXPECT_FALSE(f.pos(walkPart(life, "seekPanel"))->visible);

            auto works = walkPart(life, "works")->get<ActivityList>();
            ASSERT_NE(works->find(&f.ecs, "gather.berries"), nullptr);

            // A work is chosen in its list, then done by its button
            f.ecs.sendEvent(ActivitySelectedEvent{"walk.works", "gather.berries"});
            f.settle();
            EXPECT_EQ(life->workChosen, "gather.berries");
            EXPECT_EQ(walkPart(life, "work")->get<Button>()->label.spec.text, "Gather berries");

            f.ecs.sendEvent(ButtonActivatedEvent{0, "walk.work"});
            f.settle();
            answerEvents(f, life);

            ASSERT_FALSE(life->save.walk.record.empty());
            EXPECT_GE(life->save.stats["berries"], 1);   // What he picked: one at the least
            EXPECT_GE(walkPart(life, "record")->get<EventLog>()->size(), 1u);
            EXPECT_LE(std::atoi(walkPart(life, "daysLeft")->get<Label>()->spec.text.c_str()), 6);

            // A second click on a row does it too
            f.ecs.sendEvent(ActivityActivatedEvent{"walk.works", "gather.firewood"});
            f.settle();
            answerEvents(f, life);

            // What is for the town is his to hold, in his inventory like the rest
            EXPECT_GE(life->save.stats["firewood"], 2);

            auto ledger = life->piece<ResourceLedger>("ledger");
            ASSERT_NE(ledger, nullptr);
            EXPECT_NE(ledger->row("firewood"), nullptr);
            EXPECT_EQ(life->save.knowledge["visit.hedgerows"], 1);

            // Home is asked twice
            f.ecs.sendEvent(ButtonActivatedEvent{0, "walk.leave"});
            f.settle();
            ASSERT_TRUE(life->onWalk());
            EXPECT_NE(walkPart(life, "leave")->get<Button>()->label.spec.text.find("lost"), std::string::npos);

            f.ecs.sendEvent(ButtonActivatedEvent{0, "walk.leave"});
            f.frames(12);

            // The record is kept, the screen is gone, and the walk's month runs as any work's
            EXPECT_FALSE(life->onWalk());
            EXPECT_TRUE(life->walkPage.empty());
            ASSERT_EQ(life->save.recipes.size(), 1u);
            EXPECT_EQ(life->save.recipes[0].fields.at("zone").toString(), "hedgerows");
            EXPECT_GE(life->save.recipes[0].steps.size(), 3u);
            EXPECT_EQ(life->save.running, "walk.hedgerows");
            EXPECT_FALSE(life->paused);
            EXPECT_EQ(life->save.stats["walked.hedgerows"], 1);
            EXPECT_EQ(life->save.walksByHand, 1);
            EXPECT_EQ(life->save.walkedAt["hedgerows"], 1);
            EXPECT_EQ(life->save.carried, 0);
            EXPECT_EQ(life->save.fights, 0);

            life->onMonth();
            f.settle();

            EXPECT_TRUE(life->save.running.empty());
            EXPECT_EQ(life->save.done["walk.hedgerows"], 1);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // The Adventure page, once he has been on a walk: a tile a place he may walk to, the walks
        // he has made at the one chosen, and a button that sets out for it anew. The Life page
        // keeps no walk from then on. A place keeps its last few walks, not one.
        TEST(lifescene_test, the_adventure_page_has_the_places_and_the_walks_made_there)
        {
            MockLogger logger;
            LifeFixture f;

            LifeScene* life = walker(f);
            ASSERT_NE(life, nullptr);

            auto tabs = life->piece<Tabs>("tabs");
            auto list = life->piece<ActivityList>("activities");
            ASSERT_NE(tabs, nullptr);
            ASSERT_NE(list, nullptr);

            // Before his first walk its tile is on the Life page, and there is no Adventure page
            ASSERT_NE(list->find(&f.ecs, "walk.hedgerows"), nullptr);
            EXPECT_FALSE(tabs->tabs[2].shown);

            // A walk by hand, from wherever it is begun: a thing gathered, and home
            auto walkIt = [&]() {
                ASSERT_TRUE(life->onWalk());

                life->walkDo(WalkAct{"gather", "berries", 0});
                answerEvents(f, life);
                life->walkDo(WalkAct{"leave", "", 0});
                life->walkDo(WalkAct{"leave", "", 0});
                f.frames(12);

                ASSERT_FALSE(life->onWalk());

                life->onMonth();
                f.settle();
                ASSERT_TRUE(life->save.running.empty());
            };

            f.ecs.sendEvent(ActivityActivatedEvent{"life.activities", "walk.hedgerows"});
            f.frames(12);
            walkIt();

            ASSERT_EQ(life->save.recipes.size(), 1u);

            const std::string first = life->save.recipes[0].id;

            // The page is there, and the Life page has let go of the walks
            EXPECT_TRUE(tabs->tabs[2].shown);
            EXPECT_EQ(list->find(&f.ecs, "walk.hedgerows"), nullptr);

            // Its tab has come with a count of what stands behind it: the place he walked (Mill
            // Pond has no tile before he is eight)
            EXPECT_EQ(life->newInAdventure, 1);
            EXPECT_EQ(tabs->tabs[2].notice, 1);
            ASSERT_FALSE(tabs->tabs[2].noticeGround.empty());
            EXPECT_TRUE(f.pos(tabs->tabs[2].noticeGround)->visible);

            for (const auto& group : list->spec.groups)
                EXPECT_NE(group.label, "Adventure");

            f.ecs.sendEvent(TabSelectedEvent{tabs->root.id, "life.tabs", 2});
            f.frames(12);
            ASSERT_TRUE(life->adventureShown);

            // Looked at: the count is gone
            EXPECT_EQ(life->newInAdventure, 0);
            EXPECT_EQ(tabs->tabs[2].notice, 0);

            // (what is chosen in a list just filled is lit a moment later, once the list holds it)
            f.pass(100.0f, 50.0f);

            // Where to walk: a tile a place, as the town's places are shown, the one he walked lit:
            // its mark and its name, a seal for each level he knows it to, and a line with what kind
            // of walk it is and its cost in months and days
            auto zones = life->piece<PlaceGrid>("zones");
            auto records = life->piece<ActivityList>("records");
            ASSERT_NE(zones, nullptr);
            ASSERT_NE(records, nullptr);

            PlaceTile* hedgerows = zones->tile("walk.hedgerows");
            ASSERT_NE(hedgerows, nullptr);
            EXPECT_EQ(hedgerows->spec.name, "The Hedgerows");
            EXPECT_EQ(hedgerows->spec.line, "FORAGE \xC2\xB7 1 MO \xC2\xB7 7 AP");
            EXPECT_EQ(hedgerows->spec.level, 1);
            EXPECT_EQ(hedgerows->spec.of, 4);
            EXPECT_EQ(hedgerows->seals.size(), 4u);
            EXPECT_FALSE(hedgerows->mark.entity.empty());
            EXPECT_EQ(hedgerows->ground->get<ThemeComponent>()->element, "place.ground.selected");
            EXPECT_EQ(life->adventureZone, "hedgerows");
            EXPECT_EQ(zones->selected(), "walk.hedgerows");
            EXPECT_TRUE(f.pos(hedgerows->root)->visible);

            // Its gloss says, under the rest, what each level of knowing the place brings: the
            // first visit made it known 1 of 4, the rest is still to learn
            {
                const GlossSpec* gloss = f.ecs.getSystem<GlossRegistry>()->find(hedgerows->spec.glossKey);
                ASSERT_NE(gloss, nullptr);

                auto heading = std::find_if(gloss->rows.begin(), gloss->rows.end(), [](const GlossRow& r) { return r.heading and r.label == "ZONE KNOWLEDGE 1 OF 4"; });
                ASSERT_NE(heading, gloss->rows.end());
                ASSERT_EQ(gloss->rows.end() - heading, 4);
                EXPECT_EQ((heading + 1)->label, "Level 2 \xC2\xB7 3 studies");
                EXPECT_EQ((heading + 1)->value, "Exploring the nearby pond, a task of his life");
                EXPECT_EQ((heading + 1)->tone, "");
                EXPECT_EQ((heading + 3)->label, "Level 4 \xC2\xB7 10 studies");
                EXPECT_NE((heading + 3)->value.find("the way to the Hollow Woods"), std::string::npos);
            }

            // As wide as its panel leaves it, and a tile is the town's own
            EXPECT_NEAR(zones->spec.width, life->piece<Panel>("zonePanel")->innerWidth(), 0.5f);
            EXPECT_NEAR(f.pos(hedgerows->root)->height, placeTileHeight(&f.ecs), 0.5f);

            auto zonePanel = life->piece<Panel>("zonePanel");
            ASSERT_NE(zonePanel, nullptr);
            ASSERT_TRUE(zonePanel->aside.has_value());
            EXPECT_EQ(zonePanel->aside->spec.text, "1 OPEN");

            // The button that sets out, in the colour of time, named with the place and its months
            auto anew = life->piece<Button>("walkNew");
            ASSERT_NE(anew, nullptr);
            EXPECT_EQ(anew->spec.variant, ButtonVariant::Ochre);
            EXPECT_EQ(anew->label.spec.text, "Set out for the Hedgerows \xC2\xB7 1 mo");
            EXPECT_FALSE(anew->spec.disabled);

            // The line beside it has what the button leaves of the panel
            auto goRow = f.pos(life->named("zoneGo"));
            EXPECT_LE(f.pos(anew->root)->x + f.pos(anew->root)->width, goRow->x + zonePanel->innerWidth() + 0.5f);
            EXPECT_GT(f.pos(anew->root)->x, f.pos(life->named("zoneNote"))->x + f.pos(life->named("zoneNote"))->width - 0.5f);

            // The records of the place: a table of one line, chosen, with its own button
            auto recordsPanel = life->piece<Panel>("recordsPanel");
            ASSERT_NE(recordsPanel, nullptr);
            EXPECT_EQ(recordsPanel->title->spec.text, "Records of the Hedgerows");
            EXPECT_EQ(recordsPanel->aside->spec.text, "1 KEPT");
            EXPECT_TRUE(f.pos(life->named("records"))->visible);
            EXPECT_FALSE(f.pos(life->named("recordsNote"))->visible);

            // The table's head is inside it, over its lines: one thing more than the lines in its stack
            EXPECT_EQ(records->spec.headCost, "COST");
            EXPECT_EQ(records->body.get<VerticalLayout>()->entities.size(), 2u);

            ActivityRow* walked = records->find(&f.ecs, first);
            ASSERT_NE(walked, nullptr);
            EXPECT_EQ(walked->spec.name.rfind("Walked at ", 0), 0u);
            EXPECT_NE(walked->spec.name.find(" y "), std::string::npos);
            EXPECT_EQ(walked->cost.spec.text, "1 mo");
            EXPECT_EQ(walked->spec.each, "0 vit");
            EXPECT_NE(walked->spec.line.find("steps"), std::string::npos);
            EXPECT_NE(walked->spec.asideTop.find("berries"), std::string::npos);
            ASSERT_TRUE(walked->action.has_value());
            EXPECT_EQ(walked->action->label.spec.text, "Walk");
            EXPECT_EQ(life->save.queue.recipe, first);
            EXPECT_EQ(records->selected(), first);

            // Its cell and its button stand past its cost, inside the line
            EXPECT_GT(f.pos(walked->asideTop->entity)->x, f.pos(walked->cost.entity)->x + f.pos(walked->cost.entity)->width);
            EXPECT_GT(f.pos(walked->action->root)->x, f.pos(walked->asideTop->entity)->x + f.pos(walked->asideTop->entity)->width - 0.5f);
            EXPECT_LE(f.pos(walked->action->root)->x + f.pos(walked->action->root)->width, f.pos(walked->root)->x + f.pos(walked->root)->width);

            // Walk it again: the form of that walk, its four figures, its two buttons at the right
            EXPECT_TRUE(f.pos(life->named("queuePanel"))->visible);
            EXPECT_FALSE(f.pos(life->named("workPanel"))->visible);
            EXPECT_EQ(life->piece<Label>("queueTimes")->spec.text, "1");
            EXPECT_EQ(life->piece<Label>("takeFigure")->spec.text, "1 mo");
            EXPECT_EQ(life->piece<Label>("useFigure")->spec.text, "nothing");
            EXPECT_EQ(life->piece<Label>("bringFigure")->spec.text.rfind("+", 0), 0u);
            EXPECT_NE(life->piece<Label>("bringNote")->spec.text.find("berries"), std::string::npos);
            EXPECT_EQ(life->piece<Button>("walkMany")->spec.variant, ButtonVariant::Ochre);
            EXPECT_EQ(life->piece<Button>("walkMany")->label.spec.text, "Queue 1 \xC2\xB7 1 mo");
            EXPECT_EQ(life->piece<Button>("walkOnce")->label.spec.text, "Walk once \xC2\xB7 1 mo");
            EXPECT_FALSE(life->piece<Button>("walkOnce")->spec.disabled);
            EXPECT_TRUE(life->piece<Button>("queueFewer")->spec.disabled);
            EXPECT_NEAR(f.pos(life->piece<Button>("walkMany")->root)->x + f.pos(life->piece<Button>("walkMany")->root)->width,
                        f.pos(life->named("queueGo"))->x + life->piece<Panel>("queuePanel")->innerWidth(), 1.0f);

            // And the scribe has a line for the place, at the page's foot
            EXPECT_FALSE(life->piece<Label>("footLine")->spec.text.empty());

            // Set out anew: by hand, whatever he has kept of it
            f.ecs.sendEvent(ButtonActivatedEvent{0, "walk.new"});
            f.frames(12);
            walkIt();

            ASSERT_EQ(life->save.recipes.size(), 2u);

            const std::string second = life->save.recipes[1].id;

            EXPECT_NE(second, first);

            f.ecs.sendEvent(TabSelectedEvent{tabs->root.id, "life.tabs", 2});
            f.frames(12);
            f.pass(100.0f, 50.0f);

            // Two lines, the best first, and the first one chosen
            ASSERT_NE(records->find(&f.ecs, first), nullptr);
            ASSERT_NE(records->find(&f.ecs, second), nullptr);
            EXPECT_EQ(recordsPanel->aside->spec.text, "2 KEPT");
            ASSERT_FALSE(records->spec.groups.empty());
            ASSERT_EQ(records->spec.groups[0].rows.size(), 2u);

            const std::string best = records->spec.groups[0].rows[0].id;
            const std::string other = best == first ? second : first;

            EXPECT_EQ(life->save.queue.recipe, best);
            EXPECT_EQ(records->selected(), best);
            EXPECT_LT(f.pos(records->find(&f.ecs, best)->root)->y, f.pos(records->find(&f.ecs, other)->root)->y);

            // The other one can be chosen in its place: the form is filled for it
            f.ecs.sendEvent(ActivitySelectedEvent{"walk.records", other});
            f.settle();

            EXPECT_EQ(life->save.queue.recipe, other);

            // A line's own button walks that walk once, now: the form gives way to the walk at work
            f.ecs.sendEvent(ButtonActivatedEvent{0, "walk.run." + best});
            f.frames(12);

            EXPECT_EQ(life->save.running, "walk.hedgerows");
            EXPECT_EQ(life->save.walking, best);
            EXPECT_FALSE(f.pos(life->named("queuePanel"))->visible);
            EXPECT_TRUE(f.pos(life->named("workPanel"))->visible);
            EXPECT_NE(life->piece<Label>("atWalk")->spec.text.find("WALK 1 OF 1"), std::string::npos);
            EXPECT_TRUE(life->piece<Button>("walkNew")->spec.disabled);

            life->onMonth();
            f.settle();

            ASSERT_TRUE(life->save.running.empty());
            EXPECT_EQ(life->save.walksByRecord, 1);
            EXPECT_TRUE(f.pos(life->named("queuePanel"))->visible);
            EXPECT_FALSE(f.pos(life->named("workPanel"))->visible);

            // A place keeps five walks: a sixth takes the oldest one's place
            for (int i = 0; i < 4; ++i)
            {
                f.ecs.sendEvent(TabSelectedEvent{tabs->root.id, "life.tabs", 2});
                f.frames(6);
                f.ecs.sendEvent(ButtonActivatedEvent{0, "walk.new"});
                f.frames(12);
                walkIt();
            }

            EXPECT_EQ(life->save.recipes.size(), 5u);

            for (const auto& r : life->save.recipes)
                EXPECT_NE(r.id, first);

            // A place that opens to him while he is on another page is counted at the tab's corner,
            // and no longer once he has looked
            f.ecs.sendEvent(TabSelectedEvent{tabs->root.id, "life.tabs", 0});
            f.frames(12);

            EXPECT_EQ(tabs->tabs[2].notice, 0);

            // (Mill Pond asks the pond found, once for the world by a task of a life, and his tenth year)
            life->save.knowledge["found.millpond"] = 1;
            life->save.age = 10.0f;
            life->onMonth();
            f.settle();

            EXPECT_EQ(life->newInAdventure, 1);
            EXPECT_EQ(tabs->tabs[2].notice, 1);

            f.ecs.sendEvent(TabSelectedEvent{tabs->root.id, "life.tabs", 2});
            f.frames(12);

            EXPECT_EQ(tabs->tabs[2].notice, 0);
            ASSERT_NE(zones->tile("walk.millpond"), nullptr);
            // (its beasts: the rats and their nest)
            EXPECT_EQ(zones->tile("walk.millpond")->spec.line.rfind("FORAGE \xC2\xB7 2 BEASTS \xC2\xB7 ", 0), 0u);
            EXPECT_EQ(zones->tile("walk.millpond")->spec.level, 0);
            EXPECT_EQ(zonePanel->aside->spec.text.rfind("2 OPEN", 0), 0u);

            // A place whose tile comes onto the page is news as well, closed to him as it is: the
            // Hollow Woods and the Quarry Road show two years before he may go
            EXPECT_EQ(zones->tile("walk.hollow"), nullptr);

            f.ecs.sendEvent(TabSelectedEvent{tabs->root.id, "life.tabs", 0});
            f.frames(12);

            life->save.age = 12.0f - 1.0f / 12.0f;
            life->onMonth();
            f.settle();

            EXPECT_EQ(life->newInAdventure, 2);
            EXPECT_EQ(tabs->tabs[2].notice, 2);

            // Nothing more the months after: a tile is new once
            life->onMonth();
            f.settle();

            EXPECT_EQ(tabs->tabs[2].notice, 2);

            f.ecs.sendEvent(TabSelectedEvent{tabs->root.id, "life.tabs", 2});
            f.frames(12);

            EXPECT_EQ(tabs->tabs[2].notice, 0);
            ASSERT_NE(zones->tile("walk.hollow"), nullptr);
            ASSERT_NE(zones->tile("walk.quarry"), nullptr);
            EXPECT_EQ(zones->tile("walk.hollow")->spec.line.rfind("ASKS ", 0), 0u);
            EXPECT_EQ(zonePanel->aside->spec.text, "2 OPEN \xC2\xB7 2 CLOSED");

            // A place with no walk kept: one faint line, no table and no form
            f.ecs.sendEvent(PlaceSelectedEvent{"walk.millpond"});
            f.settle();

            EXPECT_EQ(life->adventureZone, "millpond");
            EXPECT_EQ(zones->selected(), "walk.millpond");
            EXPECT_EQ(life->piece<PlaceGrid>("places")->selected(), "");
            EXPECT_EQ(recordsPanel->title->spec.text, "Records of Mill Pond");
            EXPECT_TRUE(f.pos(life->named("recordsNote"))->visible);
            EXPECT_FALSE(f.pos(life->named("records"))->visible);
            EXPECT_FALSE(f.pos(life->named("queuePanel"))->visible);
            EXPECT_EQ(life->piece<Button>("walkNew")->label.spec.text, "Set out for Mill Pond \xC2\xB7 2 mo");

            // Back to the place he has walked: its records are on the page again, lines and all
            f.ecs.sendEvent(PlaceSelectedEvent{"walk.hedgerows"});
            f.settle();
            f.pass(100.0f, 50.0f);

            EXPECT_EQ(life->adventureZone, "hedgerows");
            EXPECT_TRUE(f.pos(life->named("records"))->visible);
            EXPECT_FALSE(f.pos(life->named("recordsNote"))->visible);
            EXPECT_TRUE(f.pos(life->named("queuePanel"))->visible);
            ASSERT_FALSE(records->spec.groups.empty());
            EXPECT_EQ(records->spec.groups[0].rows.size(), 5u);

            for (const auto& row : records->spec.groups[0].rows)
            {
                ActivityRow* line = records->find(&f.ecs, row.id);

                ASSERT_NE(line, nullptr) << row.id;
                EXPECT_TRUE(f.pos(line->root)->isRenderable()) << row.id;
                EXPECT_TRUE(f.pos(line->name.entity)->isRenderable()) << row.id;
                EXPECT_GT(f.pos(line->root)->y, f.pos(life->named("records"))->y + 20.0f) << row.id;
            }

            EXPECT_GT(f.pos(life->named("records"))->height, 40.0f);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // The Adventure page walks a record again, once or in a queue: no screen, the months of
        // each walk on the calendar, and what the record brought at each term.
        TEST(lifescene_test, a_queue_walks_a_record_again)
        {
            MockLogger logger;
            LifeFixture f;

            LifeScene* life = walker(f);
            ASSERT_NE(life, nullptr);

            f.ecs.sendEvent(ActivityActivatedEvent{"life.activities", "walk.hedgerows"});
            f.frames(12);
            ASSERT_TRUE(life->onWalk());

            life->walkDo(WalkAct{"gather", "berries", 0});
            answerEvents(f, life);
            life->walkDo(WalkAct{"gather", "firewood", 0});
            answerEvents(f, life);
            life->walkDo(WalkAct{"leave", "", 0});
            life->walkDo(WalkAct{"leave", "", 0});
            f.frames(12);

            ASSERT_FALSE(life->onWalk());
            ASSERT_EQ(life->save.recipes.size(), 1u);

            life->onMonth();
            f.settle();
            ASSERT_TRUE(life->save.running.empty());

            // Its tab is there now that he has been on a walk, and its page has his record
            auto tabs = life->piece<Tabs>("tabs");
            ASSERT_NE(tabs, nullptr);
            ASSERT_EQ(tabs->spec.items.size(), 4u);
            EXPECT_TRUE(tabs->tabs[2].shown);

            f.ecs.sendEvent(TabSelectedEvent{tabs->root.id, "life.tabs", 2});
            f.frames(12);

            EXPECT_TRUE(life->adventureShown);
            EXPECT_FALSE(life->townShown);
            EXPECT_TRUE(f.pos(life->named("adventure"))->visible);
            EXPECT_FALSE(f.pos(life->named("activities"))->visible);

            auto records = life->piece<ActivityList>("records");
            ASSERT_NE(records, nullptr);
            ASSERT_NE(records->find(&f.ecs, life->save.recipes[0].id), nullptr);
            EXPECT_EQ(life->save.queue.recipe, life->save.recipes[0].id);

            // Three walks, by the form's buttons
            f.ecs.sendEvent(ButtonActivatedEvent{0, "walk.more"});
            f.ecs.sendEvent(ButtonActivatedEvent{0, "walk.more"});
            f.settle();

            EXPECT_EQ(life->save.queue.count, 3);
            EXPECT_EQ(life->piece<Button>("walkMany")->label.spec.text, "Queue 3 \xC2\xB7 3 mo");

            const int berries = life->save.stats["berries"];
            const int firewood = life->save.stats["firewood"];
            const int gathered = berries;   // What the record picked: he held none before it

            f.ecs.sendEvent(ButtonActivatedEvent{0, "walk.many"});
            f.settle();

            // The first walk is on the calendar, with no screen
            EXPECT_FALSE(life->onWalk());
            EXPECT_TRUE(life->walkPage.empty());
            EXPECT_EQ(life->save.running, "walk.hedgerows");
            EXPECT_EQ(life->save.walking, life->save.recipes[0].id);
            EXPECT_EQ(life->save.queue.left, 2);
            EXPECT_FALSE(life->paused);

            // Its term brings what the record gathered, and the next walk sets out at once
            life->onMonth();
            f.settle();

            EXPECT_EQ(life->save.stats["berries"], berries + gathered);
            EXPECT_GT(life->save.stats["firewood"], firewood);
            EXPECT_EQ(life->save.walksByRecord, 1);
            EXPECT_EQ(life->save.running, "walk.hedgerows");
            EXPECT_EQ(life->save.queue.left, 1);

            // "Finish this walk, then stop": the queue ends with the walk under way
            f.ecs.sendEvent(ButtonActivatedEvent{0, "walk.last"});
            f.settle();
            EXPECT_TRUE(life->save.lastWalk);

            life->onMonth();
            f.settle();

            EXPECT_TRUE(life->save.running.empty());
            EXPECT_TRUE(life->save.walking.empty());
            EXPECT_EQ(life->save.queue.left, 0);
            EXPECT_EQ(life->save.walksByRecord, 2);
            EXPECT_EQ(life->save.replayedAt["hedgerows"], 2);
            EXPECT_EQ(life->save.queued, 3);
            EXPECT_NE(life->save.digest().find("\"queued\":3,"), std::string::npos);
            EXPECT_NE(life->save.digest().find("\"replay.hedgerows\":2}"), std::string::npos);
            EXPECT_EQ(life->save.stats["berries"], berries + 2 * gathered);
            EXPECT_EQ(life->save.stats["walked.hedgerows"], 3);

            // Back to the Life page: the choice has its place again
            f.ecs.sendEvent(TabSelectedEvent{tabs->root.id, "life.tabs", 0});
            f.frames(12);

            EXPECT_FALSE(life->adventureShown);
            EXPECT_TRUE(f.pos(life->named("activities"))->visible);
            EXPECT_FALSE(f.pos(life->named("adventure"))->visible);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // A fight takes the on-site screen's place and gives it back; a walk closed in its middle
        // opens where it stood.
        TEST(lifescene_test, a_fight_has_the_screen_and_a_walk_is_saved_where_it_stands)
        {
            MockLogger logger;
            LifeFixture f;

            const std::string path = "save/test_life_walk.sz";
            std::remove(path.c_str());

            LifeSceneOptions opt = LifeFixture::mockup();
            opt.fresh = true;
            opt.lives = 2;
            opt.age = 7.0f;
            opt.walkSeed = 11;
            opt.noSave = false;
            opt.savePath = path;

            LifeScene* life = f.life(opt);
            ASSERT_NE(life, nullptr);

            // A boy whose world has found the pond: Mill Pond is open to him at ten
            life->save.age = 10.0f;
            life->save.stats["walked.hedgerows"] = 1;
            life->save.knowledge["found.millpond"] = 1;
            life->save.stats["remedy"] = 1;
            life->onMonth();
            f.settle();

            f.ecs.sendEvent(ActivityActivatedEvent{"life.activities", "walk.millpond"});
            f.frames(12);

            ASSERT_TRUE(life->onWalk());
            EXPECT_TRUE(f.pos(walkPart(life, "seekPanel"))->visible);

            auto seeks = walkPart(life, "seeks")->get<ActivityList>();
            ASSERT_NE(seeks->find(&f.ecs, "rat"), nullptr);

            // Nothing chosen: the button that seeks waits
            EXPECT_TRUE(walkPart(life, "seek")->get<Button>()->spec.disabled);

            f.ecs.sendEvent(ActivitySelectedEvent{"walk.seeks", "rat"});
            f.settle();
            EXPECT_FALSE(walkPart(life, "seek")->get<Button>()->spec.disabled);

            f.ecs.sendEvent(ButtonActivatedEvent{0, "walk.seek"});
            f.frames(12);
            answerEvents(f, life);
            f.frames(12);

            // The fight has the screen
            ASSERT_EQ(life->walkScreen, "fight");

            // Counted for the analytics as he comes to it, not yet as ended
            EXPECT_EQ(life->save.fights, 1);
            EXPECT_EQ(life->save.fightsWon + life->save.fightsLost + life->save.fightsFled, 0);
            ASSERT_FALSE(walkPart(life, "moves").empty());
            EXPECT_TRUE(walkPart(life, "seeks").empty());
            EXPECT_EQ(walkPart(life, "turn")->get<Label>()->spec.text, "TURN 1");

            f.pass(100.0f, 50.0f);

            // The beasts are above and he is below: a panel a beast across the top, the size of
            // his own; under them he and his turn, side by side and level; the account at the right
            auto foeBox = f.pos(walkPart(life, "foePanel0"));
            auto heroBox = f.pos(walkPart(life, "heroPanel"));
            auto menuBox = f.pos(walkPart(life, "menuPanel"));

            EXPECT_LE(foeBox->y + foeBox->height, heroBox->y + 0.5f);
            EXPECT_NEAR(foeBox->x, heroBox->x, 0.5f);
            EXPECT_NEAR(foeBox->width, heroBox->width, 0.5f);
            EXPECT_NEAR(heroBox->y, menuBox->y, 0.5f);
            EXPECT_GT(menuBox->x, heroBox->x + heroBox->width - 0.5f);
            EXPECT_GT(f.pos(walkPart(life, "accountPanel"))->x, menuBox->x + menuBox->width - 0.5f);

            // One beast: one panel, and nothing to aim at
            EXPECT_EQ(walkPart(life, "foePanel0")->get<Panel>()->title->spec.text, "Water rat");
            EXPECT_EQ(walkPart(life, "foeLife0")->get<Label>()->spec.text, "UNHURT");
            EXPECT_TRUE(f.pos(walkPart(life, "foeBar0"))->visible);
            EXPECT_FALSE(f.pos(walkPart(life, "foePanel1"))->visible);
            EXPECT_FALSE(f.pos(walkPart(life, "aimNote"))->visible);

            // What it means to do is its move's name, and nothing else: the gloss of the move
            // says a line on it, and what it does to him only once he knows
            auto registry = f.ecs.getSystem<GlossRegistry>();
            ASSERT_NE(registry, nullptr);

            const std::string intent = walkPart(life, "foeIntent0")->get<Label>()->spec.text;
            EXPECT_TRUE(intent == "BITE" or intent == "SKITTER") << intent;

            const GlossSpec* beast = registry->find("walk/foe/0");
            ASSERT_NE(beast, nullptr);
            EXPECT_EQ(beast->title, "Water rat");
            EXPECT_FALSE(beast->text.empty());

            const GlossSpec* meant = registry->find("walk/foemove/0");
            ASSERT_NE(meant, nullptr);
            EXPECT_FALSE(meant->title.empty());
            EXPECT_FALSE(meant->text.empty());

            if (intent == "BITE")
            {
                // Never bitten by one, never watched one: what a bite takes is not said yet
                EXPECT_TRUE(meant->rows.empty());
                EXPECT_FALSE(meant->footnote.empty());
            }

            // He has learned nothing that costs breath: he is not told of it
            EXPECT_FALSE(f.pos(walkPart(life, "breath"))->visible);
            EXPECT_FALSE(f.pos(walkPart(life, "breathBar"))->visible);

            // A boy who has learned nothing has what anyone has, a plain button each: strike,
            // defend, a remedy, flee. What each would do is its gloss's to say
            ASSERT_EQ(life->moves().size(), 4u);
            EXPECT_EQ(life->moves()[0].first, "walk.move.strike");
            EXPECT_EQ(life->moves()[0].second.spec.label, "Strike");
            EXPECT_EQ(life->moves()[1].second.spec.label, "Defend");
            EXPECT_EQ(life->moves()[2].second.spec.label, "Take a remedy");
            EXPECT_EQ(life->moves()[3].second.spec.label, "Flee");

            const GlossSpec* strike = registry->find("walk/move/strike");
            ASSERT_NE(strike, nullptr);
            EXPECT_EQ(strike->title, "Strike");
            EXPECT_FALSE(strike->text.empty());
            EXPECT_FALSE(strike->rows.empty());

            // Not until it is over: the way back is not on the page
            EXPECT_FALSE(f.pos(walkPart(life, "act"))->visible);

            // A button pressed is the move done: a turn has passed
            f.ecs.sendEvent(ButtonActivatedEvent{0, "walk.move.guard"});
            f.settle();
            EXPECT_EQ(walkPart(life, "turn")->get<Label>()->spec.text, "TURN 2");
            EXPECT_GE(walkPart(life, "account")->get<EventLog>()->size(), 2u);

            // Closed here, the walk opens on its fight, at its second turn
            const size_t steps = life->save.walk.record.size();
            const size_t marks = life->save.walk.marks.size();

            ASSERT_TRUE(life->saveNow());

            f.load<EmptyScene>();

            // Opened from its save, not made anew
            opt.fresh = false;

            LifeScene* again = f.life(opt);
            ASSERT_NE(again, nullptr);
            f.frames(12);

            ASSERT_TRUE(again->onWalk());
            ASSERT_EQ(again->walkScreen, "fight");
            EXPECT_EQ(walkPart(again, "turn")->get<Label>()->spec.text, "TURN 2");
            EXPECT_EQ(again->save.walk.record.size(), steps);
            EXPECT_EQ(again->save.walk.marks.size(), marks);
            EXPECT_GE(again->save.walk.foes.size(), 1u);

            // Handed to his stance, the fight plays itself to its end
            f.ecs.sendEvent(ButtonActivatedEvent{0, "walk.hand"});
            f.settle();
            EXPECT_EQ(walkPart(again, "hand")->get<Button>()->label.spec.text, "Take back");

            for (int turn = 0; turn < 60 and again->walk.duel.at("over").toString().empty(); ++turn)
                f.pass(700.0f, 700.0f);

            ASSERT_FALSE(again->walk.duel.at("over").toString().empty());

            // One button is the way back to the pond, and the on-site screen has its place again
            f.ecs.sendEvent(ButtonActivatedEvent{0, "walk.act"});
            f.frames(12);
            answerEvents(f, again);
            f.frames(12);

            if (again->onWalk())
            {
                EXPECT_EQ(again->walkScreen, "site");
                EXPECT_FALSE(walkPart(again, "seeks").empty());
                EXPECT_GE(walkPart(again, "record")->get<EventLog>()->size(), 2u);
            }

            EXPECT_EQ(again->save.knowledge["fights.rat"], 1);

            // One fight, kept through the save, ended once and in the hands of his stance: the way
            // back and what came after it counted nothing more
            EXPECT_EQ(again->save.fights, 1);
            EXPECT_EQ(again->save.fightsWon + again->save.fightsLost + again->save.fightsFled, 1);
            EXPECT_EQ(again->save.fightsAuto, 1);

            std::remove(path.c_str());
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // The pond above the mill is found by a task of his life, once the Hedgerows are known 2 of
        // 4, and found for the world: Mill Pond opens, in this life and in every one after, and no
        // later life is offered the task again.
        TEST(lifescene_test, the_pond_is_found_once_for_the_world)
        {
            MockLogger logger;
            LifeFixture f;

            LifeSceneOptions opt = LifeFixture::mockup();
            opt.fresh = true;
            opt.lives = 2;
            opt.age = 7.0f;

            LifeScene* life = f.life(opt);
            ASSERT_NE(life, nullptr);

            auto list = life->piece<ActivityList>("activities");
            ASSERT_NE(list, nullptr);

            // Not before the Hedgerows are known 2 of 4
            life->save.age = 9.0f;
            life->onMonth();
            f.settle();

            EXPECT_EQ(list->find(&f.ecs, "pond"), nullptr);

            life->save.knowledge["study.hedgerows"] = 3;
            life->onMonth();
            f.settle();

            ASSERT_NE(list->find(&f.ecs, "pond"), nullptr);

            f.ecs.sendEvent(ActivityActivatedEvent{"life.activities", "pond"});
            f.settle();
            ASSERT_EQ(life->save.running, "pond");

            life->onMonth();
            f.settle();
            f.frames(12);

            ASSERT_TRUE(life->save.running.empty());

            // The world knows it, and he holds nothing of it
            EXPECT_EQ(life->save.knowledge["found.millpond"], 1);
            EXPECT_EQ(life->save.stats.count("know.found.millpond"), 0u);
            EXPECT_EQ(life->save.stats.count("found.millpond"), 0u);

            // A life after him: the pond is known, its walk asks only his tenth year, and the task is gone
            life->newLife();
            f.settle();

            EXPECT_EQ(life->save.knowledge["found.millpond"], 1);

            life->save.knowledge["study.hedgerows"] = 3;
            life->save.age = 10.0f;
            life->onMonth();
            f.settle();

            list = life->piece<ActivityList>("activities");
            ASSERT_NE(list, nullptr);
            EXPECT_EQ(list->find(&f.ecs, "pond"), nullptr);

            std::vector<RuleActivity> activities;
            ASSERT_TRUE(life->rules.activities(life->save.age, life->save.character(), activities));

            bool open = false;

            for (const auto& a : activities)
            {
                auto id = a.fields.find("id");

                if (id == a.fields.end() or id->second.toString() != "walk.millpond")
                    continue;

                auto locked = a.fields.find("locked");

                open = locked == a.fields.end()
                    or (locked->second.type == UnionType::BOOL ? not locked->second.get<bool>() : locked->second.get<int>() == 0);
            }

            EXPECT_TRUE(open);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // Where things move, a turn of a fight is told in its time: the rule of a blow fills, then
        // the blow is written in the account, and nothing is pressed meanwhile. The walk itself is
        // already where the turn left it.
        TEST(lifescene_test, a_turn_of_a_fight_is_told_in_its_time)
        {
            MockLogger logger;
            LifeFixture f;

            LifeSceneOptions opt = LifeFixture::mockup();
            opt.fresh = true;
            opt.lives = 2;
            opt.age = 7.0f;
            opt.walkSeed = 11;

            LifeScene* life = f.life(opt);
            ASSERT_NE(life, nullptr);

            life->save.age = 10.0f;
            life->save.stats["walked.hedgerows"] = 1;
            life->save.knowledge["found.millpond"] = 1;
            life->onMonth();
            f.settle();

            f.ecs.sendEvent(ActivityActivatedEvent{"life.activities", "walk.millpond"});
            f.frames(12);
            ASSERT_TRUE(life->onWalk());

            life->walkDo(WalkAct{"seek", "rat", 1});
            f.frames(12);
            answerEvents(f, life);
            f.frames(12);
            ASSERT_EQ(life->walkScreen, "fight");

            // As the game plays it
            Motion::setReduced(false);
            f.pass(400.0f, 50.0f);

            auto account = walkPart(life, "account")->get<EventLog>();
            const size_t before = account->size();
            const size_t written = life->save.walk.account.size();

            f.ecs.sendEvent(ButtonActivatedEvent{0, "walk.move.strike"});
            f.frames(4);

            // The turn is played, and not told yet: the account on the page has nothing new
            EXPECT_GT(life->save.walk.account.size(), written);
            EXPECT_TRUE(life->telling());
            EXPECT_EQ(account->size(), before);
            EXPECT_EQ(walkPart(life, "turn")->get<Label>()->spec.text, "TURN 1");

            // A press meanwhile is of a screen a turn late: it does nothing
            const size_t played = life->save.walk.account.size();

            f.ecs.sendEvent(ButtonActivatedEvent{0, "walk.move.strike"});
            f.frames(4);
            EXPECT_EQ(life->save.walk.account.size(), played);

            // Told: every line the turn wrote is on the page, and the screen is the next turn's
            f.pass(6000.0f, 50.0f);

            EXPECT_FALSE(life->telling());
            EXPECT_EQ(walkPart(life, "account")->get<EventLog>()->size(), played);

            if (life->walk.duel.at("over").toString().empty())
            {
                EXPECT_EQ(walkPart(life, "turn")->get<Label>()->spec.text, "TURN 2");
            }

            Motion::setReduced(true);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // The walk's screen holds in the window, in three columns and, narrower, in one.
        TEST(lifescene_test, the_walk_screen_holds_in_the_window)
        {
            MockLogger logger;
            LifeFixture f;

            LifeScene* life = walker(f);
            ASSERT_NE(life, nullptr);

            f.ecs.sendEvent(ActivityActivatedEvent{"life.activities", "walk.hedgerows"});
            f.frames(12);
            ASSERT_TRUE(life->onWalk());

            auto inside = [&](float width, float height) {
                for (const char* name : {"veil", "kicker", "zoneTitle", "daysLeft", "strip", "walkLeft", "statePanel", "workPanel", "recordPanel", "bagPanel", "homePanel"})
                {
                    EntityRef part = walkPart(life, name);
                    ASSERT_FALSE(part.empty()) << name;

                    auto pos = f.pos(part);

                    EXPECT_GE(pos->x, -0.5f) << name;
                    EXPECT_LE(pos->x + pos->width, width + 0.5f) << name << " at " << width;
                    EXPECT_GE(pos->y, -0.5f) << name;
                }

                auto veil = f.pos(walkPart(life, "veil"));

                EXPECT_NEAR(veil->width, width, 0.5f);
                EXPECT_NEAR(veil->height, height, 0.5f);
            };

            EXPECT_FALSE(life->walkNarrow);
            inside(1320.0f, 1020.0f);

            // Nothing waits for his answer and no beast lives there: the card and the seeking are
            // out of their column, and what he may do stands at its head. So does the record while
            // he has taken nothing
            f.pass(100.0f, 50.0f);

            EXPECT_FALSE(f.pos(walkPart(life, "cardPanel"))->visible);
            EXPECT_FALSE(f.pos(walkPart(life, "seekPanel"))->visible);
            EXPECT_NEAR(f.pos(walkPart(life, "workPanel"))->y, f.pos(walkPart(life, "statePanel"))->y, 0.5f);
            EXPECT_FALSE(f.pos(walkPart(life, "takenPanel"))->visible);
            EXPECT_NEAR(f.pos(walkPart(life, "recordPanel"))->y, f.pos(walkPart(life, "statePanel"))->y, 0.5f);

            // How he stands is said where it is seen, at the head of the left column
            EXPECT_EQ(walkPart(life, "vitality")->get<Label>()->spec.text, "VITALITY 40 OF 40");
            EXPECT_EQ(walkPart(life, "knowledge")->get<Label>()->spec.text, "ZONE KNOWLEDGE 1 OF 4");
            EXPECT_LT(f.pos(walkPart(life, "statePanel"))->y, f.pos(walkPart(life, "bagPanel"))->y);

            // His panel's gloss says what knowing the place means and what each level brings
            {
                const GlossSpec* known = f.ecs.getSystem<GlossRegistry>()->find("walk/knowledge");

                ASSERT_NE(known, nullptr);
                EXPECT_EQ(known->title, "Zone knowledge");
                ASSERT_EQ(known->rows.size(), 4u);
                EXPECT_EQ(known->rows[0].label, "ZONE KNOWLEDGE 1 OF 4");
                EXPECT_EQ(known->rows[1].value, "Exploring the nearby pond, a task of his life");
                EXPECT_EQ(known->footnote, "STUDY 0 OF 3 TO KNOWLEDGE 2");

                for (const char* part : {"knowledge", "knowledgeBar", "knowledgeNote"})
                    EXPECT_TRUE(walkPart(life, part)->has<TooltipComponent>()) << part;
            }
            EXPECT_LT(f.pos(walkPart(life, "bagPanel"))->y, f.pos(walkPart(life, "homePanel"))->y);

            // And what he may do is tiles, as the choice is on the Life page
            EXPECT_GE(walkPart(life, "works")->get<ActivityList>()->columns(), 2);

            // Three columns side by side, laid as the Life page is: what he may do stands between
            // what he carries and the record
            EXPECT_GT(f.pos(walkPart(life, "workPanel"))->x, f.pos(walkPart(life, "bagPanel"))->x + f.pos(walkPart(life, "bagPanel"))->width - 0.5f);
            EXPECT_GT(f.pos(walkPart(life, "recordPanel"))->x, f.pos(walkPart(life, "workPanel"))->x + f.pos(walkPart(life, "workPanel"))->width - 0.5f);

            // A smaller window: the same three, the middle one narrower. The screen is not built
            // again for it: what stands is given its new width
            f.pass(200.0f, 50.0f);

            const _unique_id screen = life->walkPage.id;
            const _unique_id berries = walkPart(life, "works")->get<ActivityList>()->find(&f.ecs, "gather.berries")->root.id;
            const float middleWas = f.pos(walkPart(life, "workPanel"))->width;

            f.resize(1100.0f, 800.0f);
            f.pass(200.0f, 50.0f);
            f.frames(12);

            EXPECT_FALSE(life->walkNarrow);
            EXPECT_EQ(life->walkPage.id, screen);
            EXPECT_NEAR(life->walkWidth, 1100.0f, 0.5f);
            inside(1100.0f, 800.0f);

            // The middle column gave up what the window lost, and what stands in it followed
            auto workBox = f.pos(walkPart(life, "workPanel"));

            EXPECT_NEAR(workBox->width, middleWas - 220.0f, 1.0f);
            EXPECT_NEAR(f.pos(walkPart(life, "seekPanel"))->width, workBox->width, 0.5f);
            EXPECT_NEAR(f.pos(walkPart(life, "workNote"))->width, workBox->width - 32.0f, 1.0f);
            EXPECT_LE(f.pos(walkPart(life, "recordPanel"))->x + f.pos(walkPart(life, "recordPanel"))->width, 1100.0f + 0.5f);
            EXPECT_GT(f.pos(walkPart(life, "recordPanel"))->x, workBox->x + workBox->width - 0.5f);

            // Its tiles are the ones that stood, inside the panel still
            auto tile = walkPart(life, "works")->get<ActivityList>()->find(&f.ecs, "gather.berries");

            ASSERT_NE(tile, nullptr);
            EXPECT_EQ(tile->root.id, berries);

            for (const auto& group : walkPart(life, "works")->get<ActivityList>()->spec.groups)
            {
                for (const auto& row : group.rows)
                {
                    auto made = walkPart(life, "works")->get<ActivityList>()->find(&f.ecs, row.id);

                    ASSERT_NE(made, nullptr) << row.id;
                    EXPECT_LE(f.pos(made->root)->x + f.pos(made->root)->width, workBox->x + workBox->width - 15.5f) << row.id;
                }
            }

            // A phone held upright: one column, every panel the window's width
            f.resize(420.0f, 800.0f);
            f.pass(200.0f, 50.0f);
            f.frames(12);

            ASSERT_TRUE(life->onWalk());
            EXPECT_TRUE(life->walkNarrow);
            EXPECT_NE(life->walkPage.id, screen);
            inside(420.0f, 800.0f);

            EXPECT_NEAR(f.pos(walkPart(life, "workPanel"))->x, f.pos(walkPart(life, "recordPanel"))->x, 0.5f);
            EXPECT_GT(f.pos(walkPart(life, "recordPanel"))->y, f.pos(walkPart(life, "workPanel"))->y);

            // A little wider, in one column still: the same screen, every panel the window's width,
            // and what stands in each as wide as it leaves
            const _unique_id column = life->walkPage.id;

            f.resize(480.0f, 800.0f);
            f.pass(200.0f, 50.0f);
            f.frames(12);

            EXPECT_TRUE(life->walkNarrow);
            EXPECT_EQ(life->walkPage.id, column);
            inside(480.0f, 800.0f);

            for (const char* name : {"statePanel", "workPanel", "bagPanel", "homePanel", "recordPanel"})
                EXPECT_NEAR(f.pos(walkPart(life, name))->width, 480.0f - 32.0f, 0.5f) << name;

            EXPECT_NEAR(f.pos(walkPart(life, "vitalityBar"))->width, 480.0f - 64.0f, 1.0f);
            EXPECT_NEAR(f.pos(walkPart(life, "bag"))->width, 480.0f - 64.0f, 1.0f);
            EXPECT_NEAR(f.pos(walkPart(life, "record"))->width, 480.0f - 64.0f, 1.0f);
            EXPECT_NEAR(f.pos(walkPart(life, "knowledgeNote"))->width, 480.0f - 64.0f, 1.0f);

            // And what he did before is still written there
            life->walkDo(WalkAct{"gather", "berries", 0});
            f.settle();
            EXPECT_GE(walkPart(life, "record")->get<EventLog>()->size(), 1u);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // The phone page at another width is the same page: its panels are given their width where
        // they stand, and what stands in each follows. Nothing is built again.
        TEST(lifescene_test, the_phone_page_is_resized_where_it_stands)
        {
            MockLogger logger;
            LifeFixture f;

            LifeSceneOptions opt = LifeFixture::mockup();
            opt.fresh = true;
            opt.lives = 2;
            opt.age = 7.0f;

            LifeScene* life = f.life(opt);
            ASSERT_NE(life, nullptr);

            f.resize(520.0f, 800.0f);
            f.frames(12);

            ASSERT_TRUE(life->phone);

            const _unique_id page = life->page.id;

            std::map<std::string, _unique_id> tiles;

            for (const auto& group : life->piece<ActivityList>("activities")->spec.groups)
            {
                for (const auto& row : group.rows)
                    tiles[row.id] = life->piece<ActivityList>("activities")->find(&f.ecs, row.id)->root.id;
            }

            ASSERT_FALSE(tiles.empty());

            const size_t ticks = life->piece<LifeClock>("clock")->ticks.size();

            f.resize(430.0f, 800.0f);
            f.frames(12);

            ASSERT_TRUE(life->phone);
            EXPECT_EQ(life->page.id, page);

            // Every panel is the column, and what stands in it as wide as it leaves
            const float column = 430.0f - 24.0f;

            for (const char* name : {"working", "may", "parts", "holds", "clockPanel", "happened"})
            {
                EXPECT_NEAR(f.pos(life->named(name))->width, column, 0.5f) << name;
                EXPECT_LE(f.pos(life->named(name))->x + f.pos(life->named(name))->width, 430.0f + 0.5f) << name;
            }

            const float inner = life->piece<Panel>("parts")->innerWidth();

            EXPECT_NEAR(inner, column - 32.0f, 0.5f);
            EXPECT_NEAR(f.pos(life->named("vit"))->width, inner, 0.5f);
            EXPECT_NEAR(f.pos(life->piece<StatLine>("vit")->groove.root)->width, inner, 0.5f);
            EXPECT_NEAR(f.pos(life->named("skills"))->width, inner, 0.5f);
            EXPECT_NEAR(f.pos(life->named("ledger"))->width, inner, 0.5f);
            EXPECT_NEAR(f.pos(life->named("log"))->width, inner, 0.5f);
            EXPECT_NEAR(f.pos(life->named("clock"))->width, inner, 0.5f);
            EXPECT_NEAR(f.pos(life->piece<LifeClock>("clock")->track)->width, inner, 0.5f);
            EXPECT_NEAR(f.pos(life->named("tabs"))->width, column, 0.5f);

            // The clock's ticks are along the narrower track, as many as there were, and none past it
            auto clock = life->piece<LifeClock>("clock");

            EXPECT_EQ(clock->ticks.size(), ticks);

            for (auto& tick : clock->ticks)
                EXPECT_LE(f.pos(tick.mark)->x, f.pos(clock->track)->x + inner + 0.5f);

            // The tiles of the choice are the ones that stood
            for (const auto& [id, was] : tiles)
            {
                auto tile = life->piece<ActivityList>("activities")->find(&f.ecs, id);

                ASSERT_NE(tile, nullptr) << id;
                EXPECT_EQ(tile->root.id, was) << id;
                EXPECT_LE(f.pos(tile->root)->x + f.pos(tile->root)->width, 430.0f - 12.0f + 0.5f) << id;
            }
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // A window dragged by its edge is a resize a pass, and a page built again at each: none of
        // them leaves anything behind. What stands once the dragging stops is what a page built
        // once at that size is made of, and nothing else.
        TEST(lifescene_test, a_window_dragged_leaves_nothing_of_the_pages_it_went_through)
        {
            MockLogger logger;

            auto boxes = [](LifeFixture& f) {
                size_t n = 0;
                const auto& entities = f.ecs.view();

                for (size_t i = 1; i <= f.ecs.getNbEntities(); ++i)
                {
                    if (entities[i] and entities[i]->has<PositionComponent>())
                        ++n;
                }

                return n;
            };

            auto open = [](LifeFixture& f, bool fight, bool walking) -> LifeScene* {
                LifeSceneOptions opt = LifeFixture::mockup();
                opt.fresh = true;
                opt.lives = 2;
                opt.age = 7.0f;
                opt.walkSeed = 11;

                LifeScene* life = f.life(opt);

                if (not life)
                    return nullptr;

                life->save.age = 10.0f;
                life->save.stats["walked.hedgerows"] = 1;
                life->save.knowledge["found.millpond"] = 1;
                life->onMonth();
                f.settle();

                if (not walking)
                    return life;

                f.ecs.sendEvent(ActivityActivatedEvent{"life.activities", "walk.millpond"});
                f.frames(12);

                if (fight)
                {
                    life->walkDo(WalkAct{"seek", "rat", 1});
                    f.frames(12);
                    answerEvents(f, life);
                    f.frames(12);
                }

                return life;
            };

            for (int which = 0; which < 3; ++which)
            {
                const bool walking = which > 0;
                const bool fight = which == 2;

                // Dragged: a pass between two sizes, down to a phone's width and back up a little
                LifeFixture dragged;
                LifeScene* a = open(dragged, fight, walking);
                ASSERT_NE(a, nullptr);

                // One size, and no pass given for it to settle
                auto drag = [&](float width) {
                    dragged.window->get<PositionComponent>()->setWidth(width);
                    dragged.window->get<PositionComponent>()->setHeight(800.0f);
                    dragged.ecs.sendEvent(ResizeEvent{width, 800.0f});
                };

                int step = 0;
                std::set<_unique_id> screens;

                for (float width = 1320.0f; width >= 400.0f; width -= 23.0f)
                {
                    drag(width);

                    // Now and then two sizes come in the same pass
                    if (++step % 3 != 0)
                        dragged.frames(1);

                    screens.insert(a->walkPage.id);
                }

                for (float width = 400.0f; width <= 470.0f; width += 7.0f)
                {
                    drag(width);
                    dragged.frames(1);

                    screens.insert(a->walkPage.id);
                }

                // The walk's screen is not built again for every width that goes by: for the
                // first, and then for where the dragging stops
                if (walking)
                    EXPECT_LE(screens.size(), 3u);

                dragged.resize(470.0f, 800.0f);
                dragged.pass(400.0f, 50.0f);
                dragged.frames(24);

                // Set down once at that size
                LifeFixture placed;
                LifeScene* b = open(placed, fight, walking);
                ASSERT_NE(b, nullptr);

                placed.resize(470.0f, 800.0f);
                placed.pass(400.0f, 50.0f);
                placed.frames(24);

                EXPECT_EQ(a->walkScreen, b->walkScreen) << which;
                EXPECT_EQ(a->walkNarrow, b->walkNarrow) << which;

                if (walking)
                    EXPECT_NEAR(a->walkWidth, 470.0f, 0.5f) << which;

                EXPECT_EQ(boxes(dragged), boxes(placed)) << (fight ? "the fight" : walking ? "the walk" : "the page");
            }
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // A field work's tile is its name and its days; what it is, costs and gives is its gloss's
        TEST(lifescene_test, a_field_work_is_a_name_a_cost_and_a_gloss)
        {
            MockLogger logger;
            LifeFixture f;

            LifeScene* life = walker(f);
            ASSERT_NE(life, nullptr);

            f.ecs.sendEvent(ActivityActivatedEvent{"life.activities", "walk.hedgerows"});
            f.frames(12);
            ASSERT_TRUE(life->onWalk());

            auto works = walkPart(life, "works")->get<ActivityList>();
            auto tile = works->find(&f.ecs, "gather.berries");

            ASSERT_NE(tile, nullptr);
            EXPECT_EQ(tile->spec.name, "Gather berries");
            EXPECT_EQ(tile->spec.cost, "1 AP");
            EXPECT_TRUE(tile->spec.until.empty());
            EXPECT_EQ(tile->spec.glossKey, "walk/work/gather.berries");

            auto registry = f.ecs.getSystem<GlossRegistry>();
            ASSERT_NE(registry, nullptr);

            const GlossSpec* gloss = registry->find("walk/work/gather.berries");

            ASSERT_NE(gloss, nullptr);
            EXPECT_EQ(gloss->title, "Gather berries");
            EXPECT_FALSE(gloss->text.empty());
            ASSERT_EQ(gloss->rows.size(), 2u);
            EXPECT_EQ(gloss->rows[0].label, "Costs");
            EXPECT_EQ(gloss->rows[0].value, "1 day of the stay");
            EXPECT_EQ(gloss->rows[1].label, "Gives");
            EXPECT_NE(gloss->rows[1].value.find("berries, to sell"), std::string::npos);

            // Every work there is has one (a first visit: berries, firewood and a study)
            for (const char* id : {"gather.firewood", "study"})
                EXPECT_NE(registry->find(std::string("walk/work/") + id), nullptr) << id;
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // A tab that is not shown leaves no gap: the Adventure page come before the town stands
        // next to Life, and once the town is known too the row reads Life, Town, Adventure.
        TEST(lifescene_test, a_tab_not_shown_leaves_no_gap)
        {
            MockLogger logger;
            LifeFixture f;

            LifeScene* life = f.life(LifeFixture::first());
            ASSERT_NE(life, nullptr);

            auto tabs = life->piece<Tabs>("tabs");
            ASSERT_NE(tabs, nullptr);
            ASSERT_EQ(tabs->spec.items.size(), 4u);

            // A first life: neither the town nor a walk
            EXPECT_TRUE(tabs->tabs[0].shown);
            EXPECT_FALSE(tabs->tabs[1].shown);
            EXPECT_FALSE(tabs->tabs[2].shown);

            auto face = [&](int i) { return f.pos(tabs->tabs[i].face); };
            const float next = face(0)->x + face(0)->width + 16.0f;

            // He has been on a walk, and does not know the town yet
            life->save.walksByHand = 1;
            life->markAdventure();
            f.settle();

            EXPECT_TRUE(tabs->tabs[2].shown);
            EXPECT_FALSE(tabs->tabs[1].shown);
            EXPECT_NEAR(face(2)->x, next, 0.5f);
            EXPECT_TRUE(face(2)->visible);
            EXPECT_FALSE(face(1)->visible);

            // The town is known: it takes its place, and the Adventure page stands after it
            tabs->setShown(&f.ecs, 1, true);
            f.settle();

            EXPECT_NEAR(face(1)->x, next, 0.5f);
            EXPECT_NEAR(face(2)->x, face(1)->x + face(1)->width + 16.0f, 0.5f);

            // And gone again, the row closes up
            tabs->setShown(&f.ecs, 1, false);
            f.settle();

            EXPECT_NEAR(face(2)->x, next, 0.5f);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // What is sold and bought at the market is on the Town page and nowhere else: while he
        // does not know the town it has no tile at all, whatever he holds to sell.
        TEST(lifescene_test, the_market_is_never_on_the_life_page)
        {
            MockLogger logger;
            LifeFixture f;

            LifeSceneOptions opt = LifeFixture::first();
            opt.lives = 2;
            opt.age = 7.0f;

            LifeScene* life = f.life(opt);
            ASSERT_NE(life, nullptr);

            // Berries from a walk, wortleaf to brew, and no town yet
            life->save.townKnown = false;
            life->save.stats.erase("town_known");
            life->save.stats["berries"] = 8;
            life->save.stats["wortleaf"] = 4;
            life->onMonth();
            f.settle();

            auto list = life->piece<ActivityList>("activities");
            ASSERT_NE(list, nullptr);

            // (berries are dried at home: that is among what he gathers, not the market's)
            bool drying = false;

            for (const auto& group : list->spec.groups)
            {
                for (const auto& row : group.rows)
                {
                    EXPECT_NE(row.id.rfind("sell.", 0), 0u) << row.id;
                    EXPECT_NE(row.id.rfind("buy.", 0), 0u) << row.id;
                    EXPECT_NE(row.id, "brew.remedy");

                    if (row.id == "dry.berries")
                    {
                        drying = true;
                        EXPECT_EQ(group.label, "Scavenge");
                    }
                }
            }

            EXPECT_TRUE(drying);
            EXPECT_EQ(stall(f, life, "sell.berries"), nullptr);

            // The town known: they are at its market, and still not on the Life page
            life->save.townKnown = true;
            life->save.stats["town_known"] = 1;
            life->onMonth();
            f.settle();

            EXPECT_NE(stall(f, life, "sell.berries"), nullptr);
            EXPECT_NE(stall(f, life, "brew.remedy"), nullptr);
            EXPECT_EQ(stall(f, life, "dry.berries"), nullptr);

            for (const auto& group : list->spec.groups)
            {
                for (const auto& row : group.rows)
                    EXPECT_NE(row.id.rfind("sell.", 0), 0u) << row.id;
            }

            // What a task asks is said in its gloss, but not the room a store still has
            auto registry = f.ecs.getSystem<GlossRegistry>();
            ASSERT_NE(registry, nullptr);

            const GlossSpec* kitchen = registry->find("activity/kitchen");
            ASSERT_NE(kitchen, nullptr);

            for (const auto& row : kitchen->rows)
                EXPECT_EQ(row.label.find("Room for"), std::string::npos) << row.label;
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // A thing sold at the market: a click chooses it and its button sells, and it stays chosen
        // for the next press; Sell all sells what is left in one press and one line of the log. Then
        // its button stays where it was, greyed, and no Pass a month comes under the mouse until
        // a press somewhere else. While at work it is sold the same, the work running on
        TEST(lifescene_test, a_thing_sold_is_sold_by_its_button_all_of_it_by_all)
        {
            MockLogger logger;
            LifeFixture f;

            LifeSceneOptions opt = LifeFixture::first();
            opt.lives = 2;
            opt.age = 7.0f;

            LifeScene* life = f.life(opt);
            ASSERT_NE(life, nullptr);

            life->save.townKnown = true;
            life->save.stats["town_known"] = 1;
            life->save.stats["berries"] = 15;
            life->save.stats["coin"] = 0;
            life->onMonth();
            f.settle();

            ASSERT_NE(stall(f, life, "sell.berries"), nullptr);
            ASSERT_TRUE(life->save.running.empty());

            auto begin = life->piece<Button>("begin");
            auto all = life->piece<Button>("all");
            ASSERT_NE(begin, nullptr);
            ASSERT_NE(all, nullptr);

            const int coinAtFirst = life->save.stats["coin"];

            f.ecs.sendEvent(ActivitySelectedEvent{"town.market", "sell.berries"});
            f.settle();

            EXPECT_EQ(life->save.stats["berries"], 15);
            EXPECT_TRUE(f.pos(life->named("begin"))->visible);
            EXPECT_EQ(begin->spec.label, "Sell");
            EXPECT_TRUE(f.pos(life->named("all"))->visible);
            EXPECT_FALSE(f.pos(life->named("skip"))->visible);

            // One lot, and chosen still
            f.ecs.sendEvent(ButtonActivatedEvent{begin->face.id, "life.begin"});
            f.settle();

            EXPECT_EQ(life->save.stats["berries"], 10);
            EXPECT_EQ(life->save.stats["coin"], coinAtFirst + 1);
            EXPECT_TRUE(f.pos(life->named("begin"))->visible);
            EXPECT_EQ(begin->spec.label, "Sell");
            EXPECT_FALSE(begin->spec.disabled);

            // The rest at once, said once
            const size_t lines = life->save.log.size();

            f.ecs.sendEvent(ButtonActivatedEvent{all->face.id, "life.all"});
            f.settle();

            EXPECT_EQ(life->save.stats["berries"], 0);
            EXPECT_EQ(life->save.stats["coin"], coinAtFirst + 3);

            int sold = 0;

            for (size_t i = lines; i < life->save.log.size(); ++i)
            {
                const LogEntry& line = life->save.log[i];

                if (line.text.find("Sell berries") == std::string::npos)
                    continue;

                ++sold;
                EXPECT_NE(line.text.find("\xC3\x97" "2"), std::string::npos) << line.text;
                EXPECT_NE(line.figure.find("+2"), std::string::npos) << line.figure;
                EXPECT_NE(line.figure.find("10"), std::string::npos) << line.figure;
            }

            EXPECT_EQ(sold, 1);

            // None left: the button where it was, greyed, and nothing in its place
            EXPECT_TRUE(f.pos(life->named("begin"))->visible);
            EXPECT_TRUE(begin->spec.disabled);
            EXPECT_EQ(begin->spec.label, "None left");
            EXPECT_FALSE(f.pos(life->named("skip"))->visible);
            EXPECT_FALSE(f.pos(life->named("all"))->visible);

            // A press on it does nothing; a press elsewhere lets it go
            const auto face = f.pos(begin->face);
            f.ecs.sendEvent(OnMouseClick{Point2D{face->x + 2.0f, face->y + 2.0f}, static_cast<MouseButton>(1)});
            f.settle();

            EXPECT_TRUE(f.pos(life->named("begin"))->visible);
            EXPECT_FALSE(f.pos(life->named("skip"))->visible);

            f.ecs.sendEvent(OnMouseClick{Point2D{1.0f, 1.0f}, static_cast<MouseButton>(1)});
            f.settle();

            EXPECT_TRUE(f.pos(life->named("skip"))->visible);
            EXPECT_FALSE(f.pos(life->named("begin"))->visible);

            // At work: a work of months begun, then berries sold under it, the work going on
            auto list = life->piece<ActivityList>("activities");
            ASSERT_NE(list, nullptr);

            std::string work;

            for (const auto& group : list->spec.groups)
            {
                for (const auto& row : group.rows)
                {
                    if (work.empty() and row.months >= 3 and row.state == ActivityState::Idle)
                        work = row.id;
                }
            }

            ASSERT_FALSE(work.empty());

            f.ecs.sendEvent(ActivitySelectedEvent{"life.activities", work});
            f.settle();
            f.ecs.sendEvent(ButtonActivatedEvent{begin->face.id, "life.begin"});
            f.settle();

            ASSERT_EQ(life->save.running, work);
            EXPECT_FALSE(f.pos(life->named("begin"))->visible);

            life->save.stats["berries"] = 5;
            life->onMonth();
            f.settle();

            ASSERT_EQ(life->save.running, work);

            f.ecs.sendEvent(ActivitySelectedEvent{"town.market", "sell.berries"});
            f.settle();

            EXPECT_TRUE(f.pos(life->named("begin"))->visible);
            EXPECT_EQ(begin->spec.label, "Sell");

            const int coin = life->save.stats["coin"];

            f.ecs.sendEvent(ButtonActivatedEvent{begin->face.id, "life.begin"});
            f.settle();

            EXPECT_EQ(life->save.stats["berries"], 0);
            EXPECT_EQ(life->save.stats["coin"], coin + 1);
            EXPECT_EQ(life->save.running, work);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // The market's rows on the Town page stand on two lines of their own, by the rules' `trade`:
        // "Buy", what he pays coin for, then "Sell", what he is paid for. What is made there (a
        // remedy) comes after them under "Make", and the other places after the market
        TEST(lifescene_test, the_market_buys_and_sells_on_lines_of_their_own)
        {
            MockLogger logger;
            LifeFixture f;

            LifeSceneOptions opt = LifeFixture::first();
            opt.lives = 2;
            opt.age = 7.0f;

            LifeScene* life = f.life(opt);
            ASSERT_NE(life, nullptr);

            life->save.townKnown = true;
            life->save.stats["town_known"] = 1;
            life->save.stats["berries"] = 15;
            life->save.stats["wortleaf"] = 4;
            life->save.stats["water"] = 2;
            life->save.stats["coin"] = 20;
            life->onMonth();
            f.settle();

            auto market = life->piece<ActivityList>("marketRows");
            ASSERT_NE(market, nullptr);

            auto groupOf = [&market](const std::string& id) {
                for (const auto& group : market->spec.groups)
                {
                    for (const auto& row : group.rows)
                    {
                        if (row.id == id)
                            return group.label;
                    }
                }

                return std::string("(none)");
            };

            EXPECT_EQ(groupOf("buy.rations"), "Buy");
            EXPECT_EQ(groupOf("buy.water"), "Buy");
            EXPECT_EQ(groupOf("sell.berries"), "Sell");
            EXPECT_EQ(groupOf("sell.water"), "Sell");
            EXPECT_EQ(groupOf("brew.remedy"), "Make");

            // Buy first, then Sell, then the rest of the market: every row under a heading
            std::vector<std::string> labels;

            for (const auto& group : market->spec.groups)
            {
                EXPECT_FALSE(group.label.empty());
                labels.push_back(group.label);
            }

            ASSERT_GE(labels.size(), 3u);
            EXPECT_EQ(labels[0], "Buy");
            EXPECT_EQ(labels[1], "Sell");
            EXPECT_EQ(labels[2], "Make");

            // A month later the same lines hold the same rows: the list reconciles them in place
            life->onMonth();
            f.settle();

            EXPECT_EQ(groupOf("buy.rations"), "Buy");
            EXPECT_EQ(groupOf("sell.berries"), "Sell");
            ASSERT_FALSE(market->spec.groups.empty());
            EXPECT_EQ(market->spec.groups.front().label, "Buy");
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // The button names the act: a sale chosen has Sell and Sell all, a buy Buy and Buy all,
        // and a making Make and Make all (a remedy brewed, berries dried)
        TEST(lifescene_test, the_button_says_buy_or_sell)
        {
            MockLogger logger;
            LifeFixture f;

            LifeSceneOptions opt = LifeFixture::first();
            opt.lives = 2;
            opt.age = 7.0f;

            LifeScene* life = f.life(opt);
            ASSERT_NE(life, nullptr);

            life->save.townKnown = true;
            life->save.stats["town_known"] = 1;
            life->save.stats["berries"] = 15;
            life->save.stats["coin"] = 20;
            life->onMonth();
            f.settle();

            auto begin = life->piece<Button>("begin");
            auto all = life->piece<Button>("all");
            ASSERT_NE(begin, nullptr);
            ASSERT_NE(all, nullptr);

            f.ecs.sendEvent(ActivitySelectedEvent{"town.market", "sell.berries"});
            f.settle();

            EXPECT_EQ(begin->spec.label, "Sell");
            EXPECT_TRUE(f.pos(life->named("all"))->visible);
            EXPECT_EQ(all->spec.label, "Sell all");

            f.ecs.sendEvent(ActivitySelectedEvent{"town.market", "buy.rations"});
            f.settle();

            EXPECT_EQ(begin->spec.label, "Buy");
            EXPECT_TRUE(f.pos(life->named("all"))->visible);
            EXPECT_FALSE(all->spec.disabled);
            EXPECT_EQ(all->spec.label, "Buy all");

            // The button sizes itself to the longer word
            EXPECT_GT(f.pos(all->face)->width, 0.0f);

            f.ecs.sendEvent(ActivitySelectedEvent{"town.market", "brew.remedy"});
            f.settle();

            EXPECT_EQ(begin->spec.label, "Make");
            EXPECT_TRUE(f.pos(life->named("all"))->visible);
            EXPECT_EQ(all->spec.label, "Make all");

            f.ecs.sendEvent(ActivitySelectedEvent{"life.activities", "dry.berries"});
            f.settle();

            EXPECT_EQ(begin->spec.label, "Make");
            EXPECT_TRUE(f.pos(life->named("all"))->visible);
            EXPECT_EQ(all->spec.label, "Make all");
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // Buy all buys as many lots as his coin and his room allow, and says so in one line of the
        // log: 23 coin at 5 a lot buys 4 lots and leaves 3; with room for one lot it buys one
        TEST(lifescene_test, buy_all_buys_what_his_coin_and_room_allow)
        {
            MockLogger logger;
            LifeFixture f;

            LifeSceneOptions opt = LifeFixture::first();
            opt.lives = 2;
            opt.age = 7.0f;

            LifeScene* life = f.life(opt);
            ASSERT_NE(life, nullptr);

            life->save.townKnown = true;
            life->save.stats["town_known"] = 1;
            life->save.stats["rations"] = 0;
            life->save.stats["coin"] = 23;
            life->onMonth();
            f.settle();

            life->save.stats["rations"] = 0;
            life->save.stats["coin"] = 23;

            ActivityRow* rations = stall(f, life, "buy.rations");
            ASSERT_NE(rations, nullptr);
            ASSERT_EQ(rations->spec.gains.size(), 2u);    // The rations, and the coin it costs
            const int lot = rations->spec.gains[0].amount;
            ASSERT_GT(lot, 0);

            auto all = life->piece<Button>("all");
            ASSERT_NE(all, nullptr);

            f.ecs.sendEvent(ActivitySelectedEvent{"town.market", "buy.rations"});
            f.settle();

            const size_t lines = life->save.log.size();

            f.ecs.sendEvent(ButtonActivatedEvent{all->face.id, "life.all"});
            f.settle();

            EXPECT_EQ(life->save.stats["coin"], 3);
            EXPECT_EQ(life->save.stats["rations"], 4 * lot);

            int bought = 0;

            for (size_t i = lines; i < life->save.log.size(); ++i)
            {
                const LogEntry& line = life->save.log[i];

                if (line.text.find("Buy rations") == std::string::npos)
                    continue;

                ++bought;
                EXPECT_NE(line.text.find("\xC3\x97" "4"), std::string::npos) << line.text;
            }

            EXPECT_EQ(bought, 1);

            // Coin left for less than a lot: the button greyed, as a sale with none left
            auto begin = life->piece<Button>("begin");
            ASSERT_NE(begin, nullptr);
            EXPECT_TRUE(begin->spec.disabled);
            EXPECT_FALSE(f.pos(life->named("all"))->visible);

            // Room for one lot only: one is bought, coin or not
            f.ecs.sendEvent(OnMouseClick{Point2D{1.0f, 1.0f}, static_cast<MouseButton>(1)});
            f.settle();

            life->save.stats["rations"] = 60 - lot;
            life->save.stats["coin"] = 23;
            life->onMonth();
            f.settle();

            life->save.stats["rations"] = 60 - lot;
            life->save.stats["coin"] = 23;

            f.ecs.sendEvent(ActivitySelectedEvent{"town.market", "buy.rations"});
            f.settle();
            f.ecs.sendEvent(ButtonActivatedEvent{all->face.id, "life.all"});
            f.settle();

            EXPECT_EQ(life->save.stats["coin"], 18);
            EXPECT_EQ(life->save.stats["rations"], 60);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // Make all makes as many as his stock allows, in one line of the log: wortleaf for three
        // remedies (two each, and a water each) makes three
        TEST(lifescene_test, make_all_makes_what_his_stock_allows)
        {
            MockLogger logger;
            LifeFixture f;

            LifeSceneOptions opt = LifeFixture::first();
            opt.lives = 2;
            opt.age = 7.0f;

            LifeScene* life = f.life(opt);
            ASSERT_NE(life, nullptr);

            life->save.townKnown = true;
            life->save.stats["town_known"] = 1;
            life->save.stats["wortleaf"] = 6;
            life->save.stats["water"] = 10;
            life->onMonth();
            f.settle();

            life->save.stats["wortleaf"] = 6;
            life->save.stats["water"] = 10;
            life->save.stats["remedy"] = 0;

            auto all = life->piece<Button>("all");
            ASSERT_NE(all, nullptr);

            f.ecs.sendEvent(ActivitySelectedEvent{"town.market", "brew.remedy"});
            f.settle();

            const size_t lines = life->save.log.size();

            f.ecs.sendEvent(ButtonActivatedEvent{all->face.id, "life.all"});
            f.settle();

            EXPECT_EQ(life->save.stats["remedy"], 3);
            EXPECT_EQ(life->save.stats["wortleaf"], 0);
            EXPECT_EQ(life->save.stats["water"], 7);

            int made = 0;

            for (size_t i = lines; i < life->save.log.size(); ++i)
            {
                const LogEntry& line = life->save.log[i];

                if (line.text.find("Brew a remedy") == std::string::npos)
                    continue;

                ++made;
                EXPECT_NE(line.text.find("\xC3\x97" "3"), std::string::npos) << line.text;
            }

            EXPECT_EQ(made, 1);
        }


        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // What is bought once (the wooden blade) has its Buy, and no Buy all
        TEST(lifescene_test, what_is_bought_once_has_no_buy_all)
        {
            MockLogger logger;
            LifeFixture f;

            LifeSceneOptions opt = LifeFixture::first();
            opt.lives = 2;
            opt.age = 7.25f;

            LifeScene* life = f.life(opt);
            ASSERT_NE(life, nullptr);

            life->save.townKnown = true;
            life->save.stats["town_known"] = 1;
            life->save.stats["coin"] = 30;
            life->onMonth();
            f.settle();

            ASSERT_NE(stall(f, life, "buy.blade"), nullptr);

            f.ecs.sendEvent(ActivitySelectedEvent{"town.market", "buy.blade"});
            f.settle();

            auto begin = life->piece<Button>("begin");
            ASSERT_NE(begin, nullptr);
            EXPECT_TRUE(f.pos(life->named("begin"))->visible);
            EXPECT_EQ(begin->spec.label, "Buy");
            EXPECT_FALSE(f.pos(life->named("all"))->visible);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // Sell all and Buy all fit beside their button on each of the three pages: inside "At work
        // now", and clear of the button that spends written time
        TEST(lifescene_test, sell_all_and_buy_all_fit_on_every_page)
        {
            MockLogger logger;
            LifeFixture f;

            LifeSceneOptions opt = LifeFixture::first();
            opt.lives = 2;
            opt.age = 7.0f;

            LifeScene* life = f.life(opt);
            ASSERT_NE(life, nullptr);

            struct Page
            {
                float width, height;
                bool compact, phone;
            };

            for (const Page& page : {Page{1320.0f, 1020.0f, false, false}, Page{844.0f, 390.0f, true, false}, Page{390.0f, 844.0f, true, true}})
            {
                f.resize(page.width, page.height);
                f.frames(12);

                ASSERT_EQ(life->compact, page.compact) << page.width;
                ASSERT_EQ(life->phone, page.phone) << page.width;

                life->save.townKnown = true;
                life->save.stats["town_known"] = 1;
                life->save.stats["berries"] = 15;
                life->save.stats["coin"] = 20;
                life->save.stats["written"] = 600;
                life->onMonth();
                f.settle();

                for (const char* id : {"sell.berries", "buy.rations"})
                {
                    f.ecs.sendEvent(ActivitySelectedEvent{"town.market", id});
                    f.settle();
                    f.frames(6);

                    auto all = life->piece<Button>("all");
                    ASSERT_NE(all, nullptr);
                    ASSERT_TRUE(f.pos(life->named("all"))->visible) << id << " at " << page.width;

                    const auto face = f.pos(life->named("all"));
                    const auto begin = f.pos(life->named("begin"));
                    const auto working = f.pos(life->named("working"));

                    // Beside its button, on its line
                    EXPECT_GT(face->width, 0.0f);
                    EXPECT_NEAR(face->x, begin->x + begin->width + 8.0f, 0.5f) << id << " at " << page.width;
                    EXPECT_NEAR(face->y, begin->y, 0.5f) << id << " at " << page.width;

                    // Inside the panel
                    EXPECT_LE(face->x + face->width, working->x + working->width) << id << " at " << page.width;
                    EXPECT_LE(face->y + face->height, working->y + working->height) << id << " at " << page.width;

                    // And clear of the button that spends written time, on the same line
                    auto haste = life->piece<Button>("haste");
                    ASSERT_NE(haste, nullptr);
                    ASSERT_TRUE(f.pos(life->named("haste"))->isRenderable()) << page.width;

                    const auto fast = f.pos(life->named("haste"));
                    EXPECT_LE(face->x + face->width, fast->x) << id << " at " << page.width;
                }
            }
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // The order of the turn has a panel of its own at the top of the right column, over the
        // account: who acts when, one under the other, and the account has the rest of the column.
        TEST(lifescene_test, the_order_of_the_turn_has_its_panel_over_the_account)
        {
            MockLogger logger;
            LifeFixture f;

            LifeSceneOptions opt = LifeFixture::mockup();
            opt.fresh = true;
            opt.age = 7.0f;
            opt.walkSeed = 11;

            LifeScene* life = f.life(opt);
            ASSERT_NE(life, nullptr);

            life->save.age = 10.0f;
            life->save.stats["walked.hedgerows"] = 1;
            life->save.knowledge["found.millpond"] = 1;
            life->onMonth();
            f.settle();

            f.ecs.sendEvent(ActivityActivatedEvent{"life.activities", "walk.millpond"});
            f.frames(12);
            ASSERT_TRUE(life->onWalk());

            life->walkDo(WalkAct{"seek", "rat", 1});
            f.frames(12);
            answerEvents(f, life);
            f.frames(12);

            ASSERT_EQ(life->walkScreen, "fight");

            f.pass(400.0f, 50.0f);
            f.settle();

            auto order = f.pos(walkPart(life, "orderPanel"));
            auto account = f.pos(walkPart(life, "accountPanel"));
            auto foe = f.pos(walkPart(life, "foePanel0"));
            auto column = f.pos(walkPart(life, "walkRight"));

            // The head keeps what the fight costs the stay, said short
            EXPECT_NE(walkPart(life, "turnNote")->get<Label>()->spec.text.find("0 DAYS SO FAR"), std::string::npos);

            // At the top of the right column, level with the beasts, the account straight under it
            EXPECT_TRUE(order->visible);
            EXPECT_NEAR(order->y, foe->y, 0.5f);
            EXPECT_NEAR(order->x, account->x, 0.5f);
            EXPECT_GT(order->x, foe->x + foe->width);
            EXPECT_NEAR(account->y, order->y + order->height + 12.0f, 1.0f);

            // And the account runs to the foot of its column, no further
            EXPECT_NEAR(account->y + account->height, column->y + column->height, 24.0f);
            EXPECT_LE(account->y + account->height, column->y + column->height + 0.5f);

            // The timeline in the panel: he now, the rat after, the end of his day
            bool now = false;
            bool ratComes = false;

            for (const auto& [text, x] : life->timelineShown())
            {
                now = now or text == "NOW";
                ratComes = ratComes or text.find("Water rat") != std::string::npos;
            }

            EXPECT_TRUE(now);
            EXPECT_TRUE(ratComes);
            EXPECT_TRUE(f.pos(walkPart(life, "timeline"))->visible);

            // A narrower window, in three columns still: the same screen and the same buttons,
            // his turn's panel narrower and the right column against the window's edge
            const _unique_id screen = life->walkPage.id;
            const _unique_id strike = life->moves().front().second.root.id;
            const float menuWas = f.pos(walkPart(life, "menuPanel"))->width;

            f.resize(1120.0f, 1020.0f);
            f.pass(400.0f, 50.0f);
            f.settle();

            EXPECT_EQ(life->walkPage.id, screen);
            ASSERT_FALSE(life->moves().empty());
            EXPECT_EQ(life->moves().front().second.root.id, strike);
            EXPECT_NEAR(f.pos(walkPart(life, "menuPanel"))->width, menuWas - 200.0f, 1.0f);
            EXPECT_NEAR(order->x + order->width, 1120.0f - 16.0f, 4.5f);
            EXPECT_GT(order->x, f.pos(walkPart(life, "menuPanel"))->x + f.pos(walkPart(life, "menuPanel"))->width - 0.5f);
            EXPECT_NEAR(f.pos(walkPart(life, "moveNote"))->width, f.pos(walkPart(life, "menuPanel"))->width - 32.0f, 1.0f);

            // The timeline, then what is coming and his quickness under it, inside the panel
            auto box = f.pos(walkPart(life, "timeline"));

            EXPECT_GT(f.pos(walkPart(life, "quick"))->y, box->y + box->height - 0.5f);
            EXPECT_LE(f.pos(walkPart(life, "quick"))->y + f.pos(walkPart(life, "quick"))->height, order->y + order->height + 0.5f);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // The fight's page shows what the rules say of it: the next six actions, his first; what
        // is on a beast on its panel; his blows on a line, his workings on the next.
        TEST(lifescene_test, a_fight_shows_its_timeline_and_what_he_knows)
        {
            MockLogger logger;
            LifeFixture f;

            LifeSceneOptions opt = LifeFixture::mockup();
            opt.fresh = true;
            opt.lives = 2;
            opt.age = 7.0f;
            opt.walkSeed = 11;

            LifeScene* life = f.life(opt);
            ASSERT_NE(life, nullptr);

            // A boy of ten whose world has found the pond, who has the edge and a spark
            life->save.age = 10.0f;
            life->save.stats["walked.hedgerows"] = 1;
            life->save.stats["slash_learned"] = 1;
            life->save.stats["forbidden_read"] = 1;
            life->save.stats["int"] = 12;
            life->save.knowledge["found.millpond"] = 1;
            life->onMonth();
            f.settle();

            f.ecs.sendEvent(ActivityActivatedEvent{"life.activities", "walk.millpond"});
            f.frames(12);
            ASSERT_TRUE(life->onWalk());

            f.ecs.sendEvent(ActivitySelectedEvent{"walk.seeks", "rat"});
            f.settle();
            f.ecs.sendEvent(ButtonActivatedEvent{0, "walk.seek"});
            f.frames(12);
            answerEvents(f, life);
            f.frames(12);

            ASSERT_EQ(life->walkScreen, "fight");
            f.pass(100.0f, 50.0f);

            // The timeline, his turn first
            bool now = false;

            for (const auto& [text, x] : life->timelineShown())
                now = now or text == "NOW";

            EXPECT_TRUE(now);

            // His blows on a line under what anyone can do, his workings on the next
            const Button* strike = nullptr;
            const Button* slash = nullptr;
            const Button* spark = nullptr;

            for (const auto& [tag, button] : life->moves())
            {
                if (tag == "walk.move.strike")
                    strike = &button;

                if (tag == "walk.move.tech.slash")
                    slash = &button;

                if (tag == "walk.move.tech.spark")
                    spark = &button;
            }

            ASSERT_NE(strike, nullptr);
            ASSERT_NE(slash, nullptr);
            ASSERT_NE(spark, nullptr);
            EXPECT_GT(f.pos(slash->root)->y, f.pos(strike->root)->y + 0.5f);
            EXPECT_GT(f.pos(spark->root)->y, f.pos(slash->root)->y + 0.5f);

            // A spark that lands leaves the rat burning, and its panel says so
            for (int i = 0; i < 3 and walkPart(life, "foeLife0")->get<Label>()->spec.text.find("BURNING") == std::string::npos; ++i)
            {
                f.ecs.sendEvent(ButtonActivatedEvent{0, "walk.move.tech.spark"});
                f.frames(4);
                f.pass(3000.0f, 50.0f);
            }

            EXPECT_NE(walkPart(life, "foeLife0")->get<Label>()->spec.text.find("BURNING"), std::string::npos)
                << walkPart(life, "foeLife0")->get<Label>()->spec.text;
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // The timeline, top right: his turns and the beasts' where they fall in time, and the end
        // of his day. The mouse on a quick move brings his next turn sooner, the old place faint.
        TEST(lifescene_test, the_timeline_moves_with_the_move_under_the_mouse)
        {
            MockLogger logger;
            LifeFixture f;

            LifeSceneOptions opt = LifeFixture::mockup();
            opt.build = "street";
            opt.walk = "hollow";
            opt.walkSeed = 11;

            LifeScene* life = f.life(opt);
            ASSERT_NE(life, nullptr);
            f.frames(12);
            ASSERT_TRUE(life->onWalk());

            f.ecs.sendEvent(ActivitySelectedEvent{"walk.seeks", "boar"});
            f.settle();
            f.ecs.sendEvent(ButtonActivatedEvent{0, "walk.seek"});
            f.frames(12);

            // (the street boy lies in wait: the card asks how to meet it, the open fight first)
            answerEvents(f, life);
            f.frames(12);
            ASSERT_EQ(life->walkScreen, "fight");
            f.pass(100.0f, 50.0f);

            auto firstX = [life](const std::string& text) {
                float x = -1.0f;

                for (const auto& [shown, at] : life->timelineShown())
                {
                    if (shown == text and (x < 0.0f or at < x))
                        x = at;
                }

                return x;
            };

            ASSERT_GE(firstX("NOW"), 0.0f);
            ASSERT_GT(firstX("TURN 2"), firstX("NOW"));
            EXPECT_LT(firstX("THE DAY ENDS"), 0.0f);   // (his day does not end with this turn)

            const float before = firstX("TURN 2");

            // The mouse on Thrust
            for (const auto& [tag, button] : life->moves())
            {
                if (tag == "walk.move.tech.thrust")
                {
                    auto box = f.pos(button.face);

                    f.ecs.sendEvent(OnMouseMove{Point2D{box->x + box->width / 2.0f, box->y + box->height / 2.0f}, nullptr});
                }
            }

            // (the scene looks at the mouse each tick of its clock)
            f.frames(4);
            f.pass(300.0f, 50.0f);

            EXPECT_LT(firstX("TURN 2"), before);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // --build keep --walk hollow --set str=20: a man of the Keep, the Hollow known through and
        // through, on its site with every beast and its boss to seek; a figure set on top.
        TEST(lifescene_test, a_build_opens_on_a_walk_to_try_its_fights)
        {
            MockLogger logger;
            LifeFixture f;

            LifeSceneOptions opt = LifeFixture::mockup();
            opt.build = "keep";
            opt.walk = "hollow";
            opt.sets = {{"str", 20}};
            opt.walkSeed = 11;

            LifeScene* life = f.life(opt);
            ASSERT_NE(life, nullptr);
            f.frames(12);

            ASSERT_TRUE(life->onWalk());
            EXPECT_NEAR(life->save.age, 25.0f, 0.01f);
            EXPECT_EQ(life->save.stats["keep_oath"], 1);
            EXPECT_EQ(life->save.stats["str"], 20);
            EXPECT_EQ(life->save.knowledge["study.hollow"], 10);

            bool grey = false;

            for (const auto& s : life->walk.seeks)
            {
                auto id = s.find("id");

                if (id != s.end() and id->second.toString() == "enc.grey")
                    grey = true;
            }

            EXPECT_TRUE(grey);

            // Without --walk it opens on the Life page with the same man
            LifeFixture g;
            LifeSceneOptions plain = LifeFixture::mockup();
            plain.build = "collegium";

            LifeScene* scholar = g.life(plain);
            ASSERT_NE(scholar, nullptr);
            g.frames(12);

            EXPECT_FALSE(scholar->onWalk());
            EXPECT_EQ(scholar->save.stats["collegium"], 1);
            EXPECT_EQ(scholar->save.knowledge["study.quarry"], 10);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // A boss fight lost is the end of the life: no record is kept, and the ending says how.
        TEST(lifescene_test, a_boss_fight_lost_ends_the_life)
        {
            MockLogger logger;
            LifeFixture f;

            LifeSceneOptions opt = LifeFixture::mockup();
            opt.fresh = true;
            opt.lives = 2;
            opt.age = 7.0f;
            opt.walkSeed = 11;

            LifeScene* life = f.life(opt);
            ASSERT_NE(life, nullptr);

            // A youth of fifteen whose world knows the Hollow through and through, with a Vitality of one
            life->save.age = 15.0f;
            life->save.stats["str"] = 9;
            life->save.stats["bow_learned"] = 1;
            life->save.stats["vit"] = 1;
            life->save.knowledge["visit.hedgerows"] = 1;
            life->save.knowledge["study.hedgerows"] = 10;
            life->save.knowledge["visit.hollow"] = 1;
            life->save.knowledge["study.hollow"] = 10;
            life->onMonth();
            f.settle();

            ASSERT_FALSE(life->ended);

            f.ecs.sendEvent(ActivityActivatedEvent{"life.activities", "walk.hollow"});
            f.frames(12);
            ASSERT_TRUE(life->onWalk());

            f.ecs.sendEvent(ActivitySelectedEvent{"walk.seeks", "enc.grey"});
            f.settle();
            f.ecs.sendEvent(ButtonActivatedEvent{0, "walk.seek"});
            f.frames(12);
            answerEvents(f, life);
            f.frames(12);

            auto over = [life]() {
                auto it = life->walk.duel.find("over");

                return it == life->walk.duel.end() ? std::string() : it->second.toString();
            };

            for (int i = 0; i < 30 and life->walkScreen == "fight" and over().empty(); ++i)
            {
                f.ecs.sendEvent(ButtonActivatedEvent{0, "walk.move.guard"});
                f.frames(4);
                f.pass(3000.0f, 50.0f);
            }

            ASSERT_EQ(over(), "lost");

            f.ecs.sendEvent(ButtonActivatedEvent{0, "walk.act"});
            f.frames(12);

            // (the line the next life opens with is the rules': rules_test.a_fall_on_a_walk_is_a_death)
            EXPECT_TRUE(life->ended);
            EXPECT_TRUE(life->save.recipes.empty());
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // A fight in one column: the beasts there are, then his turn straight under them. A panel
        // of a beast there is none of holds no room in the column.
        TEST(lifescene_test, a_fight_in_one_column_holds_only_the_beasts_there_are)
        {
            MockLogger logger;
            LifeFixture f;

            LifeSceneOptions opt = LifeFixture::mockup();
            opt.fresh = true;
            opt.walkSeed = 11;

            LifeScene* life = f.life(opt);
            ASSERT_NE(life, nullptr);

            life->save.age = 10.0f;
            life->save.stats["walked.hedgerows"] = 1;
            life->save.knowledge["found.millpond"] = 1;
            life->onMonth();
            f.settle();

            f.resize(420.0f, 800.0f);
            f.frames(12);

            f.ecs.sendEvent(ActivityActivatedEvent{"life.activities", "walk.millpond"});
            f.frames(12);
            ASSERT_TRUE(life->onWalk());
            EXPECT_TRUE(life->walkNarrow);

            life->walkDo(WalkAct{"seek", "rat", 1});
            f.frames(12);
            answerEvents(f, life);
            f.frames(12);

            ASSERT_EQ(life->walkScreen, "fight");

            // The screen settles over its first passes, once its column holds its panels
            f.pass(100.0f, 50.0f);
            f.settle();

            auto foeBox = f.pos(walkPart(life, "foePanel0"));
            auto menuBox = f.pos(walkPart(life, "menuPanel"));

            EXPECT_TRUE(foeBox->visible);
            EXPECT_FALSE(f.pos(walkPart(life, "foePanel1"))->visible);
            EXPECT_FALSE(f.pos(walkPart(life, "foePanel3"))->visible);

            // One beast: his turn is the next thing down, the column's spacing under it
            EXPECT_NEAR(menuBox->x, foeBox->x, 0.5f);
            EXPECT_GT(menuBox->y, foeBox->y + foeBox->height - 0.5f);
            EXPECT_LT(menuBox->y, foeBox->y + foeBox->height + 30.0f);

            // And what he may do is there to press, in the window's width
            ASSERT_FALSE(life->moves().empty());

            for (const auto& [tag, button] : life->moves())
            {
                auto pos = f.pos(button.root);

                EXPECT_GE(pos->x, menuBox->x - 0.5f) << tag;
                EXPECT_LE(pos->x + pos->width, 420.0f + 0.5f) << tag;
            }

            // The timeline's gauge and tags scroll with the column and are clipped to it as the rest
            // of its panel is (on the page, they were drawn over its head once scrolled up to it),
            // and they are as wide as the panel leaves them
            EntityRef orderPart = walkPart(life, "orderPanel");
            ASSERT_FALSE(orderPart.empty());
            ASSERT_TRUE(orderPart->has<Panel>());
            auto order = orderPart->get<Panel>();
            EntityRef orderRoot = order->root;
            ASSERT_TRUE(orderRoot->has<ClippedTo>());

            const auto pieces = life->timelinePieces();
            ASSERT_FALSE(pieces.empty());

            for (EntityRef piece : pieces)
            {
                ASSERT_FALSE(piece.empty());
                ASSERT_TRUE(f.ecs.getEntity(piece.id)->has<ClippedTo>()) << piece.id;
                EXPECT_EQ(f.ecs.getEntity(piece.id)->get<ClippedTo>()->clipperId, orderRoot->get<ClippedTo>()->clipperId) << piece.id;
            }

            EXPECT_NEAR(f.pos(walkPart(life, "timeline"))->width, order->innerWidth(), 0.5f);
            EXPECT_LE(f.pos(pieces.front())->x + f.pos(pieces.front())->width, f.pos(orderRoot)->x + f.pos(orderRoot)->width - 0.5f);

            // The head's kicker and title end before the turn's figure as it is drawn, with the bug
            // report's button between them, and take the room up to the button (once the screen
            // has settled)
            f.pass(300.0f, 50.0f);

            auto turn = walkPart(life, "turn");
            const float turnLeft = f.pos(turn)->x + f.pos(turn)->width - turn->get<TTFText>()->textWidth;

            ASSERT_FALSE(life->reportButton.root.empty());
            auto report = f.pos(life->reportButton.root);
            EXPECT_LE(report->x + report->width, turnLeft + 0.5f);

            for (const char* name : {"kicker", "fightTitle"})
            {
                auto head = f.pos(walkPart(life, name));

                EXPECT_LE(head->x + head->width, report->x - 15.5f) << name;
                EXPECT_GT(head->x + head->width, report->x - 30.0f) << name;
            }
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // What his life has taught him is a button of his turn as the rest is, on a line of its
        // own with what it costs, and pressed it is done. He is told of his breath then.
        TEST(lifescene_test, a_learned_move_is_a_button_of_his_turn)
        {
            MockLogger logger;
            LifeFixture f;

            LifeSceneOptions opt = LifeFixture::mockup();
            opt.fresh = true;
            opt.walkSeed = 11;

            LifeScene* life = f.life(opt);
            ASSERT_NE(life, nullptr);

            // A scholar of the Collegium, sworn to the Keep besides
            life->save.age = 10.0f;
            life->save.stats["walked.hedgerows"] = 1;
            life->save.knowledge["found.millpond"] = 1;
            life->save.stats["collegium"] = 1;
            life->save.stats["fire_course"] = 1;
            life->save.stats["keep_oath"] = 1;
            life->onMonth();
            f.settle();

            f.ecs.sendEvent(ActivityActivatedEvent{"life.activities", "walk.millpond"});
            f.frames(12);
            ASSERT_TRUE(life->onWalk());

            life->walkDo(WalkAct{"seek", "rat", 1});
            f.frames(12);
            answerEvents(f, life);
            f.frames(12);

            ASSERT_EQ(life->walkScreen, "fight");

            f.pass(100.0f, 50.0f);
            f.settle();

            auto button = [&](const std::string& tag) -> const Button* {
                for (const auto& [name, made] : life->moves())
                {
                    if (name == tag)
                        return &made;
                }

                return nullptr;
            };

            const Button* strike = button("walk.move.strike");
            const Button* heavy = button("walk.move.tech.heavy");
            const Button* firebolt = button("walk.move.tech.firebolt");

            ASSERT_NE(strike, nullptr);
            ASSERT_NE(heavy, nullptr);
            ASSERT_NE(firebolt, nullptr);
            EXPECT_EQ(button("walk.move.tech.sweep"), nullptr);

            // What anyone can do costs nothing and says so by saying nothing; what he learned says its breath
            EXPECT_TRUE(strike->costText.empty());
            EXPECT_NE(firebolt->costText.find("BREATH"), std::string::npos);
            EXPECT_EQ(firebolt->spec.variant, ButtonVariant::Study);
            EXPECT_EQ(heavy->spec.variant, ButtonVariant::Quiet);

            // On lines of their own, under what anyone can do, and inside his turn's panel: his
            // blows on one line, his workings on the next
            auto menuBox = f.pos(walkPart(life, "menuPanel"));

            EXPECT_GT(f.pos(heavy->root)->y, f.pos(strike->root)->y + f.pos(strike->root)->height - 0.5f);
            EXPECT_GT(f.pos(firebolt->root)->y, f.pos(heavy->root)->y + f.pos(heavy->root)->height - 0.5f);
            EXPECT_LE(f.pos(firebolt->root)->x + f.pos(firebolt->root)->width, menuBox->x + menuBox->width + 0.5f);
            EXPECT_LE(f.pos(firebolt->root)->y + f.pos(firebolt->root)->height, menuBox->y + menuBox->height + 0.5f);

            // He has something to spend breath on: he is told of it
            EXPECT_TRUE(f.pos(walkPart(life, "breath"))->visible);
            EXPECT_TRUE(f.pos(walkPart(life, "breathBar"))->visible);

            // Pressed, it is done: the turn has gone by, and the breath with it
            const std::string before = walkPart(life, "breath")->get<Label>()->spec.text;

            f.ecs.sendEvent(ButtonActivatedEvent{0, "walk.move.tech.firebolt"});
            f.frames(12);

            const bool over = walkPart(life, "turn")->get<Label>()->spec.text != "TURN 1" or not f.pos(walkPart(life, "hand"))->visible;

            EXPECT_TRUE(over);
            EXPECT_NE(walkPart(life, "breath")->get<Label>()->spec.text, before);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // Against a pack he aims at one beast and then at another as often as he likes in a turn:
        // the buttons of his turn stand where they stood, the one aimed at lit. When they are made
        // again (a button more or less), the screen keeps hold of nothing that is gone, and the
        // button under a mouse that has not moved takes the next press.
        TEST(lifescene_test, another_aim_keeps_the_buttons_of_his_turn)
        {
            MockLogger logger;
            LifeFixture f;

            LifeSceneOptions opt = LifeFixture::mockup();
            opt.walk = "hollow";
            opt.walkSeed = 3;

            LifeScene* life = f.life(opt);
            ASSERT_NE(life, nullptr);
            f.frames(12);
            f.pass(400.0f, 50.0f);
            ASSERT_TRUE(life->onWalk());

            // The wood known 4 of 4 from here on: its packs are found
            life->save.knowledge["study.hollow"] = 10;

            life->walkDo(WalkAct{"seek", "wolf", 3});
            f.frames(12);
            answerEvents(f, life);
            f.frames(12);

            ASSERT_EQ(life->walkScreen, "fight");
            ASSERT_EQ(life->walk.targets.size(), 3u);

            f.pass(100.0f, 50.0f);
            f.settle();

            auto faces = [&]() {
                std::vector<_unique_id> ids;

                for (const auto& [tag, button] : life->moves())
                    ids.push_back(button.face.id);

                return ids;
            };

            auto state = [&](const Button& button) { return f.ecs.getEntity(button.face.id)->get<ButtonState>(); };

            // A button a wolf, then what he may do
            const std::vector<_unique_id> stood = faces();

            ASSERT_GT(stood.size(), 4u);
            EXPECT_EQ(life->moves()[0].first, "walk.aim.0");
            EXPECT_EQ(life->moves()[2].first, "walk.aim.2");
            ASSERT_EQ(life->moves()[3].first, "walk.move.strike");

            const std::string turn = walkPart(life, "turn")->get<Label>()->spec.text;

            // The second, the third, and the second again
            for (int aim : {1, 2, 1})
            {
                f.ecs.sendEvent(ButtonActivatedEvent{0, "walk.aim." + std::to_string(aim)});
                f.frames(12);

                ASSERT_EQ(life->walkScreen, "fight") << aim;
                EXPECT_EQ(faces(), stood) << aim;

                for (int i = 0; i < 3; ++i)
                {
                    const Button& button = life->moves()[i].second;
                    const ButtonVariant lit = i == aim ? ButtonVariant::Study : ButtonVariant::Quiet;

                    EXPECT_EQ(button.spec.variant, lit) << aim << " " << i;
                    EXPECT_EQ(state(button)->variant, lit) << aim << " " << i;
                }
            }

            // In the same turn: to aim is not to strike
            EXPECT_EQ(walkPart(life, "turn")->get<Label>()->spec.text, turn);

            // The mouse on Strike, and left there
            const _unique_id strike = life->moves()[3].second.face.id;
            auto box = f.pos(life->moves()[3].second.face);
            const Point2D at{box->x + box->width / 2.0f, box->y + box->height / 2.0f};

            f.ecs.sendEvent(OnMouseMove{at, nullptr});
            f.frames(4);

            ASSERT_TRUE(state(life->moves()[3].second)->hovered);

            // A remedy more in his bag, or none left: his turn has a button more, or one less, and
            // its buttons are made again
            bool remedy = false;

            for (const auto& [tag, button] : life->moves())
                remedy = remedy or tag == "walk.move.remedy";

            life->save.stats["remedy"] = remedy ? 0 : 2;

            f.ecs.sendEvent(ButtonActivatedEvent{0, "walk.aim.2"});
            f.pass(240.0f, 16.0f);
            f.settle();

            ASSERT_EQ(life->walkScreen, "fight");
            ASSERT_EQ(faces().size(), remedy ? stood.size() - 1 : stood.size() + 1);

            for (auto id : stood)
                EXPECT_EQ(f.ecs.getEntity(id), nullptr) << id;

            // Nothing the screen knows by a name is gone: a name is looked through at every lookup
            for (const auto& [name, child] : life->walkPage->get<Prefab>()->namedChildrenIds)
                EXPECT_NE(f.ecs.getEntity(child.id), nullptr) << name;

            // Strike stands where it stood, another button: hovered with no move of the mouse,
            // and a press on it is a blow
            ASSERT_EQ(life->moves()[3].first, "walk.move.strike");
            EXPECT_NE(life->moves()[3].second.face.id, strike);
            EXPECT_TRUE(state(life->moves()[3].second)->hovered);

            const size_t told = life->walk.state.account.size();

            f.ecs.sendEvent(OnMouseClick{at, static_cast<MouseButton>(1)});
            f.frames(4);
            f.ecs.sendEvent(OnMouseRelease{at, static_cast<MouseButton>(1)});
            f.frames(12);

            EXPECT_GT(life->walk.state.account.size(), told);

            // And he aims on as before
            for (int aim : {0, 1, 0, 2})
            {
                f.ecs.sendEvent(ButtonActivatedEvent{0, "walk.aim." + std::to_string(aim)});
                f.frames(12);

                ASSERT_FALSE(life->walkPage.empty()) << aim;
            }
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // What his walks brought for the town ends with him, but for what the place he leaves his
        // coin to still asks: that much is left to it, and the next life finds the place asking less.
        TEST(lifescene_test, gift_at_death_leaves_what_the_place_asks)
        {
            MockLogger logger;
            LifeFixture f;

            LifeScene* life = f.life();
            ASSERT_NE(life, nullptr);

            f.frames(12);

            // No coin, but firewood and stone; the lodge is a place a life before him found
            life->save.stats["coin"] = 0;
            life->save.stats["firewood"] = 32;
            life->save.stats["ashwood"] = 4;
            life->save.stats["stone"] = 9;
            life->save.seen.push_back("lodge");
            life->save.stats["rations"] = 0;
            life->save.stats["vit"] = 1;
            life->onMonth();
            f.frames(12);

            ASSERT_TRUE(life->ended);
            ASSERT_FALSE(life->ending.empty());

            auto prefab = life->ending->get<Prefab>();

            // The places that ask for any of it are offered, each with what it would take: the market's
            // stall asks firewood, the mill's wheel stone, the lodge firewood and ash wood
            EntityRef line = prefab->findEntity("giftLabel");
            ASSERT_FALSE(line.empty());
            EXPECT_TRUE(f.pos(line)->visible);
            EXPECT_EQ(line->get<Label>()->spec.text, "LEAVE WHAT A PLACE CAN USE TO");

            ASSERT_EQ(life->gifts.size(), 3u);

            Button lodge;

            for (const auto& [id, button] : life->gifts)
            {
                EXPECT_TRUE(id == "market" or id == "mill" or id == "lodge") << id;

                if (id == "lodge")
                    lodge = button;
            }

            ASSERT_FALSE(lodge.face.empty());
            EXPECT_NE(lodge.spec.label.find("20 firewood, 4 ash wood"), std::string::npos);

            f.ecs.sendEvent(ButtonActivatedEvent{lodge.face.id, "life.gift.lodge"});
            f.settle();
            ASSERT_EQ(life->gift, "lodge");

            f.ecs.sendEvent(ButtonActivatedEvent{0, "life.again"});
            f.frames(12);

            // No more than its next level asks, and nothing of what it does not ask
            ASSERT_FALSE(life->ended);
            EXPECT_EQ(life->save.fund["lodge.firewood"], 20);
            EXPECT_EQ(life->save.fund["lodge.ashwood"], 4);
            EXPECT_EQ(life->save.fund.count("lodge.stone"), 0u);
            EXPECT_EQ(life->save.fund.count("lodge"), 0u);

            // The new life holds none of it: what was not left of the basics is in the stash, the
            // rest is lost
            EXPECT_EQ(life->save.stats.count("firewood"), 0u);
            EXPECT_EQ(life->save.stats.count("stone"), 0u);
            EXPECT_EQ(life->save.stash["firewood"], 12);
            EXPECT_EQ(life->save.stash["stone"], 9);
            EXPECT_EQ(life->save.stash.count("ashwood"), 0u);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // What he holds of the basics at his death is kept in the stash, and the next life can
        // draw it.
        TEST(lifescene_test, the_stash_keeps_basics_across_a_death)
        {
            MockLogger logger;
            LifeFixture f;

            LifeScene* life = f.life();
            ASSERT_NE(life, nullptr);

            f.frames(12);

            life->save.stats["stone"] = 12;
            life->save.stats["water"] = 5;
            life->save.stats["rations"] = 0;
            life->save.stats["vit"] = 1;
            life->onMonth();
            f.frames(12);

            ASSERT_TRUE(life->ended);

            life->beginAgain();
            f.frames(12);

            ASSERT_FALSE(life->ended);
            EXPECT_EQ(life->save.stash["stone"], 12);
            EXPECT_EQ(life->save.stash["water"], 5);
            EXPECT_EQ(life->save.stats["stone"], 0);

            // The new life sees it as the town's, not as a stat of his
            const ElementMap seen = life->save.character();
            ASSERT_EQ(seen.count("stash.stone"), 1u);
            EXPECT_EQ(seen.at("stash.stone").get<int>(), 12);
            EXPECT_EQ(life->save.stats.count("stash.stone"), 0u);

            auto list = life->piece<ActivityList>("activities");
            ASSERT_NE(list, nullptr);
            ASSERT_NE(list->find(&f.ecs, "draw.stone"), nullptr);

            // Drawn, it is his, and the stash holds no more of it
            f.ecs.sendEvent(ActivitySelectedEvent{"life.activities", "draw.stone"});
            f.settle();
            f.ecs.sendEvent(ActivityActivatedEvent{"life.activities", "draw.stone"});
            f.frames(12);

            EXPECT_EQ(life->save.stats["stone"], 12);
            EXPECT_EQ(life->save.stash.count("stone"), 0u);
            EXPECT_EQ(life->save.stats.count("stash.stone"), 0u);
            EXPECT_EQ(life->save.stash["water"], 5);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // The stash keeps no more than its most of any one thing: the rest ends with him.
        TEST(lifescene_test, the_stash_stops_at_its_most)
        {
            MockLogger logger;
            LifeFixture f;

            LifeScene* life = f.life();
            ASSERT_NE(life, nullptr);

            f.frames(12);

            life->save.stash["stone"] = 95;
            life->save.stats["stone"] = 12;
            life->save.stats["rations"] = 0;
            life->save.stats["vit"] = 1;
            life->onMonth();
            f.frames(12);

            ASSERT_TRUE(life->ended);

            life->beginAgain();
            f.frames(12);

            EXPECT_EQ(life->save.stash["stone"], 100);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // The gift is taken first: what a place still asks is left to it, and only the rest goes
        // into the stash. The lodge's hounds ask 30 stone, 20 of it left by a life before: of 12
        // he holds, 10 go to the lodge and 2 to the stash.
        TEST(lifescene_test, gift_comes_before_the_stash)
        {
            MockLogger logger;
            LifeFixture f;

            LifeScene* life = f.life();
            ASSERT_NE(life, nullptr);

            f.frames(12);

            life->save.town["lodge"] = 2;
            life->save.fund["lodge.stone"] = 20;
            life->save.seen.push_back("lodge");
            life->save.stats["coin"] = 0;
            life->save.stats["stone"] = 12;
            life->save.stats["rations"] = 0;
            life->save.stats["vit"] = 1;
            life->onMonth();
            f.frames(12);

            ASSERT_TRUE(life->ended);

            // The mill's wheel asks stone too: it is offered beside the lodge
            Button lodge;
            bool mill = false;

            for (const auto& [id, button] : life->gifts)
            {
                mill = mill or id == "mill";

                if (id == "lodge")
                    lodge = button;
            }

            EXPECT_TRUE(mill);
            ASSERT_FALSE(lodge.face.empty());

            f.ecs.sendEvent(ButtonActivatedEvent{lodge.face.id, "life.gift.lodge"});
            f.settle();
            ASSERT_EQ(life->gift, "lodge");

            f.ecs.sendEvent(ButtonActivatedEvent{0, "life.again"});
            f.frames(12);

            ASSERT_FALSE(life->ended);
            EXPECT_EQ(life->save.fund["lodge.stone"], 20 + 10);
            EXPECT_EQ(life->save.stash["stone"], 2);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // A basic resource a place asks is left to it at his death like what his walks bring: the
        // mill's wheel asks 20 stone and 20 water. Of 30 stone and 25 water, the mill takes 20 of
        // each, and only the rest goes into the stash.
        TEST(lifescene_test, gift_leaves_basic_resources_to_a_place)
        {
            MockLogger logger;
            LifeFixture f;

            LifeScene* life = f.life();
            ASSERT_NE(life, nullptr);

            f.frames(12);

            life->save.stats["coin"] = 0;
            life->save.stats["stone"] = 30;
            life->save.stats["water"] = 25;
            life->save.stats["rations"] = 0;
            life->save.stats["vit"] = 1;
            life->onMonth();
            f.frames(12);

            ASSERT_TRUE(life->ended);

            Button mill;

            for (const auto& [id, button] : life->gifts)
            {
                if (id == "mill")
                    mill = button;
            }

            ASSERT_FALSE(mill.face.empty());
            EXPECT_NE(mill.spec.label.find("20 stone, 20 water"), std::string::npos) << mill.spec.label;

            f.ecs.sendEvent(ButtonActivatedEvent{mill.face.id, "life.gift.mill"});
            f.settle();
            ASSERT_EQ(life->gift, "mill");

            f.ecs.sendEvent(ButtonActivatedEvent{0, "life.again"});
            f.frames(12);

            ASSERT_FALSE(life->ended);
            EXPECT_EQ(life->save.fund["mill.stone"], 20);
            EXPECT_EQ(life->save.fund["mill.water"], 20);
            EXPECT_EQ(life->save.stash["stone"], 10);
            EXPECT_EQ(life->save.stash["water"], 5);
            EXPECT_EQ(life->save.fund.count("mill"), 0u);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // A level takes from the fund only what it asks of it: the mill's wheel asks 5 coin and 20
        // stone, and a fund of 15 coin and 30 stone keeps 10 of each toward the loft.
        TEST(lifescene_test, a_raise_keeps_what_is_over_in_the_fund)
        {
            MockLogger logger;
            LifeFixture f;

            LifeScene* life = f.life();
            ASSERT_NE(life, nullptr);

            f.frames(12);

            life->save.fund["mill"] = 15;
            life->save.fund["mill.stone"] = 30;
            life->save.stats["rations"] = 24;
            life->save.stats["water"] = 20;
            life->onMonth();
            f.frames(12);

            const int coin = life->save.stats["coin"];
            const int stone = life->save.stats["stone"];

            f.ecs.sendEvent(PlaceSelectedEvent{"mill"});
            f.settle();
            f.ecs.sendEvent(PlaceSelectedEvent{"mill"});
            f.settle();

            // The fund pays the coin and the stone: only the water is his
            ASSERT_EQ(life->save.running, "raise.mill.1");
            EXPECT_EQ(life->save.stats["coin"], coin);
            EXPECT_EQ(life->save.stats["stone"], stone);
            EXPECT_EQ(life->save.stats["water"], 0);

            for (int i = 0; i < 6; ++i)
            {
                life->onMonth();
                f.settle();
            }

            f.frames(12);

            ASSERT_EQ(life->save.town["mill"], 1);
            EXPECT_EQ(life->save.fund["mill"], 10);
            EXPECT_EQ(life->save.fund["mill.stone"], 10);

            // And the loft counts them: its 10 coin paid, 30 of its 40 stone still to give
            bool seen = false;

            for (const auto& place : life->town.places)
            {
                if (place.fields.at("id").toString() != "mill")
                    continue;

                seen = true;
                EXPECT_EQ(fieldInt(place.fields, "nextCoin"), 0);
                EXPECT_EQ(fieldInt(place.fields, "fund"), 10);

                const std::string asks = place.fields.at("nextCosts").toString();
                EXPECT_EQ(asks.find("coin"), std::string::npos) << asks;
                EXPECT_NE(asks.find("30 Stone"), std::string::npos) << asks;
            }

            EXPECT_TRUE(seen);

            // Raised again with nothing over, the fund of it is gone
            life->save.stats["coin"] = 0;
            life->save.stats["stone"] = 30;
            life->save.stats["water"] = 40;
            life->save.stats["gems"] = 2;
            life->onMonth();
            f.frames(12);

            f.ecs.sendEvent(PlaceSelectedEvent{"mill"});
            f.settle();
            f.ecs.sendEvent(PlaceSelectedEvent{"mill"});
            f.settle();

            ASSERT_EQ(life->save.running, "raise.mill.2");

            for (int i = 0; i < 9; ++i)
            {
                life->onMonth();
                f.settle();
            }

            f.frames(12);

            ASSERT_EQ(life->save.town["mill"], 2);
            EXPECT_EQ(life->save.fund.count("mill"), 0u);
            EXPECT_EQ(life->save.fund.count("mill.stone"), 0u);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // The ending says what goes into the stash, after the gift: choosing a place that asks for
        // some of it changes the line where it stands
        TEST(lifescene_test, the_ending_says_what_goes_into_the_stash)
        {
            MockLogger logger;
            LifeFixture f;

            LifeScene* life = f.life();
            ASSERT_NE(life, nullptr);

            f.frames(12);

            for (const char* other : {"berries", "water", "firewood", "clay"})
                life->save.stats.erase(other);

            life->save.town["lodge"] = 2;
            life->save.fund["lodge.stone"] = 20;
            life->save.seen.push_back("lodge");
            life->save.stats["coin"] = 0;
            life->save.stats["stone"] = 12;
            life->save.stats["rations"] = 0;
            life->save.stats["vit"] = 1;
            life->onMonth();
            f.frames(12);

            ASSERT_TRUE(life->ended);
            ASSERT_FALSE(life->ending.empty());

            auto said = [&]() { return life->ending->get<Prefab>()->findEntity("endStory")->get<Label>()->spec.text; };

            EXPECT_EQ(said().rfind(life->epitaph.text, 0), 0u) << said();
            EXPECT_NE(said().find("His stores go to the family's stash: 12 stone."), std::string::npos) << said();

            // The lodge takes ten of it: two go into the stash
            Button lodge;

            for (const auto& [id, button] : life->gifts)
            {
                if (id == "lodge")
                    lodge = button;
            }

            ASSERT_FALSE(lodge.face.empty());
            f.ecs.sendEvent(ButtonActivatedEvent{lodge.face.id, "life.gift.lodge"});
            f.settle();
            ASSERT_EQ(life->gift, "lodge");
            EXPECT_NE(said().find("His stores go to the family's stash: 2 stone."), std::string::npos) << said();

            // Let go of, all of it again
            f.ecs.sendEvent(ButtonActivatedEvent{lodge.face.id, "life.gift.lodge"});
            f.settle();
            ASSERT_EQ(life->gift, "");
            EXPECT_NE(said().find("His stores go to the family's stash: 12 stone."), std::string::npos) << said();
        }

        // What he holds over his limit at his death is kept whole, and the draw tile says what the
        // stash holds beside what a draw takes
        TEST(lifescene_test, the_stash_keeps_what_was_over_the_limit)
        {
            MockLogger logger;
            LifeFixture f;

            LifeScene* life = f.life();
            ASSERT_NE(life, nullptr);

            f.frames(12);

            life->save.stats["firewood"] = 40;
            life->save.stats["rations"] = 0;
            life->save.stats["vit"] = 1;
            life->onMonth();
            f.frames(12);

            ASSERT_TRUE(life->ended);

            life->beginAgain();
            f.frames(12);

            ASSERT_FALSE(life->ended);
            EXPECT_EQ(life->save.stash["firewood"], 40);

            auto list = life->piece<ActivityList>("activities");
            ASSERT_NE(list, nullptr);
            auto tile = list->find(&f.ecs, "draw.firewood");
            ASSERT_NE(tile, nullptr);
            EXPECT_EQ(tile->spec.each, "40 IN THE STASH");
            EXPECT_EQ(tile->spec.cost, "30 firewood from the stash");

            // Drawn, he holds what he has room for, and the rest stays
            f.ecs.sendEvent(ActivitySelectedEvent{"life.activities", "draw.firewood"});
            f.settle();
            f.ecs.sendEvent(ActivityActivatedEvent{"life.activities", "draw.firewood"});
            f.frames(12);

            EXPECT_EQ(life->save.stats["firewood"], 30);
            EXPECT_EQ(life->save.stash["firewood"], 10);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // Once the town is his, the stash is drawn at the storehouse: its tile is on the Town page,
        // and the draw stands there under its name, not on the Life page.
        TEST(lifescene_test, drawing_from_the_storehouse)
        {
            MockLogger logger;
            LifeFixture f;

            LifeScene* life = f.life();
            ASSERT_NE(life, nullptr);

            f.frames(12);

            life->save.stats["stone"] = 40;
            life->save.stats["rations"] = 0;
            life->save.stats["vit"] = 1;
            life->onMonth();
            f.frames(12);

            ASSERT_TRUE(life->ended);

            life->beginAgain();
            f.frames(12);

            ASSERT_FALSE(life->ended);
            ASSERT_EQ(life->save.stash["stone"], 40);

            // Before he knows the town, the draw is on the Life page
            auto list = life->piece<ActivityList>("activities");
            ASSERT_NE(list, nullptr);
            EXPECT_NE(list->find(&f.ecs, "draw.stone"), nullptr);

            // He knows the town: a month tells the page so
            life->save.townKnown = true;
            life->save.stats["rations"] = 40;
            life->onMonth();
            f.frames(12);

            auto grid = life->piece<PlaceGrid>("places");
            ASSERT_NE(grid, nullptr);
            EXPECT_NE(grid->tile("storehouse"), nullptr);

            EXPECT_EQ(list->find(&f.ecs, "draw.stone"), nullptr);
            ASSERT_NE(stall(f, life, "draw.stone"), nullptr);

            // Under the storehouse's name, after the market's rows (its buys first)
            auto market = life->piece<ActivityList>("marketRows");
            ASSERT_NE(market, nullptr);
            ASSERT_GE(market->spec.groups.size(), 2u);
            EXPECT_EQ(market->spec.groups.front().label, "Buy");
            EXPECT_EQ(market->spec.groups.back().label, "The Storehouse");
            ASSERT_FALSE(market->spec.groups.back().rows.empty());
            EXPECT_EQ(market->spec.groups.back().rows.front().id, "draw.stone");

            const int held = life->save.stats["stone"];
            const int kept = life->save.stash["stone"];

            f.ecs.sendEvent(ActivitySelectedEvent{"town.market", "draw.stone"});
            f.settle();
            f.ecs.sendEvent(ActivityActivatedEvent{"town.market", "draw.stone"});
            f.frames(12);

            const int drawn = life->save.stats["stone"] - held;

            EXPECT_GT(drawn, 0);
            EXPECT_EQ(life->save.stash["stone"], kept - drawn);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // A save written before the stash reads an empty one, and what he held is read as it was
        // written, even above its limit.
        TEST(lifescene_test, an_old_save_has_an_empty_stash)
        {
            MockLogger logger;
            LifeFixture f;

            const std::string path = "save/test_life_old_stash.sz";
            std::remove(path.c_str());

            LifeScene* life = f.life();
            ASSERT_NE(life, nullptr);

            LifeSave old = life->save;
            old.stash["stone"] = 7;
            old.stats["firewood"] = 50;

            ASSERT_TRUE(old.save(path));

            // The key taken out of the file, with all it holds: the lines under it are indented
            // deeper, and its closing brace is at its own depth
            std::string written;
            {
                std::ifstream in(path);
                std::stringstream all;
                all << in.rdbuf();
                written = all.str();
            }

            std::istringstream lines(written);
            std::string kept;
            std::string line;
            size_t under = std::string::npos;
            bool erased = false;

            while (std::getline(lines, line))
            {
                const size_t indent = line.find_first_not_of(" \t");

                if (under != std::string::npos)
                {
                    if (indent != std::string::npos and indent > under)
                        continue;

                    under = std::string::npos;

                    if (indent != std::string::npos and line[indent] == '}')
                        continue;
                }

                if (indent != std::string::npos and line.compare(indent, 6, "stash:") == 0)
                {
                    under = indent;
                    erased = true;
                    continue;
                }

                kept += line + "\n";
            }

            ASSERT_TRUE(erased);
            {
                std::ofstream out(path, std::ios::trunc);
                out << kept;
            }

            LifeSave read;
            ASSERT_TRUE(read.load(path));

            EXPECT_TRUE(read.stash.empty());
            EXPECT_EQ(read.stats["firewood"], 50);
            EXPECT_EQ(read.name, old.name);

            std::remove(path.c_str());
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // --walk <zone> opens a life made fit for the zone on its way there: every zone of the
        // table can be tried from the command line.
        TEST(lifescene_test, a_life_can_be_opened_on_a_walk)
        {
            for (const char* zone : {"hedgerows", "millpond", "hollow", "quarry"})
            {
                MockLogger logger;
                LifeFixture f;

                LifeSceneOptions opt = LifeFixture::mockup();
                opt.walk = zone;
                opt.walkSeed = 3;

                // As the game plays it: what moves, moves
                Motion::setReduced(false);

                LifeScene* life = f.life(opt);
                ASSERT_NE(life, nullptr) << zone;
                f.frames(12);
                f.pass(1500.0f, 16.0f);

                Motion::setReduced(true);

                ASSERT_TRUE(life->onWalk()) << zone;
                EXPECT_EQ(life->save.walk.run.at("zone").toString(), zone);
                EXPECT_EQ(life->walkScreen, "site") << zone;
                EXPECT_FALSE(life->walkPage.empty()) << zone;
                EXPECT_EQ(life->save.lives, 2) << zone;

                // Never written: a trial is not a life to keep
                EXPECT_FALSE(life->saveNow());

                f.load<EmptyScene>();
            }
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // A story (rules/storytable.pg) is a row of the Life page that opens on the walk's on-site
        // screen: its days are chosen there, and each month they add up to is a month of his life,
        // run there and then. The tests' story `trial` (4 days, 4 months) has the hidden row
        // `story.trial`, listed only for a life holding `test.stories`.
        namespace
        {
            LifeSceneOptions storyOptions(float age = 12.5f)
            {
                LifeSceneOptions opt = LifeFixture::mockup();
                opt.fresh = true;
                opt.lives = 2;
                opt.age = age;
                opt.walkSeed = 11;

                return opt;
            }

            // A life the trial is listed for, with the coin of its lectures and food for its months
            LifeScene* storyLife(LifeFixture& f, const LifeSceneOptions& opt)
            {
                LifeScene* life = f.life(opt);

                if (not life)
                    return nullptr;

                life->save.stats["test.stories"] = 1;
                life->save.stats["coin"] = 10;
                life->save.stats["rations"] = 12;

                // The list as he stands now
                life->onMonth();
                f.settle();

                return life;
            }

            std::string runText(const LifeScene* life, const std::string& key)
            {
                auto it = life->save.walk.run.find(key);

                return it == life->save.walk.run.end() ? std::string() : it->second.toString();
            }

            std::string siteText(const LifeScene* life, const std::string& key)
            {
                auto it = life->walk.site.find(key);

                return it == life->walk.site.end() ? std::string() : it->second.toString();
            }

            void openStory(LifeFixture& f)
            {
                f.ecs.sendEvent(ActivityActivatedEvent{"life.activities", "story.trial"});
                f.frames(12);
            }

            // A tile of the day chosen, then done by the button; what comes up after it is answered,
            // but the examination, which is left for the test to sit
            void takeTile(LifeFixture& f, LifeScene* life, const std::string& tile)
            {
                answerEvents(f, life);

                f.ecs.sendEvent(ActivitySelectedEvent{"walk.works", "tile." + tile});
                f.settle();
                f.ecs.sendEvent(ButtonActivatedEvent{0, "walk.work"});
                f.frames(8);

                for (int guard = 0; guard < 4 and life->onWalk() and siteText(life, "screen") == "event" and runText(life, "event") != "story.exam"; ++guard)
                {
                    f.ecs.sendEvent(ButtonActivatedEvent{0, "walk.option.0"});
                    f.settle();
                }
            }

            // Cards answered by their first answer, up to the masters' question of the third day
            void upToTheQuestion(LifeFixture& f, LifeScene* life)
            {
                for (int guard = 0; guard < 4 and siteText(life, "screen") == "event" and runText(life, "event") != "story.crossroads"; ++guard)
                {
                    f.ecs.sendEvent(ButtonActivatedEvent{0, "walk.option.0"});
                    f.settle();
                }
            }
        }

        TEST(lifescene_test, a_story_row_opens_the_story)
        {
            MockLogger logger;
            LifeFixture f;

            LifeScene* life = storyLife(f, storyOptions());
            ASSERT_NE(life, nullptr);

            auto list = life->piece<ActivityList>("activities");
            ASSERT_NE(list, nullptr);
            ASSERT_NE(list->find(&f.ecs, "story.trial"), nullptr);

            const int begun = life->save.begun;

            openStory(f);

            ASSERT_TRUE(life->onWalk());
            EXPECT_EQ(runText(life, "story"), "trial");
            EXPECT_EQ(runText(life, "day"), "1");
            EXPECT_EQ(life->save.running, "story.trial");
            EXPECT_EQ(life->save.monthsIn, 0);
            EXPECT_EQ(life->save.begun, begun + 1);
            EXPECT_EQ(life->rules.running, "story.trial");
            EXPECT_TRUE(life->paused);
            EXPECT_EQ(life->walkScreen, "site");
            EXPECT_FALSE(life->walkPage.empty());

            // Not a walk's words: no way home, nothing met on a way, no place to know
            f.settle();
            EXPECT_EQ(walkPart(life, "homePanel")->get<Panel>()->title->spec.text, "If he left now");
            EXPECT_EQ(walkPart(life, "cardPanel")->get<Panel>()->title->spec.text, "On this day");

            auto registry = f.ecs.getSystem<GlossRegistry>();
            ASSERT_NE(registry, nullptr);
            const GlossSpec* stands = registry->find("walk/knowledge");
            ASSERT_NE(stands, nullptr);
            EXPECT_EQ(stands->title, "The trial");

            // The calendar does not run under it: the days move it
            const float age = life->save.age;
            f.pass(5000.0f, 250.0f);

            EXPECT_FLOAT_EQ(life->save.age, age);
            EXPECT_TRUE(life->paused);

            // Nor does the key that goes on with a work stopped
            f.ecs.sendEvent(OnSDLScanCode{SDL_SCANCODE_SPACE, 0});
            f.settle();
            EXPECT_TRUE(life->paused);
        }

        TEST(lifescene_test, days_run_the_life_months)
        {
            MockLogger logger;
            LifeFixture f;

            LifeScene* life = storyLife(f, storyOptions());
            ASSERT_NE(life, nullptr);

            openStory(f);
            ASSERT_TRUE(life->onWalk());

            // One ration left: the first month eats it, the next ones find nothing
            life->save.stats["rations"] = 1;
            life->save.stats["vit"] = 40;
            life->save.stats["vitmax"] = 40;

            const float age = life->save.age;
            const size_t lines = life->save.log.size();

            takeTile(f, life, "library");

            ASSERT_TRUE(life->onWalk());
            EXPECT_FLOAT_EQ(life->save.age, age + 1.0f / 12.0f);
            EXPECT_EQ(life->save.monthsIn, 1);
            EXPECT_EQ(life->save.stats["rations"], 0);
            EXPECT_TRUE(life->paused);

            // The month's account is the log's, as any month's: the last ration went
            EXPECT_GT(life->save.log.size(), lines);

            takeTile(f, life, "library");

            EXPECT_FLOAT_EQ(life->save.age, age + 2.0f / 12.0f);
            EXPECT_EQ(life->save.monthsIn, 2);

            // A hungry month took from his Vitality, and the page says what it left him
            const int hungry = life->save.stats["vit"];
            EXPECT_LT(hungry, 40);
            EXPECT_EQ(runText(life, "hp"), std::to_string(hungry));

            takeTile(f, life, "lecture");

            EXPECT_FLOAT_EQ(life->save.age, age + 3.0f / 12.0f);
            EXPECT_EQ(life->save.monthsIn, 3);
            EXPECT_TRUE(life->paused);

            // The day's work did not give back what the month before had taken
            EXPECT_LT(life->save.stats["vit"], hungry);
            EXPECT_EQ(runText(life, "hp"), std::to_string(life->save.stats["vit"]));
        }

        TEST(lifescene_test, a_story_is_never_kept_as_a_record)
        {
            MockLogger logger;
            LifeFixture f;

            LifeScene* life = storyLife(f, storyOptions());
            ASSERT_NE(life, nullptr);

            const size_t recipes = life->save.recipes.size();
            const int walks = life->save.walksByHand;
            const float age = life->save.age;

            openStory(f);
            ASSERT_TRUE(life->onWalk());

            takeTile(f, life, "lecture");
            takeTile(f, life, "library");
            takeTile(f, life, "library");

            // The fourth day is the examination's
            answerEvents(f, life);
            f.frames(12);

            // Over: no record, no walk counted, no place walked, and nothing running
            EXPECT_FALSE(life->onWalk());
            EXPECT_TRUE(life->walkPage.empty());
            EXPECT_EQ(life->save.recipes.size(), recipes);
            EXPECT_EQ(life->save.walksByHand, walks);
            EXPECT_TRUE(life->save.walkedAt.empty());
            EXPECT_TRUE(life->save.running.empty());
            EXPECT_TRUE(life->rules.running.empty());

            // Its four months ran on its days, and its term came with the last
            EXPECT_FLOAT_EQ(life->save.age, age + 4.0f / 12.0f);
            EXPECT_EQ(life->save.done["story.trial"], 1);
            EXPECT_TRUE(life->paused);

            // The months stay where the story left them until he chooses what is next
            f.pass(5000.0f, 250.0f);
            EXPECT_FLOAT_EQ(life->save.age, age + 4.0f / 12.0f);
        }

        TEST(lifescene_test, leaving_a_story_early_spends_only_its_days)
        {
            MockLogger logger;
            LifeFixture f;

            LifeScene* life = storyLife(f, storyOptions());
            ASSERT_NE(life, nullptr);

            const int before = life->save.stats["rations"];
            const float age = life->save.age;

            openStory(f);
            ASSERT_TRUE(life->onWalk());

            // The board gave its four months' rations as it began
            const int board = (life->save.stats["rations"] - before) / 4;
            ASSERT_GT(board, 0);

            takeTile(f, life, "library");
            ASSERT_TRUE(life->onWalk());

            const int fed = life->save.stats["rations"];

            // Leave is asked twice
            answerEvents(f, life);
            f.ecs.sendEvent(ButtonActivatedEvent{0, "walk.leave"});
            f.settle();
            ASSERT_TRUE(life->onWalk());
            f.ecs.sendEvent(ButtonActivatedEvent{0, "walk.leave"});
            f.frames(12);

            EXPECT_FALSE(life->onWalk());
            EXPECT_TRUE(life->save.running.empty());
            EXPECT_EQ(life->save.done.count("story.trial"), 0u);

            // One day of four is one month of four
            EXPECT_FLOAT_EQ(life->save.age, age + 1.0f / 12.0f);

            // The rations of the three months he did not stay go back
            EXPECT_EQ(life->save.stats["rations"], fed - 3 * board);
        }

        TEST(lifescene_test, a_month_in_a_story_can_end_the_life)
        {
            MockLogger logger;
            LifeFixture f;

            LifeScene* life = storyLife(f, storyOptions());
            ASSERT_NE(life, nullptr);

            openStory(f);
            ASSERT_TRUE(life->onWalk());

            life->save.stats["rations"] = 0;
            life->save.stats["coin"] = 0;
            life->save.stats["vit"] = 1;

            takeTile(f, life, "library");

            EXPECT_TRUE(life->ended);
            EXPECT_FALSE(life->onWalk());
            EXPECT_TRUE(life->walkPage.empty());
            EXPECT_FALSE(life->ending.empty());
        }

        TEST(lifescene_test, a_story_saved_mid_term_reopens_where_it_was)
        {
            MockLogger logger;
            LifeFixture f;

            const std::string path = "save/test_life_story.sz";
            std::remove(path.c_str());

            LifeSceneOptions opt = storyOptions();
            opt.noSave = false;
            opt.savePath = path;

            LifeScene* life = storyLife(f, opt);
            ASSERT_NE(life, nullptr);

            openStory(f);
            ASSERT_TRUE(life->onWalk());

            takeTile(f, life, "lecture");

            // The second day's work brings the third day, and its cards
            f.ecs.sendEvent(ActivitySelectedEvent{"walk.works", "tile.library"});
            f.settle();
            f.ecs.sendEvent(ButtonActivatedEvent{0, "walk.work"});
            f.frames(8);
            upToTheQuestion(f, life);

            ASSERT_EQ(siteText(life, "screen"), "event");
            ASSERT_EQ(runText(life, "event"), "story.crossroads");

            const float age = life->save.age;
            const std::string day = runText(life, "day");

            ASSERT_TRUE(life->saveNow());

            f.load<EmptyScene>();
            opt.fresh = false;

            LifeScene* again = f.life(opt);
            ASSERT_NE(again, nullptr);
            f.frames(12);

            ASSERT_TRUE(again->onWalk());
            EXPECT_EQ(again->walkScreen, "site");
            EXPECT_EQ(siteText(again, "screen"), "event");
            EXPECT_EQ(runText(again, "event"), "story.crossroads");
            EXPECT_EQ(runText(again, "day"), day);
            EXPECT_EQ(again->save.running, "story.trial");
            EXPECT_FLOAT_EQ(again->save.age, age);
            EXPECT_TRUE(f.pos(walkPart(again, "cardPanel"))->visible);
            EXPECT_TRUE(again->paused);

            // No month runs on load, nor while the card waits
            f.pass(5000.0f, 250.0f);
            EXPECT_FLOAT_EQ(again->save.age, age);

            // Mid-fight: the yard of the tests' fight story (no row opens it), sparring with the wolf
            RuleWalk drill;
            ASSERT_TRUE(again->rules.walk(again->save.age, again->save.character(), again->save.name, "enter", 11, WalkState{}, WalkAct{"story", "drill", 0},
                                          std::vector<WalkRecipe>{}, RecordList{}, WalkQueue{}, "", drill));
            again->save.walk = drill.state;
            again->save.running.clear();
            ASSERT_TRUE(again->saveNow());

            f.load<EmptyScene>();

            LifeScene* yard = f.life(opt);
            ASSERT_NE(yard, nullptr);
            f.frames(12);

            ASSERT_TRUE(yard->onWalk());
            EXPECT_EQ(runText(yard, "story"), "drill");

            takeTile(f, yard, "bout");
            ASSERT_EQ(yard->walkScreen, "fight");

            f.ecs.sendEvent(ButtonActivatedEvent{0, "walk.move.guard"});
            f.settle();

            const size_t told = yard->save.walk.account.size();
            const float yardAge = yard->save.age;
            ASSERT_TRUE(yard->saveNow());

            f.load<EmptyScene>();

            LifeScene* fight = f.life(opt);
            ASSERT_NE(fight, nullptr);
            f.frames(12);

            ASSERT_TRUE(fight->onWalk());
            EXPECT_EQ(fight->walkScreen, "fight");
            EXPECT_EQ(fight->save.walk.account.size(), told);
            EXPECT_FLOAT_EQ(fight->save.age, yardAge);

            f.pass(5000.0f, 250.0f);
            EXPECT_FLOAT_EQ(fight->save.age, yardAge);

            f.load<EmptyScene>();
            std::remove(path.c_str());
        }

        TEST(lifescene_test, an_event_due_during_a_story_waits_for_its_end)
        {
            MockLogger logger;
            LifeFixture f;

            // Ten and ten months once the list is filled: the Flood's answers open at eleven, on
            // the story's second month
            LifeScene* life = storyLife(f, storyOptions(10.75f));
            ASSERT_NE(life, nullptr);

            openStory(f);
            ASSERT_TRUE(life->onWalk());

            for (const char* tile : {"lecture", "library", "library"})
            {
                takeTile(f, life, tile);

                ASSERT_TRUE(life->onWalk()) << tile;
                EXPECT_EQ(life->save.running, "story.trial") << tile;
                EXPECT_TRUE(life->save.setAside.empty()) << tile;
                EXPECT_TRUE(life->paused) << tile;
            }

            EXPECT_GE(life->save.age, 11.0f);

            // Its answers cannot be begun from under the story
            f.ecs.sendEvent(ActivityActivatedEvent{"life.activities", "flood.salvage"});
            f.settle();
            EXPECT_EQ(life->save.running, "story.trial");
            EXPECT_TRUE(life->save.setAside.empty());

            // The examination: the story is over, and the Flood is on the list
            answerEvents(f, life);
            f.frames(12);

            ASSERT_FALSE(life->onWalk());
            EXPECT_TRUE(life->save.setAside.empty());

            auto list = life->piece<ActivityList>("activities");
            ASSERT_NE(list, nullptr);
            EXPECT_NE(list->find(&f.ecs, "flood.salvage"), nullptr);
        }

        TEST(lifescene_test, a_story_card_shows_four_answers)
        {
            MockLogger logger;
            LifeFixture f;

            LifeScene* life = storyLife(f, storyOptions());
            ASSERT_NE(life, nullptr);

            openStory(f);
            ASSERT_TRUE(life->onWalk());

            takeTile(f, life, "lecture");

            f.ecs.sendEvent(ActivitySelectedEvent{"walk.works", "tile.library"});
            f.settle();
            f.ecs.sendEvent(ButtonActivatedEvent{0, "walk.work"});
            f.frames(8);
            upToTheQuestion(f, life);

            ASSERT_EQ(runText(life, "event"), "story.crossroads");
            ASSERT_EQ(life->walk.options.size(), 4u);

            auto shown = [&](LifeScene* l) {
                const char* labels[] = {"The art of numbers", "The art of letters", "The art of the stars", "The art of healing"};

                for (int i = 0; i < 4; ++i)
                {
                    const std::string name = "option" + std::to_string(i);
                    EntityRef button = walkPart(l, name);

                    ASSERT_FALSE(button.empty()) << name;
                    EXPECT_TRUE(f.pos(button)->visible) << name;
                    EXPECT_EQ(button->get<Button>()->label.spec.text, labels[i]) << name;
                    EXPECT_FALSE(button->get<Button>()->spec.disabled) << name;
                    EXPECT_TRUE(f.pos(walkPart(l, "optionLine" + std::to_string(i)))->visible) << name;
                }
            };

            f.pass(100.0f, 50.0f);
            EXPECT_FALSE(life->walkNarrow);
            shown(life);

            // A phone held upright: one column, the same four
            f.resize(420.0f, 800.0f);
            f.pass(200.0f, 50.0f);
            f.frames(12);

            ASSERT_TRUE(life->walkNarrow);
            shown(life);

            // The fourth is pressed as the others are
            f.ecs.sendEvent(ButtonActivatedEvent{0, "walk.option.3"});
            f.settle();
            EXPECT_EQ(runText(life, "m.d"), "1");
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // The days of a story are seen to pass: the head's gauge of the days and its stage, what
        // is new, what has gone, and how he stands, all written where they stand
        namespace
        {
            std::string walkText(LifeScene* life, const std::string& name)
            {
                EntityRef part = walkPart(life, name);

                return part.empty() or not part->has<Label>() ? std::string("<none>") : part->get<Label>()->spec.text;
            }

            ProgressRule* walkRule(LifeScene* life, const std::string& name)
            {
                EntityRef part = walkPart(life, name);

                return part.empty() or not part->has<ProgressRule>() ? nullptr : part->get<ProgressRule>().operator->();
            }

            // What the head's gauge of the days says, and how far it is filled
            std::string gaugeText(LifeScene* life)
            {
                ProgressRule* rule = walkRule(life, "strip");

                return rule and rule->caption ? rule->caption->spec.text : std::string("<none>");
            }

            float gaugePercent(LifeScene* life)
            {
                ProgressRule* rule = walkRule(life, "strip");

                return rule ? rule->spec.percent : -1.0f;
            }

            bool tileFlaired(LifeFixture& f, LifeScene* life, const std::string& tile)
            {
                EntityRef part = walkPart(life, "works");

                if (part.empty() or not part->has<ActivityList>())
                    return false;

                ActivityRow* row = part->get<ActivityList>()->find(&f.ecs, "tile." + tile);

                return row and row->spec.adventure;
            }

            std::string fieldText(const ElementMap& map, const std::string& key)
            {
                auto it = map.find(key);

                return it == map.end() ? std::string() : it->second.toString();
            }

            // An ending page that cannot be built (a list, not a node): a life that ends begins the
            // next one at once, as when the game has no ending to show
            std::string noEnding()
            {
                const std::string path = (std::filesystem::temp_directory_path() / "chronicle-no-ending.yaml").string();

                std::ofstream out(path);
                out << "- not\n- a node\n";

                return path;
            }

            // What stands under an entity: its pieces, and theirs, through prefabs and layouts
            size_t countUnder(LifeFixture& f, EntityRef root)
            {
                std::set<_unique_id> seen;
                std::vector<EntityRef> open{root};

                while (not open.empty())
                {
                    EntityRef at = open.back();
                    open.pop_back();

                    if (at.empty() or not seen.insert(at.id).second)
                        continue;

                    if (at->has<Prefab>())
                    {
                        for (auto id : at->get<Prefab>()->childrenIds)
                        {
                            if (auto child = f.ecs.getEntity(id))
                                open.push_back(child);
                        }
                    }

                    if (at->has<VerticalLayout>())
                    {
                        for (auto& child : at->get<VerticalLayout>()->entities)
                            open.push_back(child);
                    }

                    if (at->has<HorizontalLayout>())
                    {
                        for (auto& child : at->get<HorizontalLayout>()->entities)
                            open.push_back(child);
                    }
                }

                return seen.size();
            }
        }

        TEST(lifescene_test, a_story_head_shows_its_day)
        {
            MockLogger logger;
            LifeFixture f;

            LifeScene* life = storyLife(f, storyOptions());
            ASSERT_NE(life, nullptr);

            openStory(f);
            ASSERT_TRUE(life->onWalk());
            f.pass(300.0f, 50.0f);

            // The first day of four, on the gauge of the days, and the stage it begins
            ASSERT_NE(walkRule(life, "strip"), nullptr);
            EXPECT_EQ(gaugeText(life), "Day 1 of 4");
            EXPECT_NEAR(gaugePercent(life), 25.0f, 0.01f);
            EXPECT_EQ(walkText(life, "headNote"), "THE FIRST DAYS");
            EXPECT_EQ(walkText(life, "kicker"), "4 DAYS \xC2\xB7 4 MONTHS OF HIS LIFE");
            EXPECT_EQ(walkText(life, "zoneLine"), "Everything is new, and every door is open to him.");

            // How he stands, in words, under his Vitality; no place to know
            EXPECT_FALSE(siteText(life, "standing").empty());
            EXPECT_EQ(walkText(life, "standing"), siteText(life, "standing"));
            ASSERT_FALSE(walkPart(life, "standing").empty());
            EXPECT_TRUE(f.pos(walkPart(life, "standing"))->visible);
            EXPECT_FALSE(f.pos(walkPart(life, "knowledge"))->visible);
            EXPECT_FALSE(f.pos(walkPart(life, "knowledgeBar"))->visible);
            EXPECT_FALSE(f.pos(walkPart(life, "knowledgeNote"))->visible);

            // A day on: the gauge moves, a month is gone, and the stage's text was said once
            takeTile(f, life, "library");
            f.pass(300.0f, 50.0f);

            EXPECT_EQ(gaugeText(life), "Day 2 of 4 \xC2\xB7 1 month gone");
            EXPECT_NEAR(gaugePercent(life), 50.0f, 0.01f);
            EXPECT_EQ(walkText(life, "zoneLine"), "");
            EXPECT_EQ(walkText(life, "headNote"), "THE FIRST DAYS");

            // The third day is the last stage's first: its title and its text
            takeTile(f, life, "lecture");
            f.pass(300.0f, 50.0f);

            EXPECT_EQ(gaugeText(life), "Day 3 of 4 \xC2\xB7 2 months gone");
            EXPECT_EQ(walkText(life, "headNote"), "THE LAST DAYS");
            EXPECT_EQ(walkText(life, "zoneLine"), "What he has heard lets him follow more.");
            EXPECT_EQ(walkText(life, "standing"), siteText(life, "standing"));
        }

        // A phone's head has one line for the stage: its title, over the gauge; its text is the
        // record's line of that day. Wide again, the head says it as before
        TEST(lifescene_test, a_phone_story_head_says_the_stage_title_only)
        {
            MockLogger logger;
            LifeFixture f;

            LifeScene* life = storyLife(f, storyOptions());
            ASSERT_NE(life, nullptr);

            openStory(f);
            ASSERT_TRUE(life->onWalk());
            f.pass(300.0f, 50.0f);

            const std::string text = "Everything is new, and every door is open to him.";

            ASSERT_FALSE(life->walkNarrow);
            EXPECT_EQ(walkText(life, "zoneLine"), text);

            f.resize(390.0f, 844.0f);
            f.pass(200.0f, 50.0f);
            f.frames(12);

            ASSERT_TRUE(life->onWalk());
            ASSERT_TRUE(life->walkNarrow);
            EXPECT_EQ(walkText(life, "zoneLine"), "");
            EXPECT_EQ(walkText(life, "headNote"), "THE FIRST DAYS");

            // The record has it, as its first line
            ASSERT_FALSE(life->save.walk.record.empty());
            EXPECT_EQ(life->save.walk.record.front().at("text").toString(), "The first days. " + text);

            // A day that offers its tiles keeps its day's work
            EXPECT_TRUE(f.pos(walkPart(life, "workPanel"))->visible);

            f.resize(1320.0f, 1020.0f);
            f.pass(200.0f, 50.0f);
            f.frames(12);

            ASSERT_FALSE(life->walkNarrow);
            EXPECT_EQ(walkText(life, "zoneLine"), text);
        }

        TEST(lifescene_test, a_fresh_tile_carries_the_flair)
        {
            MockLogger logger;
            LifeFixture f;

            LifeScene* life = storyLife(f, storyOptions());
            ASSERT_NE(life, nullptr);

            openStory(f);
            ASSERT_TRUE(life->onWalk());
            f.pass(300.0f, 50.0f);

            // Everything is new on the first day: nothing is marked
            EXPECT_FALSE(tileFlaired(f, life, "lecture"));
            EXPECT_FALSE(tileFlaired(f, life, "library"));

            takeTile(f, life, "lecture");
            takeTile(f, life, "library");
            f.pass(300.0f, 50.0f);

            // The advanced course comes on the third day, and is marked that day
            ASSERT_EQ(runText(life, "day"), "3");
            EXPECT_TRUE(tileFlaired(f, life, "advanced"));
            EXPECT_FALSE(tileFlaired(f, life, "library"));

            // Not the next
            takeTile(f, life, "library");
            f.pass(300.0f, 50.0f);

            ASSERT_EQ(runText(life, "day"), "4");
            EXPECT_FALSE(tileFlaired(f, life, "advanced"));
        }

        TEST(lifescene_test, a_gone_line_names_what_left)
        {
            MockLogger logger;
            LifeFixture f;

            LifeScene* life = storyLife(f, storyOptions());
            ASSERT_NE(life, nullptr);

            openStory(f);
            ASSERT_TRUE(life->onWalk());
            f.pass(300.0f, 50.0f);

            ASSERT_FALSE(walkPart(life, "goneNote").empty());
            EXPECT_FALSE(f.pos(walkPart(life, "goneNote"))->visible);

            takeTile(f, life, "lecture");
            f.pass(300.0f, 50.0f);
            EXPECT_FALSE(f.pos(walkPart(life, "goneNote"))->visible);

            // The halls are walked the first two days only (the tile `early`): on the third they
            // are said gone
            takeTile(f, life, "library");
            f.pass(300.0f, 50.0f);

            ASSERT_EQ(runText(life, "day"), "3");
            EXPECT_TRUE(f.pos(walkPart(life, "goneNote"))->visible);
            EXPECT_EQ(walkText(life, "goneNote"), "No longer offered: Walk the halls");

            // Said once: the next day names what left then
            takeTile(f, life, "library");
            f.pass(300.0f, 50.0f);

            ASSERT_EQ(runText(life, "day"), "4");
            EXPECT_EQ(walkText(life, "goneNote").find("Walk the halls"), std::string::npos);
        }

        TEST(lifescene_test, a_story_page_is_not_rebuilt_by_a_day)
        {
            MockLogger logger;
            LifeFixture f;

            LifeScene* life = storyLife(f, storyOptions());
            ASSERT_NE(life, nullptr);

            openStory(f);
            ASSERT_TRUE(life->onWalk());
            f.pass(300.0f, 50.0f);

            const _unique_id page = life->walkPage.id;
            const size_t middle = countUnder(f, walkPart(life, "walkMiddle"));

            auto list = walkPart(life, "works")->get<ActivityList>();
            ActivityRow* library = list->find(&f.ecs, "tile.library");
            ASSERT_NE(library, nullptr);
            const _unique_id tile = library->root.id;

            takeTile(f, life, "library");
            f.pass(300.0f, 50.0f);

            ASSERT_EQ(runText(life, "day"), "2");
            EXPECT_EQ(life->walkPage.id, page);
            EXPECT_EQ(countUnder(f, walkPart(life, "walkMiddle")), middle);

            ActivityRow* again = list->find(&f.ecs, "tile.library");
            ASSERT_NE(again, nullptr);
            EXPECT_EQ(again->root.id, tile);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // A story's months stop with his life, and are written once, when the day is done

        // A month that ends his life where no ending can be shown begins the next one at once: the
        // story's other months are not the new boy's
        TEST(lifescene_test, a_story_stops_when_its_month_begins_another_life)
        {
            MockLogger logger;
            LifeFixture f;

            LifeSceneOptions opt = storyOptions();
            opt.endingFile = noEnding();

            LifeScene* life = storyLife(f, opt);
            ASSERT_NE(life, nullptr);

            openStory(f);
            ASSERT_TRUE(life->onWalk());

            const int lives = life->save.lives;

            life->save.stats["rations"] = 0;
            life->save.stats["coin"] = 0;
            life->save.stats["vit"] = 1;

            // Two months asked: the first ends him, the second is not run
            life->runStoryMonths(2);

            EXPECT_EQ(life->save.lives, lives + 1);
            EXPECT_FALSE(life->ended);
            EXPECT_FLOAT_EQ(life->save.age, 6.0f);
        }

        TEST(lifescene_test, a_story_day_that_begins_another_life_closes_the_story)
        {
            MockLogger logger;
            LifeFixture f;

            LifeSceneOptions opt = storyOptions();
            opt.endingFile = noEnding();

            LifeScene* life = storyLife(f, opt);
            ASSERT_NE(life, nullptr);

            openStory(f);
            ASSERT_TRUE(life->onWalk());

            const int lives = life->save.lives;

            life->save.stats["rations"] = 0;
            life->save.stats["coin"] = 0;
            life->save.stats["vit"] = 1;

            // Two months to the day: the first ends him, the second is not the new boy's
            life->save.walk.run["months"] = ElementType{8};

            takeTile(f, life, "library");

            EXPECT_EQ(life->save.lives, lives + 1);
            EXPECT_FALSE(life->onWalk());
            EXPECT_FALSE(life->walk.state.story());
            EXPECT_TRUE(life->walkPage.empty());
            EXPECT_TRUE(life->save.running.empty());
            EXPECT_FLOAT_EQ(life->save.age, 6.0f);
        }

        // The rations its board gives are in his bag as the story opens
        TEST(lifescene_test, a_story_opens_with_what_its_row_gave)
        {
            MockLogger logger;
            LifeFixture f;

            LifeScene* life = storyLife(f, storyOptions());
            ASSERT_NE(life, nullptr);

            const int before = life->save.stats["rations"];

            openStory(f);
            ASSERT_TRUE(life->onWalk());
            ASSERT_GT(life->save.stats["rations"], before);

            const ElementMap* rations = nullptr;

            for (const auto& row : life->walk.bag)
            {
                if (fieldText(row, "id") == "rations")
                    rations = &row;
            }

            ASSERT_NE(rations, nullptr);
            EXPECT_EQ(fieldText(*rations, "value"), std::to_string(life->save.stats["rations"]));
        }

        // An act whose day adds up to two months is written once, after both: a death in the
        // second is not lost to a save written after the first
        TEST(lifescene_test, a_story_day_of_two_months_is_written_once)
        {
            MockLogger logger;
            LifeFixture f;

            const std::string path = "save/test_life_story_months.sz";
            std::remove(path.c_str());

            LifeSceneOptions opt = storyOptions();
            opt.noSave = false;
            opt.savePath = path;

            LifeScene* life = storyLife(f, opt);
            ASSERT_NE(life, nullptr);

            openStory(f);
            ASSERT_TRUE(life->onWalk());

            // Eight months on four days: the first day's end runs two
            life->save.walk.run["months"] = ElementType{8};
            life->save.stats["rations"] = 0;
            life->save.stats["coin"] = 0;
            life->save.stats["vit"] = 6;
            life->save.stats["vitmax"] = 40;
            ASSERT_TRUE(life->saveNow());

            const float age = life->save.age;

            takeTile(f, life, "library");
            ASSERT_TRUE(life->ended);

            // Opened again: the day before its act, not a month into it
            f.load<EmptyScene>();
            opt.fresh = false;

            LifeScene* again = f.life(opt);
            ASSERT_NE(again, nullptr);
            f.frames(12);

            ASSERT_TRUE(again->onWalk());
            EXPECT_EQ(runText(again, "day"), "1");
            EXPECT_FLOAT_EQ(again->save.age, age);

            f.load<EmptyScene>();
            std::remove(path.c_str());
        }

        // A step of the guide that waits for a work to begin ends as the story begins
        TEST(lifescene_test, a_story_begun_ends_the_guide_step_waiting_for_it)
        {
            MockLogger logger;
            LifeFixture f;

            LifeScene* life = storyLife(f, storyOptions());
            ASSERT_NE(life, nullptr);

            GuideStep waiting;
            waiting.id = "test.story";
            waiting.until = {ElementMap{{"fact", ElementType{std::string("working.story.trial")}}, {"value", ElementType{1}}, {"op", ElementType{std::string("==")}}}};
            waiting.active = true;
            life->step = waiting;

            life->beginStory("story.trial");

            ASSERT_TRUE(life->onWalk());
            EXPECT_FALSE(life->step.active);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // The Collegium's term, lived (rules/storytable.pg's `collegium`): its sixteen days are its
        // 24 months. A youth fit to enter it, with the coin of its fees and a pupil of Mara's
        namespace
        {
            LifeScene* collegiumLife(LifeFixture& f, const LifeSceneOptions& opt)
            {
                LifeScene* life = f.life(opt);

                if (not life)
                    return nullptr;

                life->save.stats["int"] = 13;
                life->save.stats["letters"] = 3;
                life->save.stats["lore"] = 2;
                life->save.stats["mara_student"] = 1;
                life->save.stats["coin"] = 40;
                life->save.stats["rations"] = 12;
                life->save.stats["vit"] = 50;
                life->save.stats["vitmax"] = 50;

                life->onMonth();
                f.settle();

                return life;
            }

            bool collegiumScene(const std::string& event)
            {
                return event == "story.course" or event == "story.sent_away" or event == "story.theory" or event == "story.practical";
            }

            // One thing done in the term, as the page sends it; then a fight handed to his stance and
            // back from it, and the chance meetings that came answered by their first answer
            void termDo(LifeFixture& f, LifeScene* life, const std::string& kind, const std::string& target = "")
            {
                life->walkDo(WalkAct{kind, target, 0});
                f.settle();

                for (int guard = 0; guard < 60 and life->onWalk() and life->walkScreen == "fight"; ++guard)
                {
                    const bool over = life->save.walk.fight.count("over") > 0 and not life->save.walk.fight.at("over").toString().empty();

                    life->walkDo(WalkAct{over ? "back" : "step", "", 0});
                    f.settle();
                }

                for (int guard = 0; guard < 4 and life->onWalk() and siteText(life, "screen") == "event" and not collegiumScene(runText(life, "event")); ++guard)
                {
                    for (const auto& option : life->walk.options)
                    {
                        auto locked = option.find("locked");

                        if (locked == option.end() or locked->second.get<int>() == 0)
                        {
                            life->walkDo(WalkAct{"option", option.at("id").toString(), 0});
                            break;
                        }
                    }

                    f.settle();
                }
            }

            void termTiles(LifeFixture& f, LifeScene* life, const std::vector<std::string>& tiles)
            {
                for (const auto& tile : tiles)
                {
                    ASSERT_TRUE(life->onWalk()) << tile;
                    ASSERT_EQ(siteText(life, "screen"), "site") << tile << " " << runText(life, "event");

                    termDo(f, life, "tile." + tile);
                }
            }
        }

        // A pass makes him a scholar of the Collegium: the row's `enters` is his aim once the story
        // is passed (not at the term the last month brings, inside the scene), its works open and
        // its familiar slot is his
        TEST(lifescene_test, a_passed_term_makes_him_a_mage)
        {
            MockLogger logger;
            LifeFixture f;

            LifeScene* life = collegiumLife(f, storyOptions(16.5f));
            ASSERT_NE(life, nullptr);

            auto list = life->piece<ActivityList>("activities");
            ASSERT_NE(list, nullptr);
            ASSERT_NE(list->find(&f.ecs, "collegium"), nullptr);

            const float age = life->save.age;

            f.ecs.sendEvent(ActivityActivatedEvent{"life.activities", "collegium"});
            f.frames(12);

            ASSERT_TRUE(life->onWalk());
            EXPECT_EQ(runText(life, "story"), "collegium");
            EXPECT_EQ(life->save.running, "collegium");

            termTiles(f, life, {"lectures", "lectures", "lectures", "familiar", "familiar", "familiar"});

            ASSERT_EQ(runText(life, "event"), "story.course");
            termDo(f, life, "option", "fire");

            termTiles(f, life, {"fire", "fire", "fire", "fire", "fire", "binding", "practice", "library"});

            ASSERT_EQ(runText(life, "event"), "story.theory");
            termDo(f, life, "option", "taught");

            ASSERT_EQ(runText(life, "event"), "story.practical");
            termDo(f, life, "option", "face");
            f.frames(12);

            // Over, and passed: twenty-four months gone, the term counted, and he is a mage
            ASSERT_FALSE(life->onWalk());
            EXPECT_NEAR(life->save.age, age + 2.0f, 1e-3f);
            EXPECT_EQ(life->save.done["collegium"], 1);
            EXPECT_EQ(life->save.stats["collegium"], 1);
            EXPECT_EQ(life->save.stats["fire_course"], 1);
            EXPECT_EQ(life->save.stats.count("dropout.collegium"), 0u);
            EXPECT_EQ(life->save.aim, "mage");

            // Its works open, in their order: its bread first
            life->onMonth();
            f.settle();

            EXPECT_NE(list->find(&f.ecs, "study"), nullptr);
            EXPECT_EQ(list->find(&f.ecs, "collegium"), nullptr);

            // Its familiar slot: one bound, and a second may be
            life->save.stats["scavenged"] = 2;
            life->save.stats["berries"] = 12;
            life->save.stats["rations"] = 24;
            life->save.stats["fam.sprite"] = 1;
            life->onMonth();
            f.settle();

            EXPECT_NE(list->find(&f.ecs, "bind.sprite"), nullptr);
        }

        // A failed term is a drop-out's: the term comes with the last month as any row's does, but
        // the row's `enters` is not his aim, and the class is not his
        TEST(lifescene_test, a_failed_term_is_no_mage)
        {
            MockLogger logger;
            LifeFixture f;

            LifeScene* life = collegiumLife(f, storyOptions(16.5f));
            ASSERT_NE(life, nullptr);

            life->save.stats["int"] = 12;
            life->save.stats.erase("mara_student");

            const std::string aim = life->save.aim;

            life->beginStory("collegium");
            ASSERT_TRUE(life->onWalk());

            // The curriculum and no more: no day of revision, nothing he holds counts for him
            termTiles(f, life, {"lectures", "familiar", "familiar", "familiar", "familiar", "familiar"});

            for (int day = 7; day <= 14 and life->onWalk() and siteText(life, "screen") == "site"; ++day)
                termDo(f, life, day <= 13 ? "tile.scriptorium" : "tile.rest");

            ASSERT_EQ(runText(life, "event"), "story.theory");
            termDo(f, life, "option", "taught");

            ASSERT_EQ(runText(life, "event"), "story.practical");
            termDo(f, life, "option", "face");
            f.frames(12);

            ASSERT_FALSE(life->onWalk());
            EXPECT_EQ(life->save.done["collegium"], 1);
            EXPECT_EQ(life->save.stats["collegium"], 0);
            EXPECT_EQ(life->save.stats["dropout.collegium"], 1);
            EXPECT_EQ(life->save.aim, aim);
            EXPECT_NE(life->save.aim, "mage");
        }

        // A drop-out keeps what the days gave him (the fire course's Firebolt), has spent the months
        // of his days and no more, and the Collegium is shut to him
        TEST(lifescene_test, a_collegium_dropout_keeps_his_spell)
        {
            MockLogger logger;
            LifeFixture f;

            LifeScene* life = collegiumLife(f, storyOptions(16.5f));
            ASSERT_NE(life, nullptr);

            auto list = life->piece<ActivityList>("activities");
            ASSERT_NE(list, nullptr);

            const float age = life->save.age;

            life->beginStory("collegium");
            ASSERT_TRUE(life->onWalk());

            termTiles(f, life, {"lectures", "lectures", "library", "library", "library", "library"});
            ASSERT_EQ(runText(life, "event"), "story.course");
            termDo(f, life, "option", "fire");
            termTiles(f, life, {"fire", "fire", "fire"});

            ASSERT_EQ(runText(life, "day"), "10");
            EXPECT_EQ(life->save.stats["fire_course"], 1);

            // Nine days of sixteen: thirteen months of twenty-four, all run as the days went
            EXPECT_NEAR(life->save.age, age + 13.0f / 12.0f, 1e-3f);

            answerEvents(f, life);
            life->walkDo(WalkAct{"leave", "", 0});
            f.settle();
            ASSERT_TRUE(life->onWalk());
            life->walkDo(WalkAct{"leave", "", 0});
            f.frames(12);

            ASSERT_FALSE(life->onWalk());
            EXPECT_EQ(life->save.stats["fire_course"], 1);
            EXPECT_EQ(life->save.stats["collegium"], 0);
            EXPECT_EQ(life->save.stats["dropout.collegium"], 1);
            EXPECT_EQ(life->save.done.count("collegium"), 0u);
            EXPECT_TRUE(life->save.running.empty());
            EXPECT_NEAR(life->save.age, age + 13.0f / 12.0f, 1e-3f);

            // None after: the months stay where his leaving put them
            f.pass(5000.0f, 250.0f);
            EXPECT_NEAR(life->save.age, age + 13.0f / 12.0f, 1e-3f);

            life->onMonth();
            f.settle();
            EXPECT_EQ(list->find(&f.ecs, "collegium"), nullptr);

            // The Life page says what he kept, and that he left
            auto ledger = life->piece<ResourceLedger>("ledger");
            ASSERT_NE(ledger, nullptr);
            ASSERT_NE(ledger->row("fire_course"), nullptr);
            EXPECT_EQ(ledger->row("fire_course")->name.spec.text, "Firebolt");
            ASSERT_NE(ledger->row("dropout.collegium"), nullptr);
            EXPECT_EQ(ledger->row("dropout.collegium")->name.spec.text, "Left the Collegium");
        }

        // A drop-out of the Collegium is a man of no class: a door still open to him by his age and
        // what it asks is open, and the Keep can make a spell-warrior of him
        TEST(lifescene_test, a_dropout_can_swear_to_the_keep)
        {
            MockLogger logger;
            LifeFixture f;

            LifeScene* life = collegiumLife(f, storyOptions(16.75f));
            ASSERT_NE(life, nullptr);

            life->save.stats["str"] = 12;
            life->save.stats["arms"] = 3;
            life->save.stats["discipline"] = 2;

            life->beginStory("collegium");
            ASSERT_TRUE(life->onWalk());

            termTiles(f, life, {"lectures"});

            answerEvents(f, life);
            life->walkDo(WalkAct{"leave", "", 0});
            f.settle();
            life->walkDo(WalkAct{"leave", "", 0});
            f.frames(12);

            ASSERT_FALSE(life->onWalk());
            ASSERT_EQ(life->save.stats["dropout.collegium"], 1);

            life->onMonth();
            f.settle();
            EXPECT_GE(life->save.age, 17.0f);

            auto list = life->piece<ActivityList>("activities");
            ASSERT_NE(list, nullptr);
            ASSERT_NE(list->find(&f.ecs, "keep"), nullptr);

            f.ecs.sendEvent(ActivityActivatedEvent{"life.activities", "keep"});
            f.frames(12);

            EXPECT_EQ(life->save.running, "keep");
        }

        // A life that a month of the term ends is not written after its death: opened again, it is
        // the day before (and its month comes again)
        TEST(lifescene_test, a_life_ended_in_the_collegium_is_not_saved_dead)
        {
            MockLogger logger;
            LifeFixture f;

            const std::string path = "save/test_life_collegium_death.sz";
            std::remove(path.c_str());

            LifeSceneOptions opt = storyOptions(16.5f);
            opt.noSave = false;
            opt.savePath = path;

            LifeScene* life = collegiumLife(f, opt);
            ASSERT_NE(life, nullptr);

            life->beginStory("collegium");
            ASSERT_TRUE(life->onWalk());

            life->save.stats["rations"] = 0;
            life->save.stats["coin"] = 0;
            life->save.stats["vit"] = 1;
            ASSERT_TRUE(life->saveNow());

            const float age = life->save.age;

            termDo(f, life, "tile.library");
            ASSERT_TRUE(life->ended);

            LifeSave read;
            ASSERT_TRUE(read.load(path));

            EXPECT_TRUE(read.walk.under());
            EXPECT_EQ(read.walk.run.at("day").toString(), "1");
            EXPECT_FLOAT_EQ(read.age, age);
            EXPECT_EQ(read.running, "collegium");

            f.load<EmptyScene>();
            std::remove(path.c_str());
        }

        // A scholar of the Collegium written before its term was a story learned Firebolt with the
        // class: read again, he keeps it (the fire course is his). A save of today is left as it is
        TEST(lifescene_test, an_old_collegium_save_keeps_firebolt)
        {
            MockLogger logger;
            LifeFixture f;

            const std::string path = "save/test_life_old_collegium.sz";
            std::remove(path.c_str());

            LifeScene* life = f.life();
            ASSERT_NE(life, nullptr);

            EXPECT_TRUE(life->save.coursesTaught);

            LifeSave old = life->save;
            old.coursesTaught = false;
            old.stats["collegium"] = 1;

            ASSERT_TRUE(old.save(path));

            LifeSave read;
            ASSERT_TRUE(read.load(path));

            EXPECT_EQ(read.stats["fire_course"], 1);
            EXPECT_TRUE(read.coursesTaught);

            // One who followed another course keeps his own
            old.stats["frost_course"] = 1;
            ASSERT_TRUE(old.save(path));

            LifeSave frost;
            ASSERT_TRUE(frost.load(path));
            EXPECT_EQ(frost.stats.count("fire_course"), 0u);

            // A scholar of today who stayed on the basics is not given it
            LifeSave today = life->save;
            today.stats["collegium"] = 1;
            ASSERT_TRUE(today.save(path));

            LifeSave basic;
            ASSERT_TRUE(basic.load(path));
            EXPECT_EQ(basic.stats.count("fire_course"), 0u);

            // Nor one who was not of the Collegium
            old.stats.erase("collegium");
            old.stats.erase("frost_course");
            ASSERT_TRUE(old.save(path));

            LifeSave none;
            ASSERT_TRUE(none.load(path));
            EXPECT_EQ(none.stats.count("fire_course"), 0u);

            // One written in the middle of its term, a row of months then (its coin paid): its row
            // gives nothing at its term now, so what it gave is his as he is read, and its months
            // run out as they were
            old.running = "collegium";
            old.monthsIn = 10;
            old.stats["arcana"] = 0;
            old.stats["lore"] = 2;
            ASSERT_TRUE(old.save(path));

            LifeSave midTerm;
            ASSERT_TRUE(midTerm.load(path));
            EXPECT_EQ(midTerm.running, "collegium");
            EXPECT_EQ(midTerm.monthsIn, 10);
            EXPECT_EQ(midTerm.stats["collegium"], 1);
            EXPECT_EQ(midTerm.stats["arcana"], 1);
            EXPECT_EQ(midTerm.stats["lore"], 3);
            EXPECT_EQ(midTerm.stats["fire_course"], 1);

            // Written again and read again, it is not given twice
            ASSERT_TRUE(midTerm.save(path));

            LifeSave again;
            ASSERT_TRUE(again.load(path));
            EXPECT_EQ(again.stats["arcana"], 1);
            EXPECT_EQ(again.stats["lore"], 3);

            std::remove(path.c_str());
        }

        // An old save whose term a life event had set aside (it is taken up again as a row of
        // months, whose gains are nothing now): read again, the class and the fire course are his
        TEST(lifescene_test, an_old_save_with_the_collegium_set_aside_keeps_the_class)
        {
            MockLogger logger;
            LifeFixture f;

            const std::string path = "save/test_life_old_collegium_aside.sz";
            std::remove(path.c_str());

            LifeScene* life = f.life();
            ASSERT_NE(life, nullptr);

            LifeSave old = life->save;
            old.coursesTaught = false;
            old.running = "";
            old.setAside = "collegium";
            old.setAsideMonths = 10;
            old.stats.erase("collegium");
            old.stats["arcana"] = 0;
            old.stats["lore"] = 2;

            ASSERT_TRUE(old.save(path));

            LifeSave read;
            ASSERT_TRUE(read.load(path));

            EXPECT_EQ(read.setAside, "collegium");
            EXPECT_EQ(read.stats["collegium"], 1);
            EXPECT_EQ(read.stats["arcana"], 1);
            EXPECT_EQ(read.stats["lore"], 3);
            EXPECT_EQ(read.stats["fire_course"], 1);

            std::remove(path.c_str());
        }

        // What the term taught, and that he left it, are his rows on the Life page: the drop-out
        // who kept Firebolt sees it there before a fight
        TEST(lifescene_test, what_a_term_taught_is_on_the_life_page)
        {
            MockLogger logger;
            LifeFixture f;

            LifeScene* life = f.life();
            ASSERT_NE(life, nullptr);

            auto ledger = life->piece<ResourceLedger>("ledger");
            ASSERT_NE(ledger, nullptr);

            // As the term's days would leave them
            life->save.stats["rations"] = 24;

            const std::vector<std::pair<std::string, std::string>> rows = {
                {"fire_course", "Firebolt"}, {"frost_course", "Frostbolt"}, {"storm_course", "Stormbolt"},
                {"spark_taught", "Spark"}, {"rime_taught", "Rime"}, {"static_taught", "Static"},
                {"suspected", "Suspected of the forbidden shelf"}, {"top_floor", "Spoken of on the top floor"},
                {"dropout.collegium", "Left the Collegium"}};

            for (const auto& [stat, name] : rows)
            {
                EXPECT_EQ(ledger->row(stat), nullptr) << stat;
                life->save.stats[stat] = 1;
            }

            life->onMonth();
            f.settle();

            for (const auto& [stat, name] : rows)
            {
                ASSERT_NE(ledger->row(stat), nullptr) << stat;
                EXPECT_EQ(ledger->row(stat)->name.spec.text, name) << stat;
                EXPECT_EQ(ledger->row(stat)->figure.spec.text, "") << stat;
            }
        }

        // The row asks the coin of the first days, and takes none: shut with 5, begun with 6, and
        // the 6 still his on the first day
        TEST(lifescene_test, the_collegium_asks_the_coin_of_its_first_days)
        {
            MockLogger logger;
            LifeFixture f;

            LifeScene* life = collegiumLife(f, storyOptions(16.5f));
            ASSERT_NE(life, nullptr);

            life->save.stats["coin"] = 5;
            life->onMonth();
            f.settle();

            f.ecs.sendEvent(ActivityActivatedEvent{"life.activities", "collegium"});
            f.frames(12);
            EXPECT_FALSE(life->onWalk());

            life->save.stats["coin"] = 6;
            life->onMonth();
            f.settle();

            f.ecs.sendEvent(ActivityActivatedEvent{"life.activities", "collegium"});
            f.frames(12);

            ASSERT_TRUE(life->onWalk());
            EXPECT_EQ(runText(life, "story"), "collegium");
            EXPECT_EQ(life->save.stats["coin"], 6);
        }

        // The examinations' days offer nothing to choose: the day's work leaves the column (in
        // place, at either width), and its dead button and its notes with it
        TEST(lifescene_test, an_examination_day_has_no_days_work)
        {
            MockLogger logger;
            LifeFixture f;

            LifeScene* life = collegiumLife(f, storyOptions(16.5f));
            ASSERT_NE(life, nullptr);

            life->beginStory("collegium");
            ASSERT_TRUE(life->onWalk());

            termTiles(f, life, {"lectures", "lectures", "lectures", "familiar", "familiar", "familiar"});
            ASSERT_EQ(runText(life, "event"), "story.course");
            termDo(f, life, "option", "fire");
            termTiles(f, life, {"fire", "fire", "fire", "fire", "fire", "binding", "practice"});

            ASSERT_EQ(runText(life, "day"), "14");
            f.pass(300.0f, 50.0f);
            ASSERT_FALSE(walkPart(life, "workPanel").empty());
            EXPECT_TRUE(f.pos(walkPart(life, "workPanel"))->visible);

            const _unique_id page = life->walkPage.id;

            termTiles(f, life, {"library"});
            f.pass(300.0f, 50.0f);

            ASSERT_EQ(runText(life, "day"), "15");
            ASSERT_EQ(runText(life, "event"), "story.theory");
            EXPECT_EQ(life->walkPage.id, page);
            EXPECT_FALSE(f.pos(walkPart(life, "workPanel"))->visible);
            EXPECT_FALSE(f.pos(walkPart(life, "goneNote"))->visible);
            EXPECT_TRUE(f.pos(walkPart(life, "cardPanel"))->visible);

            // On a phone, and back
            f.resize(390.0f, 844.0f);
            f.pass(200.0f, 50.0f);
            f.frames(12);

            ASSERT_TRUE(life->walkNarrow);
            EXPECT_FALSE(f.pos(walkPart(life, "workPanel"))->visible);
            EXPECT_TRUE(f.pos(walkPart(life, "cardPanel"))->visible);

            f.resize(1320.0f, 1020.0f);
            f.pass(200.0f, 50.0f);
            f.frames(12);

            ASSERT_FALSE(life->walkNarrow);
            EXPECT_FALSE(f.pos(walkPart(life, "workPanel"))->visible);

            // The practical's day the same
            termDo(f, life, "option", "taught");
            f.pass(300.0f, 50.0f);

            ASSERT_EQ(runText(life, "event"), "story.practical");
            EXPECT_FALSE(f.pos(walkPart(life, "workPanel"))->visible);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // The Options page: a folder of its own for a test's settings and saves, empty
        static std::filesystem::path optionsScratch()
        {
            const auto* info = ::testing::UnitTest::GetInstance()->current_test_info();
            const auto folder = std::filesystem::temp_directory_path() / (std::string("chronicle-life-") + info->name());

            std::error_code ec;
            std::filesystem::remove_all(folder, ec);
            std::filesystem::create_directories(folder, ec);

            return folder;
        }

        // The Options tab takes the choice's place in the middle column, as the Town page does, and
        // says what the chronicle is; the Life tab gives the column back
        TEST(lifescene_test, options_tab_takes_the_column)
        {
            MockLogger logger;
            LifeFixture f;

            LifeScene* life = f.life();
            ASSERT_NE(life, nullptr);

            auto tabs = life->piece<Tabs>("tabs");
            ASSERT_NE(tabs, nullptr);
            ASSERT_EQ(life->optionsTab(), 3);
            EXPECT_TRUE(tabs->tabs[3].shown);
            EXPECT_FALSE(f.pos(life->named("options"))->isRenderable());

            f.ecs.sendEvent(TabSelectedEvent{tabs->root.id, "life.tabs", 3});
            f.frames(12);

            EXPECT_TRUE(life->optionsShown);
            EXPECT_EQ(tabs->active(), 3);
            EXPECT_TRUE(f.pos(life->named("options"))->isRenderable());
            EXPECT_FALSE(f.pos(life->named("activities"))->isRenderable());

            // What it says: who and how old, which life, never written (the mockup's), the file Export writes
            EXPECT_EQ(life->piece<Label>("chronicleWho")->spec.text, "Aldren of Bellmoor · 17 yr");
            EXPECT_EQ(life->piece<Label>("chronicleWhere")->spec.text, "Life 1 · Bellmoor · never written (--no-save)");
            EXPECT_EQ(life->piece<Label>("exportNote")->spec.text, "chronicle-aldren-life1.sz");
            EXPECT_EQ(life->piece<Panel>("burnPanel")->title->spec.color, "vermilion");
            EXPECT_EQ(life->piece<Label>("optionsFoot")->spec.text.rfind("Alpha ", 0), 0u);

            // No failure said: neither line under Export nor under Load
            EXPECT_FALSE(f.pos(life->named("exportFail"))->isRenderable());
            EXPECT_FALSE(f.pos(life->named("loadNote"))->isRenderable());

            // Each row as tall as what it holds, and its line as wide as its control leaves it
            auto panel = life->piece<Panel>("playPanel");
            ASSERT_NE(panel, nullptr);
            EXPECT_NEAR(f.pos(life->named("clickLine"))->width + f.pos(life->named("clickSwitch"))->width + 16.0f, panel->innerWidth(), 0.5f);
            EXPECT_GE(f.pos(life->named("clickRow"))->height, f.pos(life->named("clickLine"))->height - 0.5f);
            EXPECT_GE(f.pos(life->named("clickRow"))->height, 44.0f);

            // The Life tab: the choice back, the page out of the Tab order
            f.ecs.sendEvent(TabSelectedEvent{tabs->root.id, "life.tabs", 0});
            f.frames(12);

            EXPECT_FALSE(life->optionsShown);
            EXPECT_TRUE(f.pos(life->named("activities"))->isRenderable());
            EXPECT_FALSE(f.pos(life->named("options"))->isRenderable());

            f.load<EmptyScene>();
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // Begin on first click: off, a click on a work chooses it; on, the click begins it. The
        // setting is written at once and read by the next scene
        TEST(lifescene_test, options_begin_on_first_click)
        {
            MockLogger logger;
            LifeFixture f;

            const auto folder = optionsScratch();

            // The settings are kept in the save's file: a life that is written
            LifeSceneOptions opt = LifeFixture::first();
            opt.noSave = false;
            opt.savePath = (folder / "life.sz").string();

            LifeScene* life = f.life(opt);
            ASSERT_NE(life, nullptr);
            EXPECT_FALSE(life->settings.beginOnFirstClick);
            EXPECT_EQ(life->piece<Label>("guide")->spec.text, "Choose a work, then press Begin");

            auto list = life->piece<ActivityList>("activities");
            ASSERT_NE(list, nullptr);
            ASSERT_NE(list->find(&f.ecs, "glean"), nullptr);

            auto click = [&](const char* id) {
                auto box = f.pos(list->find(&f.ecs, id)->root);
                const Point2D at{box->x + box->width / 2.0f, box->y + box->height / 2.0f};

                f.ecs.sendEvent(OnMouseMove{at, nullptr});
                f.frames(4);
                f.ecs.sendEvent(OnMouseClick{at, static_cast<MouseButton>(1)});
                f.frames(4);
                f.ecs.sendEvent(OnMouseRelease{at, static_cast<MouseButton>(1)});
                f.frames(12);
            };

            // Off: chosen, not begun
            click("glean");
            EXPECT_EQ(life->chosen, "glean");
            EXPECT_TRUE(life->save.running.empty());

            // The switch turned on: written, and the list begins on the click
            f.ecs.sendEvent(SwitchToggledEvent{0, "options.click", true});
            f.settle();

            EXPECT_TRUE(life->settings.beginOnFirstClick);
            EXPECT_EQ(f.ecs.getSystem<ActivitySystem>()->beginOnFirstClick.count("life.activities"), 1u);
            EXPECT_EQ(life->piece<Label>("guide")->spec.text, "Click a work to begin it");

            Settings written;
            ASSERT_TRUE(written.load(opt.savePath));
            EXPECT_TRUE(written.beginOnFirstClick);

            // A long wait: no double click about it
            f.pass(1000.0f);
            ASSERT_TRUE(life->save.running.empty());

            click("glean");
            EXPECT_EQ(life->save.running, "glean");

            // The next scene reads it
            f.load<EmptyScene>();

            LifeScene* next = f.life(opt);
            ASSERT_NE(next, nullptr);
            EXPECT_TRUE(next->settings.beginOnFirstClick);

            f.load<EmptyScene>();
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // The light of the page: the card chosen lights every screen at once and is written; the T
        // key goes the same way
        TEST(lifescene_test, options_theme_is_lit_and_kept)
        {
            MockLogger logger;
            LifeFixture f;

            const auto folder = optionsScratch();

            LifeSceneOptions opt = LifeFixture::mockup();
            opt.noSave = false;
            opt.savePath = (folder / "life.sz").string();

            LifeScene* life = f.life(opt);
            ASSERT_NE(life, nullptr);
            ASSERT_EQ(f.theme->currentTheme(), "day");

            f.ecs.sendEvent(RadioChosenEvent{0, "options.light", "candle"});
            f.frames(6);

            EXPECT_EQ(f.theme->currentTheme(), "candle");
            EXPECT_EQ(life->piece<RadioCards>("lightCards")->selected(), "candle");

            Settings written;
            ASSERT_TRUE(written.load(opt.savePath));
            EXPECT_EQ(written.theme, "candle");

            // The page's ground is the candle's vellum now
            const auto ground = life->named("page")->get<Simple2DObject>()->colors;
            const auto candle = f.theme->theme().color("vellum", "candle");
            EXPECT_FLOAT_EQ(ground.x, candle.x);
            EXPECT_FLOAT_EQ(ground.z, candle.z);

            f.ecs.sendEvent(OnSDLScanCode{SDL_SCANCODE_T, 0});
            f.frames(6);

            EXPECT_EQ(f.theme->currentTheme(), "day");
            EXPECT_EQ(life->piece<RadioCards>("lightCards")->selected(), "day");
            ASSERT_TRUE(written.load(opt.savePath));
            EXPECT_EQ(written.theme, "day");

            f.load<EmptyScene>();
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // Wipe: the button opens a dialog and nothing more. It says what is lost, keeps the keys to
        // itself, and its button stays greyed until BURN is typed. Burnt: the save and its copy are
        // gone, a first life begins at six, and the settings stay
        TEST(lifescene_test, options_wipe_asks_for_the_word)
        {
            MockLogger logger;
            LifeFixture f;

            const auto folder = optionsScratch();

            LifeSceneOptions opt = LifeFixture::mockup();
            opt.noSave = false;
            opt.savePath = (folder / "life.sz").string();

            // A file with the settings alone, as a burnt chronicle leaves it: the mockup's life is opened
            Settings chosen;
            chosen.beginOnFirstClick = true;
            ASSERT_TRUE(chosen.save(opt.savePath));

            LifeScene* life = f.life(opt);
            ASSERT_NE(life, nullptr);

            ASSERT_TRUE(life->saveNow());
            { std::ofstream bak(opt.savePath + ".bak"); bak << "the last one"; }

            const int lives = life->save.lives;

            f.ecs.sendEvent(ButtonActivatedEvent{0, "options.wipe"});
            f.frames(12);

            ASSERT_FALSE(life->dialog.empty());
            EXPECT_EQ(life->dialogKind, "burn");
            EXPECT_TRUE(std::filesystem::exists(opt.savePath));

            auto find = [&](const char* name) { return life->dialog->get<Prefab>()->findEntity(name); };

            EXPECT_EQ(find("lostLivesFigure")->get<Label>()->spec.text, "− 1");
            EXPECT_EQ(find("lostTownFigure")->get<Label>()->spec.text, "− 0 buildings");
            EXPECT_EQ(find("burnGloss")->get<Label>()->spec.text, "One life, a town, and everything the Guild had learned — gone to ash.");

            auto confirm = find("burnConfirm")->get<Button>();
            EXPECT_TRUE(confirm->face->get<ButtonState>()->disabled);

            // Pressed anyway (a key that reached it): nothing
            f.ecs.sendEvent(ButtonActivatedEvent{0, "options.burn"});
            f.frames(12);
            ASSERT_EQ(f.scenes->systemScene, life);
            EXPECT_TRUE(std::filesystem::exists(opt.savePath));

            // The field has the keys: a letter the game knows (N, a new life) is a letter
            f.ecs.sendEvent(OnSDLScanCode{SDL_SCANCODE_N, 0});
            f.frames(4);
            EXPECT_EQ(life->save.lives, lives);

            // Tab goes round the dialog alone: the field, Keep, and not the greyed button
            auto order = f.ecs.getSystem<FocusOrderSystem>();
            EXPECT_EQ(order->current(), find("burnWord")->get<WordField>()->face.id);
            f.ecs.sendEvent(OnSDLScanCode{SDL_SCANCODE_TAB, 0});
            f.frames(2);
            EXPECT_EQ(order->current(), find("burnKeep")->get<Button>()->face.id);
            f.ecs.sendEvent(OnSDLScanCode{SDL_SCANCODE_TAB, 0});
            f.frames(2);
            EXPECT_EQ(order->current(), find("burnWord")->get<WordField>()->face.id);

            // In any case
            for (const char* letter : {"b", "U", "r", "N"})
                f.ecs.sendEvent(OnSDLTextInput{letter});
            f.frames(6);

            EXPECT_FALSE(confirm->face->get<ButtonState>()->disabled);

            f.ecs.sendEvent(ButtonActivatedEvent{0, "options.burn"});
            f.frames(24);

            auto after = dynamic_cast<LifeScene*>(f.scenes->systemScene);
            ASSERT_NE(after, nullptr);
            ASSERT_NE(after, life);

            // The old life and its copy are gone; the file keeps the settings, and a life there now is
            // the new one's own, written as it began
            EXPECT_FALSE(std::filesystem::exists(opt.savePath + ".bak"));

            Settings kept;
            ASSERT_TRUE(kept.load(opt.savePath));
            EXPECT_TRUE(kept.beginOnFirstClick);

            if (LifeSave now; now.load(opt.savePath))
            {
                EXPECT_EQ(now.lives, 1);
                EXPECT_EQ(now.termsDone(), 0);
                EXPECT_NE(now.profession, "Sworn man of the Keep at Bellmoor");
            }

            EXPECT_EQ(after->save.lives, 1);
            EXPECT_FLOAT_EQ(after->save.age, 6.0f);
            EXPECT_EQ(after->save.termsDone(), 0);
            EXPECT_TRUE(after->dialog.empty());
            EXPECT_TRUE(after->settings.beginOnFirstClick);

            // The last life's facts went with it: the mockup's oath to the Keep is not this boy's
            EXPECT_FALSE(f.facts->hasFact("stat.keep_oath"));

            f.load<EmptyScene>();
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // Escape, or Keep the chronicle, closes the dialog: nothing lost
        TEST(lifescene_test, options_wipe_kept)
        {
            MockLogger logger;
            LifeFixture f;

            LifeScene* life = f.life();
            ASSERT_NE(life, nullptr);

            f.ecs.sendEvent(ButtonActivatedEvent{0, "options.wipe"});
            f.frames(12);
            ASSERT_FALSE(life->dialog.empty());

            f.ecs.sendEvent(OnSDLScanCode{SDL_SCANCODE_ESCAPE, 0});
            f.frames(6);
            EXPECT_TRUE(life->dialog.empty());
            EXPECT_TRUE(life->dialogFaces.empty());

            f.ecs.sendEvent(ButtonActivatedEvent{0, "options.wipe"});
            f.frames(12);
            ASSERT_FALSE(life->dialog.empty());

            f.ecs.sendEvent(ButtonActivatedEvent{0, "options.keep"});
            f.frames(6);
            EXPECT_TRUE(life->dialog.empty());

            EXPECT_EQ(f.scenes->systemScene, life);
            EXPECT_EQ(life->save.name, "Aldren of Bellmoor");

            f.load<EmptyScene>();
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // Load: a file that is no save, or a newer one, is refused with a line and nothing touched.
        // A save, over a game under way, is asked about first; Keep keeps this one, Replace takes
        // the file's: written as the save, the last one kept as .bak, and the scene made again on it
        TEST(lifescene_test, options_load_checks_then_replaces)
        {
            MockLogger logger;
            LifeFixture f;

            const auto folder = optionsScratch();

            LifeSceneOptions opt = LifeFixture::mockup();
            opt.noSave = false;
            opt.savePath = (folder / "life.sz").string();

            LifeScene* life = f.life(opt);
            ASSERT_NE(life, nullptr);

            // His settings, written with the life
            life->settings.theme = "candle";
            life->writeSettings();
            ASSERT_TRUE(life->saveNow());

            life->showOptions(true);
            f.frames(6);

            // Not a save
            const auto foreign = folder / "foreign.sz";
            { std::ofstream file(foreign); file << "nothing of a chronicle here\n"; }

            life->takeSave(foreign.string());
            f.frames(6);

            EXPECT_EQ(life->loadFailed, "Not a Chronicle save.");
            EXPECT_TRUE(life->dialog.empty());
            EXPECT_TRUE(f.pos(life->named("loadNote"))->isRenderable());
            EXPECT_EQ(life->named("loadNote")->get<MarkedLabel>()->label.spec.text, "Not a Chronicle save.");
            EXPECT_EQ(f.scenes->systemScene, life);

            // Another life's save
            LifeSave other = boyOfSeven();
            other.name = "Wenna of the Mill";
            other.lives = 3;
            const auto otherPath = folder / "other.sz";
            ASSERT_TRUE(other.save(otherPath.string()));

            life->takeSave(otherPath.string());
            f.frames(12);

            // A game under way: asked first, and kept
            ASSERT_EQ(life->dialogKind, "replace");
            EXPECT_TRUE(life->loadFailed.empty());

            f.ecs.sendEvent(ButtonActivatedEvent{0, "options.keepSave"});
            f.frames(6);

            EXPECT_TRUE(life->dialog.empty());
            EXPECT_EQ(f.scenes->systemScene, life);
            EXPECT_EQ(life->save.name, "Aldren of Bellmoor");

            // Asked again, and replaced
            life->takeSave(otherPath.string());
            f.frames(12);
            ASSERT_EQ(life->dialogKind, "replace");

            f.ecs.sendEvent(ButtonActivatedEvent{0, "options.replace"});
            f.frames(24);

            auto after = dynamic_cast<LifeScene*>(f.scenes->systemScene);
            ASSERT_NE(after, nullptr);
            ASSERT_NE(after, life);

            EXPECT_EQ(after->save.name, "Wenna of the Mill");
            EXPECT_EQ(after->save.lives, 3);
            EXPECT_FALSE(after->optionsShown);

            LifeSave written;
            ASSERT_TRUE(written.load(opt.savePath));
            EXPECT_EQ(written.name, "Wenna of the Mill");

            LifeSave kept;
            ASSERT_TRUE(kept.load(opt.savePath + ".bak"));
            EXPECT_EQ(kept.name, "Aldren of Bellmoor");

            // The settings are not the file's: they stayed in the save, and with the new scene
            Settings settings;
            ASSERT_TRUE(settings.load(opt.savePath));
            EXPECT_EQ(settings.theme, "candle");
            EXPECT_EQ(after->settings.theme, "candle");

            f.load<EmptyScene>();
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // A chronicle not begun is replaced without a question
        TEST(lifescene_test, options_load_over_nothing_asks_nothing)
        {
            MockLogger logger;
            LifeFixture f;

            const auto folder = optionsScratch();

            LifeSceneOptions opt = LifeFixture::first();
            opt.noSave = false;
            opt.savePath = (folder / "life.sz").string();

            LifeScene* life = f.life(opt);
            ASSERT_NE(life, nullptr);
            ASSERT_TRUE(life->untouched());

            LifeSave other = boyOfSeven();
            other.name = "Wenna of the Mill";
            const auto otherPath = folder / "other.sz";
            ASSERT_TRUE(other.save(otherPath.string()));

            life->takeSave(otherPath.string());
            f.frames(24);

            auto after = dynamic_cast<LifeScene*>(f.scenes->systemScene);
            ASSERT_NE(after, nullptr);
            EXPECT_EQ(after->save.name, "Wenna of the Mill");
            EXPECT_TRUE(after->dialog.empty());

            f.load<EmptyScene>();
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // The bug report (lifereport.cpp): a button in the window's corner over every screen, the
        // screen caught before the dialog opens, the keys the dialog's while it is up, the limit
        namespace
        {
            constexpr float GuideVeilZ = 110.0f;   // The guide's shade (lifescene.cpp, VeilZ)
            constexpr float TooltipZ = 150.0f;     // Where the engine's TooltipSystem puts a gloss (TOOLTIP_Z)
            constexpr const char * const ReportTag = "life.report";

            // The buttons that open the report, wherever they are
            int reportButtons(LifeFixture& f)
            {
                int count = 0;
                const auto& entities = f.ecs.view();

                for (size_t i = 1; i <= f.ecs.getNbEntities(); ++i)
                {
                    auto entity = entities[i];

                    if (entity and entity->has<ButtonState>() and entity->get<ButtonState>()->tag == ReportTag)
                        ++count;
                }

                return count;
            }

            // Where the text of a label aligned right begins, as it is drawn
            float drawnLeft(LifeFixture& f, EntityRef label)
            {
                auto l = label->get<Label>();
                const TextStyle& style = f.theme->style(l->spec.style);

                TextLayoutParams params;
                params.spacing = style.lineSpacingPx;
                params.letterSpacing = style.letterSpacingPx;

                const float drawn = f.ecs.getSystem<TTFTextSystem>()->measureText(style.fontAlias, l->spec.text, params).width;
                auto pos = f.pos(label);

                return pos->x + pos->width - drawn;
            }

            // The big figure at the head's right, on the screen in view: the walk's, or the page's
            EntityRef headFigure(LifeScene* life)
            {
                if (not life->walkPage.empty() and life->walkPage->has<Prefab>())
                {
                    for (const char* name : {"daysLeft", "turn"})
                    {
                        if (EntityRef e = life->walkPage->get<Prefab>()->findEntity(name); not e.empty() and e->has<Label>())
                            return e;
                    }

                    return EntityRef{};
                }

                for (const char* name : {"date", "age"})
                {
                    if (EntityRef e = life->named(name); not e.empty() and e->has<Label>())
                        return e;
                }

                return EntityRef{};
            }

            // The button is there, drawn, in the window's top right corner, over the guide's shade
            // and under the glosses, and beside the head's figure, not over it
            void expectReportButton(LifeFixture& f, LifeScene* life, const std::string& where)
            {
                SCOPED_TRACE(where);

                ASSERT_FALSE(life->reportButton.root.empty());
                EXPECT_EQ(reportButtons(f), 1);

                auto face = f.pos(life->reportButton.face);
                EXPECT_TRUE(face->isRenderable());

                // Its corner: inside the window, at its top right
                EXPECT_GE(face->x, life->windowWidth / 2.0f);
                EXPECT_LE(face->x + face->width, life->windowWidth + 0.5f);
                EXPECT_GE(face->y, 0.0f);
                EXPECT_LE(face->y + face->height, 60.0f);

                // Every part of it in the band between the shade and the glosses
                auto state = life->reportButton.face->get<ButtonState>();
                std::vector<_unique_id> parts = {life->reportButton.face.id, state->ground, state->frame};
                parts.insert(parts.end(), state->inked.begin(), state->inked.end());

                for (auto id : parts)
                {
                    auto part = f.ecs.getEntity(id);

                    if (not part)
                        continue;

                    EXPECT_GT(part->get<PositionComponent>()->z, GuideVeilZ) << id;
                    EXPECT_LT(part->get<PositionComponent>()->z, TooltipZ) << id;
                }

                // The head's figure is read whole
                if (EntityRef figure = headFigure(life); not figure.empty() and f.pos(figure)->isRenderable())
                {
                    EXPECT_LE(face->x + face->width, drawnLeft(f, figure) + 0.5f) << "the button covers " << figure->get<Label>()->spec.text;
                }

                // And the head's lines at its left end before it, as they are drawn
                for (const char* name : {"title", "kicker", "zoneTitle", "fightTitle"})
                {
                    EntityRef line = life->walkPage.empty() ? life->named(name) : (life->walkPage->has<Prefab>() ? life->walkPage->get<Prefab>()->findEntity(name) : EntityRef{});

                    if (line.empty() or not line->has<Label>() or not line->has<TTFText>() or not f.pos(line)->isRenderable())
                        continue;

                    auto box = f.pos(line);
                    const float drawnRight = box->x + std::min(box->width, line->get<TTFText>()->textWidth);

                    EXPECT_LE(drawnRight, face->x + 0.5f) << "the button covers the end of " << name << ": " << line->get<Label>()->spec.text;
                }

                // It says what it is
                ASSERT_TRUE(life->reportButton.face->has<TooltipComponent>());
                auto glosses = f.ecs.getSystem<GlossRegistry>();
                const GlossSpec* gloss = glosses->find(life->reportButton.face->get<TooltipComponent>()->text);
                ASSERT_NE(gloss, nullptr);
                EXPECT_EQ(gloss->title, "Report a bug");
            }

            // The report's folder: the save beside it, as in the game
            LifeSceneOptions reportOptions(const std::filesystem::path& folder)
            {
                LifeSceneOptions opt = LifeFixture::mockup();
                opt.noSave = false;
                opt.savePath = (folder / "life.sz").string();
                opt.now = 1800000000ull;

                return opt;
            }

            EntityRef inReport(LifeScene* life, const char* name)
            {
                if (life->dialog.empty() or not life->dialog->has<Prefab>())
                    return EntityRef{};

                return life->dialog->get<Prefab>()->findEntity(name);
            }

            // The frames the renderer is asked for, and whether the dialog was up at each
            struct FrameAsked : public System<Listener<SaveCurrentFrameEvent>>
            {
                LifeScene* life = nullptr;
                int asked = 0;
                int dialogUp = 0;

                void onEvent(const SaveCurrentFrameEvent&) override
                {
                    ++asked;

                    if (life and (life->reportOpen or not life->dialog.empty()))
                        ++dialogUp;
                }
            };

            std::string fileText(const std::string& path)
            {
                std::ifstream file(path, std::ios::binary);
                std::stringstream text;
                text << file.rdbuf();

                return text.str();
            }
        }

        TEST(lifescene_test, a_report_button_stands_on_every_screen)
        {
            MockLogger logger;
            LifeFixture f;

            // The Life page, then the Town's, the Adventure's and the Options'
            {
                LifeScene* life = f.life();
                ASSERT_NE(life, nullptr);
                expectReportButton(f, life, "life");

                life->showTown(true);
                f.frames(12);
                expectReportButton(f, life, "town");

                life->showAdventure(true);
                f.frames(12);
                expectReportButton(f, life, "adventure");

                life->showOptions(true);
                f.frames(12);
                expectReportButton(f, life, "options");

                // Under the guide's shade's band no more: a dialog up hides it, and it comes back
                f.ecs.sendEvent(ButtonActivatedEvent{0, "options.wipe"});
                f.frames(12);
                ASSERT_EQ(life->dialogKind, "burn");
                EXPECT_FALSE(f.pos(life->reportButton.face)->isRenderable());

                f.ecs.sendEvent(OnSDLScanCode{SDL_SCANCODE_ESCAPE, 0});
                f.frames(6);
                expectReportButton(f, life, "after the wipe's dialog");

                f.load<EmptyScene>();
                EXPECT_EQ(reportButtons(f), 0);
            }

            // A walk, then a fight
            {
                LifeSceneOptions opt = LifeFixture::mockup();
                opt.build = "street";
                opt.walk = "hollow";
                opt.walkSeed = 11;

                LifeScene* life = f.life(opt);
                ASSERT_NE(life, nullptr);
                f.frames(12);
                ASSERT_TRUE(life->onWalk());
                answerEvents(f, life);
                f.frames(12);
                expectReportButton(f, life, "walk");

                f.ecs.sendEvent(ActivitySelectedEvent{"walk.seeks", "boar"});
                f.settle();
                f.ecs.sendEvent(ButtonActivatedEvent{0, "walk.seek"});
                f.frames(12);
                answerEvents(f, life);
                f.frames(12);
                ASSERT_EQ(life->walkScreen, "fight");
                expectReportButton(f, life, "fight");

                f.load<EmptyScene>();
            }

            // A story
            {
                LifeScene* life = storyLife(f, storyOptions());
                ASSERT_NE(life, nullptr);
                openStory(f);
                ASSERT_TRUE(life->save.walk.story());
                expectReportButton(f, life, "story");

                f.load<EmptyScene>();
            }

            // The ending
            {
                LifeSceneOptions opt = LifeFixture::mockup();
                opt.fresh = true;
                opt.lives = 2;
                opt.age = 7.0f;

                LifeScene* life = f.life(opt);
                ASSERT_NE(life, nullptr);

                life->save.stats["rations"] = 0;
                life->save.stats["vit"] = 1;
                life->onMonth();
                f.settle();
                f.frames(12);
                ASSERT_TRUE(life->ended);
                expectReportButton(f, life, "ending");

                f.load<EmptyScene>();
            }

            // A phone: the Life page and a walk, and the window going from one to the other
            f.resize(390.0f, 844.0f);
            {
                LifeScene* life = f.life();
                ASSERT_NE(life, nullptr);
                f.frames(12);
                ASSERT_TRUE(life->phone);
                expectReportButton(f, life, "phone life");

                f.resize(1600.0f, 1000.0f);
                expectReportButton(f, life, "life at 1600");

                f.resize(390.0f, 844.0f);
                expectReportButton(f, life, "life back on a phone");

                f.load<EmptyScene>();
            }
            {
                LifeSceneOptions opt = LifeFixture::mockup();
                opt.build = "street";
                opt.walk = "hollow";
                opt.walkSeed = 11;

                LifeScene* life = f.life(opt);
                ASSERT_NE(life, nullptr);
                f.frames(12);
                ASSERT_TRUE(life->onWalk());
                answerEvents(f, life);
                f.frames(12);
                expectReportButton(f, life, "phone walk");

                f.ecs.sendEvent(ActivitySelectedEvent{"walk.seeks", "boar"});
                f.settle();
                f.ecs.sendEvent(ButtonActivatedEvent{0, "walk.seek"});
                f.frames(12);
                answerEvents(f, life);
                f.frames(12);
                ASSERT_EQ(life->walkScreen, "fight");
                expectReportButton(f, life, "phone fight");

                f.load<EmptyScene>();
            }
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // Load and Burn make the scene again: the button comes back with it, once, and nothing the
        // last one asked for (a frame, a send) answers into it
        TEST(lifescene_test, the_report_button_survives_a_remade_scene)
        {
            MockLogger logger;
            LifeFixture f;
            auto grab = f.ecs.createSystem<FrameGrab>();

            const auto folder = optionsScratch();
            LifeSceneOptions opt = reportOptions(folder);

            LifeScene* life = f.life(opt);
            ASSERT_NE(life, nullptr);
            ASSERT_TRUE(life->saveNow());
            EXPECT_EQ(reportButtons(f), 1);

            // Pressed: the frame is asked for, and the chronicle burnt before it comes
            f.ecs.sendEvent(ButtonActivatedEvent{0, ReportTag});
            f.frames(4);
            ASSERT_TRUE(grab->waiting());

            life->wipe();
            f.frames(24);

            auto burnt = dynamic_cast<LifeScene*>(f.scenes->systemScene);
            ASSERT_NE(burnt, nullptr);
            ASSERT_NE(burnt, life);
            EXPECT_FALSE(grab->waiting());
            EXPECT_FALSE(burnt->reportOpen);
            expectReportButton(f, burnt, "burnt");

            // A save loaded in its place
            LifeSave other = boyOfSeven();
            other.name = "Wenna of the Mill";
            const auto otherPath = folder / "other.sz";
            ASSERT_TRUE(other.save(otherPath.string()));

            burnt->takeSave(otherPath.string());
            f.frames(24);

            auto loaded = dynamic_cast<LifeScene*>(f.scenes->systemScene);
            ASSERT_NE(loaded, nullptr);
            EXPECT_EQ(loaded->save.name, "Wenna of the Mill");
            expectReportButton(f, loaded, "loaded");

            f.load<EmptyScene>();
            EXPECT_EQ(reportButtons(f), 0);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // The picture is the screen he saw: the frame is asked for before the dialog is up, and the
        // dialog comes with it. A frame that never comes does not keep the dialog away
        TEST(lifescene_test, the_report_opens_after_the_screen_is_caught)
        {
            MockLogger logger;
            LifeFixture f;
            auto grab = f.ecs.createSystem<FrameGrab>();
            auto asked = f.ecs.createSystem<FrameAsked>();

            LifeScene* life = f.life();
            ASSERT_NE(life, nullptr);
            asked->life = life;

            f.ecs.sendEvent(ButtonActivatedEvent{0, ReportTag});
            f.frames(4);

            EXPECT_EQ(asked->asked, 1);
            EXPECT_EQ(asked->dialogUp, 0);
            EXPECT_TRUE(grab->waiting());
            EXPECT_TRUE(life->dialog.empty());
            EXPECT_FALSE(life->reportOpen);

            // Pressed again meanwhile: still the one frame
            f.ecs.sendEvent(ButtonActivatedEvent{0, ReportTag});
            f.frames(4);
            EXPECT_EQ(asked->asked, 1);

            std::vector<unsigned char> pixels(8 * 4 * 4, 128);
            f.ecs.sendEvent(SavedFrameData{pixels, 8, 4});
            f.frames(6);

            EXPECT_FALSE(grab->waiting());
            ASSERT_TRUE(life->reportOpen);
            EXPECT_EQ(life->dialogKind, "report");
            EXPECT_EQ(life->reportScreen.rfind("data:image/jpeg;base64,", 0), 0u);

            // Its words are typed at once, and say what is sent with them
            ASSERT_FALSE(inReport(life, "reportWords").empty());
            EXPECT_TRUE(f.ecs.getSystem<WordFieldSystem>()->anyTyping());
            EXPECT_EQ(inReport(life, "reportTitle")->get<Label>()->spec.text, "Report a bug");

            f.ecs.sendEvent(ButtonActivatedEvent{0, "report.cancel"});
            f.frames(6);
            EXPECT_FALSE(life->reportOpen);
            EXPECT_TRUE(life->dialog.empty());

            // No frame comes: after half a second the dialog opens without its picture
            f.ecs.sendEvent(ButtonActivatedEvent{0, ReportTag});
            f.frames(4);
            EXPECT_EQ(asked->asked, 2);
            EXPECT_FALSE(life->reportOpen);

            f.pass(250.0f, 50.0f);
            EXPECT_FALSE(life->reportOpen);

            f.pass(400.0f, 50.0f);
            ASSERT_TRUE(life->reportOpen);
            EXPECT_TRUE(life->reportScreen.empty());
            EXPECT_FALSE(grab->waiting());

            // And the frame that comes late is no one's
            f.ecs.sendEvent(SavedFrameData{pixels, 8, 4});
            f.frames(6);
            EXPECT_TRUE(life->reportScreen.empty());

            f.load<EmptyScene>();
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // What is typed in the dialog is his words, never the game's keys: no new life, no tab, no
        // month. Escape closes it, and a key of the same moment is not played either
        TEST(lifescene_test, keys_in_the_report_dialog_do_not_play)
        {
            MockLogger logger;
            LifeFixture f;

            LifeScene* life = f.life();
            ASSERT_NE(life, nullptr);

            const int lives = life->save.lives;
            const std::string name = life->save.name;
            const float age = life->save.age;
            auto tabs = life->piece<Tabs>("tabs");
            ASSERT_NE(tabs, nullptr);
            const int tab = tabs->active();

            auto unplayed = [&](const char* when) {
                SCOPED_TRACE(when);
                EXPECT_EQ(life->save.lives, lives);
                EXPECT_EQ(life->save.name, name);
                EXPECT_FLOAT_EQ(life->save.age, age);
                EXPECT_EQ(tabs->active(), tab);
                EXPECT_FALSE(life->townShown);
                EXPECT_FALSE(life->optionsShown);
                EXPECT_FALSE(life->adventureShown);
                EXPECT_EQ(f.theme->currentTheme(), "day");
            };

            // No frame grab in this game: the dialog opens at once
            f.ecs.sendEvent(ButtonActivatedEvent{0, ReportTag});
            f.frames(6);
            ASSERT_TRUE(life->reportOpen);

            for (auto key : {SDL_SCANCODE_N, SDL_SCANCODE_M, SDL_SCANCODE_T, SDL_SCANCODE_S, SDL_SCANCODE_SPACE, SDL_SCANCODE_1, SDL_SCANCODE_2, SDL_SCANCODE_3, SDL_SCANCODE_4, SDL_SCANCODE_LEFT, SDL_SCANCODE_RIGHT, SDL_SCANCODE_RETURN})
                f.ecs.sendEvent(OnSDLScanCode{key, 0});
            f.frames(6);

            unplayed("typed while open");
            EXPECT_TRUE(life->reportOpen);

            // The letters are his words
            for (const char* letter : {"N", "o", " ", "1"})
                f.ecs.sendEvent(OnSDLTextInput{letter});
            f.frames(6);
            EXPECT_EQ(inReport(life, "reportWords")->get<WordField>()->text(&f.ecs), "No 1");

            // Escape, and N in the same moment
            f.ecs.sendEvent(OnSDLScanCode{SDL_SCANCODE_ESCAPE, 0});
            f.ecs.sendEvent(OnSDLScanCode{SDL_SCANCODE_N, 0});
            f.frames(6);

            EXPECT_FALSE(life->reportOpen);
            EXPECT_TRUE(life->dialog.empty());
            unplayed("escape then N");

            // The words are kept for the next time
            EXPECT_EQ(life->reportFlow.message, "No 1");

            f.ecs.sendEvent(ButtonActivatedEvent{0, ReportTag});
            f.frames(6);
            ASSERT_TRUE(life->reportOpen);
            EXPECT_EQ(inReport(life, "reportWords")->get<WordField>()->text(&f.ecs), "No 1");

            f.ecs.sendEvent(OnSDLScanCode{SDL_SCANCODE_ESCAPE, 0});
            f.frames(6);
            EXPECT_FALSE(life->reportOpen);

            // Shut, the game has its keys back
            f.ecs.sendEvent(OnSDLScanCode{SDL_SCANCODE_T, 0});
            f.frames(6);
            EXPECT_EQ(f.theme->currentTheme(), "candle");

            // (and back to the day's: the theme is kept from one test to the next)
            f.ecs.sendEvent(OnSDLScanCode{SDL_SCANCODE_T, 0});
            f.frames(6);
            EXPECT_EQ(f.theme->currentTheme(), "day");

            f.load<EmptyScene>();
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // Three reports in a quarter of an hour: the dialog still opens, Send is greyed and it says
        // when he may send again. Under the limit, Send writes the report and counts it
        TEST(lifescene_test, the_limit_greys_send)
        {
            MockLogger logger;
            LifeFixture f;

            const auto folder = optionsScratch();
            LifeSceneOptions opt = reportOptions(folder);
            const auto now = static_cast<int64_t>(opt.now);

            ReportLimit limit;
            limit.sent(now - 60);
            limit.sent(now - 30);
            limit.sent(now - 10);
            { std::ofstream file(folder / "reports.limit"); file << limit.text(); }

            LifeScene* life = f.life(opt);
            ASSERT_NE(life, nullptr);

            f.ecs.sendEvent(ButtonActivatedEvent{0, ReportTag});
            f.frames(6);
            ASSERT_EQ(life->dialogKind, "report");

            auto send = inReport(life, "reportSend")->get<Button>();
            EXPECT_TRUE(send->face->get<ButtonState>()->disabled);
            EXPECT_EQ(inReport(life, "reportLine")->get<Label>()->spec.text, "You may send another in 14 minutes");

            // Pressed anyway: nothing written, nothing counted, the words kept
            f.ecs.sendEvent(OnSDLTextInput{"x"});
            f.frames(4);
            f.ecs.sendEvent(ButtonActivatedEvent{0, "report.send"});
            f.frames(6);

            EXPECT_TRUE(life->reportOpen);
            EXPECT_FALSE(std::filesystem::exists(folder / "reports"));
            EXPECT_EQ(fileText((folder / "reports.limit").string()), limit.text());
            EXPECT_EQ(life->reportFlow.message, "x");

            f.load<EmptyScene>();

            // Under it: Send is open, writes the report beside the save and counts it
            std::filesystem::remove(folder / "reports.limit");

            life = f.life(opt);
            ASSERT_NE(life, nullptr);
            f.pass(800.0f);   // The page has arrived: a toast is said

            f.ecs.sendEvent(ButtonActivatedEvent{0, ReportTag});
            f.frames(6);
            ASSERT_EQ(life->dialogKind, "report");
            EXPECT_FALSE(inReport(life, "reportSend")->get<Button>()->face->get<ButtonState>()->disabled);

            for (const char* letter : {"i", "t", " ", "b", "r", "o", "k", "e"})
                f.ecs.sendEvent(OnSDLTextInput{letter});
            f.frames(4);
            f.ecs.sendEvent(ButtonActivatedEvent{0, "report.send"});
            f.frames(6);

            EXPECT_FALSE(life->reportOpen);
            EXPECT_TRUE(life->reportFlow.message.empty());
            EXPECT_TRUE(std::filesystem::exists(folder / "reports"));

            ReportLimit counted = ReportLimit::fromText(fileText((folder / "reports.limit").string()));
            EXPECT_EQ(counted.waitSeconds(now), 0);
            counted.sent(now);
            counted.sent(now);
            EXPECT_FALSE(counted.allowed(now));   // one before these two

            // Said where it went
            EXPECT_TRUE(std::any_of(life->shownToasts.begin(), life->shownToasts.end(), [](const LifeScene::Toast& t) { return t.text.rfind("Written to ", 0) == 0; }));

            f.load<EmptyScene>();
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // A report sends the save as it was last written, and never writes it
        TEST(lifescene_test, a_report_never_writes_the_save)
        {
            MockLogger logger;
            LifeFixture f;

            const auto folder = optionsScratch();
            LifeSceneOptions opt = reportOptions(folder);

            LifeScene* life = f.life(opt);
            ASSERT_NE(life, nullptr);
            ASSERT_TRUE(life->saveNow());

            const std::string before = fileText(opt.savePath);
            const auto written = std::filesystem::last_write_time(opt.savePath);
            ASSERT_FALSE(before.empty());

            // Changed since, and not written: the report has the file's, not the scene's
            life->save.stats["coin"] += 7;

            f.ecs.sendEvent(ButtonActivatedEvent{0, ReportTag});
            f.frames(6);
            ASSERT_TRUE(life->reportOpen);

            f.ecs.sendEvent(OnSDLTextInput{"z"});
            f.frames(4);
            f.ecs.sendEvent(ButtonActivatedEvent{0, "report.send"});
            f.frames(6);
            ASSERT_FALSE(life->reportOpen);

            EXPECT_EQ(fileText(opt.savePath), before);
            EXPECT_EQ(std::filesystem::last_write_time(opt.savePath), written);

            // What went: the save as written, his words, the life's own log and where he stood
            std::filesystem::path report;
            for (const auto& entry : std::filesystem::directory_iterator(folder / "reports"))
                report = entry.path();
            ASSERT_FALSE(report.empty());

            EXPECT_EQ(fileText((report / "life.sz").string()), before);
            EXPECT_EQ(fileText((report / "message.txt").string()), "z");
            EXPECT_NE(fileText((report / "log.txt").string()).find("--- life ---"), std::string::npos);
            EXPECT_NE(fileText((report / "place.txt").string()).find("page=life"), std::string::npos);

            // And the player's id beside the save, which a report makes once
            EXPECT_TRUE(std::filesystem::exists(folder / "player.id"));

            f.load<EmptyScene>();
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // Under the report's dialog nothing of the game answers, whatever reached the kit: a row
        // begun, a tab, a place, a button, a switch, a card (on the web the dialog is the page's own,
        // so the `dialog` slot is empty and the scene must say no itself)
        TEST(lifescene_test, the_game_does_not_answer_under_the_report)
        {
            MockLogger logger;
            LifeFixture f;

            LifeScene* life = f.life();
            ASSERT_NE(life, nullptr);
            life->save.stats["rations"] = 24;

            const float age = life->save.age;
            const std::string running = life->save.running;
            const bool click = life->settings.beginOnFirstClick;

            f.ecs.sendEvent(ButtonActivatedEvent{0, ReportTag});
            f.frames(6);
            ASSERT_TRUE(life->reportOpen);

            // As the web's would be: the scene's own flag, nothing in the slot
            life->closeDialog();
            life->reportOpen = true;
            ASSERT_TRUE(life->dialog.empty());

            f.ecs.sendEvent(ActivitySelectedEvent{"life.activities", "yard"});
            f.ecs.sendEvent(ActivityActivatedEvent{"life.activities", "yard"});
            f.ecs.sendEvent(TabSelectedEvent{0, "life.tabs", 1});
            f.ecs.sendEvent(PlaceSelectedEvent{"inn"});
            f.ecs.sendEvent(ButtonActivatedEvent{0, "life.skip"});
            f.ecs.sendEvent(ButtonActivatedEvent{0, "life.begin"});
            f.ecs.sendEvent(ButtonActivatedEvent{0, "options.wipe"});
            f.ecs.sendEvent(SwitchToggledEvent{0, "options.click", not click});
            f.ecs.sendEvent(RadioChosenEvent{0, "options.light", "candle"});
            f.frames(12);

            EXPECT_EQ(life->save.running, running);
            EXPECT_FLOAT_EQ(life->save.age, age);
            EXPECT_TRUE(life->chosen.empty());
            EXPECT_FALSE(life->townShown);
            EXPECT_FALSE(life->optionsShown);
            EXPECT_TRUE(life->dialog.empty());
            EXPECT_EQ(life->settings.beginOnFirstClick, click);
            EXPECT_EQ(f.theme->currentTheme(), "day");

            // Shut, a row is begun again
            life->closeReport();
            f.ecs.sendEvent(ActivityActivatedEvent{"life.activities", "yard"});
            f.frames(12);
            EXPECT_EQ(life->save.running, "yard");

            f.load<EmptyScene>();
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // A life never written says so in its report, rather than send no save
        TEST(lifescene_test, a_report_of_a_life_never_written_says_so)
        {
            MockLogger logger;
            LifeFixture f;

            const auto folder = optionsScratch();
            LifeSceneOptions opt = reportOptions(folder);
            opt.noSave = true;

            LifeScene* life = f.life(opt);
            ASSERT_NE(life, nullptr);
            ASSERT_FALSE(std::filesystem::exists(opt.savePath));

            f.ecs.sendEvent(ButtonActivatedEvent{0, ReportTag});
            f.frames(6);
            ASSERT_TRUE(life->reportOpen);
            f.ecs.sendEvent(ButtonActivatedEvent{0, "report.send"});
            f.frames(6);
            ASSERT_FALSE(life->reportOpen);

            std::filesystem::path report;
            for (const auto& entry : std::filesystem::directory_iterator(folder / "reports"))
                report = entry.path();
            ASSERT_FALSE(report.empty());

            EXPECT_EQ(fileText((report / "life.sz").string()).rfind("(no save", 0), 0u);
            EXPECT_FALSE(std::filesystem::exists(opt.savePath));

            f.load<EmptyScene>();
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // One dialog at a time: the report does not open over the wipe's question, nor the wipe's
        // over the report
        TEST(lifescene_test, the_report_and_the_wipe_never_both_open)
        {
            MockLogger logger;
            LifeFixture f;

            LifeScene* life = f.life();
            ASSERT_NE(life, nullptr);

            life->showOptions(true);
            f.frames(6);

            f.ecs.sendEvent(ButtonActivatedEvent{0, "options.wipe"});
            f.frames(12);
            ASSERT_EQ(life->dialogKind, "burn");

            f.ecs.sendEvent(ButtonActivatedEvent{0, ReportTag});
            f.frames(6);
            EXPECT_EQ(life->dialogKind, "burn");
            EXPECT_FALSE(life->reportOpen);

            f.ecs.sendEvent(OnSDLScanCode{SDL_SCANCODE_ESCAPE, 0});
            f.frames(6);

            f.ecs.sendEvent(ButtonActivatedEvent{0, ReportTag});
            f.frames(6);
            ASSERT_EQ(life->dialogKind, "report");

            f.ecs.sendEvent(ButtonActivatedEvent{0, "options.wipe"});
            f.frames(12);
            EXPECT_EQ(life->dialogKind, "report");
            EXPECT_TRUE(life->reportOpen);

            f.load<EmptyScene>();
        }
    }
}
