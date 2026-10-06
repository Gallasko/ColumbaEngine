#include "stdafx.h"

#ifdef __linux__
#include <SDL2/SDL.h>
#elif _WIN32
#include <SDL.h>
#endif

#include <algorithm>
#include <functional>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "UI/activityrow.h"
#include "Core/motion.h"

#include "ECS/entitysystem.h"
#include "UI/themesystem.h"
#include "ECS/entitysystem_fwd.h"   // ResizeEvent
#include "UI/ttftext.h"
#include "UI/iconsystem.h"
#include "UI/sizer.h"
#include "UI/prefab.h"
#include "UI/focusable.h"
#include "Input/inputcomponent.h"
#include "Systems/gamefacts.h"
#include "Systems/tween.h"
#include "Systems/coresystems.h"
#include "2D/position.h"
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
            struct ActivityRecorder : public System<Listener<ActivitySelectedEvent>, Listener<ActivityActivatedEvent>, Listener<PrefabChangedEvent>, StoragePolicy>
            {
                virtual std::string getSystemName() const override { return "Activity Recorder"; }

                virtual void onEvent(const ActivitySelectedEvent& event) override { selected.push_back(event); }

                virtual void onEvent(const ActivityActivatedEvent& event) override { activated.push_back(event); }

                virtual void onEvent(const PrefabChangedEvent& event) override { changed.push_back(event.prefabId); }

                size_t changesOf(_unique_id id) const { return static_cast<size_t>(std::count(changed.begin(), changed.end(), id)); }

                std::vector<ActivitySelectedEvent> selected;
                std::vector<ActivityActivatedEvent> activated;
                std::vector<_unique_id> changed;
            };

            // Builds from inside a frame, as a scene does from its init: the ECS is running, so
            // what it creates is pending until the next flush and cannot be found by id.
            struct FrameBuilder : public System<>
            {
                virtual std::string getSystemName() const override { return "Frame Builder"; }

                virtual void execute() override
                {
                    if (not build)
                        return;

                    build(ecsRef);
                    build = nullptr;
                }

                std::function<void(EntitySystem*)> build;
            };

            struct ActivityFixture
            {
                EntitySystem ecs;
                MasterRenderer renderer;
                TTFTextSystem* ttf = nullptr;
                IconSystem* icons = nullptr;
                ThemeSystem* theme = nullptr;
                FocusOrderSystem* focusOrder = nullptr;
                ActivitySystem* activities = nullptr;
                ActivityRecorder* recorder = nullptr;
                FrameBuilder* builder = nullptr;
                WorldFacts* facts = nullptr;
                FactFeed* feed = nullptr;

                ActivityFixture()
                {
                    Motion::setReduced(true);
                    ecs.createSystem<PositionComponentSystem>();
                    ecs.createSystem<LayoutSystem>();
                    ecs.createSystem<PrefabSystem>();   // Hands a row's clip and visibility down to its parts
                    ecs.succeed<PositionComponentSystem, LayoutSystem>();
                    ecs.succeed<PositionComponentSystem, PrefabSystem>();
                    ttf = ecs.createSystem<TTFTextSystem>(&renderer);
                    ecs.createSystem<Simple2DObjectSystem>(&renderer);
                    ecs.createSystem<RoundedRect2DObjectSystem>(&renderer);
                    ecs.createSystem<HatchRect2DObjectSystem>(&renderer);
                    ecs.createSystem<DottedLine2DObjectSystem>(&renderer);
                    ecs.createSystem<StrokeRect2DObjectSystem>(&renderer);
                    icons = ecs.createSystem<IconSystem>(&renderer);
                    ecs.createSystem<MouseHoverSystem>();
                    ecs.createSystem<FocusableSystem>();
                    focusOrder = ecs.createSystem<FocusOrderSystem>();
                    ecs.createSystem<TweenSystem>();
                    facts = createTestFacts(&ecs);
                    feed = ecs.createSystem<FactFeed>();
                    theme = ecs.createSystem<ThemeSystem>();
                    activities = ecs.createSystem<ActivitySystem>();
                    ecs.succeed<MouseHoverSystem, ActivitySystem>();
                    recorder = ecs.createSystem<ActivityRecorder>();
                    builder = ecs.createSystem<FrameBuilder>();
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

                void pump() { ecs.executeOnce(); ecs.executeOnce(); ecs.executeOnce(); }

                void hover(float x, float y) { ecs.sendEvent(OnMouseMove{Point2D{x, y}, nullptr}); pump(); }

                void press(float x, float y) { ecs.sendEvent(OnMouseClick{Point2D{x, y}, static_cast<MouseButton>(1)}); pump(); }

                void release(float x, float y) { ecs.sendEvent(OnMouseRelease{Point2D{x, y}, static_cast<MouseButton>(1)}); pump(); }

                void key(SDL_Scancode k, Uint16 mod = 0) { ecs.sendEvent(OnSDLScanCode{k, mod}); pump(); }

                // Hover, press and release in the middle of an entity.
                void click(EntityRef e)
                {
                    const float x = pos(e)->x + pos(e)->width / 2.0f;
                    const float y = pos(e)->y + pos(e)->height / 2.0f;

                    hover(x, y);
                    press(x, y);
                    release(x, y);
                }

                ActivityRow place(const ActivityRowSpec& spec, float x = 100.0f, float y = 100.0f)
                {
                    ActivityRow row = makeActivityRow(&ecs, spec);
                    row.root->get<PositionComponent>()->setX(x);
                    row.root->get<PositionComponent>()->setY(y);
                    pump();

                    return row;
                }

                ActivityList placeList(const ActivityListSpec& spec, float x = 100.0f, float y = 100.0f)
                {
                    ActivityList list = makeActivityList(&ecs, spec);
                    list.root->get<PositionComponent>()->setX(x);
                    list.root->get<PositionComponent>()->setY(y);
                    pump();
                    pump();

                    return list;
                }

                float asc(const std::string& style)
                {
                    const TextStyle& s = theme->style(style);

                    return ttf->measureText(s.fontAlias, "H", 1.0f, 0.0f, 0.0f, s.letterSpacingPx).ascender;
                }

                CompRef<PositionComponent> pos(EntityRef e) { return ecs.getEntity(e.id)->get<PositionComponent>(); }

                CompRef<PositionComponent> pos(_unique_id id) { return ecs.getEntity(id)->get<PositionComponent>(); }

                std::string element(EntityRef e) { return ecs.getEntity(e.id)->get<ThemeComponent>()->element; }

                std::string element(_unique_id id) { return ecs.getEntity(id)->get<ThemeComponent>()->element; }

                ActivityRowState* state(EntityRef root) { return ecs.getEntity(root.id)->get<ActivityRowState>().component; }

                constant::Vector4D color(const std::string& token, const std::string& id) { return theme->theme().color(token, id); }
            };

            ActivityRowSpec idleSpec(const std::string& id, const std::string& name = "Train at the yard")
            {
                ActivityRowSpec spec;
                spec.id = id;
                spec.name = name;
                spec.glyph = "training";
                spec.months = 6;
                spec.gains = {{"STR", 1}, {"VIT", 1}};

                return spec;
            }

            ActivityRowSpec lockedSpec(const std::string& id)
            {
                ActivityRowSpec spec = idleSpec(id, "Squire at the keep");
                spec.state = ActivityState::Locked;
                spec.requirements = {{"Strength", 15, 18}, {"Swordsmanship", 3, 4}, {"Vitality", 12, 10}};

                return spec;
            }

            ActivityRowSpec runningSpec(const std::string& id)
            {
                ActivityRowSpec spec = idleSpec(id, "Letters at the abbey");
                spec.glyph = "study";
                spec.state = ActivityState::Running;
                spec.percent = 50.0f;
                spec.caption = "MONTH 3 OF 6";

                return spec;
            }

            std::string pair(const std::string& stat, const std::string& amount)
            {
                return stat + "\xC2\xA0" + amount;
            }
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // A list with a tile width lays its rows as tiles, as many to a line as fit, 8 apart: the name
        // (two lines at most) over the time, no mark. The tiles of a line are as tall as the
        // tallest, a group starts a line of its own, and another width lays them again.
        TEST(activityrow_test, tiles_on_lines)
        {
            MockLogger logger;
            ActivityFixture s;

            ActivityRowSpec bought = idleSpec("e", "Buy");
            bought.months = 0;

            ActivityListSpec spec;
            spec.id = "choice";
            spec.width = 464.0f;
            spec.tileWidth = 148.0f;
            spec.groups = {
                {"Work", {idleSpec("a", "Carry"), idleSpec("b", "Practice with Wooden Blades and with Shields"), idleSpec("c", "Mill"), idleSpec("d", "Yard")}},
                {"Market", {bought}},
            };
            ActivityList list = s.placeList(spec);
            s.pump();

            // Three of 149 and a third fit in 464, 8 apart
            EXPECT_EQ(list.columns(), 3);
            EXPECT_NEAR(list.tileSize(), (464.0f - 16.0f) / 3.0f, 0.01f);

            ActivityRow* a = list.row(&s.ecs, "a");
            ActivityRow* b = list.row(&s.ecs, "b");
            ActivityRow* c = list.row(&s.ecs, "c");
            ActivityRow* d = list.row(&s.ecs, "d");
            ActivityRow* e = list.row(&s.ecs, "e");

            for (ActivityRow* row : {a, b, c, d, e})
            {
                ASSERT_NE(row, nullptr);
                EXPECT_TRUE(row->spec.tile);
                EXPECT_TRUE(row->spec.compact);
                EXPECT_NEAR(s.pos(row->root)->width, list.tileSize(), 0.5f);

                // A tile: no mark, no rank, no gains, its name in the control face
                EXPECT_TRUE(row->mark.entity.empty());
                EXPECT_FALSE(row->rank.has_value());
                EXPECT_FALSE(row->gains.has_value());
                EXPECT_EQ(row->name.spec.style, "control");
            }

            // Side by side on the first line, then the fourth under the first
            const float step = list.tileSize() + 8.0f;

            EXPECT_NEAR(s.pos(a->root)->x, s.pos(list.root)->x, 0.5f);
            EXPECT_NEAR(s.pos(b->root)->x, s.pos(a->root)->x + step, 0.5f);
            EXPECT_NEAR(s.pos(c->root)->x, s.pos(a->root)->x + 2.0f * step, 0.5f);
            EXPECT_NEAR(s.pos(b->root)->y, s.pos(a->root)->y, 0.5f);
            EXPECT_NEAR(s.pos(c->root)->y, s.pos(a->root)->y, 0.5f);
            EXPECT_NEAR(s.pos(d->root)->x, s.pos(a->root)->x, 0.5f);

            // The long name takes two lines; its line's tiles are as tall as it is
            EXPECT_GT(b->natural, a->natural);
            EXPECT_NEAR(s.pos(b->root)->height, b->natural, 0.5f);
            EXPECT_NEAR(s.pos(a->root)->height, b->natural, 0.5f);
            EXPECT_NEAR(s.pos(c->root)->height, b->natural, 0.5f);
            EXPECT_NEAR(s.pos(d->root)->y, s.pos(a->root)->y + b->natural + 8.0f, 0.5f);
            EXPECT_NEAR(s.pos(d->root)->height, d->natural, 0.5f);

            // The time under the name, from the tile's left edge
            EXPECT_NEAR(s.pos(a->name.entity)->x, s.pos(a->root)->x + 8.0f, 0.5f);
            EXPECT_NEAR(s.pos(a->name.entity)->y, s.pos(a->root)->y + 8.0f, 0.5f);
            EXPECT_NEAR(s.pos(a->cost.entity)->y, s.pos(a->name.entity)->y + s.pos(a->name.entity)->height + 2.0f, 0.5f);
            EXPECT_NEAR(s.pos(a->costMark.entity)->x, s.pos(a->root)->x + 8.0f, 0.5f);
            EXPECT_GT(s.pos(a->cost.entity)->x, s.pos(a->costMark.entity)->x);

            // Another group, another line, under its heading; its kind on its ground
            EXPECT_NEAR(s.pos(e->root)->x, s.pos(a->root)->x, 0.5f);
            EXPECT_GT(s.pos(e->root)->y, s.pos(d->root)->y + s.pos(d->root)->height);
            EXPECT_EQ(s.element(e->ground), "activity.kind.instant");
            EXPECT_EQ(s.element(a->ground), "activity.kind.timed");

            // A closing on one tile: its line grows with it, and what is under moves down
            const float marketY = s.pos(e->root)->y;

            d->setUntil(&s.ecs, "CLOSES IN 3 MO", true);
            s.pump();
            s.pump();

            EXPECT_NEAR(s.pos(d->root)->height, d->natural, 0.5f);
            EXPECT_NEAR(s.pos(e->root)->y, marketY + 18.0f, 0.5f);

            // Selection is the list's, as with rows
            list.select(&s.ecs, "c");
            s.pump();

            EXPECT_EQ(list.selected(), "c");

            // Wider: four to a line, the tiles laid again as they stood
            list.setSize(&s.ecs, 620.0f, 0.0f);
            s.pump();
            s.pump();

            EXPECT_EQ(list.columns(), 4);

            a = list.row(&s.ecs, "a");
            d = list.row(&s.ecs, "d");
            ASSERT_NE(a, nullptr);
            ASSERT_NE(d, nullptr);

            EXPECT_NEAR(s.pos(a->root)->width, (620.0f - 24.0f) / 4.0f, 0.5f);
            EXPECT_NEAR(s.pos(d->root)->y, s.pos(a->root)->y, 0.5f);
            EXPECT_NEAR(s.pos(d->root)->x, s.pos(a->root)->x + 3.0f * ((620.0f - 24.0f) / 4.0f + 8.0f), 0.5f);
            EXPECT_EQ(d->spec.until, "CLOSES IN 3 MO");
            EXPECT_TRUE(d->spec.urgent);

            // Without a tile width the rows are rows
            ActivityListSpec plain;
            plain.groups = {{"Training", {idleSpec("x"), idleSpec("y")}}};
            ActivityList rows = s.placeList(plain, 100.0f, 600.0f);

            EXPECT_EQ(rows.columns(), 0);
            EXPECT_FALSE(rows.row(&s.ecs, "x")->spec.tile);
            EXPECT_NEAR(s.pos(rows.row(&s.ecs, "y")->root)->y, s.pos(rows.row(&s.ecs, "x")->root)->y + 68.0f, 0.5f);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // A compact row is its name, its time and its closing: no rank, no `each`, no gains, no rule
        // and no list of what it asks, whatever its state. Its ground says what kind of thing it is:
        // work that takes months, a thing done at once, what he cannot do yet, the work he is at.
        TEST(activityrow_test, compact_row_and_its_kinds)
        {
            MockLogger logger;
            ActivityFixture s;

            ActivityRowSpec spec = idleSpec("train.yard");
            spec.compact = true;
            spec.rank = "RANK 2";
            spec.count = "DONE 2";
            spec.each = "AT THE YARD";
            ActivityRow timed = s.place(spec, 100.0f, 100.0f);

            // The title line alone, between its paddings
            EXPECT_FLOAT_EQ(s.pos(timed.root)->height, 12.0f + 24.0f + 12.0f);
            EXPECT_FALSE(timed.rank.has_value());
            EXPECT_FALSE(timed.each.has_value());
            EXPECT_FALSE(timed.gains.has_value());
            EXPECT_FALSE(timed.progress.has_value());
            EXPECT_FALSE(timed.reqs.has_value());
            EXPECT_EQ(timed.name.spec.text, "Train at the yard");
            EXPECT_EQ(timed.cost.spec.text, "6 mo");

            // What it does not show it still keeps, for whoever says it
            EXPECT_EQ(timed.spec.count, "DONE 2");
            timed.setCount(&s.ecs, "DONE 3");
            EXPECT_EQ(timed.spec.count, "DONE 3");
            EXPECT_FALSE(timed.rank.has_value());

            ActivityRowSpec bought = idleSpec("buy.rations", "Buy rations");
            bought.compact = true;
            bought.months = 0;
            ActivityRow instant = s.place(bought, 100.0f, 200.0f);

            ActivityRowSpec barred = lockedSpec("train.squire");
            barred.compact = true;
            ActivityRow locked = s.place(barred, 100.0f, 300.0f);

            ActivityRowSpec begun = runningSpec("study.letters");
            begun.compact = true;
            ActivityRow running = s.place(begun, 100.0f, 400.0f);

            // Three grounds at rest, and the one of the work he is at
            EXPECT_EQ(s.element(timed.ground), "activity.kind.timed");
            EXPECT_EQ(s.element(instant.ground), "activity.kind.instant");
            EXPECT_EQ(s.element(locked.ground), "activity.kind.locked");
            EXPECT_EQ(s.element(running.ground), "activity.kind.running");
            EXPECT_EQ(instant.cost.spec.text, "NOW");

            // Locked and running rows are as short: the gloss says what they ask and where they are
            EXPECT_FLOAT_EQ(s.pos(locked.root)->height, 48.0f);
            EXPECT_FALSE(locked.reqs.has_value());
            EXPECT_FLOAT_EQ(s.pos(running.root)->height, 48.0f);
            EXPECT_FALSE(running.progress.has_value());

            // Hover deepens the kind's own ground; a locked row has no hover
            s.hover(300.0f, 120.0f);
            EXPECT_EQ(s.element(timed.ground), "activity.kind.timed.hover");

            s.hover(300.0f, 220.0f);
            EXPECT_EQ(s.element(timed.ground), "activity.kind.timed");
            EXPECT_EQ(s.element(instant.ground), "activity.kind.instant.hover");

            s.hover(300.0f, 320.0f);
            EXPECT_EQ(s.element(instant.ground), "activity.kind.instant");
            EXPECT_EQ(s.element(locked.ground), "activity.kind.locked");

            s.hover(1000.0f, 800.0f);

            // A state change repaints the ground and builds nothing
            timed.setState(&s.ecs, ActivityState::Locked);
            s.pump();

            EXPECT_EQ(s.element(timed.ground), "activity.kind.locked");
            EXPECT_FALSE(timed.reqs.has_value());
            EXPECT_FLOAT_EQ(s.pos(timed.root)->height, 48.0f);

            // Its closing stands under the title line, and the row holds it
            timed.setState(&s.ecs, ActivityState::Idle);
            timed.setUntil(&s.ecs, "CLOSES IN 14 MO");
            s.pump();

            ASSERT_TRUE(timed.until.has_value());
            EXPECT_NEAR(s.pos(timed.until->entity)->y, s.pos(timed.root)->y + 12.0f + 24.0f + 2.0f, 0.5f);
            EXPECT_FLOAT_EQ(s.pos(timed.root)->height, 48.0f + 2.0f + 16.0f);

            // A thing done at once that comes to take months changes its kind
            instant.setMonths(&s.ecs, 3);
            s.pump();

            EXPECT_EQ(s.element(instant.ground), "activity.kind.timed");
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // When it closes is a line of its own under the middle block, in every state: the row grows by
        // it, an urgent one is written in the loss's colour, and with nothing to say the line goes.
        TEST(activityrow_test, closing_line)
        {
            MockLogger logger;
            ActivityFixture s;

            ActivityRowSpec spec = idleSpec("train.yard");
            spec.until = "CLOSES IN 14 MO";
            ActivityRow row = s.place(spec);

            const float y = s.pos(row.root)->y;

            ASSERT_TRUE(row.until.has_value());
            EXPECT_EQ(row.until->spec.text, "CLOSES IN 14 MO");
            EXPECT_EQ(s.element(row.until->entity), "activity.until");

            // Under the gains, two pixels apart, on the text's left edge; the row holds it
            EXPECT_NEAR(s.pos(row.until->entity)->y, y + 12.0f + 24.0f + 4.0f + 16.0f + 2.0f, 0.5f);
            EXPECT_NEAR(s.pos(row.until->entity)->x, s.pos(row.name.entity)->x, 0.5f);
            EXPECT_FLOAT_EQ(s.pos(row.root)->height, 68.0f + 2.0f + 16.0f);

            // Near: the same line, in the loss's colour
            row.setUntil(&s.ecs, "CLOSES THIS MONTH", true);
            s.pump();

            EXPECT_EQ(row.until->spec.text, "CLOSES THIS MONTH");
            EXPECT_EQ(s.element(row.until->entity), "activity.until.urgent");
            EXPECT_FLOAT_EQ(s.pos(row.root)->height, 86.0f);

            // Locked: it stands under the list of what the row asks
            row.setRequirements(&s.ecs, {{"Strength", 8, 12}});
            row.setState(&s.ecs, ActivityState::Locked);
            s.pump();

            ASSERT_TRUE(row.reqs.has_value());
            ASSERT_TRUE(row.until.has_value());
            EXPECT_NEAR(s.pos(row.until->entity)->y, s.pos(row.reqs->root)->y + row.reqs->height(&s.ecs) + 2.0f, 0.5f);

            // Nothing to say: the line goes, and the row is as it was without one
            row.setState(&s.ecs, ActivityState::Idle);
            row.setUntil(&s.ecs, "");
            s.pump();

            EXPECT_FALSE(row.until.has_value());
            EXPECT_FLOAT_EQ(s.pos(row.root)->height, 68.0f);

            // A row built without one has none
            ActivityRow plain = s.place(idleSpec("mill"));

            EXPECT_FALSE(plain.until.has_value());
            EXPECT_FLOAT_EQ(s.pos(plain.root)->height, 68.0f);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // A long list of gains takes another line in the room between the mark and the cost: the row
        // grows by it and nothing runs out of the row. Wider, or with less to say, it is one line.
        TEST(activityrow_test, gains_wrap_in_a_narrow_row)
        {
            MockLogger logger;
            ActivityFixture s;

            ActivityRowSpec spec = idleSpec("rob", "Rob the Counting House");
            spec.width = 300.0f;
            spec.gains = {{"COIN", 100}, {"LEDGERS", 1}, {"ENEMY", 1}, {"STEALTH", 1}, {"GUILE", 1}};
            ActivityRow row = s.place(spec);

            ASSERT_TRUE(row.gains.has_value());
            EXPECT_EQ(row.gains->spec.overflow, Overflow::Wrap);
            EXPECT_NEAR(row.gains->spec.width, row.middleWidth(), 0.5f);

            // Lines of 16, more than one, and the row is as tall as they make it
            const float wrapped = s.pos(row.gains->entity)->height;

            EXPECT_GE(wrapped, 32.0f);
            EXPECT_NEAR(s.pos(row.root)->height, 12.0f + 24.0f + 4.0f + wrapped + 12.0f, 0.5f);
            EXPECT_LE(s.pos(row.gains->entity)->x + s.pos(row.gains->entity)->width, s.pos(row.cost.entity)->x);

            // Wider: one line again
            row.setWidth(&s.ecs, 900.0f);
            s.pump();

            EXPECT_FLOAT_EQ(s.pos(row.gains->entity)->height, 16.0f);
            EXPECT_FLOAT_EQ(s.pos(row.root)->height, 68.0f);

            // Narrow again, with less to say: one line too
            row.setWidth(&s.ecs, 300.0f);
            s.pump();

            EXPECT_GE(s.pos(row.root)->height, 84.0f);

            row.setGains(&s.ecs, {{"COIN", 100}});
            s.pump();

            EXPECT_FLOAT_EQ(s.pos(row.root)->height, 68.0f);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(activityrow_test, idle_geometry)
        {
            MockLogger logger;
            ActivityFixture s;

            ActivityRowSpec spec = idleSpec("train.yard");
            spec.rank = "RANK 2";
            spec.each = "AT THE YARD";
            ActivityRow row = s.place(spec);

            const float x = s.pos(row.root)->x;
            const float y = s.pos(row.root)->y;

            EXPECT_FLOAT_EQ(s.pos(row.root)->width, 620.0f);
            EXPECT_FLOAT_EQ(s.pos(row.root)->height, 68.0f);

            EXPECT_EQ(row.mark.spec.size, MarkSize::S24);
            EXPECT_NEAR(s.pos(row.mark.entity)->x, x + 12.0f + 1.0f, 0.5f);
            EXPECT_NEAR(s.pos(row.mark.entity)->y + 12.0f, y + 34.0f, 0.5f);
            EXPECT_EQ(s.element(row.mark.entity), "activity.mark");

            EXPECT_NEAR(s.pos(row.name.entity)->x, x + 12.0f + 26.0f + 12.0f, 0.5f);
            EXPECT_NEAR(s.pos(row.name.entity)->y, y + 12.0f, 0.5f);
            EXPECT_EQ(row.name.spec.style, "tab");
            EXPECT_EQ(s.element(row.name.entity), "activity.name");

            ASSERT_TRUE(row.rank.has_value());
            EXPECT_NEAR(s.pos(row.rank->entity)->y + s.asc("caption"), s.pos(row.name.entity)->y + s.asc("tab"), 0.5f);
            EXPECT_NEAR(s.pos(row.rank->entity)->x, s.pos(row.name.entity)->x + s.pos(row.name.entity)->width + 8.0f, 0.5f);
            EXPECT_EQ(s.element(row.rank->entity), "activity.rank");

            ASSERT_TRUE(row.gains.has_value());
            EXPECT_EQ(row.gains->spec.text, pair("STR", "+1") + "   " + pair("VIT", "+1"));
            EXPECT_EQ(s.element(row.gains->entity), "activity.gains");
            EXPECT_NEAR(s.pos(row.gains->entity)->y, y + 12.0f + 24.0f + 4.0f, 0.5f);
            EXPECT_FALSE(row.progress.has_value());
            EXPECT_FALSE(row.reqs.has_value());

            EXPECT_EQ(row.cost.spec.text, "6 mo");
            EXPECT_EQ(s.element(row.cost.entity), "activity.cost");
            EXPECT_NEAR(s.pos(row.cost.entity)->x + s.pos(row.cost.entity)->width, x + 608.0f, 0.5f);
            EXPECT_EQ(s.element(row.costMark.entity), "activity.cost.mark");
            EXPECT_NEAR(s.pos(row.costMark.entity)->x + 14.0f, s.pos(row.cost.entity)->x - 4.0f, 0.5f);

            ASSERT_TRUE(row.each.has_value());
            EXPECT_EQ(s.element(row.each->entity), "activity.each");
            EXPECT_NEAR(s.pos(row.each->entity)->y, s.pos(row.cost.entity)->y + s.pos(row.cost.entity)->height + 2.0f, 0.5f);
            EXPECT_NEAR(s.pos(row.each->entity)->x + s.pos(row.each->entity)->width, x + 608.0f, 0.5f);

            EXPECT_FLOAT_EQ(s.pos(row.rule)->height, 1.0f);
            EXPECT_NEAR(s.pos(row.rule)->y, y + 67.0f, 0.5f);
            EXPECT_TRUE(s.pos(row.rule)->isVisible());
            EXPECT_EQ(s.element(row.rule), "activity.row.rule");

            EXPECT_EQ(s.element(row.ground), "activity.row.ground");
            EXPECT_EQ(s.element(row.edge), "activity.row.edge");
            EXPECT_FLOAT_EQ(s.pos(row.edge)->width, 3.0f);
            EXPECT_FLOAT_EQ(s.pos(row.edge)->height, 68.0f);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(activityrow_test, running_row)
        {
            MockLogger logger;
            ActivityFixture s;

            ActivityRow row = s.place(runningSpec("study.letters"));
            const float y = s.pos(row.root)->y;

            ASSERT_TRUE(row.progress.has_value());
            EXPECT_FALSE(row.gains.has_value());
            EXPECT_NEAR(row.progress->spec.width, row.middleWidth(), 0.01f);
            EXPECT_TRUE(row.progress->nib.has_value());
            EXPECT_NEAR(s.pos(row.progress->root)->y, y + 12.0f + 24.0f + 4.0f, 0.5f);
            EXPECT_FLOAT_EQ(s.pos(row.root)->height, 81.0f);

            EXPECT_EQ(s.element(row.mark.entity), "activity.mark.running");
            EXPECT_EQ(s.element(row.edge), "activity.row.edge.running");
            EXPECT_EQ(s.element(row.ground), "activity.row.ground.running");

            row.setPercent(&s.ecs, 75.0f);
            s.pump();

            EXPECT_FLOAT_EQ(row.progress->shown, 75.0f);
            EXPECT_FLOAT_EQ(s.ecs.getEntity(row.root.id)->get<ActivityRow>()->progress->shown, 75.0f);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(activityrow_test, locked_row)
        {
            MockLogger logger;
            ActivityFixture s;

            ActivityRow row = s.place(lockedSpec("train.squire"));

            ASSERT_TRUE(row.reqs.has_value());
            EXPECT_FALSE(row.gains.has_value());
            EXPECT_TRUE(row.reqs->spec.dense);
            EXPECT_FLOAT_EQ(row.reqs->spec.width, std::min(300.0f, row.middleWidth()));
            EXPECT_EQ(row.reqs->size(), 3u);
            EXPECT_FLOAT_EQ(s.pos(row.root)->height, 24.0f + 4.0f + 3.0f * 16.0f + 8.0f + 24.0f);

            EXPECT_EQ(s.element(row.name.entity), "activity.name.locked");
            EXPECT_EQ(s.element(row.cost.entity), "activity.cost.locked");
            EXPECT_EQ(s.element(row.mark.entity), "activity.mark.locked");

            EXPECT_FALSE(row.reqs->rows[0].item.isMet());

            row.setRequirement(&s.ecs, 0, 18, 18);
            s.pump();

            ASSERT_TRUE(row.reqs->rows[0].name.mark.has_value());
            EXPECT_TRUE(row.reqs->rows[0].item.isMet());
            EXPECT_EQ(row.reqs->rows[0].name.mark->spec.name, "check");
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(activityrow_test, set_state_swaps_middle)
        {
            MockLogger logger;
            ActivityFixture s;

            ActivityRowSpec spec = idleSpec("train.yard");
            spec.caption = "MONTH 1 OF 6";
            spec.requirements = {{"Strength", 15, 18}};
            ActivityRow row = s.place(spec);

            auto middles = [&row]() {
                return static_cast<int>(row.gains.has_value()) + static_cast<int>(row.progress.has_value()) + static_cast<int>(row.reqs.has_value());
            };

            EXPECT_EQ(middles(), 1);
            EXPECT_FLOAT_EQ(s.pos(row.root)->height, 68.0f);

            size_t before = s.recorder->changesOf(row.root.id);
            row.setState(&s.ecs, ActivityState::Running);
            s.pump();

            EXPECT_EQ(middles(), 1);
            EXPECT_TRUE(row.progress.has_value());
            EXPECT_FLOAT_EQ(s.pos(row.root)->height, 81.0f);
            EXPECT_GT(s.recorder->changesOf(row.root.id), before);

            before = s.recorder->changesOf(row.root.id);
            row.setState(&s.ecs, ActivityState::Locked);
            s.pump();

            EXPECT_EQ(middles(), 1);
            EXPECT_TRUE(row.reqs.has_value());
            EXPECT_FLOAT_EQ(s.pos(row.root)->height, 24.0f + 4.0f + 16.0f + 24.0f);
            EXPECT_GT(s.recorder->changesOf(row.root.id), before);

            before = s.recorder->changesOf(row.root.id);
            row.setState(&s.ecs, ActivityState::Idle);
            s.pump();

            EXPECT_EQ(middles(), 1);
            EXPECT_TRUE(row.gains.has_value());
            EXPECT_FLOAT_EQ(s.pos(row.root)->height, 68.0f);
            EXPECT_GT(s.recorder->changesOf(row.root.id), before);

            // The copy on the root follows the caller's
            auto live = s.ecs.getEntity(row.root.id)->get<ActivityRow>();
            EXPECT_EQ(live->spec.state, ActivityState::Idle);
            EXPECT_TRUE(live->gains.has_value());
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(activityrow_test, hover_and_stripe)
        {
            MockLogger logger;
            ActivityFixture s;

            ActivityRowSpec spec = idleSpec("train.yard");
            spec.stripe = true;
            ActivityRow striped = s.place(spec, 100.0f, 100.0f);
            ActivityRow running = s.place(runningSpec("study.letters"), 100.0f, 300.0f);
            ActivityRow locked = s.place(lockedSpec("train.squire"), 100.0f, 500.0f);

            EXPECT_EQ(s.element(striped.ground), "activity.row.ground.stripe");

            s.hover(300.0f, 130.0f);
            EXPECT_EQ(s.element(striped.ground), "activity.row.ground.hover");

            s.hover(1000.0f, 800.0f);
            EXPECT_EQ(s.element(striped.ground), "activity.row.ground.stripe");

            s.hover(300.0f, 330.0f);
            EXPECT_TRUE(s.state(running.root)->hovered);
            EXPECT_EQ(s.element(running.ground), "activity.row.ground.running");

            s.hover(300.0f, 530.0f);
            EXPECT_TRUE(s.state(locked.root)->hovered);
            EXPECT_EQ(s.element(locked.ground), "activity.row.ground");
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(activityrow_test, list_selects_one)
        {
            MockLogger logger;
            ActivityFixture s;

            ActivityListSpec spec;
            spec.groups = {{"Training", {idleSpec("a"), idleSpec("b"), idleSpec("c"), idleSpec("d")}}};
            ActivityList list = s.placeList(spec);

            ActivityRow* b = list.row(&s.ecs, "b");
            ActivityRow* c = list.row(&s.ecs, "c");
            ASSERT_NE(b, nullptr);
            ASSERT_NE(c, nullptr);

            s.click(b->root);

            ASSERT_EQ(s.recorder->selected.size(), 1u);
            EXPECT_EQ(s.recorder->selected[0].list, "activities");
            EXPECT_EQ(s.recorder->selected[0].id, "b");
            EXPECT_EQ(list.selected(), "b");
            EXPECT_EQ(s.element(b->edge), "activity.row.edge.selected");
            EXPECT_EQ(s.element(c->edge), "activity.row.edge");
            EXPECT_EQ(s.element(list.row(&s.ecs, "a")->edge), "activity.row.edge");

            s.click(c->root);

            ASSERT_EQ(s.recorder->selected.size(), 2u);
            EXPECT_EQ(s.recorder->selected[1].id, "c");
            EXPECT_EQ(s.element(b->edge), "activity.row.edge");
            EXPECT_EQ(s.element(c->edge), "activity.row.edge.selected");

            list.select(&s.ecs, "");
            s.pump();

            EXPECT_EQ(list.selected(), "");
            EXPECT_EQ(s.element(c->edge), "activity.row.edge");
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(activityrow_test, locked_and_running_refuse_selection)
        {
            MockLogger logger;
            ActivityFixture s;

            ActivityListSpec spec;
            spec.groups = {{"Training", {idleSpec("a"), lockedSpec("b"), runningSpec("c")}}};
            ActivityList list = s.placeList(spec);

            ActivityRow* b = list.row(&s.ecs, "b");
            ASSERT_NE(b, nullptr);

            s.click(b->root);

            EXPECT_TRUE(s.recorder->selected.empty());
            EXPECT_EQ(s.element(b->edge), "activity.row.edge");
            EXPECT_EQ(list.selected(), "");

            const unsigned int warnings = logger.getNbWarning();
            list.select(&s.ecs, "c");
            s.pump();

            EXPECT_GT(logger.getNbWarning(), warnings);
            EXPECT_EQ(list.selected(), "");
            EXPECT_TRUE(s.recorder->selected.empty());
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(activityrow_test, activate_on_confirm)
        {
            MockLogger logger;
            ActivityFixture s;

            ActivityListSpec spec;
            spec.groups = {{"Training", {idleSpec("a"), idleSpec("b")}}};
            ActivityList list = s.placeList(spec);

            ActivityRow* a = list.row(&s.ecs, "a");
            ActivityRow* b = list.row(&s.ecs, "b");
            ASSERT_NE(a, nullptr);
            ASSERT_NE(b, nullptr);

            // First release selects, the second one within 400 ms confirms
            s.click(a->root);
            EXPECT_EQ(s.recorder->selected.size(), 1u);
            EXPECT_TRUE(s.recorder->activated.empty());

            s.click(a->root);
            ASSERT_EQ(s.recorder->activated.size(), 1u);
            EXPECT_EQ(s.recorder->activated[0].list, "activities");
            EXPECT_EQ(s.recorder->activated[0].id, "a");

            // Enter on another row selects it first and does not confirm
            s.focusOrder->focus(b->root.id);
            s.pump();
            s.key(SDL_SCANCODE_RETURN);

            EXPECT_EQ(s.recorder->selected.size(), 2u);
            EXPECT_EQ(s.recorder->selected[1].id, "b");
            EXPECT_EQ(s.recorder->activated.size(), 1u);

            s.key(SDL_SCANCODE_RETURN);

            ASSERT_EQ(s.recorder->activated.size(), 2u);
            EXPECT_EQ(s.recorder->activated[1].id, "b");
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(activityrow_test, keyboard_skips_locked)
        {
            MockLogger logger;
            ActivityFixture s;

            ActivityListSpec spec;
            spec.groups = {{"Training", {idleSpec("a"), lockedSpec("b"), idleSpec("c")}}};
            ActivityList list = s.placeList(spec);

            ActivityRow* a = list.row(&s.ecs, "a");
            ActivityRow* b = list.row(&s.ecs, "b");
            ActivityRow* c = list.row(&s.ecs, "c");
            ASSERT_NE(a, nullptr);
            ASSERT_NE(b, nullptr);
            ASSERT_NE(c, nullptr);

            s.key(SDL_SCANCODE_TAB);
            EXPECT_EQ(s.focusOrder->current(), a->root.id);
            EXPECT_TRUE(s.pos(a->ring)->isVisible());
            EXPECT_EQ(s.element(a->ring), "activity.row.ring");

            s.key(SDL_SCANCODE_TAB);
            EXPECT_EQ(s.focusOrder->current(), c->root.id);
            EXPECT_FALSE(s.pos(a->ring)->isVisible());
            EXPECT_FALSE(s.pos(b->ring)->isVisible());
            EXPECT_TRUE(s.pos(c->ring)->isVisible());

            // The ring sits 2 px inside the row
            EXPECT_NEAR(s.pos(c->ring)->x, s.pos(c->root)->x + 2.0f, 0.5f);
            EXPECT_NEAR(s.pos(c->ring)->y, s.pos(c->root)->y + 2.0f, 0.5f);
            EXPECT_NEAR(s.pos(c->ring)->width, s.pos(c->root)->width - 4.0f, 0.5f);
            EXPECT_NEAR(s.pos(c->ring)->height, s.pos(c->root)->height - 4.0f, 0.5f);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // A click leaves the row current in the focus order, without a ring: Space (the Life scene's
        // month key) must not select it then. After Tab, with the ring showing, it does.
        TEST(activityrow_test, space_after_a_click_selects_nothing)
        {
            MockLogger logger;
            ActivityFixture s;

            ActivityListSpec spec;
            spec.groups = {{"Training", {idleSpec("a"), idleSpec("b")}}};
            ActivityList list = s.placeList(spec);

            ActivityRow* a = list.row(&s.ecs, "a");
            ASSERT_NE(a, nullptr);

            s.click(a->root);
            ASSERT_EQ(list.selected(), "a");

            list.select(&s.ecs, "");
            s.pump();
            ASSERT_EQ(list.selected(), "");

            s.key(SDL_SCANCODE_SPACE);
            EXPECT_EQ(list.selected(), "");
            EXPECT_TRUE(s.recorder->activated.empty());

            s.key(SDL_SCANCODE_RETURN);
            EXPECT_EQ(list.selected(), "");

            // With the keyboard on the row, Space selects it
            s.key(SDL_SCANCODE_TAB);
            ASSERT_TRUE(s.focusOrder->keyboardFocus());
            s.key(SDL_SCANCODE_SPACE);
            EXPECT_FALSE(list.selected().empty());
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(activityrow_test, stripes_alternate_across_groups)
        {
            MockLogger logger;
            ActivityFixture s;

            ActivityListSpec spec;
            spec.groups = {
                {"Training", {idleSpec("a"), idleSpec("b")}},
                {"Study", {idleSpec("c"), idleSpec("d"), idleSpec("e")}},
            };
            ActivityList list = s.placeList(spec);

            const std::vector<std::string> ids = {"a", "b", "c", "d", "e"};

            for (size_t i = 0; i < ids.size(); ++i)
            {
                ActivityRow* r = list.row(&s.ecs, ids[i]);
                ASSERT_NE(r, nullptr);

                EXPECT_EQ(s.state(r->root)->stripe, i % 2 == 1) << ids[i];
                EXPECT_EQ(s.element(r->ground), i % 2 == 1 ? "activity.row.ground.stripe" : "activity.row.ground") << ids[i];
                EXPECT_EQ(s.pos(r->rule)->isVisible(), i + 1 < ids.size()) << ids[i];
            }

            auto listState = s.ecs.getEntity(list.root.id)->get<ActivityListState>();
            ASSERT_EQ(listState->headings.size(), 2u);

            auto first = s.ecs.getEntity(listState->headings[0]);
            auto second = s.ecs.getEntity(listState->headings[1]);
            const _unique_id firstLabel = first->get<ActivityHeadingState>()->label;
            const _unique_id secondLabel = second->get<ActivityHeadingState>()->label;

            EXPECT_EQ(s.element(firstLabel), "activity.group");
            EXPECT_FLOAT_EQ(first->get<PositionComponent>()->height, 24.0f + 4.0f);
            EXPECT_FLOAT_EQ(second->get<PositionComponent>()->height, 12.0f + 24.0f + 4.0f);
            EXPECT_NEAR(s.pos(firstLabel)->y, first->get<PositionComponent>()->y, 0.5f);
            EXPECT_NEAR(s.pos(secondLabel)->y, second->get<PositionComponent>()->y + 12.0f, 0.5f);

            // The rows follow their heading with no gap
            ActivityRow* a = list.row(&s.ecs, "a");
            ActivityRow* b = list.row(&s.ecs, "b");
            ActivityRow* c = list.row(&s.ecs, "c");
            EXPECT_NEAR(s.pos(a->root)->y, first->get<PositionComponent>()->y + 28.0f, 0.5f);
            EXPECT_NEAR(second->get<PositionComponent>()->y, s.pos(b->root)->y + s.pos(b->root)->height, 0.5f);
            EXPECT_NEAR(s.pos(c->root)->y, second->get<PositionComponent>()->y + 40.0f, 0.5f);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(activityrow_test, set_row_state_restacks)
        {
            MockLogger logger;
            ActivityFixture s;

            // b carries the caption it will show once running: rule 10 + 4 + caption 15
            ActivityRowSpec bSpec = idleSpec("b");
            bSpec.caption = "MONTH 1 OF 6";

            ActivityListSpec spec;
            spec.groups = {{"Training", {idleSpec("a"), bSpec, idleSpec("c")}}};
            ActivityList list = s.placeList(spec);

            ActivityRow* b = list.row(&s.ecs, "b");
            ActivityRow* c = list.row(&s.ecs, "c");
            ASSERT_NE(b, nullptr);
            ASSERT_NE(c, nullptr);

            list.select(&s.ecs, "b");
            s.pump();
            ASSERT_EQ(list.selected(), "b");

            const float cY = s.pos(c->root)->y;

            list.setRowState(&s.ecs, "b", ActivityState::Running);
            s.pump();
            s.pump();

            EXPECT_FLOAT_EQ(s.pos(b->root)->height, 81.0f);
            EXPECT_NEAR(s.pos(c->root)->y, cY + 13.0f, 0.5f);
            EXPECT_EQ(list.selected(), "");
            EXPECT_EQ(s.element(b->edge), "activity.row.edge.running");
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // Rebuilt rows leave through the layout, which destroys each one as it lets it go: a row
        // is never destroyed while the layout still holds it (the list walks those every frame).
        TEST(activityrow_test, set_rows_never_leaves_a_dead_row_in_the_layout)
        {
            MockLogger logger;
            ActivityFixture s;

            ActivityListSpec spec;
            spec.groups = {{"Training", {idleSpec("a"), idleSpec("b")}}};
            ActivityList list = s.placeList(spec);

            auto layout = s.ecs.getEntity(list.body.id)->get<VerticalLayout>();
            ASSERT_EQ(layout->entities.size(), 3u);   // The heading and two rows

            std::vector<_unique_id> old;
            for (auto& e : layout->entities)
                old.push_back(e.id);

            const unsigned int errors = logger.getNbError();

            list.setRows(&s.ecs, {{"Work", {idleSpec("c")}}});

            // Until the layout has let them go, the old rows are alive
            for (auto id : old)
                EXPECT_NE(s.ecs.getEntity(id), nullptr) << id;

            for (int frame = 0; frame < 4; ++frame)
            {
                s.ecs.executeOnce();

                for (auto& e : layout->entities)
                    EXPECT_NE(s.ecs.getEntity(e.id), nullptr) << "frame " << frame << ": the layout holds a dead row " << e.id;
            }

            for (auto id : old)
                EXPECT_EQ(s.ecs.getEntity(id), nullptr) << id;

            EXPECT_EQ(layout->entities.size(), 2u);
            EXPECT_NE(list.row(&s.ecs, "c"), nullptr);
            EXPECT_EQ(logger.getNbError(), errors);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(activityrow_test, list_is_as_tall_as_its_rows)
        {
            MockLogger logger;
            ActivityFixture s;

            ActivityListSpec spec;
            spec.groups = {{"Training", {idleSpec("a"), idleSpec("b"), idleSpec("c")}}};
            ActivityList list = s.placeList(spec);
            s.pump();

            // The first heading (24 + 4) and three idle rows
            EXPECT_FLOAT_EQ(s.pos(list.root)->width, 620.0f);
            EXPECT_NEAR(s.pos(list.root)->height, 28.0f + 3.0f * 68.0f, 0.5f);
            EXPECT_NEAR(s.pos(list.body)->y, s.pos(list.root)->y, 0.5f);
            EXPECT_FALSE(s.ecs.getEntity(list.body.id)->get<VerticalLayout>()->scrollable);
            EXPECT_TRUE(list.scroll.empty());
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(activityrow_test, list_scrolls_when_given_a_height)
        {
            MockLogger logger;
            ActivityFixture s;

            ActivityListSpec spec;
            spec.height = 200.0f;
            spec.groups = {{"Training", {idleSpec("a"), idleSpec("b"), idleSpec("c"), idleSpec("d"), idleSpec("e")}}};
            ActivityList list = s.placeList(spec);
            s.pump();

            // The list keeps its height whatever it holds
            EXPECT_FLOAT_EQ(s.pos(list.root)->height, 200.0f);
            EXPECT_FLOAT_EQ(s.pos(list.body)->height, 200.0f);

            auto layout = s.ecs.getEntity(list.body.id)->get<VerticalLayout>();
            EXPECT_TRUE(layout->scrollable);
            EXPECT_NEAR(layout->contentHeight, 28.0f + 5.0f * 68.0f, 0.5f);

            // The rows and their parts are clipped to the body
            ActivityRow* a = list.row(&s.ecs, "a");
            ASSERT_NE(a, nullptr);

            auto rowEnt = s.ecs.getEntity(a->root.id);
            auto groundEnt = s.ecs.getEntity(a->ground.id);
            auto nameEnt = s.ecs.getEntity(a->name.entity.id);
            ASSERT_TRUE(rowEnt->has<ClippedTo>());
            EXPECT_EQ(rowEnt->get<ClippedTo>()->clipperId, list.body.id);
            EXPECT_TRUE(groundEnt->has<ClippedTo>());
            EXPECT_TRUE(nameEnt->has<ClippedTo>());

            // The thumb sits on the right edge, as tall as the share of the rows in view
            ASSERT_FALSE(list.scroll.empty());
            EXPECT_EQ(s.element(list.scroll), "activity.scroll");
            EXPECT_TRUE(s.pos(list.scroll)->isVisible());
            EXPECT_FLOAT_EQ(s.pos(list.scroll)->width, 4.0f);
            EXPECT_NEAR(s.pos(list.scroll)->x, s.pos(list.root)->x + 620.0f - 4.0f, 0.5f);
            EXPECT_NEAR(s.pos(list.scroll)->height, 200.0f * 200.0f / (28.0f + 5.0f * 68.0f), 0.5f);

            // A row out of view takes no click
            ActivityRow* e = list.row(&s.ecs, "e");
            ASSERT_NE(e, nullptr);
            EXPECT_GT(s.pos(e->root)->y, s.pos(list.root)->y + 200.0f);

            s.click(e->root);
            EXPECT_TRUE(s.recorder->selected.empty());
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // A press and a drag scroll a list that has a height; the drag's release (cancelled by the
        // layout) selects nothing, and a click in place still does.
        TEST(activityrow_test, list_drags_without_selecting)
        {
            MockLogger logger;
            ActivityFixture s;

            ActivityListSpec spec;
            spec.height = 200.0f;
            spec.groups = {{"Training", {idleSpec("a"), idleSpec("b"), idleSpec("c"), idleSpec("d"), idleSpec("e")}}};
            ActivityList list = s.placeList(spec);
            s.pump();

            auto layout = s.ecs.getEntity(list.body.id)->get<VerticalLayout>();
            EXPECT_TRUE(layout->dragToScroll);
            ASSERT_FLOAT_EQ(layout->yOffset, 0.0f);

            ActivityRow* b = list.row(&s.ecs, "b");
            ASSERT_NE(b, nullptr);

            const float x = s.pos(b->root)->x + 200.0f;
            const float y = s.pos(b->root)->y + 30.0f;

            s.hover(x, y);
            s.press(x, y);
            s.hover(x, y - 80.0f);

            EXPECT_FLOAT_EQ(layout->yOffset, 80.0f);

            // What the mouse click system sends for the release of a drag
            s.ecs.sendEvent(OnMouseRelease{Point2D{x, y - 80.0f}, static_cast<MouseButton>(1), true});
            s.pump();

            EXPECT_TRUE(s.recorder->selected.empty());
            EXPECT_EQ(list.selected(), "");

            // A click in place selects the row under it
            ActivityRow* c = list.row(&s.ecs, "c");
            ASSERT_NE(c, nullptr);
            s.click(c->root);

            EXPECT_EQ(list.selected(), "c");
            EXPECT_FLOAT_EQ(layout->yOffset, 80.0f);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(activityrow_test, set_name_and_glyph)
        {
            MockLogger logger;
            ActivityFixture s;

            ActivityRowSpec spec = idleSpec("train.yard", "Train");
            spec.rank = "RANK 2";
            spec.width = 288.0f;
            ActivityRow row = s.place(spec);

            // A short name hugs its text and the rank follows it
            EXPECT_EQ(row.name.spec.overflow, Overflow::Grow);
            EXPECT_NEAR(s.pos(row.rank->entity)->x, s.pos(row.name.entity)->x + s.pos(row.name.entity)->width + 8.0f, 0.5f);

            row.setName(&s.ecs, "Guard the eastern caravan to Bellmoor");
            row.setGlyph(&s.ecs, "swordsmanship");
            s.pump();

            // A long one is elided in the room left between the mark and the cost
            EXPECT_EQ(row.spec.name, "Guard the eastern caravan to Bellmoor");
            EXPECT_EQ(row.name.spec.overflow, Overflow::Ellipsis);
            EXPECT_LT(s.pos(row.rank->entity)->x + s.pos(row.rank->entity)->width, s.pos(row.costMark.entity)->x);
            EXPECT_EQ(row.mark.spec.name, "swordsmanship");

            row.setName(&s.ecs, "Rest");
            s.pump();

            EXPECT_EQ(row.name.spec.overflow, Overflow::Grow);
            EXPECT_NEAR(s.pos(row.rank->entity)->x, s.pos(row.name.entity)->x + s.pos(row.name.entity)->width + 8.0f, 0.5f);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(activityrow_test, gains_text)
        {
            MockLogger logger;
            ActivityFixture s;

            EXPECT_EQ(gainsText({{"STR", 1}, {"VIT", -2}}), pair("STR", "+1") + "   " + pair("VIT", "\xE2\x88\x92" "2"));
            EXPECT_EQ(gainsText({}), "");

            ActivityRowSpec spec = idleSpec("rest");
            spec.gains.clear();
            ActivityRow row = s.place(spec);

            ASSERT_TRUE(row.gains.has_value());
            EXPECT_EQ(row.gains->spec.text, "");
            EXPECT_FLOAT_EQ(s.pos(row.root)->height, 68.0f);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(activityrow_test, fed_from_worldfacts)
        {
            MockLogger logger;
            ActivityFixture s;

            ActivityListSpec spec;
            spec.groups = {{"Training", {runningSpec("study.letters"), lockedSpec("train.squire")}}};
            ActivityList list = s.placeList(spec);

            s.feed->onFact = [&](const std::string& name, const ElementType& v) {
                if (name == "activity.running.percent")
                {
                    if (auto r = list.row(&s.ecs, "study.letters"))
                        r->setPercent(&s.ecs, v.get<float>(), false);
                }
                else if (name == "activity.train.squire.state")
                {
                    list.setRowState(&s.ecs, "train.squire", v.get<std::string>() == "idle" ? ActivityState::Idle : ActivityState::Locked);
                }
                else if (name == "activity.train.squire.req.0.current")
                {
                    if (auto r = list.row(&s.ecs, "train.squire"))
                        r->setRequirement(&s.ecs, 0, v.get<int>(), 18);
                }
            };

            s.facts->setFact("activity.running.percent", 60.0f);
            s.facts->setFact("activity.train.squire.req.0.current", 18);

            // The update leaves on the first pass and is delivered on the next one
            s.pump();
            s.pump();

            ActivityRow* running = list.row(&s.ecs, "study.letters");
            ActivityRow* squire = list.row(&s.ecs, "train.squire");
            ASSERT_NE(running, nullptr);
            ASSERT_NE(squire, nullptr);
            ASSERT_TRUE(running->progress.has_value());
            EXPECT_FLOAT_EQ(running->progress->shown, 60.0f);
            ASSERT_TRUE(squire->reqs.has_value());
            EXPECT_TRUE(squire->reqs->rows[0].item.isMet());

            s.facts->setFact("activity.train.squire.state", std::string("idle"));
            s.pump();
            s.pump();

            EXPECT_EQ(squire->spec.state, ActivityState::Idle);
            EXPECT_TRUE(squire->gains.has_value());
            EXPECT_FALSE(squire->reqs.has_value());
            EXPECT_EQ(s.element(squire->name.entity), "activity.name");
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(activityrow_test, built_during_a_frame)
        {
            MockLogger logger;
            ActivityFixture s;

            ActivityRow idle;
            ActivityRow running;
            ActivityRow locked;
            ActivityList list;

            s.builder->build = [&](EntitySystem* ecs) {
                idle = makeActivityRow(ecs, idleSpec("alone.idle"));
                running = makeActivityRow(ecs, runningSpec("alone.running"));
                locked = makeActivityRow(ecs, lockedSpec("alone.locked"));

                ActivityListSpec spec;
                spec.groups = {
                    {"Training", {idleSpec("a"), lockedSpec("b")}},
                    {"Study", {runningSpec("c")}},
                };
                list = makeActivityList(ecs, spec);

                // The setters work on what the frame just built
                locked.setRequirement(ecs, 0, 18, 18);
                running.setPercent(ecs, 75.0f, false);
            };

            s.pump();
            s.pump();

            ASSERT_FALSE(idle.root.empty());
            ASSERT_FALSE(running.root.empty());
            ASSERT_FALSE(locked.root.empty());
            ASSERT_FALSE(list.root.empty());

            // A row born running or locked is painted so from its first frame, list or not
            EXPECT_FLOAT_EQ(s.pos(idle.root)->height, 68.0f);
            EXPECT_EQ(s.element(idle.ground), "activity.row.ground");

            EXPECT_FLOAT_EQ(s.pos(running.root)->height, 81.0f);
            EXPECT_EQ(s.element(running.ground), "activity.row.ground.running");
            EXPECT_EQ(s.element(running.edge), "activity.row.edge.running");
            EXPECT_EQ(s.element(running.mark.entity), "activity.mark.running");
            ASSERT_TRUE(running.progress.has_value());
            EXPECT_FLOAT_EQ(running.progress->shown, 75.0f);

            EXPECT_FLOAT_EQ(s.pos(locked.root)->height, 24.0f + 4.0f + 3.0f * 16.0f + 8.0f + 24.0f);
            EXPECT_EQ(s.element(locked.name.entity), "activity.name.locked");
            EXPECT_EQ(s.element(locked.cost.entity), "activity.cost.locked");
            ASSERT_TRUE(locked.reqs.has_value());
            EXPECT_TRUE(locked.reqs->rows[0].item.isMet());

            // The list adopted its rows: owner, stripes, last rule, selection
            ActivityRow* a = list.row(&s.ecs, "a");
            ActivityRow* b = list.row(&s.ecs, "b");
            ActivityRow* c = list.row(&s.ecs, "c");
            ASSERT_NE(a, nullptr);
            ASSERT_NE(b, nullptr);
            ASSERT_NE(c, nullptr);

            EXPECT_EQ(s.state(a->root)->list, list.root.id);
            EXPECT_FALSE(s.state(a->root)->stripe);
            EXPECT_TRUE(s.state(b->root)->stripe);
            EXPECT_TRUE(s.state(c->root)->last);
            EXPECT_FALSE(s.pos(c->rule)->isVisible());
            EXPECT_EQ(s.element(b->name.entity), "activity.name.locked");
            EXPECT_EQ(s.element(c->edge), "activity.row.edge.running");

            list.select(&s.ecs, "a");
            s.pump();

            EXPECT_EQ(list.selected(), "a");
            EXPECT_EQ(s.element(a->edge), "activity.row.edge.selected");
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(activityrow_test, theme_switch)
        {
            MockLogger logger;
            ActivityFixture s;

            ActivityRow row = s.place(runningSpec("study.letters"));

            s.theme->setTheme("candle");
            s.pump();

            const auto edge = s.ecs.getEntity(row.edge.id)->get<Simple2DObject>()->colors;
            EXPECT_FLOAT_EQ(edge.x, s.color("verdigris", "candle").x);
            EXPECT_FLOAT_EQ(edge.y, s.color("verdigris", "candle").y);
            EXPECT_FLOAT_EQ(edge.z, s.color("verdigris", "candle").z);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(activityrow_test, z_bands)
        {
            MockLogger logger;
            ActivityFixture s;

            ActivityRowSpec spec = runningSpec("study.letters");
            spec.rank = "RANK 2";
            spec.each = "AT THE ABBEY";
            spec.z = 20;
            ActivityRow row = s.place(spec);

            EXPECT_FLOAT_EQ(s.pos(row.root)->z, 20.0f);
            EXPECT_FLOAT_EQ(s.pos(row.ground)->z, 20.0f);
            EXPECT_FLOAT_EQ(s.pos(row.rule)->z, 21.0f);
            EXPECT_FLOAT_EQ(s.pos(row.edge)->z, 22.0f);
            EXPECT_FLOAT_EQ(s.pos(row.ring)->z, 22.0f);
            EXPECT_FLOAT_EQ(s.pos(row.mark.entity)->z, 23.0f);
            EXPECT_FLOAT_EQ(s.pos(row.costMark.entity)->z, 23.0f);
            EXPECT_FLOAT_EQ(s.pos(row.name.entity)->z, 24.0f);
            EXPECT_FLOAT_EQ(s.pos(row.rank->entity)->z, 24.0f);
            EXPECT_FLOAT_EQ(s.pos(row.cost.entity)->z, 24.0f);
            EXPECT_FLOAT_EQ(s.pos(row.each->entity)->z, 24.0f);

            ASSERT_TRUE(row.progress.has_value());
            EXPECT_FLOAT_EQ(s.pos(row.progress->root)->z, 23.0f);
            EXPECT_LE(s.pos(row.progress->frame)->z, 29.0f);
            ASSERT_TRUE(row.progress->nib.has_value());
            EXPECT_LE(s.pos(row.progress->nib->entity)->z, 29.0f);

            ActivityRow locked = s.place(lockedSpec("train.squire"), 100.0f, 300.0f);

            ASSERT_TRUE(locked.reqs.has_value());
            EXPECT_FLOAT_EQ(s.pos(locked.reqs->root)->z, 23.0f);
            EXPECT_LE(s.pos(locked.reqs->rows[0].name.label.entity)->z, 29.0f);
        }
    }
}
