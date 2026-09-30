#include "stdafx.h"

#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "UI/resourceledger.h"
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
            struct LedgerFixture
            {
                EntitySystem ecs;
                MasterRenderer renderer;
                TTFTextSystem* ttf = nullptr;
                IconSystem* icons = nullptr;
                ThemeSystem* theme = nullptr;
                WorldFacts* facts = nullptr;
                FactFeed* feed = nullptr;

                LedgerFixture()
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

                ResourceLedger make(const ResourceLedgerSpec& spec, float x = 100.0f, float y = 100.0f)
                {
                    ResourceLedger ledger = makeResourceLedger(&ecs, spec);
                    ledger.root->get<PositionComponent>()->setX(x);
                    ledger.root->get<PositionComponent>()->setY(y);
                    settle();
                    settle();

                    return ledger;
                }

                float asc(const std::string& style)
                {
                    const TextStyle& s = theme->style(style);

                    return ttf->measureText(s.fontAlias, "H", 1.0f, 0.0f, 0.0f, s.letterSpacingPx).ascender;
                }

                constant::Vector4D color(const std::string& token, const std::string& id) { return theme->theme().color(token, id); }

                CompRef<PositionComponent> pos(EntityRef e) { return ecs.getEntity(e.id)->get<PositionComponent>(); }

                float left(EntityRef e, const ResourceLedger& l) { return pos(e)->x - pos(l.root)->x; }

                float right(EntityRef e, const ResourceLedger& l) { return left(e, l) + pos(e)->width; }

                float top(EntityRef e, const ResourceLedger& l) { return pos(e)->y - pos(l.root)->y; }

                float bottom(EntityRef e, const ResourceLedger& l) { return top(e, l) + pos(e)->height; }

                std::string element(EntityRef e) { return ecs.getEntity(e.id)->get<ThemeComponent>()->element; }

                bool alive(EntityRef e) { return ecs.getEntity(e.id) != nullptr; }
            };

            LedgerRowSpec rowSpec(const std::string& id, const std::string& name, const std::string& value, const std::string& rate = "", LedgerTone tone = LedgerTone::None, const std::string& glyph = "gold")
            {
                LedgerRowSpec spec;
                spec.id = id;
                spec.glyph = glyph;
                spec.name = name;
                spec.value = value;
                spec.rate = rate;
                spec.tone = tone;

                return spec;
            }

            ResourceLedgerSpec coinSpec()
            {
                ResourceLedgerSpec spec;
                spec.width = 288.0f;
                spec.groups = {{"purse", "PURSE", {rowSpec("coin", "Coin", "412", "+2 / mo", LedgerTone::Coin)}}};

                return spec;
            }

            ResourceLedgerSpec threeRows()
            {
                ResourceLedgerSpec spec;
                spec.width = 288.0f;
                spec.groups = {{"purse", "PURSE", {
                    rowSpec("coin", "Coin", "412", "", LedgerTone::Coin),
                    rowSpec("rations", "Rations", "18"),
                    rowSpec("iron", "Iron", "6", "", LedgerTone::None, "strength"),
                }}};

                return spec;
            }
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(resourceledger_test, row_geometry)
        {
            MockLogger logger;
            LedgerFixture s;

            ResourceLedger ledger = s.make(coinSpec());

            ASSERT_EQ(ledger.groups.size(), 1u);
            ASSERT_EQ(ledger.groups[0].rows.size(), 1u);
            const ResourceLedger::Group& purse = ledger.groups[0];
            const ResourceLedger::Row& coin = purse.rows[0];

            // The heading: a 16 px label line at the top, nothing above the first group
            EXPECT_NEAR(s.top(purse.heading, ledger), 0.0f, 0.01f);
            EXPECT_NEAR(s.top(purse.label.entity, ledger), 0.0f, 0.01f);
            EXPECT_FLOAT_EQ(s.pos(purse.label.entity)->height, 16.0f);
            EXPECT_EQ(purse.label.spec.text, "PURSE");
            EXPECT_EQ(s.element(purse.label.entity), "ledger.group");

            // The line: 26 tall, under the heading's 4 px
            EXPECT_NEAR(s.top(coin.line, ledger), 20.0f, 0.01f);
            EXPECT_FLOAT_EQ(s.pos(coin.line)->height, 26.0f);
            EXPECT_FLOAT_EQ(s.pos(coin.line)->width, 288.0f);

            EXPECT_EQ(coin.mark.spec.size, MarkSize::S16);
            EXPECT_EQ(s.element(coin.mark.entity), "ledger.mark.coin");
            EXPECT_NEAR(s.left(coin.mark.entity, ledger), 0.0f, 0.01f);
            EXPECT_NEAR(s.top(coin.mark.entity, ledger), 25.0f, 0.01f);

            EXPECT_EQ(s.element(coin.name.entity), "ledger.name");
            EXPECT_NEAR(s.left(coin.name.entity, ledger), 24.0f, 0.01f);
            EXPECT_NEAR(s.top(coin.name.entity, ledger), 23.0f, 0.01f);

            ASSERT_TRUE(coin.rate.has_value());
            EXPECT_EQ(coin.rate->spec.text, "+2 / mo");
            EXPECT_EQ(s.element(coin.rate->entity), "ledger.rate");
            EXPECT_NEAR(s.right(coin.rate->entity, ledger), 288.0f, 0.01f);

            EXPECT_EQ(coin.figure.spec.text, "412");
            EXPECT_EQ(s.element(coin.figure.entity), "ledger.figure");
            EXPECT_NEAR(s.right(coin.figure.entity, ledger), s.left(coin.rate->entity, ledger) - 8.0f, 0.01f);

            // One baseline for name, figure and rate
            const float baseline = s.pos(coin.name.entity)->y + s.asc("body-sm");
            EXPECT_NEAR(s.pos(coin.figure.entity)->y + s.asc("figure-sm"), baseline, 0.5f);
            EXPECT_NEAR(s.pos(coin.rate->entity)->y + s.asc("tick"), baseline, 0.5f);

            // The leader, sized by the engine from the name's end to the figure's start
            EXPECT_EQ(s.element(coin.leader), "ledger.leader");
            EXPECT_NEAR(s.left(coin.leader, ledger), s.right(coin.name.entity, ledger) + 8.0f, 0.01f);
            EXPECT_NEAR(s.right(coin.leader, ledger), s.left(coin.figure.entity, ledger) - 8.0f, 0.01f);
            EXPECT_NEAR(s.pos(coin.leader)->y, baseline - 3.0f, 0.01f);
            EXPECT_FLOAT_EQ(s.pos(coin.leader)->height, 1.0f);

            EXPECT_FLOAT_EQ(s.pos(ledger.root)->height, 46.0f);
            EXPECT_FLOAT_EQ(ledger.height(&s.ecs), 46.0f);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(resourceledger_test, figures_share_right_edge)
        {
            MockLogger logger;
            LedgerFixture s;

            ResourceLedger ledger = s.make(threeRows());

            for (const auto& r : ledger.groups[0].rows)
                EXPECT_NEAR(s.right(r.figure.entity, ledger), 288.0f, 0.01f) << r.spec.id;

            ResourceLedger::Row* coin = ledger.row("coin");
            ASSERT_NE(coin, nullptr);

            const float figureLeft = s.left(coin->figure.entity, ledger);
            const float leaderWidth = s.pos(coin->leader)->width;

            ledger.setValue(&s.ecs, "coin", "1 204");
            s.settle();

            EXPECT_EQ(coin->figure.spec.text, "1 204");
            EXPECT_EQ(coin->spec.value, "1 204");
            EXPECT_NEAR(s.right(coin->figure.entity, ledger), 288.0f, 0.01f);

            const float widened = figureLeft - s.left(coin->figure.entity, ledger);
            EXPECT_GT(widened, 0.0f);
            EXPECT_NEAR(s.pos(coin->leader)->width, leaderWidth - widened, 0.01f);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(resourceledger_test, rate_and_loss)
        {
            MockLogger logger;
            LedgerFixture s;

            ResourceLedger ledger = s.make(coinSpec());
            ResourceLedger::Row* coin = ledger.row("coin");
            ASSERT_NE(coin, nullptr);

            ledger.setRate(&s.ecs, "coin", "\xE2\x88\x92" "1 / mo");
            s.settle();

            ASSERT_TRUE(coin->rate.has_value());
            EXPECT_EQ(coin->rate->spec.text, "\xE2\x88\x92" "1 / mo");
            EXPECT_EQ(s.element(coin->rate->entity), "ledger.rate.loss");
            EXPECT_NEAR(s.right(coin->rate->entity, ledger), 288.0f, 0.01f);
            EXPECT_NEAR(s.right(coin->figure.entity, ledger), s.left(coin->rate->entity, ledger) - 8.0f, 0.01f);

            const EntityRef rateEntity = coin->rate->entity;

            ledger.setRate(&s.ecs, "coin", "");
            s.settle();

            EXPECT_FALSE(coin->rate.has_value());
            EXPECT_FALSE(s.alive(rateEntity));
            EXPECT_NEAR(s.right(coin->figure.entity, ledger), 288.0f, 0.01f);
            EXPECT_NEAR(s.right(coin->leader, ledger), s.left(coin->figure.entity, ledger) - 8.0f, 0.01f);

            // And back: a gain again
            ledger.setRate(&s.ecs, "coin", "+3 / mo");
            s.settle();

            ASSERT_TRUE(coin->rate.has_value());
            EXPECT_EQ(s.element(coin->rate->entity), "ledger.rate");
            EXPECT_NEAR(s.right(coin->figure.entity, ledger), s.left(coin->rate->entity, ledger) - 8.0f, 0.01f);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(resourceledger_test, tones_and_muted)
        {
            MockLogger logger;
            LedgerFixture s;

            ResourceLedgerSpec spec;
            spec.groups = {
                {"standing", "STANDING", {rowSpec("guild.bellmoor", "Bellmoor Guild", "3", "", LedgerTone::Guild, "guild")}},
                {"kept", "KEPT BETWEEN LIVES", {rowSpec("relic.sunstone", "Sunstone relic", "1 of 3", "", LedgerTone::Relic, "relic")}},
            };
            ResourceLedger ledger = s.make(spec);

            ResourceLedger::Row* guild = ledger.row("guild.bellmoor");
            ResourceLedger::Row* relic = ledger.row("relic.sunstone");
            ASSERT_NE(guild, nullptr);
            ASSERT_NE(relic, nullptr);

            EXPECT_EQ(s.element(guild->mark.entity), "ledger.mark.guild");
            EXPECT_EQ(s.element(guild->figure.entity), "ledger.figure");
            EXPECT_EQ(s.element(relic->mark.entity), "ledger.mark.relic");
            EXPECT_EQ(s.element(relic->figure.entity), "ledger.figure.relic");

            // State beats tone: a muted relic is ink-faint, not gold
            ledger.setMuted(&s.ecs, "relic.sunstone", true);
            s.settle();

            EXPECT_EQ(s.element(relic->mark.entity), "ledger.mark.muted");
            EXPECT_EQ(s.element(relic->name.entity), "ledger.name.muted");
            EXPECT_EQ(s.element(relic->figure.entity), "ledger.figure.muted");

            const auto faint = s.color("ink-faint", s.theme->currentTheme());
            const auto figure = s.ecs.getEntity(relic->figure.entity.id)->get<TTFText>()->colors;
            EXPECT_FLOAT_EQ(figure.x, faint.x);
            EXPECT_FLOAT_EQ(figure.y, faint.y);
            EXPECT_FLOAT_EQ(figure.z, faint.z);

            ledger.setMuted(&s.ecs, "relic.sunstone", false);
            s.settle();

            EXPECT_EQ(s.element(relic->mark.entity), "ledger.mark.relic");
            EXPECT_EQ(s.element(relic->name.entity), "ledger.name");
            EXPECT_EQ(s.element(relic->figure.entity), "ledger.figure.relic");
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(resourceledger_test, groups_spacing)
        {
            MockLogger logger;
            LedgerFixture s;

            ResourceLedgerSpec spec;
            spec.groups = {
                {"purse", "PURSE", {rowSpec("coin", "Coin", "412"), rowSpec("rations", "Rations", "18")}},
                {"stores", "STORES", {rowSpec("iron", "Iron", "6")}},
            };
            ResourceLedger ledger = s.make(spec);

            const auto& purse = ledger.groups[0];
            const auto& stores = ledger.groups[1];

            EXPECT_NEAR(s.top(purse.label.entity, ledger), 0.0f, 0.01f);
            EXPECT_NEAR(s.top(purse.rows[1].line, ledger), 20.0f + 26.0f, 0.01f);

            // 12 px between the last line of a group and the next heading's text
            EXPECT_NEAR(s.top(stores.label.entity, ledger), s.bottom(purse.rows[1].line, ledger) + 12.0f, 0.01f);
            EXPECT_NEAR(s.top(stores.rows[0].line, ledger), s.bottom(stores.label.entity, ledger) + 4.0f, 0.01f);

            EXPECT_FLOAT_EQ(ledger.height(&s.ecs), 20.0f + 2.0f * 26.0f + 32.0f + 26.0f);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(resourceledger_test, add_and_remove_rows)
        {
            MockLogger logger;
            LedgerFixture s;

            ResourceLedgerSpec spec;
            spec.groups = {
                {"purse", "PURSE", {rowSpec("coin", "Coin", "412")}},
                {"stores", "STORES", {rowSpec("iron", "Iron", "6")}},
            };
            ResourceLedger ledger = s.make(spec);

            const float before = ledger.height(&s.ecs);
            const float ironTop = s.top(ledger.row("iron")->line, ledger);

            // Into the first group: the lines below move down by one line
            ledger.addRow(&s.ecs, "purse", rowSpec("rations", "Rations", "18"));
            s.settle();
            s.settle();

            EXPECT_FLOAT_EQ(ledger.height(&s.ecs), before + 26.0f);
            ASSERT_EQ(ledger.groups[0].rows.size(), 2u);
            EXPECT_EQ(ledger.groups[0].rows[1].spec.id, "rations");
            EXPECT_NEAR(s.top(ledger.row("rations")->line, ledger), 20.0f + 26.0f, 0.01f);
            EXPECT_NEAR(s.top(ledger.row("iron")->line, ledger), ironTop + 26.0f, 0.01f);

            // Appended to the last group
            ledger.addRow(&s.ecs, "stores", rowSpec("timber", "Timber", "14", "+3 / mo"));
            s.settle();
            s.settle();

            EXPECT_FLOAT_EQ(ledger.height(&s.ecs), before + 52.0f);
            EXPECT_NEAR(s.top(ledger.row("timber")->line, ledger), s.top(ledger.row("iron")->line, ledger) + 26.0f, 0.01f);

            // Removed: the line and its parts go
            const EntityRef rationsLine = ledger.row("rations")->line;
            const EntityRef rationsName = ledger.row("rations")->name.entity;

            ledger.removeRow(&s.ecs, "rations");
            s.settle();
            s.settle();

            EXPECT_EQ(ledger.row("rations"), nullptr);
            EXPECT_FALSE(s.alive(rationsLine));
            EXPECT_FALSE(s.alive(rationsName));
            EXPECT_FLOAT_EQ(ledger.height(&s.ecs), before + 26.0f);
            EXPECT_NEAR(s.top(ledger.row("iron")->line, ledger), ironTop, 0.01f);

            // An unknown group is created at the end with the label given
            ledger.addRow(&s.ecs, "kept", rowSpec("relic.sunstone", "Sunstone relic", "1 of 3", "", LedgerTone::Relic, "relic"), "KEPT BETWEEN LIVES");
            s.settle();
            s.settle();

            ASSERT_EQ(ledger.groups.size(), 3u);
            EXPECT_EQ(ledger.groups[2].spec.id, "kept");
            EXPECT_EQ(ledger.groups[2].label.spec.text, "KEPT BETWEEN LIVES");
            EXPECT_FLOAT_EQ(ledger.height(&s.ecs), before + 26.0f + 32.0f + 26.0f);
            EXPECT_NEAR(s.top(ledger.groups[2].label.entity, ledger), s.bottom(ledger.row("timber")->line, ledger) + 12.0f, 0.01f);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(resourceledger_test, leader_too_short_warns)
        {
            MockLogger logger;
            LedgerFixture s;

            // A first ledger with room to spare: the engine's first-use warnings land before the count
            ResourceLedgerSpec warm;
            warm.groups = {{"purse", "PURSE", {rowSpec("coin", "Coin", "412")}}};
            s.make(warm, 100.0f, 500.0f);

            const unsigned int before = logger.getNbWarning();

            ResourceLedgerSpec spec;
            spec.width = 200.0f;
            spec.groups = {{"standing", "STANDING", {rowSpec("guild.long", "The Worshipful Company of Bellmoor Smiths", "3", "", LedgerTone::Guild, "guild")}}};
            ASSERT_EQ(spec.groups[0].rows[0].name.size(), 41u);

            ResourceLedger ledger = s.make(spec);

            EXPECT_EQ(logger.getNbWarning() - before, 1u);
            EXPECT_LT(s.pos(ledger.row("guild.long")->leader)->width, 24.0f);

            // A figure that changes does not warn again while the leader stays short
            ledger.setValue(&s.ecs, "guild.long", "4");
            EXPECT_EQ(logger.getNbWarning() - before, 1u);

            // A short name at the same width is fine
            const unsigned int after = logger.getNbWarning();
            ResourceLedgerSpec fine;
            fine.width = 200.0f;
            fine.groups = {{"purse", "PURSE", {rowSpec("coin", "Coin", "412", "+2 / mo", LedgerTone::Coin)}}};
            s.make(fine, 100.0f, 300.0f);
            EXPECT_EQ(logger.getNbWarning(), after);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(resourceledger_test, fed_from_worldfacts)
        {
            MockLogger logger;
            LedgerFixture s;

            ResourceLedgerSpec spec = coinSpec();
            spec.groups.push_back({"kept", "KEPT BETWEEN LIVES", {rowSpec("relic.sunstone", "Sunstone relic", "1 of 3", "", LedgerTone::Relic, "relic")}});
            ResourceLedger ledger = s.make(spec);

            s.feed->onFact = [&](const std::string& name, const ElementType& v) {
                if (name == "resources.coin.value")
                    ledger.setValue(&s.ecs, "coin", v.get<std::string>());
                else if (name == "resources.coin.rate")
                    ledger.setRate(&s.ecs, "coin", v.get<std::string>());
                else if (name == "resources.relic.sunstone.muted")
                    ledger.setMuted(&s.ecs, "relic.sunstone", v.get<bool>());
                else if (name == "resources.timber.value")
                    ledger.addRow(&s.ecs, "stores", rowSpec("timber", "Timber", v.get<std::string>(), "+3 / mo"), "STORES");
            };

            const float before = ledger.height(&s.ecs);

            s.facts->setFact("resources.coin.value", std::string("512"));
            s.facts->setFact("resources.coin.rate", std::string("+4 / mo"));
            s.facts->setFact("resources.relic.sunstone.muted", true);
            s.facts->setFact("resources.timber.value", std::string("14"));

            // The update leaves on the first pass and is delivered on the next one
            s.settle();
            s.settle();
            s.settle();

            EXPECT_EQ(ledger.row("coin")->figure.spec.text, "512");
            ASSERT_TRUE(ledger.row("coin")->rate.has_value());
            EXPECT_EQ(ledger.row("coin")->rate->spec.text, "+4 / mo");
            EXPECT_TRUE(ledger.row("relic.sunstone")->spec.muted);
            EXPECT_EQ(s.element(ledger.row("relic.sunstone")->figure.entity), "ledger.figure.muted");

            // A row added while the ECS runs: a new group at the end, laid out like the others
            ASSERT_NE(ledger.row("timber"), nullptr);
            EXPECT_EQ(ledger.groups.back().spec.id, "stores");
            EXPECT_FLOAT_EQ(ledger.height(&s.ecs), before + 32.0f + 26.0f);
            EXPECT_NEAR(s.right(ledger.row("timber")->figure.entity, ledger), s.left(ledger.row("timber")->rate->entity, ledger) - 8.0f, 0.01f);
            EXPECT_NEAR(s.left(ledger.row("timber")->leader, ledger), s.right(ledger.row("timber")->name.entity, ledger) + 8.0f, 0.01f);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(resourceledger_test, theme_switch)
        {
            MockLogger logger;
            LedgerFixture s;

            ResourceLedger ledger = s.make(coinSpec());
            const ResourceLedger::Row* coin = ledger.row("coin");

            s.theme->setTheme("candle");
            s.settle();

            const auto mark = s.ecs.getEntity(coin->mark.entity.id)->get<IconComponent>()->colors;
            EXPECT_FLOAT_EQ(mark.x, s.color("ochre", "candle").x);
            EXPECT_FLOAT_EQ(mark.y, s.color("ochre", "candle").y);
            EXPECT_FLOAT_EQ(mark.z, s.color("ochre", "candle").z);

            const auto leader = s.ecs.getEntity(coin->leader.id)->get<DottedLine2DObject>()->colors;
            EXPECT_FLOAT_EQ(leader.x, s.color("rule-hair", "candle").x);
            EXPECT_FLOAT_EQ(leader.y, s.color("rule-hair", "candle").y);
            EXPECT_FLOAT_EQ(leader.z, s.color("rule-hair", "candle").z);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(resourceledger_test, z_bands)
        {
            MockLogger logger;
            LedgerFixture s;

            ResourceLedgerSpec spec = coinSpec();
            spec.z = 20;
            ResourceLedger ledger = s.make(spec);

            const auto& purse = ledger.groups[0];
            const auto& coin = purse.rows[0];

            EXPECT_FLOAT_EQ(s.pos(ledger.root)->z, 20.0f);
            EXPECT_FLOAT_EQ(s.pos(purse.heading)->z, 20.0f);
            EXPECT_FLOAT_EQ(s.pos(coin.line)->z, 20.0f);
            EXPECT_FLOAT_EQ(s.pos(coin.mark.entity)->z, 21.0f);
            EXPECT_FLOAT_EQ(s.pos(coin.leader)->z, 21.0f);
            EXPECT_FLOAT_EQ(s.pos(purse.label.entity)->z, 22.0f);
            EXPECT_FLOAT_EQ(s.pos(coin.name.entity)->z, 22.0f);
            EXPECT_FLOAT_EQ(s.pos(coin.figure.entity)->z, 22.0f);
            EXPECT_FLOAT_EQ(s.pos(coin.rate->entity)->z, 22.0f);

            for (auto e : {ledger.root, purse.heading, coin.line, coin.mark.entity, coin.leader, coin.name.entity, coin.figure.entity, coin.rate->entity})
            {
                const float z = s.pos(e)->z;
                EXPECT_FLOAT_EQ(z, static_cast<float>(static_cast<int>(z)));
            }
        }
    }
}
