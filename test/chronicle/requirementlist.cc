#include "stdafx.h"

#include <cmath>
#include <string>

#include <gtest/gtest.h>

#include "UI/requirementlist.h"
#include "UI/paint.h"
#include "Core/textmetrics.h"
#include "Core/textstyle.h"

#include "ECS/entitysystem.h"
#include "ECS/entitysystem_fwd.h"
#include "UI/ttftext.h"
#include "UI/iconsystem.h"
#include "UI/sizer.h"
#include "UI/prefab.h"
#include "UI/gamedataview.h"
#include "Systems/coresystems.h"
#include "2D/simple2dobject.h"

#include "mocklogger.h"

using namespace chronicle;

namespace pg
{
    namespace test
    {
        namespace
        {
            struct ReqFixture
            {
                Tokens tokens = Tokens::load("chronicle/tokens.json");
                EntitySystem ecs;
                MasterRenderer renderer;
                TTFTextSystem* ttf = nullptr;
                IconSystem* icons = nullptr;
                PaintSystem* paint = nullptr;
                GameDataView* view = nullptr;
                TextStyles styles;

                ReqFixture()
                {
                    ecs.createSystem<PositionComponentSystem>();
                    ecs.createSystem<LayoutSystem>();
                    ecs.succeed<PositionComponentSystem, LayoutSystem>();
                    ttf = ecs.createSystem<TTFTextSystem>(&renderer);
                    ecs.createSystem<Simple2DObjectSystem>(&renderer);
                    icons = ecs.createSystem<IconSystem>(&renderer);
                    view = ecs.createSystem<GameDataView>();
                    paint = ecs.createSystem<PaintSystem>(&tokens);
                    styles = TextStyles::fromTokens(tokens);
                    styles.registerAll(ttf, "fonts");
                    installIconEntries();
                }

                void installIconEntries()
                {
                    std::vector<IconEntry> marks;
                    const int sizes[5] = {14, 16, 18, 24, 48};
                    for (const auto& name : markNames())
                        for (int s : sizes)
                            marks.push_back({name, s, {s, s}, {0.0f, 0.0f}, {0.1f, 0.1f}});
                    icons->registerEntriesForTest("chronicle", marks, 1024, 1024);
                    renderer.registerTexture("IconAtlas_chronicle", OpenGLTexture{});
                }

                void settle() { ecs.executeOnce(); ecs.executeOnce(); ecs.executeOnce(); }

                RequirementList make(const RequirementListSpec& spec, float x = 100.0f, float y = 100.0f)
                {
                    RequirementList list = makeRequirementList(&ecs, tokens, styles, spec);
                    list.root->get<PositionComponent>()->setX(x);
                    list.root->get<PositionComponent>()->setY(y);
                    settle();
                    return list;
                }

                float asc(const std::string& style)
                {
                    const TextStyle& s = styles.get(style);
                    return ttf->measureText(s.fontAlias, "H", 1.0f, 0.0f, 0.0f, s.letterSpacingPx).ascender;
                }

                float textW(const std::string& style, const std::string& text)
                {
                    const TextStyle& s = styles.get(style);
                    return ttf->measureText(s.fontAlias, text, 1.0f, 0.0f, 0.0f, s.letterSpacingPx).width;
                }

                CompRef<PositionComponent> pos(EntityRef e) { return ecs.getEntity(e.id)->get<PositionComponent>(); }
                float rightEdge(EntityRef e) { return pos(e)->x + pos(e)->width; }
                std::string token(EntityRef e) { return ecs.getEntity(e.id)->get<PaintComponent>()->token; }
                CompRef<IconComponent> iconOf(EntityRef e) { return ecs.getEntity(e.id)->get<IconComponent>(); }

                RequirementListSpec threeNumeric(bool dense = false)
                {
                    RequirementListSpec spec;
                    spec.width = 256.0f;
                    spec.dense = dense;
                    spec.items = {
                        {"Strength", 15, 18},
                        {"Swordsmanship", 3, 4},
                        {"Vitality", 12, 10},
                    };
                    return spec;
                }
            };
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(requirementlist_test, row_geometry_roomy)
        {
            MockLogger logger;
            ReqFixture s;

            RequirementList list = s.make(s.threeNumeric());

            EXPECT_FLOAT_EQ(s.pos(list.root)->width, 256.0f);
            EXPECT_NEAR(s.pos(list.root)->height, 3.0f * 20.0f + 2.0f * 4.0f, 0.5f);

            for (size_t i = 0; i < 3; ++i)
            {
                const auto& row = list.rows[i];
                EXPECT_NEAR(s.pos(row.name.root)->y, s.pos(list.root)->y + static_cast<float>(i) * 24.0f, 0.5f);
                EXPECT_NEAR(s.pos(row.name.root)->x, s.pos(list.root)->x, 0.5f);

                ASSERT_TRUE(row.name.mark.has_value());
                EXPECT_EQ(row.name.mark->spec.size, MarkSize::S14);
                EXPECT_NEAR(s.pos(row.name.mark->entity)->y,
                            s.pos(row.name.root)->y + (20.0f - 14.0f) / 2.0f, 0.5f);

                ASSERT_TRUE(row.value.has_value());
                EXPECT_NEAR(s.rightEdge(row.value->entity), s.pos(list.root)->x + 256.0f, 0.5f);

                const float labelBaseline = s.pos(row.name.label.entity)->y + s.asc("body-sm");
                const float valueBaseline = s.pos(row.value->entity)->y + s.asc("figure-sm");
                EXPECT_NEAR(labelBaseline, valueBaseline, 0.5f);
            }
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(requirementlist_test, row_geometry_dense)
        {
            MockLogger logger;
            ReqFixture s;

            RequirementList list = s.make(s.threeNumeric(true));

            EXPECT_NEAR(s.pos(list.root)->height, 3.0f * 16.0f + 2.0f * 4.0f, 0.5f);
            EXPECT_EQ(list.rows[0].name.label.spec.style, "tick");
            ASSERT_TRUE(list.rows[0].value.has_value());
            EXPECT_EQ(list.rows[0].value->spec.style, "tick");
            ASSERT_TRUE(list.rows[0].name.mark.has_value());
            EXPECT_EQ(list.rows[0].name.mark->spec.size, MarkSize::S14);   // the mark stays 14
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(requirementlist_test, met_and_unmet_tones)
        {
            MockLogger logger;
            ReqFixture s;

            RequirementList list = s.make(s.threeNumeric());

            // {"Strength", 15, 18}: unmet.
            EXPECT_EQ(list.rows[0].name.mark->spec.name, "cross");
            EXPECT_EQ(s.token(list.rows[0].name.mark->entity), "status-loss");
            EXPECT_EQ(s.token(list.rows[0].value->entity), "status-loss");
            EXPECT_EQ(s.token(list.rows[0].name.label.entity), "ink");

            // {"Vitality", 12, 10}: met.
            EXPECT_EQ(list.rows[2].name.mark->spec.name, "check");
            EXPECT_EQ(s.token(list.rows[2].name.mark->entity), "status-gain");
            EXPECT_EQ(s.token(list.rows[2].value->entity), "status-gain");
            EXPECT_EQ(s.token(list.rows[2].name.label.entity), "ink-muted");
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(requirementlist_test, explicit_met_wins)
        {
            MockLogger logger;
            ReqFixture s;

            RequirementListSpec spec;
            spec.items = {
                {"Strength", 15, 18, 1},   // met although 15 < 18
                {"Vitality", 12, 10, 0},   // unmet although 12 >= 10
            };
            RequirementList list = s.make(spec);

            EXPECT_EQ(list.rows[0].name.mark->spec.name, "check");
            EXPECT_EQ(s.token(list.rows[0].name.mark->entity), "status-gain");
            EXPECT_EQ(list.rows[1].name.mark->spec.name, "cross");
            EXPECT_EQ(s.token(list.rows[1].name.mark->entity), "status-loss");
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(requirementlist_test, non_numeric_row)
        {
            MockLogger logger;
            ReqFixture s;

            RequirementListSpec spec;
            spec.items = {{"Has the Guild's letter", -1, 0, 0}};
            RequirementList list = s.make(spec);

            EXPECT_FALSE(list.rows[0].value.has_value());
            EXPECT_NEAR(s.pos(list.rows[0].name.label.entity)->width, 256.0f, 0.5f);
            EXPECT_EQ(list.rows[0].name.mark->spec.name, "cross");

            list.setMet(&s.ecs, 0, true);
            s.settle();
            EXPECT_EQ(list.rows[0].name.mark->spec.name, "check");
            EXPECT_EQ(s.token(list.rows[0].name.mark->entity), "status-gain");

            // A non-numeric row with met -1 logs once and shows as unmet.
            const size_t before = logger.getNbWarning();
            RequirementListSpec bad;
            bad.items = {{"A condition with no verdict", -1, 0, -1}};
            RequirementList list2 = s.make(bad, 100.0f, 300.0f);
            EXPECT_GE(logger.getNbWarning(), before + 1);
            EXPECT_EQ(list2.rows[0].name.mark->spec.name, "cross");
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(requirementlist_test, value_pair_text)
        {
            MockLogger logger;
            ReqFixture s;

            RequirementList list = s.make(s.threeNumeric());
            EXPECT_EQ(list.rows[0].value->spec.text, "15 / 18");
            const float edge = s.rightEdge(list.rows[0].value->entity);

            list.setItem(&s.ecs, s.styles, 0, 9, 18);
            s.settle();
            EXPECT_EQ(list.rows[0].value->spec.text, "9 / 18");
            EXPECT_NEAR(s.rightEdge(list.rows[0].value->entity), edge, 0.01f);

            list.setItem(&s.ecs, s.styles, 0, 18, 18);
            s.settle();
            EXPECT_EQ(list.rows[0].name.mark->spec.name, "check");
            EXPECT_EQ(s.token(list.rows[0].name.mark->entity), "status-gain");
            EXPECT_EQ(s.token(list.rows[0].name.label.entity), "ink-muted");
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(requirementlist_test, set_met_then_clear)
        {
            MockLogger logger;
            ReqFixture s;

            RequirementList list = s.make(s.threeNumeric());

            list.setMet(&s.ecs, 0, true);
            EXPECT_EQ(s.token(list.rows[0].name.mark->entity), "status-gain");

            list.clearMet(&s.ecs, 0);   // 15 < 18 -> derived unmet again
            EXPECT_EQ(s.token(list.rows[0].name.mark->entity), "status-loss");
            EXPECT_EQ(list.rows[0].name.mark->spec.name, "cross");
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(requirementlist_test, label_leaves_room_for_value)
        {
            MockLogger logger;
            ReqFixture s;

            RequirementListSpec spec;
            spec.items = {{"Reputation among the Bellmoor carters and the eastern road guild", 40, 22}};
            RequirementList list = s.make(spec);

            ASSERT_TRUE(list.rows[0].value.has_value());
            const float valueW = s.pos(list.rows[0].value->entity)->width;
            EXPECT_NEAR(s.pos(list.rows[0].name.label.entity)->width, 256.0f - valueW - 8.0f, 0.5f);

            // The natural text is wider than the box: it must elide.
            EXPECT_GT(s.textW("body-sm", spec.items[0].label), s.pos(list.rows[0].name.label.entity)->width);

            // A wider pair narrows the label.
            const float labelW = s.pos(list.rows[0].name.label.entity)->width;
            list.setItem(&s.ecs, s.styles, 0, 100, 120);
            s.settle();
            EXPECT_LT(s.pos(list.rows[0].name.label.entity)->width, labelW);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(requirementlist_test, set_items_rebuilds)
        {
            MockLogger logger;
            ReqFixture s;

            RequirementList list = s.make(s.threeNumeric());
            EXPECT_EQ(list.size(), 3u);

            std::vector<Requirement> five = {
                {"One", 1, 2}, {"Two", 2, 2}, {"Three", 3, 2}, {"Four", 0, 1}, {"Five", 5, 5},
            };
            list.setItems(&s.ecs, s.tokens, s.styles, five);
            s.settle();
            EXPECT_EQ(list.size(), 5u);
            EXPECT_NEAR(list.height(&s.ecs), 5.0f * 20.0f + 4.0f * 4.0f, 0.5f);
            EXPECT_EQ(list.root->get<Prefab>()->childrenIds.size(), 10u);   // name root + value per row

            list.setItems(&s.ecs, s.tokens, s.styles, {});
            s.settle();
            EXPECT_EQ(list.size(), 0u);
            EXPECT_NEAR(list.height(&s.ecs), 0.0f, 0.01f);
            EXPECT_EQ(list.root->get<Prefab>()->childrenIds.size(), 0u);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(requirementlist_test, empty_list)
        {
            MockLogger logger;
            ReqFixture s;

            RequirementListSpec spec;   // items {}
            RequirementList list = s.make(spec);
            EXPECT_EQ(list.size(), 0u);
            EXPECT_NEAR(list.height(&s.ecs), 0.0f, 0.01f);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(requirementlist_test, fed_from_gamedataview)
        {
            MockLogger logger;
            ReqFixture s;

            RequirementListSpec spec;
            spec.items = {
                {"Strength", 15, 18},
                {"Swordsmanship", 3, 4},
                {"Has the Guild's letter", -1, 0, 0},
            };
            RequirementList list = s.make(spec);

            s.view->subscribe("milestone.reqs.0.current", [&](const ElementType& v) {
                list.setItem(&s.ecs, s.styles, 0, v.get<int>(), 18);
            });
            s.view->subscribe("milestone.reqs.2.met", [&](const ElementType& v) {
                list.setMet(&s.ecs, 2, v.get<bool>());
            });

            s.view->set("milestone.reqs.0.current", ElementType{18});
            s.settle();
            EXPECT_EQ(list.rows[0].value->spec.text, "18 / 18");
            EXPECT_EQ(list.rows[0].name.mark->spec.name, "check");

            s.view->set("milestone.reqs.2.met", ElementType{true});
            s.settle();
            EXPECT_EQ(list.rows[2].name.mark->spec.name, "check");
            EXPECT_EQ(s.token(list.rows[2].name.mark->entity), "status-gain");
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(requirementlist_test, theme_repaints)
        {
            MockLogger logger;
            ReqFixture s;

            RequirementList list = s.make(s.threeNumeric());   // row 0 unmet

            s.tokens.setTheme(Theme::Candle);
            s.ecs.sendEvent(ThemeChangedEvent{Theme::Candle});
            s.settle();

            // status-loss aliases vermilion.
            EXPECT_FLOAT_EQ(s.iconOf(list.rows[0].name.mark->entity)->colors.x,
                            s.tokens.colour("vermilion", Theme::Candle).x);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(requirementlist_test, z_bands)
        {
            MockLogger logger;
            ReqFixture s;

            RequirementListSpec spec = s.threeNumeric();
            spec.z = 20;
            RequirementList list = s.make(spec);

            EXPECT_FLOAT_EQ(s.pos(list.root)->z, 20.0f);
            EXPECT_FLOAT_EQ(s.pos(list.rows[0].name.root)->z, 20.0f);
            EXPECT_FLOAT_EQ(s.pos(list.rows[0].name.mark->entity)->z, 21.0f);
            EXPECT_FLOAT_EQ(s.pos(list.rows[0].name.label.entity)->z, 21.0f);
            EXPECT_FLOAT_EQ(s.pos(list.rows[0].value->entity)->z, 21.0f);
        }
    }
}
