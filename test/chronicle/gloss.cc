#include "stdafx.h"

#include <gtest/gtest.h>

#include "UI/gloss.h"
#include "UI/paint.h"
#include "Core/textstyle.h"

#include "ECS/entitysystem.h"
#include "ECS/entitysystem_fwd.h"   // ResizeEvent
#include "UI/ttftext.h"
#include "UI/tooltip.h"
#include "Input/inputcomponent.h"
#include "Systems/coresystems.h"
#include "2D/simple2dobject.h"
#include "2D/decoratedshapes.h"

#include "mocklogger.h"

using namespace chronicle;

namespace pg
{
    namespace test
    {
        namespace
        {
            struct GlossFixture
            {
                Tokens tokens = Tokens::load("chronicle/tokens.json");
                EntitySystem ecs;
                MasterRenderer renderer;
                TTFTextSystem* ttf = nullptr;
                TooltipSystem* tip = nullptr;
                GlossRegistry* reg = nullptr;
                PaintSystem* paint = nullptr;
                TextStyles styles;

                GlossFixture()
                {
                    ecs.createSystem<PositionComponentSystem>();
                    ttf = ecs.createSystem<TTFTextSystem>(&renderer);
                    ecs.createSystem<Simple2DObjectSystem>(&renderer);
                    ecs.createSystem<HatchRect2DObjectSystem>(&renderer);
                    ecs.createSystem<DottedLine2DObjectSystem>(&renderer);
                    ecs.createSystem<StrokeRect2DObjectSystem>(&renderer);
                    ecs.createSystem<MouseHoverSystem>();
                    tip = ecs.createSystem<TooltipSystem>();
                    ecs.succeed<MouseHoverSystem, TooltipSystem>();
                    paint = ecs.createSystem<PaintSystem>(&tokens);
                    styles = TextStyles::fromTokens(tokens);
                    styles.registerAll(ttf, "fonts");
                    tip->setDefaultFont("chr-body-sm");
                    reg = ecs.createSystem<GlossRegistry>(&tokens, &styles);
                    ecs.sendEvent(ResizeEvent{1320.0f, 860.0f});
                }

                void pump() { ecs.executeOnce(); ecs.executeOnce(); ecs.executeOnce(); }
                void settle() { pump(); }
                void hover(float x, float y) { ecs.sendEvent(OnMouseMove{Point2D{x, y}, nullptr}); pump(); }
                void tick(float ms) { ecs.sendEvent(TickEvent{ms}); pump(); }
                void click() { ecs.sendEvent(OnMouseClick{Point2D{0, 0}, static_cast<MouseButton>(1)}); pump(); }

                EntityRef makeTarget(float x, float y, float w, float h)
                {
                    auto e = ecs.createEntity();
                    auto p = ecs.attach<PositionComponent>(e);
                    p->setX(x); p->setY(y); p->setWidth(w); p->setHeight(h);
                    return e;
                }

                CompRef<PositionComponent> pos(EntityRef e) { return ecs.getEntity(e.id)->get<PositionComponent>(); }
                CompRef<Simple2DObject> s2d(EntityRef e) { return ecs.getEntity(e.id)->get<Simple2DObject>(); }
                CompRef<StrokeRect2DObject> strokeOf(EntityRef e) { return ecs.getEntity(e.id)->get<StrokeRect2DObject>(); }
                CompRef<TTFText> ttfOf(EntityRef e) { return ecs.getEntity(e.id)->get<TTFText>(); }
                std::string token(EntityRef e) { return ecs.getEntity(e.id)->get<PaintComponent>()->token; }
            };

            const char* PARA30 =
                "He had not the strength for it and the guild would not wait so the smith took him "
                "on for the winter and asked no questions of the boy at all";
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(gloss_test, margin_geometry)
        {
            MockLogger logger;
            GlossFixture s;

            GlossSpec spec; spec.kind = GlossKind::Margin; spec.text = PARA30;
            Gloss g = makeGloss(&s.ecs, s.tokens, s.styles, spec);
            s.settle();

            EXPECT_FLOAT_EQ(s.pos(g.root)->width, 240.0f);
            EXPECT_NEAR(s.pos(g.edge)->width, 2.0f, 0.01f);
            EXPECT_NEAR(s.pos(g.edge)->height, s.pos(g.root)->height, 0.5f);
            EXPECT_EQ(s.token(g.edge), "rule-hair");

            ASSERT_TRUE(g.text.has_value());
            EXPECT_NEAR(s.pos(g.text->entity)->x, s.pos(g.root)->x + 14.0f, 0.5f);
            EXPECT_NEAR(s.pos(g.text->entity)->width, 226.0f, 0.5f);
            EXPECT_EQ(g.text->spec.style, "gloss");
            EXPECT_NEAR(s.pos(g.root)->height, s.pos(g.text->entity)->height, 0.5f);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(gloss_test, margin_ignores_title_rows)
        {
            MockLogger logger;
            GlossFixture s;

            GlossSpec spec; spec.kind = GlossKind::Margin; spec.title = "x"; spec.text = "some text";
            spec.rows = {{"Time", "6 mo"}};
            Gloss g = makeGloss(&s.ecs, s.tokens, s.styles, spec);
            s.settle();

            EXPECT_FALSE(g.title.has_value());
            EXPECT_TRUE(g.rows.empty());
            EXPECT_GE(logger.getNbWarning(), 1u);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(gloss_test, tooltip_stack)
        {
            MockLogger logger;
            GlossFixture s;

            GlossSpec spec; spec.kind = GlossKind::Tooltip; spec.title = "Strength";
            spec.text = "Lifting, striking, enduring. Grows at the yard and in the mines over a long season.";
            spec.rows = {{"Now", "14"}, {"At term", "17"}, {"Warrior at 18 asks", "18"}};
            spec.footnote = "WARRIOR AT 18 ASKS STRENGTH 18";
            Gloss g = makeGloss(&s.ecs, s.tokens, s.styles, spec);
            s.settle();

            EXPECT_EQ(s.token(g.ground), "folio");
            EXPECT_EQ(s.token(g.frame), "rule-ruled");
            EXPECT_FLOAT_EQ(s.strokeOf(g.frame)->strokeWidth, 1.0f);

            const float rootY = s.pos(g.root)->y;
            ASSERT_TRUE(g.title.has_value());
            ASSERT_TRUE(g.text.has_value());
            ASSERT_EQ(g.rows.size(), 3u);
            ASSERT_TRUE(g.footnote.has_value());

            auto top = [&](EntityRef e) { return s.pos(e)->y; };
            auto bottom = [&](EntityRef e) { return s.pos(e)->y + s.pos(e)->height; };

            EXPECT_NEAR(top(g.title->entity), rootY + 12.0f, 0.5f);
            EXPECT_NEAR(top(g.text->entity), bottom(g.title->entity) + 4.0f, 0.5f);
            EXPECT_NEAR(top(g.rows[0].first.entity), bottom(g.text->entity) + 4.0f, 0.5f);
            EXPECT_NEAR(top(g.rows[2].first.entity), bottom(g.rows[1].first.entity) + 4.0f, 0.5f);
            EXPECT_NEAR(top(g.footnote->entity), bottom(g.rows[2].first.entity) + 8.0f, 0.5f);
            EXPECT_NEAR(s.pos(g.root)->height, bottom(g.footnote->entity) + 12.0f - rootY, 0.5f);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(gloss_test, tooltip_row_values_right_aligned)
        {
            MockLogger logger;
            GlossFixture s;

            GlossSpec spec; spec.kind = GlossKind::Tooltip; spec.text = "figures";
            spec.rows = {{"Now", "14"}, {"At term", "170"}, {"Cap", "9"}};
            Gloss g = makeGloss(&s.ecs, s.tokens, s.styles, spec);
            s.settle();

            const float edge = s.pos(g.root)->x + 12.0f + 256.0f;
            for (const auto& row : g.rows)
            {
                EXPECT_NEAR(s.pos(row.second.entity)->x + s.pos(row.second.entity)->width, edge, 0.5f);
                EXPECT_EQ(row.second.spec.style, "figure-sm");
                EXPECT_EQ(row.first.spec.style, "body-sm");
            }
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(gloss_test, tooltip_without_title_or_footnote)
        {
            MockLogger logger;
            GlossFixture s;

            GlossSpec spec; spec.kind = GlossKind::Tooltip; spec.text = "just a line of text here";
            Gloss g = makeGloss(&s.ecs, s.tokens, s.styles, spec);
            s.settle();

            EXPECT_NEAR(g.height(&s.ecs), 12.0f + s.pos(g.text->entity)->height + 12.0f, 0.5f);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(gloss_test, registry_set_find_erase)
        {
            MockLogger logger;
            GlossFixture s;

            GlossSpec spec; spec.title = "Strength"; spec.rows = {{"Now", "14"}, {"Cap", "18"}};
            s.reg->set("train", spec);
            EXPECT_TRUE(s.reg->has("train"));
            ASSERT_NE(s.reg->find("train"), nullptr);
            EXPECT_EQ(s.reg->find("train")->rows.size(), 2u);
            EXPECT_EQ(s.reg->find("train")->kind, GlossKind::Tooltip);   // forced

            s.reg->erase("train");
            EXPECT_FALSE(s.reg->has("train"));
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(gloss_test, gloss_style_shows_registered_spec)
        {
            MockLogger logger;
            GlossFixture s;

            GlossSpec spec; spec.title = "Strength"; spec.text = "Lifting and striking.";
            s.reg->set("train", spec);

            EntityRef target = s.makeTarget(100.0f, 100.0f, 50.0f, 20.0f);
            attachGloss(&s.ecs, target, "train");
            s.pump();

            s.hover(110.0f, 110.0f);
            s.tick(250.0f);

            EXPECT_TRUE(s.tip->isShowing());
            EXPECT_EQ(s.tip->shownFor, target.id);
            ASSERT_TRUE(s.reg->lastBuilt.title.has_value());
            EXPECT_EQ(s.reg->lastBuilt.title->spec.text, "Strength");
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(gloss_test, gloss_placement_is_engines)
        {
            MockLogger logger;
            GlossFixture s;

            GlossSpec spec; spec.text = "A short gloss.";
            s.reg->set("t", spec);

            EntityRef target = s.makeTarget(100.0f, 100.0f, 50.0f, 20.0f);
            attachGloss(&s.ecs, target, "t");
            s.pump();
            s.hover(110.0f, 110.0f);
            s.tick(250.0f);

            EXPECT_NEAR(s.pos(s.reg->lastBuilt.root)->x, 110.0f + 14.0f, 0.5f);
            EXPECT_NEAR(s.pos(s.reg->lastBuilt.root)->y, 110.0f + 18.0f, 0.5f);

            // Near the bottom of an 800-high screen the tooltip flips above.
            s.ecs.sendEvent(ResizeEvent{1320.0f, 800.0f});
            EntityRef low = s.makeTarget(100.0f, 780.0f, 50.0f, 20.0f);
            attachGloss(&s.ecs, low, "t");
            s.pump();
            s.hover(110.0f, 790.0f);
            s.tick(250.0f);
            EXPECT_LE(s.pos(s.reg->lastBuilt.root)->y + s.pos(s.reg->lastBuilt.root)->height, 796.0f);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(gloss_test, click_hides)
        {
            MockLogger logger;
            GlossFixture s;

            GlossSpec spec; spec.text = "x";
            s.reg->set("t", spec);
            EntityRef target = s.makeTarget(100.0f, 100.0f, 50.0f, 20.0f);
            attachGloss(&s.ecs, target, "t");
            s.pump();
            s.hover(110.0f, 110.0f);
            s.tick(250.0f);
            ASSERT_TRUE(s.tip->isShowing());

            s.click();
            EXPECT_FALSE(s.tip->isShowing());
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(gloss_test, missing_key_is_visible)
        {
            MockLogger logger;
            GlossFixture s;

            EntityRef target = s.makeTarget(100.0f, 100.0f, 50.0f, 20.0f);
            attachGloss(&s.ecs, target, "nope");
            s.pump();
            s.hover(110.0f, 110.0f);
            s.tick(250.0f);

            ASSERT_TRUE(s.tip->isShowing());
            ASSERT_TRUE(s.reg->lastBuilt.text.has_value());
            EXPECT_NE(s.reg->lastBuilt.text->spec.text.find("nope"), std::string::npos);
            EXPECT_EQ(s.token(s.reg->lastBuilt.text->entity), "vermilion");
            EXPECT_GE(logger.getNbError(), 1u);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(gloss_test, set_text_reflows)
        {
            MockLogger logger;
            GlossFixture s;

            // The italic `gloss` font is absent from testdeps (that is what register_all's skip
            // test relies on), so the text cannot actually wrap here; assert the reflow plumbing
            // instead: setText re-drives the root height from the text box, and the edge follows.
            GlossSpec spec; spec.kind = GlossKind::Margin; spec.text = "Short.";
            Gloss g = makeGloss(&s.ecs, s.tokens, s.styles, spec);
            s.settle();

            g.setText(&s.ecs, s.styles, PARA30);
            s.settle();
            EXPECT_EQ(g.text->spec.text, PARA30);
            EXPECT_NEAR(s.pos(g.root)->height, s.pos(g.text->entity)->height, 0.5f);
            EXPECT_NEAR(s.pos(g.edge)->height, s.pos(g.root)->height, 0.5f);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(gloss_test, tooltip_repaints)
        {
            MockLogger logger;
            GlossFixture s;

            GlossSpec spec; spec.text = "x";
            s.reg->set("t", spec);
            EntityRef target = s.makeTarget(100.0f, 100.0f, 50.0f, 20.0f);
            attachGloss(&s.ecs, target, "t");
            s.pump();
            s.hover(110.0f, 110.0f);
            s.tick(250.0f);
            ASSERT_TRUE(s.tip->isShowing());

            EntityRef ground = s.reg->lastBuilt.ground;
            s.tokens.setTheme(Theme::Candle);
            s.ecs.sendEvent(ThemeChangedEvent{Theme::Candle});
            s.pump();

            EXPECT_FLOAT_EQ(s.s2d(ground)->colors.x, s.tokens.colour("folio", Theme::Candle).x);
        }
    }
}
