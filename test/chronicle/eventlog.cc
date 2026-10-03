#include "stdafx.h"

#include <string>
#include <variant>
#include <vector>

#include <gtest/gtest.h>

#ifdef __linux__
#include <SDL2/SDL.h>
#elif _WIN32
#include <SDL.h>
#endif

#include "UI/eventlog.h"
#include "Core/motion.h"

#include "ECS/entitysystem.h"
#include "UI/themesystem.h"
#include "ECS/entitysystem_fwd.h"   // ResizeEvent
#include "UI/ttftext.h"
#include "UI/iconsystem.h"
#include "UI/sizer.h"
#include "UI/prefab.h"
#include "Input/inputcomponent.h"
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
            struct LogFixture
            {
                EntitySystem ecs;
                MasterRenderer renderer;
                TTFTextSystem* ttf = nullptr;
                IconSystem* icons = nullptr;
                ThemeSystem* theme = nullptr;
                WorldFacts* facts = nullptr;
                FactFeed* feed = nullptr;

                LogFixture()
                {
                    Motion::setReduced(true);
                    ecs.createSystem<PositionComponentSystem>();
                    ecs.createSystem<LayoutSystem>();
                    ecs.createSystem<PrefabSystem>();
                    ecs.succeed<PositionComponentSystem, LayoutSystem>();
                    ecs.succeed<PositionComponentSystem, PrefabSystem>();
                    ttf = ecs.createSystem<TTFTextSystem>(&renderer);
                    ecs.createSystem<Simple2DObjectSystem>(&renderer);
                    ecs.createSystem<HatchRect2DObjectSystem>(&renderer);
                    ecs.createSystem<DottedLine2DObjectSystem>(&renderer);
                    ecs.createSystem<StrokeRect2DObjectSystem>(&renderer);
                    icons = ecs.createSystem<IconSystem>(&renderer);
                    ecs.createSystem<MouseHoverSystem>();
                    ecs.createSystem<FocusableSystem>();   // The thumb is focusable: dragging it scrolls
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

                EventLog make(const EventLogSpec& spec, float x = 100.0f, float y = 100.0f)
                {
                    EventLog log = makeEventLog(&ecs, spec);
                    log.root->get<PositionComponent>()->setX(x);
                    log.root->get<PositionComponent>()->setY(y);
                    settle();
                    settle();

                    return log;
                }

                // The wheel as the input system sends it: the list's own event, with the delta.
                void wheel(const EventLog& log, int delta)
                {
                    auto event = ecs.getEntity(log.list.id)->get<MouseWheelComponent>()->event;
                    event.values["x"] = ElementType{0};
                    event.values["y"] = ElementType{delta};
                    ecs.sendEvent(event);
                    settle();
                }

                float asc(const std::string& style)
                {
                    const TextStyle& s = theme->style(style);

                    return ttf->measureText(s.fontAlias, "H", 1.0f, 0.0f, 0.0f, s.letterSpacingPx).ascender;
                }

                constant::Vector4D color(const std::string& token, const std::string& id) { return theme->theme().color(token, id); }

                CompRef<PositionComponent> pos(EntityRef e) { return ecs.getEntity(e.id)->get<PositionComponent>(); }

                float left(EntityRef e, const EventLog& log) { return pos(e)->x - pos(log.root)->x; }

                float top(EntityRef e, const EventLog& log) { return pos(e)->y - pos(log.root)->y; }

                float right(EntityRef e) { return pos(e)->x + pos(e)->width; }

                std::string element(EntityRef e) { return ecs.getEntity(e.id)->get<ThemeComponent>()->element; }

                std::string iconName(const Mark& mark) { return ecs.getEntity(mark.entity.id)->get<IconComponent>()->iconName; }

                VerticalLayout* layout(const EventLog& log) { return ecs.getEntity(log.list.id)->get<VerticalLayout>().component; }

                // A point inside the entity, tested against its clip chain.
                bool shown(EntityRef e) { return inClipBound(e, pos(e)->x + 1.0f, pos(e)->y + 1.0f); }
            };

            LogEntry entry(float age, const std::string& text, LogKind kind = LogKind::Note, const std::string& figure = "", const std::string& glyph = "")
            {
                LogEntry e;
                e.age = age;
                e.text = text;
                e.kind = kind;
                e.figure = figure;
                e.glyph = glyph;

                return e;
            }

            // n rows, ten a year from 7.
            std::vector<LogEntry> life(int n)
            {
                std::vector<LogEntry> entries;

                for (int i = 0; i < n; ++i)
                    entries.push_back(entry(7.0f + 0.1f * static_cast<float>(i), "Month " + std::to_string(i), i % 2 ? LogKind::Gain : LogKind::Note, i % 2 ? "+1 str" : ""));

                return entries;
            }

            const EventLog::Row& rowAt(const EventLog& log, size_t i) { return std::get<EventLog::Row>(log.items.at(i)); }

            const EventLog::Year& yearAt(const EventLog& log, size_t i) { return std::get<EventLog::Year>(log.items.at(i)); }

            // The n-th row, not counting rubrics.
            const EventLog::Row& nthRow(const EventLog& log, size_t n)
            {
                for (const auto& item : log.items)
                {
                    if (const auto* row = std::get_if<EventLog::Row>(&item))
                    {
                        if (n == 0)
                            return *row;

                        --n;
                    }
                }

                return std::get<EventLog::Row>(log.items.back());
            }
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(eventlog_test, gutter_geometry)
        {
            MockLogger logger;
            LogFixture s;

            EventLog log = s.make(EventLogSpec{});

            EXPECT_FLOAT_EQ(s.pos(log.root)->width, 300.0f);
            EXPECT_FLOAT_EQ(s.pos(log.root)->height, 560.0f);

            EXPECT_FLOAT_EQ(s.pos(log.gutter)->width, 300.0f);
            EXPECT_FLOAT_EQ(s.pos(log.gutter)->height, 560.0f);
            EXPECT_EQ(s.element(log.gutter), "log.gutter");

            EXPECT_NEAR(s.left(log.edge, log), 0.0f, 0.01f);
            EXPECT_FLOAT_EQ(s.pos(log.edge)->width, 2.0f);
            EXPECT_FLOAT_EQ(s.pos(log.edge)->height, 560.0f);
            EXPECT_EQ(s.element(log.edge), "log.edge");

            EXPECT_NEAR(s.left(log.list, log), 14.0f, 0.01f);
            EXPECT_NEAR(s.top(log.list, log), 8.0f, 0.01f);
            // The lines are 274; the list runs 8 px further right, the thumb's lane
            EXPECT_FLOAT_EQ(s.pos(log.list)->width, 282.0f);
            EXPECT_FLOAT_EQ(s.pos(log.list)->height, 544.0f);
            EXPECT_FLOAT_EQ(log.lineWidth(), 274.0f);
            EXPECT_TRUE(s.layout(log)->scrollable);
            EXPECT_EQ(s.layout(log)->spacing, 1u);
            EXPECT_TRUE(s.ecs.getEntity(log.list.id)->has<MouseWheelComponent>());

            EXPECT_EQ(log.size(), 0u);
            EXPECT_TRUE(log.atEnd(&s.ecs));

            // Nothing to scroll: no thumb
            EXPECT_EQ(s.element(log.scroll), "log.scroll");
            EXPECT_EQ(s.layout(log)->verticalScrollBar.id, log.scroll.id);
            EXPECT_FALSE(s.pos(log.scroll)->isVisible());
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(eventlog_test, rubrics_on_year_change)
        {
            MockLogger logger;
            LogFixture s;

            EventLogSpec spec;
            spec.entries = {entry(14.2f, "Apprenticed at the yard"), entry(14.8f, "A fever in the spring"), entry(15.1f, "Came of age at the guild")};
            EventLog log = s.make(spec);

            ASSERT_EQ(log.items.size(), 5u);
            EXPECT_TRUE(std::holds_alternative<EventLog::Year>(log.items[0]));
            EXPECT_TRUE(std::holds_alternative<EventLog::Row>(log.items[1]));
            EXPECT_TRUE(std::holds_alternative<EventLog::Row>(log.items[2]));
            EXPECT_TRUE(std::holds_alternative<EventLog::Year>(log.items[3]));
            EXPECT_TRUE(std::holds_alternative<EventLog::Row>(log.items[4]));
            EXPECT_EQ(log.size(), 3u);
            EXPECT_EQ(log.lastYear, 15);

            const auto& first = yearAt(log, 0);
            EXPECT_EQ(first.year, 14);
            EXPECT_EQ(first.text, "IN HIS 14TH YEAR");

            // The words in the display face, the ordinal in the text face's lining figures
            ASSERT_TRUE(first.rubric.has_value());
            EXPECT_EQ(first.rubric->spec.text, "IN HIS");
            EXPECT_EQ(first.rubric->spec.style, "gloss-title");
            EXPECT_EQ(s.element(first.rubric->entity), "log.year");
            EXPECT_EQ(first.ordinal.spec.text, "14TH");
            EXPECT_EQ(first.ordinal.spec.style, "figure");
            EXPECT_EQ(s.element(first.ordinal.entity), "log.year.ordinal");
            EXPECT_EQ(first.suffix.spec.text, "YEAR");
            EXPECT_EQ(s.element(first.suffix.entity), "log.year");

            // One baseline, in reading order, a space apart
            const float baseline = s.pos(first.rubric->entity)->y + s.asc("gloss-title");
            EXPECT_NEAR(s.pos(first.ordinal.entity)->y + s.asc("figure"), baseline, 0.5f);
            EXPECT_NEAR(s.pos(first.suffix.entity)->y + s.asc("gloss-title"), baseline, 0.5f);
            EXPECT_GT(s.pos(first.ordinal.entity)->x, s.right(first.rubric->entity));
            EXPECT_GT(s.pos(first.suffix.entity)->x, s.right(first.ordinal.entity));
            EXPECT_LT(s.pos(first.ordinal.entity)->x - s.right(first.rubric->entity), 8.0f);

            // The first rubric has nothing above it; the next has 12 px
            EXPECT_FLOAT_EQ(s.pos(first.line)->height, 22.0f);
            EXPECT_NEAR(s.pos(first.rubric->entity)->y, s.pos(first.line)->y, 0.01f);
            EXPECT_NEAR(s.pos(first.line)->y, s.pos(log.list)->y, 0.01f);

            const auto& second = yearAt(log, 3);
            EXPECT_EQ(second.text, "IN HIS 15TH YEAR");
            EXPECT_FLOAT_EQ(s.pos(second.line)->height, 34.0f);
            EXPECT_NEAR(s.pos(second.rubric->entity)->y, s.pos(second.line)->y + 12.0f, 0.01f);

            // Lines one pixel apart
            EXPECT_NEAR(s.pos(rowAt(log, 1).line)->y, s.pos(first.line)->y + 22.0f + 1.0f, 0.01f);
            EXPECT_NEAR(s.pos(rowAt(log, 2).line)->y, s.pos(rowAt(log, 1).line)->y + 20.0f + 1.0f, 0.01f);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(eventlog_test, ordinals)
        {
            const std::vector<std::pair<int, std::string>> cases = {
                {1, "1ST"}, {2, "2ND"}, {3, "3RD"}, {4, "4TH"}, {11, "11TH"}, {12, "12TH"}, {13, "13TH"},
                {21, "21ST"}, {22, "22ND"}, {23, "23RD"}, {43, "43RD"}, {111, "111TH"}, {101, "101ST"},
            };

            for (const auto& [n, text] : cases)
                EXPECT_EQ(ordinal(n), text) << n;
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // Ages are written year.month, the month 1 to 12, and the rubric's year counts the same
        // whole months: 16.12 is followed by 17.1, under the 17th year.
        TEST(eventlog_test, ages_count_months)
        {
            EXPECT_EQ(logAge(7.0f), "7.1");
            EXPECT_EQ(logAge(14.3f), "14.4");
            EXPECT_EQ(logAge(16.9f), "16.11");
            EXPECT_EQ(logAge(43.95f), "43.12");

            float age = 16.9f;
            std::vector<std::string> months;

            for (int i = 0; i < 4; ++i)
            {
                months.push_back(logAge(age));
                age += 1.0f / 12.0f;
            }

            EXPECT_EQ(months, (std::vector<std::string>{"16.11", "16.12", "17.1", "17.2"}));

            EXPECT_EQ(logYear(16.9f + 1.0f / 12.0f), 16);
            EXPECT_EQ(logYear(16.9f + 2.0f / 12.0f), 17);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(eventlog_test, row_geometry_and_kinds)
        {
            MockLogger logger;
            LogFixture s;

            EventLogSpec spec;
            spec.entries = {
                entry(14.3f, "Gored in the North Forest", LogKind::Loss, "\xE2\x88\x92" "9 vit"),
                entry(14.4f, "Stronger for the winter", LogKind::Gain, "+1 str"),
                entry(14.5f, "Paid for the harvest", LogKind::Coin, "+12"),
                entry(14.6f, "Took the Squire's oath", LogKind::Milestone),
                entry(14.7f, "It rained all summer"),
            };
            EventLog log = s.make(spec);

            const auto& loss = rowAt(log, 1);
            EXPECT_EQ(loss.age.spec.text, "14.4");   // In his fourth month
            EXPECT_EQ(s.element(loss.age.entity), "log.age");
            EXPECT_FLOAT_EQ(s.pos(loss.age.entity)->width, 30.0f);
            EXPECT_NEAR(s.pos(loss.age.entity)->x, s.pos(loss.line)->x, 0.01f);

            EXPECT_EQ(s.iconName(loss.mark), "cross");
            EXPECT_EQ(loss.mark.spec.size, MarkSize::S14);
            EXPECT_EQ(s.element(loss.mark.entity), "log.mark.loss");
            EXPECT_NEAR(s.pos(loss.mark.entity)->x, s.pos(loss.line)->x + 38.0f, 0.01f);
            EXPECT_NEAR(s.pos(loss.mark.entity)->y, s.pos(loss.line)->y + 3.0f, 0.01f);

            EXPECT_EQ(s.element(loss.text.entity), "log.text.loss");
            EXPECT_EQ(loss.text.spec.style, "body-sm");
            EXPECT_NEAR(s.pos(loss.text.entity)->x, s.pos(loss.line)->x + 60.0f, 0.01f);

            ASSERT_TRUE(loss.figure.has_value());
            EXPECT_EQ(loss.figure->spec.text, "\xE2\x88\x92" "9 vit");
            EXPECT_EQ(s.element(loss.figure->entity), "log.figure.loss");
            EXPECT_NEAR(s.right(loss.figure->entity), s.right(loss.line), 0.01f);

            const float baseline = s.pos(loss.text.entity)->y + s.asc("body-sm");
            EXPECT_NEAR(s.pos(loss.age.entity)->y + s.asc("caption"), baseline, 0.5f);
            EXPECT_NEAR(s.pos(loss.figure->entity)->y + s.asc("figure-sm"), baseline, 0.5f);

            const auto& gain = rowAt(log, 2);
            EXPECT_EQ(s.iconName(gain.mark), "check");
            EXPECT_EQ(s.element(gain.mark.entity), "log.mark.gain");
            EXPECT_EQ(s.element(gain.text.entity), "log.text.gain");
            EXPECT_EQ(s.element(gain.figure->entity), "log.figure.gain");

            const auto& coin = rowAt(log, 3);
            EXPECT_EQ(s.iconName(coin.mark), "gold");
            EXPECT_EQ(s.element(coin.mark.entity), "log.mark.coin");
            EXPECT_EQ(s.element(coin.text.entity), "log.text");
            EXPECT_EQ(s.element(coin.figure->entity), "log.figure.coin");

            const auto& milestone = rowAt(log, 4);
            EXPECT_EQ(s.iconName(milestone.mark), "seal");
            EXPECT_EQ(s.element(milestone.mark.entity), "log.mark.milestone");
            EXPECT_EQ(s.element(milestone.text.entity), "log.text.milestone");
            EXPECT_EQ(milestone.text.spec.style, "figure-sm");
            EXPECT_FALSE(milestone.figure.has_value());

            const auto& note = rowAt(log, 5);
            EXPECT_EQ(s.iconName(note.mark), "quill");
            EXPECT_EQ(s.element(note.mark.entity), "log.mark");
            EXPECT_EQ(s.element(note.text.entity), "log.text.note");
            EXPECT_EQ(note.text.spec.style, "gloss");
            EXPECT_FALSE(note.figure.has_value());

            // The italic note keeps the row's baseline
            EXPECT_NEAR(s.pos(note.age.entity)->y + s.asc("caption"), s.pos(note.text.entity)->y + s.asc("gloss"), 0.5f);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(eventlog_test, explicit_glyph_wins)
        {
            MockLogger logger;
            LogFixture s;

            EventLogSpec spec;
            spec.entries = {entry(14.3f, "Carried the anvil", LogKind::Gain, "+1 str", "strength")};
            EventLog log = s.make(spec);

            const auto& row = rowAt(log, 1);
            EXPECT_EQ(s.iconName(row.mark), "strength");
            EXPECT_EQ(s.element(row.mark.entity), "log.mark.gain");
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // A long line is read whole: it wraps in the room the figure leaves, the row grows by the
        // lines it takes, and the next row stands under it. Nothing is cut.
        TEST(eventlog_test, text_wraps_in_the_room_the_figure_leaves)
        {
            MockLogger logger;
            LogFixture s;

            const std::string longText = "Gored in the North Forest by a boar he had been told twice to leave alone";

            EventLogSpec spec;
            spec.entries = {
                entry(14.3f, longText, LogKind::Loss, "\xE2\x88\x92" "9 vit"),
                entry(14.4f, "Stronger for the winter", LogKind::Gain, "+1 str"),
            };
            EventLog log = s.make(spec);

            const auto& row = rowAt(log, 1);
            const auto& next = rowAt(log, 2);

            // On one line it would not fit; wrapped, it is whole
            TextLayoutParams params;
            params.maxWidth = row.text.spec.width;
            params.overflow = Overflow::Ellipsis;
            params.letterSpacing = row.text.letterSpacingPx;
            params.spacing = row.text.lineSpacingPx;
            EXPECT_TRUE(s.ttf->measureText(row.text.fontAlias, longText, params).elided);

            EXPECT_EQ(row.text.spec.overflow, Overflow::Wrap);
            EXPECT_EQ(row.text.spec.text, longText);

            // The row is as tall as its text, several lines of 20
            const float textHeight = s.pos(row.text.entity)->height;

            EXPECT_GE(textHeight, 40.0f);
            EXPECT_NEAR(s.pos(row.line)->height, textHeight, 0.01f);

            // The figure is whole, on the first line, and the text keeps clear of it
            EXPECT_EQ(row.figure->spec.text, "\xE2\x88\x92" "9 vit");
            EXPECT_LE(s.right(row.text.entity), s.pos(row.figure->entity)->x - 8.0f + 0.01f);
            EXPECT_NEAR(s.right(row.figure->entity), s.right(row.line), 0.01f);
            EXPECT_LT(s.pos(row.figure->entity)->y, s.pos(row.line)->y + 20.0f);
            EXPECT_LT(s.pos(row.age.entity)->y, s.pos(row.line)->y + 20.0f);

            // The next row stands under the whole of it, one pixel apart, and a short one is one line
            EXPECT_NEAR(s.pos(next.line)->y, s.pos(row.line)->y + textHeight + 1.0f, 0.01f);
            EXPECT_FLOAT_EQ(s.pos(next.line)->height, 20.0f);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // A figure too long to share the line (many things gained at once) goes under the text,
        // wrapped on the same right edge: the text takes the whole line, and the row is as tall as
        // the two.
        TEST(eventlog_test, a_long_figure_goes_under_the_text)
        {
            MockLogger logger;
            LogFixture s;

            const std::string many = "+100\xC2\xA0" "coin +1\xC2\xA0" "ledgers +1\xC2\xA0" "enemy +1\xC2\xA0" "stealth +1\xC2\xA0" "guile";

            EventLogSpec spec;
            spec.entries = {
                entry(22.5f, "Rob the Counting House", LogKind::Gain, many),
                entry(22.6f, "Lay low", LogKind::Note),
            };
            EventLog log = s.make(spec);

            const auto& row = rowAt(log, 1);
            const auto& next = rowAt(log, 2);

            ASSERT_TRUE(row.figure.has_value());
            EXPECT_EQ(row.figure->spec.text, many);
            EXPECT_EQ(row.figure->spec.overflow, Overflow::Wrap);
            EXPECT_EQ(row.figure->spec.align, Align::Right);

            // The text has the whole line, the figure the same room under it
            EXPECT_NEAR(row.text.spec.width, row.figure->spec.width, 0.01f);
            EXPECT_FLOAT_EQ(s.pos(row.text.entity)->height, 20.0f);
            EXPECT_NEAR(s.pos(row.figure->entity)->y, s.pos(row.line)->y + 20.0f, 0.01f);
            EXPECT_NEAR(s.right(row.figure->entity), s.right(row.line), 0.01f);
            EXPECT_GE(s.pos(row.figure->entity)->height, 40.0f);

            // The row holds both, and the next one stands under it
            EXPECT_NEAR(s.pos(row.line)->height, 20.0f + s.pos(row.figure->entity)->height, 0.01f);
            EXPECT_NEAR(s.pos(next.line)->y, s.pos(row.line)->y + s.pos(row.line)->height + 1.0f, 0.01f);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(eventlog_test, footnote)
        {
            MockLogger logger;
            LogFixture s;

            EventLog log = s.make(EventLogSpec{});

            log.setFootnote(&s.ecs, "THE CHRONICLE IS WRITTEN FROM THIS LOG");
            s.settle();

            ASSERT_TRUE(log.footnote.has_value());
            EXPECT_EQ(s.element(log.footnote->entity), "log.footnote");
            EXPECT_NEAR(s.top(log.footnote->entity, log), 564.0f, 0.01f);

            const float footHeight = s.pos(log.footnote->entity)->height;
            EXPECT_FLOAT_EQ(footHeight, 15.0f);
            EXPECT_FLOAT_EQ(s.pos(log.root)->height, 564.0f + footHeight);

            log.setFootnote(&s.ecs, "");
            s.settle();

            EXPECT_FALSE(log.footnote.has_value());
            EXPECT_FLOAT_EQ(s.pos(log.root)->height, 560.0f);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(eventlog_test, rows_outside_clip_are_culled)
        {
            MockLogger logger;
            LogFixture s;

            EventLogSpec spec;
            spec.entries = life(60);
            EventLog log = s.make(spec);

            ASSERT_EQ(log.size(), 60u);

            // Built at the end: the first rows are above the well and clipped
            EXPECT_TRUE(log.atEnd(&s.ecs));
            EXPECT_LT(s.pos(nthRow(log, 0).line)->y, s.pos(log.list)->y);
            EXPECT_FALSE(s.shown(nthRow(log, 0).line));
            EXPECT_FALSE(s.shown(nthRow(log, 0).text.entity));
            EXPECT_TRUE(s.shown(nthRow(log, 59).line));

            // At the top: the last rows are below the well and clipped, and still there
            s.wheel(log, 1000);

            EXPECT_FLOAT_EQ(s.layout(log)->yOffset, 0.0f);
            EXPECT_TRUE(s.shown(nthRow(log, 0).line));

            const float listBottom = s.pos(log.list)->y + s.pos(log.list)->height;
            const auto& last = nthRow(log, 59);
            EXPECT_GT(s.pos(last.line)->y, listBottom);
            EXPECT_FALSE(s.shown(last.line));
            EXPECT_FALSE(s.shown(last.text.entity));
            EXPECT_FALSE(s.shown(last.mark.entity));
            EXPECT_EQ(log.size(), 60u);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(eventlog_test, wheel_scrolls)
        {
            MockLogger logger;
            LogFixture s;

            EventLogSpec spec;
            spec.entries = life(60);
            EventLog log = s.make(spec);

            s.wheel(log, 1000);
            ASSERT_FLOAT_EQ(s.layout(log)->yOffset, 0.0f);

            const float firstY = s.pos(nthRow(log, 0).line)->y;

            // A notch down: the content moves up by the layout's scroll speed
            s.wheel(log, -2);

            EXPECT_NEAR(s.pos(nthRow(log, 0).line)->y, firstY - 2.0f * s.layout(log)->scrollSpeed, 0.01f);
            EXPECT_FALSE(log.atEnd(&s.ecs));

            // Never past the end
            s.wheel(log, -1000);

            EXPECT_TRUE(log.atEnd(&s.ecs));
            EXPECT_FLOAT_EQ(s.layout(log)->yOffset, s.layout(log)->contentHeight - s.pos(log.list)->height);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // A press and a drag in the well scroll the life, as the wheel does.
        TEST(eventlog_test, drag_scrolls)
        {
            MockLogger logger;
            LogFixture s;

            EventLogSpec spec;
            spec.entries = life(60);
            EventLog log = s.make(spec);

            s.wheel(log, 1000);
            ASSERT_FLOAT_EQ(s.layout(log)->yOffset, 0.0f);
            EXPECT_TRUE(s.layout(log)->dragToScroll);

            const float x = s.pos(log.list)->x + 100.0f;
            const float y = s.pos(log.list)->y + 300.0f;

            s.ecs.sendEvent(OnMouseClick{Point2D{x, y}, SDL_BUTTON_LEFT});
            s.ecs.sendEvent(OnMouseMove{Point2D{x, y - 120.0f}, nullptr});
            s.settle();

            EXPECT_FLOAT_EQ(s.layout(log)->yOffset, 120.0f);
            EXPECT_FALSE(log.atEnd(&s.ecs));

            // Dragged all the way: at the end, and new rows are followed again
            s.ecs.sendEvent(OnMouseMove{Point2D{x, y - 5000.0f}, nullptr});
            s.ecs.sendEvent(OnMouseRelease{Point2D{x, y - 5000.0f}, SDL_BUTTON_LEFT, true});
            s.settle();

            EXPECT_TRUE(log.atEnd(&s.ecs));
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(eventlog_test, no_autoscroll_while_reading_up)
        {
            MockLogger logger;
            LogFixture s;

            EventLogSpec spec;
            spec.entries = life(60);
            EventLog log = s.make(spec);

            log.scrollToEnd(&s.ecs);
            s.settle();
            ASSERT_TRUE(log.atEnd(&s.ecs));

            // At the end, a new row: the view follows it
            log.append(&s.ecs, entry(13.1f, "A letter from the Guild", LogKind::Coin, "+5"));
            s.settle();

            EXPECT_TRUE(log.atEnd(&s.ecs));
            EXPECT_TRUE(s.shown(nthRow(log, 60).line));

            // Reading further up: new rows do not move the view
            s.wheel(log, 4);
            ASSERT_FALSE(log.atEnd(&s.ecs));

            const float offset = s.layout(log)->yOffset;
            const float firstY = s.pos(nthRow(log, 0).line)->y;

            log.append(&s.ecs, entry(13.2f, "Snow on the pass"));
            log.append(&s.ecs, entry(14.0f, "Turned fourteen", LogKind::Milestone));
            s.settle();

            EXPECT_FLOAT_EQ(s.layout(log)->yOffset, offset);
            EXPECT_FLOAT_EQ(s.pos(nthRow(log, 0).line)->y, firstY);
            EXPECT_FALSE(log.atEnd(&s.ecs));
            EXPECT_EQ(log.size(), 63u);

            // Back to the end on request
            log.scrollToEnd(&s.ecs);
            s.settle();

            EXPECT_TRUE(log.atEnd(&s.ecs));
            EXPECT_TRUE(s.shown(nthRow(log, 62).line));
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(eventlog_test, scroll_thumb)
        {
            MockLogger logger;
            LogFixture s;

            EventLogSpec spec;
            spec.entries = life(60);
            EventLog log = s.make(spec);

            auto thumb = s.pos(log.scroll);
            const auto list = s.pos(log.list);
            const float content = s.layout(log)->contentHeight;

            // Shown once the rows overflow, in the lane right of the lines, sized to the view
            EXPECT_TRUE(thumb->isVisible());
            EXPECT_FLOAT_EQ(thumb->width, 4.0f);
            EXPECT_NEAR(thumb->x + thumb->width, list->x + list->width, 0.01f);
            EXPECT_GE(thumb->x, s.right(nthRow(log, 59).line) + 4.0f);
            EXPECT_NEAR(thumb->height, list->height * list->height / content, 0.01f);

            // At the end, it sits at the bottom of the track; at the top, at the top
            EXPECT_NEAR(thumb->y + thumb->height, list->y + list->height, 0.01f);

            s.wheel(log, 1000);
            EXPECT_NEAR(s.pos(log.scroll)->y, list->y, 0.01f);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // A new line and its parts are not drawn until the layout has placed the line in view:
        // drawn at once, a mark would show for a frame where it was made.
        TEST(eventlog_test, parts_hidden_until_placed)
        {
            MockLogger logger;
            LogFixture s;

            EventLogSpec spec;
            spec.entries = life(60);
            EventLog log = s.make(spec);

            log.append(&s.ecs, entry(13.1f, "A letter from the Guild", LogKind::Coin, "+5"));

            const auto& row = nthRow(log, 60);
            EXPECT_FALSE(s.pos(row.line)->isRenderable());
            EXPECT_FALSE(s.pos(row.mark.entity)->isRenderable());
            EXPECT_FALSE(s.pos(row.text.entity)->isRenderable());

            s.settle();

            // At the end, it is followed into view, and drawn where it belongs
            EXPECT_TRUE(s.pos(row.line)->isRenderable());
            EXPECT_TRUE(s.pos(row.mark.entity)->isRenderable());
            EXPECT_TRUE(s.pos(row.age.entity)->isRenderable());
            EXPECT_TRUE(s.pos(row.text.entity)->isRenderable());
            EXPECT_TRUE(s.pos(row.figure->entity)->isRenderable());
            EXPECT_NEAR(s.pos(row.mark.entity)->x, s.pos(row.line)->x + 38.0f, 0.01f);

            // Reading up: the next one lands below the view and stays undrawn
            s.wheel(log, 4);
            log.append(&s.ecs, entry(13.2f, "Snow on the pass"));
            s.settle();

            const auto& below = nthRow(log, 61);
            EXPECT_FALSE(s.pos(below.line)->isRenderable());
            EXPECT_FALSE(s.pos(below.mark.entity)->isRenderable());
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(eventlog_test, clear)
        {
            MockLogger logger;
            LogFixture s;

            EventLogSpec spec;
            spec.entries = life(12);
            EventLog log = s.make(spec);

            const EntityRef oldLine = nthRow(log, 0).line;
            const EntityRef oldText = nthRow(log, 0).text.entity;

            log.clear(&s.ecs);
            s.settle();

            EXPECT_EQ(log.size(), 0u);
            EXPECT_TRUE(log.items.empty());
            EXPECT_EQ(log.lastYear, -1);
            EXPECT_TRUE(s.layout(log)->entities.empty());
            EXPECT_EQ(s.ecs.getEntity(oldLine.id), nullptr);
            EXPECT_EQ(s.ecs.getEntity(oldText.id), nullptr);

            log.append(&s.ecs, entry(7.5f, "Born again, in a manner of speaking"));
            s.settle();

            ASSERT_EQ(log.items.size(), 2u);
            EXPECT_TRUE(std::holds_alternative<EventLog::Year>(log.items[0]));
            EXPECT_EQ(yearAt(log, 0).text, "IN HIS 7TH YEAR");
            EXPECT_FLOAT_EQ(s.pos(yearAt(log, 0).line)->height, 22.0f);
            EXPECT_NEAR(s.pos(yearAt(log, 0).line)->y, s.pos(log.list)->y, 0.01f);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // The scene owns what happens and calls append from its own handler: nothing else is needed.
        TEST(eventlog_test, fed_from_worldfacts)
        {
            MockLogger logger;
            LogFixture s;

            EventLog log = s.make(EventLogSpec{});
            float age = 14.0f;

            s.feed->onFact = [&](const std::string& name, const ElementType& v) {
                if (name == "life.age")
                    age = v.get<float>();
                else if (name == "life.event")
                    log.append(&s.ecs, entry(age, v.get<std::string>(), LogKind::Gain, "+1 str"));
            };

            s.facts->setFact("life.age", 14.3f);
            s.settle();
            s.settle();
            s.facts->setFact("life.event", std::string("Carried the anvil"));
            s.settle();
            s.settle();

            ASSERT_EQ(log.size(), 1u);
            ASSERT_EQ(log.items.size(), 2u);
            EXPECT_EQ(rowAt(log, 1).age.spec.text, "14.4");
            EXPECT_EQ(rowAt(log, 1).text.spec.text, "Carried the anvil");
            EXPECT_EQ(s.element(rowAt(log, 1).text.entity), "log.text.gain");
            EXPECT_NEAR(s.pos(rowAt(log, 1).line)->y, s.pos(log.list)->y + 23.0f, 0.01f);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(eventlog_test, theme_switch)
        {
            MockLogger logger;
            LogFixture s;

            EventLogSpec spec;
            spec.entries = {entry(14.3f, "Gored in the North Forest", LogKind::Loss, "\xE2\x88\x92" "9 vit")};
            EventLog log = s.make(spec);

            s.theme->setTheme("candle");
            s.settle();

            const auto gutter = s.ecs.getEntity(log.gutter.id)->get<Simple2DObject>()->colors;
            EXPECT_FLOAT_EQ(gutter.x, s.color("vellum-worn", "candle").x);
            EXPECT_FLOAT_EQ(gutter.y, s.color("vellum-worn", "candle").y);
            EXPECT_FLOAT_EQ(gutter.z, s.color("vellum-worn", "candle").z);

            // status-loss is vermilion
            const auto figure = s.ecs.getEntity(rowAt(log, 1).figure->entity.id)->get<TTFText>()->colors;
            EXPECT_FLOAT_EQ(figure.x, s.color("vermilion", "candle").x);
            EXPECT_FLOAT_EQ(figure.y, s.color("vermilion", "candle").y);
            EXPECT_FLOAT_EQ(figure.z, s.color("vermilion", "candle").z);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(eventlog_test, z_bands)
        {
            MockLogger logger;
            LogFixture s;

            EventLogSpec spec;
            spec.entries = {entry(14.3f, "Gored in the North Forest", LogKind::Loss, "\xE2\x88\x92" "9 vit")};
            spec.footnote = "THE CHRONICLE IS WRITTEN FROM THIS LOG";
            spec.z = 20;
            EventLog log = s.make(spec);

            const auto& year = yearAt(log, 0);
            const auto& row = rowAt(log, 1);

            EXPECT_FLOAT_EQ(s.pos(log.root)->z, 20.0f);
            EXPECT_FLOAT_EQ(s.pos(log.gutter)->z, 20.0f);
            EXPECT_FLOAT_EQ(s.pos(log.edge)->z, 21.0f);
            EXPECT_FLOAT_EQ(s.pos(log.footnote->entity)->z, 21.0f);
            EXPECT_FLOAT_EQ(s.pos(log.list)->z, 22.0f);
            // A layout holds its children at its own z: the lines share the list's
            EXPECT_FLOAT_EQ(s.pos(year.line)->z, 22.0f);
            EXPECT_FLOAT_EQ(s.pos(row.line)->z, 22.0f);
            EXPECT_FLOAT_EQ(s.pos(row.mark.entity)->z, 24.0f);
            EXPECT_FLOAT_EQ(s.pos(row.age.entity)->z, 24.0f);
            EXPECT_FLOAT_EQ(s.pos(row.text.entity)->z, 25.0f);
            EXPECT_FLOAT_EQ(s.pos(row.figure->entity)->z, 25.0f);
            EXPECT_FLOAT_EQ(s.pos(year.rubric->entity)->z, 25.0f);
            EXPECT_FLOAT_EQ(s.pos(year.ordinal.entity)->z, 25.0f);
            EXPECT_FLOAT_EQ(s.pos(year.suffix.entity)->z, 25.0f);
            EXPECT_FLOAT_EQ(s.pos(log.scroll)->z, 23.0f);

            for (auto e : {log.root, log.gutter, log.edge, log.list, log.scroll, year.line, row.line, row.mark.entity, row.age.entity, row.text.entity, row.figure->entity, year.rubric->entity, year.ordinal.entity})
            {
                const float z = s.pos(e)->z;
                EXPECT_FLOAT_EQ(z, static_cast<float>(static_cast<int>(z)));
            }
        }
    }
}
