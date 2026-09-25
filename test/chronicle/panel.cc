#include "stdafx.h"

#include <cmath>

#include <gtest/gtest.h>

#include "UI/panel.h"
#include "UI/paint.h"
#include "Core/textstyle.h"

#include "ECS/entitysystem.h"
#include "UI/ttftext.h"
#include "UI/iconsystem.h"
#include "UI/sizer.h"
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
            struct PanelFixture
            {
                Tokens tokens = Tokens::load("chronicle/tokens.json");
                EntitySystem ecs;
                MasterRenderer renderer;
                TTFTextSystem* ttf = nullptr;
                IconSystem* icons = nullptr;
                PaintSystem* paint = nullptr;
                TextStyles styles;

                PanelFixture()
                {
                    ecs.createSystem<PositionComponentSystem>();
                    ecs.createSystem<LayoutSystem>();
                    ecs.succeed<PositionComponentSystem, LayoutSystem>();
                    ttf = ecs.createSystem<TTFTextSystem>(&renderer);
                    ecs.createSystem<Simple2DObjectSystem>(&renderer);
                    ecs.createSystem<HatchRect2DObjectSystem>(&renderer);
                    ecs.createSystem<DottedLine2DObjectSystem>(&renderer);
                    ecs.createSystem<StrokeRect2DObjectSystem>(&renderer);
                    icons = ecs.createSystem<IconSystem>(&renderer);
                    paint = ecs.createSystem<PaintSystem>(&tokens);
                    styles = TextStyles::fromTokens(tokens);
                    styles.registerAll(ttf, "fonts");

                    installIconEntries();
                }

                void installIconEntries()
                {
                    std::vector<IconEntry> marks;
                    const int msizes[5] = {14, 16, 18, 24, 48};
                    for (const auto& name : markNames())
                        for (int s : msizes)
                            marks.push_back({name, s, {s, s}, {0.0f, 0.0f}, {0.1f, 0.1f}});
                    icons->registerEntriesForTest("chronicle", marks, 1024, 1024);
                    renderer.registerTexture("IconAtlas_chronicle", OpenGLTexture{});

                    std::vector<IconEntry> orn;
                    const char* onames[7] = {"knot", "flourish", "corner-tl", "corner-tr", "corner-bl", "corner-br", "versal-curls"};
                    const int osizes[4] = {22, 28, 72, 120};
                    for (const char* n : onames)
                        for (int s : osizes)
                            orn.push_back({n, s, {s, s}, {0.0f, 0.0f}, {0.1f, 0.1f}});
                    icons->registerEntriesForTest("chronicle-ornaments", orn, 1024, 1024);
                    renderer.registerTexture("IconAtlas_chronicle-ornaments", OpenGLTexture{});
                }

                void settle() { ecs.executeOnce(); ecs.executeOnce(); ecs.executeOnce(); }

                EntityRef bodyRow(const std::string& text = "Row")
                {
                    LabelSpec s; s.style = "body"; s.text = text; s.colour = "ink"; s.z = 20;
                    return makeLabel(&ecs, tokens, styles, s).entity;
                }

                float asc(const std::string& style)
                {
                    const TextStyle& s = styles.get(style);
                    return ttf->measureText(s.fontAlias, "H", 1.0f, 0.0f, 0.0f, s.letterSpacingPx).ascender;
                }

                CompRef<PositionComponent> pos(EntityRef e) { return ecs.getEntity(e.id)->get<PositionComponent>(); }
                CompRef<Simple2DObject> s2d(EntityRef e) { return ecs.getEntity(e.id)->get<Simple2DObject>(); }
                CompRef<StrokeRect2DObject> strokeOf(EntityRef e) { return ecs.getEntity(e.id)->get<StrokeRect2DObject>(); }
                CompRef<IconComponent> iconOf(EntityRef e) { return ecs.getEntity(e.id)->get<IconComponent>(); }
                CompRef<TTFText> ttfOf(EntityRef e) { return ecs.getEntity(e.id)->get<TTFText>(); }
            };
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(panel_test, ruled_geometry)
        {
            MockLogger logger;
            PanelFixture s;

            Panel p = makePanel(&s.ecs, s.tokens, s.styles, {PanelFrame::Ruled, 320.0f, "Parts", "strength", "LEDGER"});
            p.addChild(&s.ecs, s.bodyRow());
            s.settle();

            EXPECT_FLOAT_EQ(s.pos(p.root)->width, 320.0f);
            EXPECT_NEAR(s.pos(p.root)->height, 107.0f, 0.5f);

            EXPECT_NEAR(s.pos(p.ground)->width, 320.0f, 0.5f);
            EXPECT_NEAR(s.pos(p.ground)->height, 107.0f, 0.5f);

            EXPECT_NEAR(s.pos(p.frame)->width, 320.0f, 0.5f);
            EXPECT_NEAR(s.pos(p.frame)->height, 107.0f, 0.5f);
            EXPECT_FLOAT_EQ(s.strokeOf(p.frame)->strokeWidth, 1.0f);
            EXPECT_FALSE(s.strokeOf(p.frame)->doubled);
            EXPECT_FLOAT_EQ(s.strokeOf(p.frame)->colors.x, s.tokens.colour("rule-ruled").x);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(panel_test, hair_frame)
        {
            MockLogger logger;
            PanelFixture s;

            Panel p = makePanel(&s.ecs, s.tokens, s.styles, {PanelFrame::Hair, 320.0f, "Skills", "study"});
            p.addChild(&s.ecs, s.bodyRow());
            s.settle();

            EXPECT_FLOAT_EQ(s.strokeOf(p.frame)->strokeWidth, 1.0f);
            EXPECT_FLOAT_EQ(s.strokeOf(p.frame)->colors.x, s.tokens.colour("rule-hair").x);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(panel_test, plain_has_nothing)
        {
            MockLogger logger;
            PanelFixture s;

            PanelSpec spec; spec.frame = PanelFrame::Plain; spec.width = 320.0f; spec.heading = "Notes";
            Panel p = makePanel(&s.ecs, s.tokens, s.styles, spec);
            p.addChild(&s.ecs, s.bodyRow());
            s.settle();

            EXPECT_TRUE(p.ground.empty());
            EXPECT_TRUE(p.frame.empty());
            EXPECT_FLOAT_EQ(p.padding, 0.0f);
            EXPECT_NEAR(s.pos(p.root)->height, 51.0f + 24.0f, 0.5f);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(panel_test, illuminated_frame_and_corners)
        {
            MockLogger logger;
            PanelFixture s;

            PanelSpec spec; spec.frame = PanelFrame::Illuminated; spec.width = 320.0f;
            Panel p = makePanel(&s.ecs, s.tokens, s.styles, spec);
            p.addChild(&s.ecs, s.bodyRow());
            s.settle();

            EXPECT_FLOAT_EQ(p.padding, 24.0f);
            EXPECT_TRUE(s.strokeOf(p.frame)->doubled);
            EXPECT_FLOAT_EQ(s.strokeOf(p.frame)->strokeWidth, 1.0f);
            EXPECT_FLOAT_EQ(s.strokeOf(p.frame)->gap, 1.0f);
            EXPECT_FLOAT_EQ(s.strokeOf(p.frame)->colors.x, s.tokens.colour("gold-edge").x);

            ASSERT_EQ(p.corners.size(), 4u);
            const float W = s.pos(p.root)->width, H = s.pos(p.root)->height;
            const float rx = s.pos(p.root)->x, ry = s.pos(p.root)->y, rz = s.pos(p.root)->z;
            const float ex[4] = {rx + 2.0f, rx + W - 30.0f, rx + 2.0f, rx + W - 30.0f};
            const float ey[4] = {ry + 2.0f, ry + 2.0f, ry + H - 30.0f, ry + H - 30.0f};
            for (int i = 0; i < 4; ++i)
            {
                EXPECT_FLOAT_EQ(s.pos(p.corners[i].root)->width, 28.0f);
                EXPECT_NEAR(s.pos(p.corners[i].root)->x, ex[i], 0.5f);
                EXPECT_NEAR(s.pos(p.corners[i].root)->y, ey[i], 0.5f);
                EXPECT_FLOAT_EQ(s.pos(p.corners[i].root)->z, rz + 2.0f);
            }
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(panel_test, no_heading_no_rule)
        {
            MockLogger logger;
            PanelFixture s;

            Panel p = makePanel(&s.ecs, s.tokens, s.styles, {PanelFrame::Ruled, 320.0f, ""});
            p.addChild(&s.ecs, s.bodyRow());
            s.settle();

            EXPECT_FALSE(p.rule.has_value());
            EXPECT_FALSE(p.title.has_value());
            EXPECT_NEAR(s.pos(p.root)->height, 56.0f, 0.5f);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(panel_test, head_row_positions)
        {
            MockLogger logger;
            PanelFixture s;

            Panel p = makePanel(&s.ecs, s.tokens, s.styles, {PanelFrame::Ruled, 320.0f, "Parts", "strength", "LEDGER"});
            p.addChild(&s.ecs, s.bodyRow());
            s.settle();

            ASSERT_TRUE(p.glyph.has_value());
            ASSERT_TRUE(p.title.has_value());
            ASSERT_TRUE(p.aside.has_value());

            const float rx = s.pos(p.root)->x;
            EXPECT_NEAR(s.pos(p.glyph->entity)->x, rx + 16.0f, 0.5f);
            const float glyphCentre = s.pos(p.glyph->entity)->y + s.pos(p.glyph->entity)->height / 2.0f;
            EXPECT_NEAR(glyphCentre, s.pos(p.title->entity)->y + 13.0f, 0.5f);
            EXPECT_NEAR(s.pos(p.title->entity)->x, s.pos(p.glyph->entity)->x + 18.0f + 8.0f, 0.5f);

            const float asideRight = s.pos(p.aside->entity)->x + s.pos(p.aside->entity)->width;
            EXPECT_NEAR(asideRight, s.pos(p.root)->x + s.pos(p.root)->width - 16.0f, 0.5f);
            EXPECT_FLOAT_EQ(s.ttfOf(p.aside->entity)->colors.x, s.tokens.colour("ink-muted").x);
            EXPECT_EQ(p.aside->spec.style, "label");
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(panel_test, head_baselines_align)
        {
            MockLogger logger;
            PanelFixture s;
            Panel p = makePanel(&s.ecs, s.tokens, s.styles, {PanelFrame::Ruled, 320.0f, "Parts", "strength", "LEDGER"});
            p.addChild(&s.ecs, s.bodyRow());
            s.settle();
            const float asideBaseline = s.pos(p.aside->entity)->y + s.asc("label");
            const float titleBaseline = s.pos(p.title->entity)->y + s.asc("heading");
            EXPECT_NEAR(asideBaseline, titleBaseline, 0.5f);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(panel_test, title_width_makes_room)
        {
            MockLogger logger;
            PanelFixture s;

            const float inner = 320.0f - 32.0f;

            Panel p = makePanel(&s.ecs, s.tokens, s.styles, {PanelFrame::Ruled, 320.0f, "Parts", "strength", "LEDGER"});
            p.addChild(&s.ecs, s.bodyRow());
            s.settle();
            const float asideWidth = s.pos(p.aside->entity)->width;
            EXPECT_NEAR(s.pos(p.title->entity)->width, inner - 26.0f - (asideWidth + 8.0f), 0.5f);

            Panel bare = makePanel(&s.ecs, s.tokens, s.styles, {PanelFrame::Ruled, 320.0f, "Parts"});
            bare.addChild(&s.ecs, s.bodyRow());
            s.settle();
            EXPECT_NEAR(s.pos(bare.title->entity)->width, inner, 0.5f);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(panel_test, add_child_grows_root)
        {
            MockLogger logger;
            PanelFixture s;

            Panel p = makePanel(&s.ecs, s.tokens, s.styles, {PanelFrame::Ruled, 320.0f, "Parts", "strength"});
            EntityRef first = s.bodyRow("One");
            p.addChild(&s.ecs, first);
            s.settle();
            EXPECT_NEAR(s.pos(p.root)->height, 107.0f, 0.5f);
            const float firstY = s.pos(first)->y;

            EntityRef second = s.bodyRow("Two");
            p.addChild(&s.ecs, second);
            s.settle();
            EXPECT_NEAR(s.pos(p.root)->height, 143.0f, 0.5f);
            EXPECT_NEAR(s.pos(second)->y, firstY + 24.0f + 12.0f, 0.5f);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(panel_test, remove_child_shrinks)
        {
            MockLogger logger;
            PanelFixture s;

            Panel p = makePanel(&s.ecs, s.tokens, s.styles, {PanelFrame::Ruled, 320.0f, "Parts", "strength"});
            p.addChild(&s.ecs, s.bodyRow("One"));
            EntityRef second = s.bodyRow("Two");
            p.addChild(&s.ecs, second);
            s.settle();
            EXPECT_NEAR(s.pos(p.root)->height, 143.0f, 0.5f);

            p.removeChild(&s.ecs, second);
            s.settle();
            EXPECT_NEAR(s.pos(p.root)->height, 107.0f, 0.5f);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(panel_test, body_is_inner_width)
        {
            MockLogger logger;
            PanelFixture s;

            Panel p = makePanel(&s.ecs, s.tokens, s.styles, {PanelFrame::Ruled, 320.0f, "Parts"});
            EXPECT_FLOAT_EQ(p.innerWidth(), 288.0f);

            LabelSpec ws; ws.style = "body"; ws.overflow = Overflow::Wrap; ws.width = p.innerWidth();
            ws.text = "The quick brown fox jumps over the lazy dog and keeps on running past the edge"; ws.z = 20;
            Label wrapped = makeLabel(&s.ecs, s.tokens, s.styles, ws);
            p.addChild(&s.ecs, wrapped.entity);
            s.settle();

            EXPECT_NEAR(s.pos(p.body)->width, 288.0f, 0.5f);
            EXPECT_LE(s.ttfOf(wrapped.entity)->textWidth, 288.0f);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(panel_test, set_width_follows)
        {
            MockLogger logger;
            PanelFixture s;

            Panel p = makePanel(&s.ecs, s.tokens, s.styles, {PanelFrame::Ruled, 320.0f, "Parts", "strength", "LEDGER"});
            p.addChild(&s.ecs, s.bodyRow());
            s.settle();

            p.setWidth(&s.ecs, 480.0f);
            s.settle();

            EXPECT_NEAR(s.pos(p.ground)->width, 480.0f, 0.5f);
            EXPECT_NEAR(s.pos(p.frame)->width, 480.0f, 0.5f);
            EXPECT_NEAR(s.pos(p.body)->width, 448.0f, 0.5f);
            EXPECT_NEAR(s.pos(p.rule->root)->width, 448.0f, 0.5f);

            const float asideWidth = s.pos(p.aside->entity)->width;
            EXPECT_NEAR(s.pos(p.title->entity)->width, 448.0f - 26.0f - (asideWidth + 8.0f), 0.5f);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(panel_test, z_bands)
        {
            MockLogger logger;
            PanelFixture s;

            Panel p = makePanel(&s.ecs, s.tokens, s.styles, {PanelFrame::Illuminated, 320.0f, "Parts", "strength", "LEDGER"});
            p.addChild(&s.ecs, s.bodyRow());
            s.settle();

            EXPECT_FLOAT_EQ(s.pos(p.root)->z, 10.0f);
            EXPECT_FLOAT_EQ(s.pos(p.ground)->z, 10.0f);
            EXPECT_FLOAT_EQ(s.pos(p.frame)->z, 11.0f);
            EXPECT_FLOAT_EQ(s.pos(p.corners[0].root)->z, 12.0f);
            EXPECT_FLOAT_EQ(s.pos(p.glyph->entity)->z, 13.0f);
            EXPECT_FLOAT_EQ(s.pos(p.title->entity)->z, 14.0f);
            EXPECT_FLOAT_EQ(s.pos(p.aside->entity)->z, 14.0f);
            EXPECT_FLOAT_EQ(s.pos(p.rule->root)->z, 13.0f);
            EXPECT_FLOAT_EQ(s.pos(p.body)->z, 20.0f);

            // contentZ must clear the head band; a too-low value is corrected to z+10.
            PanelSpec bad; bad.frame = PanelFrame::Ruled; bad.width = 320.0f; bad.heading = "X"; bad.z = 10; bad.contentZ = 12;
            Panel corrected = makePanel(&s.ecs, s.tokens, s.styles, bad);
            EXPECT_EQ(corrected.spec.contentZ, 20);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(panel_test, repaints_with_theme)
        {
            MockLogger logger;
            PanelFixture s;

            Panel p = makePanel(&s.ecs, s.tokens, s.styles, {PanelFrame::Ruled, 320.0f, "Parts", "strength"});
            p.addChild(&s.ecs, s.bodyRow());
            s.settle();

            s.tokens.setTheme(Theme::Candle);
            s.ecs.sendEvent(ThemeChangedEvent{Theme::Candle});
            s.settle();

            EXPECT_FLOAT_EQ(s.s2d(p.ground)->colors.x, s.tokens.colour("folio", Theme::Candle).x);
            EXPECT_FLOAT_EQ(s.strokeOf(p.frame)->colors.x, s.tokens.colour("rule-ruled", Theme::Candle).x);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(panel_test, nested_panel)
        {
            MockLogger logger;
            PanelFixture s;

            Panel outer = makePanel(&s.ecs, s.tokens, s.styles, {PanelFrame::Ruled, 320.0f, "Who he is"});

            PanelSpec is; is.frame = PanelFrame::Ruled; is.width = 288.0f; is.heading = "Skills"; is.z = 20; is.contentZ = 30;
            Panel inner = makePanel(&s.ecs, s.tokens, s.styles, is);
            inner.addChild(&s.ecs, s.bodyRow());
            s.settle();
            const float innerH = s.pos(inner.root)->height;

            outer.addChild(&s.ecs, inner.root);
            s.settle();

            EXPECT_NEAR(s.pos(outer.root)->height, 16.0f + 51.0f + innerH + 16.0f, 0.5f);
            EXPECT_FLOAT_EQ(s.pos(inner.root)->z, 20.0f);
            EXPECT_FLOAT_EQ(s.pos(inner.body)->z, 30.0f);
        }
    }
}
