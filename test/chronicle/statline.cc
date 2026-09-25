#include "stdafx.h"

#include <cmath>
#include <string>

#include <gtest/gtest.h>

#include "UI/statline.h"
#include "UI/panel.h"
#include "UI/paint.h"
#include "UI/gloss.h"
#include "Core/motion.h"
#include "Core/textstyle.h"

#include "ECS/entitysystem.h"
#include "ECS/entitysystem_fwd.h"   // ResizeEvent
#include "UI/ttftext.h"
#include "UI/iconsystem.h"
#include "UI/sizer.h"
#include "UI/prefab.h"
#include "UI/tooltip.h"
#include "UI/gamedataview.h"
#include "Systems/tween.h"
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
            struct StatLineFixture
            {
                Tokens tokens = Tokens::load("chronicle/tokens.json");
                EntitySystem ecs;
                MasterRenderer renderer;
                TTFTextSystem* ttf = nullptr;
                IconSystem* icons = nullptr;
                PaintSystem* paint = nullptr;
                GameDataView* view = nullptr;
                TooltipSystem* tip = nullptr;
                GlossRegistry* reg = nullptr;
                TextStyles styles;

                StatLineFixture()
                {
                    Motion::setReduced(true);   // the animation test flips this back
                    ecs.createSystem<PositionComponentSystem>();
                    ecs.createSystem<LayoutSystem>();
                    ecs.succeed<PositionComponentSystem, LayoutSystem>();
                    ttf = ecs.createSystem<TTFTextSystem>(&renderer);
                    ecs.createSystem<Simple2DObjectSystem>(&renderer);
                    ecs.createSystem<HatchRect2DObjectSystem>(&renderer);
                    ecs.createSystem<DottedLine2DObjectSystem>(&renderer);
                    ecs.createSystem<StrokeRect2DObjectSystem>(&renderer);
                    icons = ecs.createSystem<IconSystem>(&renderer);
                    ecs.createSystem<MouseHoverSystem>();
                    tip = ecs.createSystem<TooltipSystem>();
                    ecs.succeed<MouseHoverSystem, TooltipSystem>();
                    ecs.createSystem<TweenSystem>();
                    view = ecs.createSystem<GameDataView>();
                    paint = ecs.createSystem<PaintSystem>(&tokens);
                    styles = TextStyles::fromTokens(tokens);
                    styles.registerAll(ttf, "fonts");
                    tip->setDefaultFont("chr-body-sm");
                    reg = ecs.createSystem<GlossRegistry>(&tokens, &styles);
                    installIconEntries();
                    ecs.sendEvent(ResizeEvent{1320.0f, 860.0f});
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

                    // The Parts panel (test 13) draws a divider ornament.
                    std::vector<IconEntry> orn;
                    const char* onames[7] = {"knot", "flourish", "corner-tl", "corner-tr", "corner-bl", "corner-br", "versal-curls"};
                    const int osizes[4] = {22, 28, 72, 120};
                    for (const char* n : onames)
                        for (int os : osizes)
                            orn.push_back({n, os, {os, os}, {0.0f, 0.0f}, {0.1f, 0.1f}});
                    icons->registerEntriesForTest("chronicle-ornaments", orn, 1024, 1024);
                    renderer.registerTexture("IconAtlas_chronicle-ornaments", OpenGLTexture{});
                }

                void settle() { ecs.executeOnce(); ecs.executeOnce(); ecs.executeOnce(); }
                void tick(float ms) { ecs.sendEvent(TickEvent{ms}); ecs.executeOnce(); ecs.executeOnce(); ecs.executeOnce(); }

                StatLine make(const StatLineSpec& spec, float x = 100.0f, float y = 100.0f)
                {
                    StatLine sl = makeStatLine(&ecs, tokens, styles, spec);
                    sl.root->get<PositionComponent>()->setX(x);
                    sl.root->get<PositionComponent>()->setY(y);
                    settle();
                    return sl;
                }

                float asc(const std::string& style)
                {
                    const TextStyle& s = styles.get(style);
                    return ttf->measureText(s.fontAlias, "H", 1.0f, 0.0f, 0.0f, s.letterSpacingPx).ascender;
                }

                CompRef<PositionComponent> pos(EntityRef e) { return ecs.getEntity(e.id)->get<PositionComponent>(); }
                float rightEdge(EntityRef box) { return pos(box)->x + pos(box)->width; }
                std::string token(EntityRef e) { return ecs.getEntity(e.id)->get<PaintComponent>()->token; }
                bool hasTween(EntityRef e) { return ecs.getEntity(e.id)->has<TweenComponent>(); }
            };
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(statline_test, head_geometry)
        {
            MockLogger logger;
            StatLineFixture s;

            StatLine sl = s.make({288.0f, "Strength", "strength", 14, 30});

            EXPECT_FLOAT_EQ(s.pos(sl.root)->width, 288.0f);
            EXPECT_NEAR(s.pos(sl.root)->height, 34.0f, 0.5f);

            // name at the root's left, mark S16, both ink-muted; caps label text.
            EXPECT_NEAR(s.pos(sl.name.root)->x, s.pos(sl.root)->x, 0.5f);
            ASSERT_TRUE(sl.name.mark.has_value());
            EXPECT_EQ(sl.name.mark->spec.size, MarkSize::S16);
            EXPECT_EQ(s.token(sl.name.mark->entity), "ink-muted");
            EXPECT_EQ(sl.name.label.spec.text, "STRENGTH");
            EXPECT_EQ(sl.name.label.spec.style, "label");
            EXPECT_EQ(s.token(sl.name.label.entity), "ink-muted");

            // figure at the right, ink.
            EXPECT_EQ(sl.figure.spec.text, "14");
            EXPECT_EQ(sl.figure.spec.style, "figure");
            EXPECT_EQ(s.token(sl.figure.entity), "ink");
            EXPECT_NEAR(s.rightEdge(sl.figure.entity), s.pos(sl.root)->x + 288.0f, 0.5f);

            // one baseline.
            const float nameBaseline = s.pos(sl.name.label.entity)->y + s.asc("label");
            const float figBaseline = s.pos(sl.figure.entity)->y + s.asc("figure");
            EXPECT_NEAR(nameBaseline, figBaseline, 0.5f);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(statline_test, groove_is_progress_rule_at_8)
        {
            MockLogger logger;
            StatLineFixture s;

            StatLine sl = s.make({288.0f, "Strength", "strength", 14, 30});

            EXPECT_FLOAT_EQ(s.pos(sl.groove.track)->height, 8.0f);
            EXPECT_FALSE(sl.groove.nib.has_value());
            EXPECT_NEAR(s.pos(sl.groove.track)->width, 288.0f, 0.5f);
            EXPECT_NEAR(s.pos(sl.groove.root)->y, s.pos(sl.root)->y + 26.0f, 0.5f);
            EXPECT_NEAR(s.pos(sl.groove.fill)->width, 286.0f * 14.0f / 30.0f, 0.01f);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(statline_test, figure_does_not_reflow)
        {
            MockLogger logger;
            StatLineFixture s;

            StatLine sl = s.make({288.0f, "Strength", "strength", 14, 30});
            const float nameX = s.pos(sl.name.root)->x;
            const float grooveX = s.pos(sl.groove.root)->x;
            const float edge0 = s.rightEdge(sl.figure.entity);

            sl.setValue(&s.ecs, s.styles, 9, false);
            s.settle();
            const float w9 = s.pos(sl.figure.entity)->width;
            EXPECT_NEAR(s.rightEdge(sl.figure.entity), edge0, 0.01f);

            sl.setValue(&s.ecs, s.styles, 15, false);
            s.settle();
            const float w15 = s.pos(sl.figure.entity)->width;
            EXPECT_NEAR(s.rightEdge(sl.figure.entity), edge0, 0.01f);
            EXPECT_GT(w15, w9);   // "15" is wider than "9", but grows leftward
            EXPECT_NEAR(s.pos(sl.name.root)->x, nameX, 0.01f);
            EXPECT_NEAR(s.pos(sl.groove.root)->x, grooveX, 0.01f);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(statline_test, projected_shows_only_above_value)
        {
            MockLogger logger;
            StatLineFixture s;

            StatLine sl = s.make({288.0f, "Strength", "strength", 14, 30, 17});

            ASSERT_TRUE(sl.projected.has_value());
            EXPECT_EQ(sl.projected->spec.text, std::string("\xE2\x86\x92 ") + "17");
            EXPECT_EQ(sl.projected->spec.style, "tick");
            EXPECT_EQ(s.token(sl.projected->entity), "progress-forecast");
            EXPECT_NEAR(s.rightEdge(sl.projected->entity), s.pos(sl.root)->x + 288.0f, 0.5f);

            const float projBaseline = s.pos(sl.projected->entity)->y + s.asc("tick");
            const float figBaseline = s.pos(sl.figure.entity)->y + s.asc("figure");
            EXPECT_NEAR(projBaseline, figBaseline, 0.5f);

            EXPECT_NEAR(s.rightEdge(sl.figure.entity), s.pos(sl.projected->entity)->x - 4.0f, 0.5f);
            EXPECT_NEAR(sl.groove.spec.forecastPercent, 100.0f * 17.0f / 30.0f, 0.01f);

            // A projection at the value is not a projection.
            sl.setProjected(&s.ecs, s.styles, 14);
            s.settle();
            EXPECT_FALSE(sl.projected.has_value());
            EXPECT_FLOAT_EQ(sl.groove.spec.forecastPercent, 0.0f);
            EXPECT_NEAR(s.rightEdge(sl.figure.entity), s.pos(sl.root)->x + 288.0f, 0.5f);

            // Below the value: same.
            sl.setProjected(&s.ecs, s.styles, 12);
            s.settle();
            EXPECT_FALSE(sl.projected.has_value());
            EXPECT_FLOAT_EQ(sl.groove.spec.forecastPercent, 0.0f);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(statline_test, value_rising_past_projection_clears_it)
        {
            MockLogger logger;
            StatLineFixture s;

            StatLine sl = s.make({288.0f, "Strength", "strength", 14, 30, 17});
            ASSERT_TRUE(sl.projected.has_value());

            sl.setValue(&s.ecs, s.styles, 17, false);
            s.settle();
            EXPECT_FALSE(sl.projected.has_value());
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(statline_test, threshold_stands_on_track)
        {
            MockLogger logger;
            StatLineFixture s;

            StatLineSpec spec; spec.width = 288.0f; spec.threshold = 18; spec.max = 30;
            StatLine sl = s.make(spec);

            EXPECT_FLOAT_EQ(s.pos(sl.threshold)->width, 2.0f);
            EXPECT_FLOAT_EQ(s.pos(sl.threshold)->height, 14.0f);
            EXPECT_EQ(s.token(sl.threshold), "status-time");

            const float centre = s.pos(sl.threshold)->x + 1.0f;
            EXPECT_NEAR(centre, s.pos(sl.groove.root)->x + 1.0f + 286.0f * 18.0f / 30.0f, 0.5f);
            EXPECT_NEAR(s.pos(sl.threshold)->y, s.pos(sl.groove.root)->y - 3.0f, 0.5f);

            sl.setThreshold(&s.ecs, 0);
            s.settle();
            EXPECT_FALSE(s.pos(sl.threshold)->isVisible());   // entity kept

            sl.setThreshold(&s.ecs, 40);
            s.settle();
            EXPECT_TRUE(s.pos(sl.threshold)->isVisible());
            EXPECT_EQ(sl.spec.threshold, 30);                 // clamped to max
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(statline_test, note_below_groove)
        {
            MockLogger logger;
            StatLineFixture s;

            StatLineSpec spec; spec.width = 288.0f; spec.note = "WARRIOR AT 18 ASKS 18";
            StatLine sl = s.make(spec);

            ASSERT_TRUE(sl.note.has_value());
            EXPECT_EQ(sl.note->spec.style, "caption");
            EXPECT_EQ(s.token(sl.note->entity), "ink-faint");
            EXPECT_NEAR(s.pos(sl.note->entity)->y, s.pos(sl.groove.root)->y + 8.0f + 4.0f, 0.5f);
            EXPECT_NEAR(s.pos(sl.root)->height, 53.0f, 0.5f);

            sl.setNote(&s.ecs, s.styles, "");
            s.settle();
            EXPECT_NEAR(s.pos(sl.root)->height, 34.0f, 0.5f);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(statline_test, label_is_upper_cased)
        {
            MockLogger logger;
            StatLineFixture s;

            StatLineSpec a; a.label = "Intellect"; a.glyph = "intelligence";
            EXPECT_EQ(s.make(a).name.label.spec.text, "INTELLECT");

            StatLineSpec b; b.label = "Vitalité"; b.glyph = "vitality";
            EXPECT_EQ(s.make(b, 100.0f, 300.0f).name.label.spec.text, "VITALITé");   // ASCII only
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(statline_test, value_is_clamped)
        {
            MockLogger logger;
            StatLineFixture s;

            StatLine sl = s.make({288.0f, "Strength", "strength", 40, 30});
            EXPECT_EQ(sl.figure.spec.text, "30");
            EXPECT_NEAR(sl.groove.shown, 100.0f, 0.01f);

            sl.setValue(&s.ecs, s.styles, -3, false);
            s.settle();
            EXPECT_EQ(sl.figure.spec.text, "0");
            EXPECT_NEAR(sl.groove.shown, 0.0f, 0.01f);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(statline_test, animation_is_inherited)
        {
            MockLogger logger;
            StatLineFixture s;
            Motion::setReduced(false);

            StatLine sl = s.make({288.0f, "Strength", "strength", 14, 30});
            sl.setValue(&s.ecs, s.styles, 24);   // animate

            EXPECT_TRUE(s.hasTween(sl.groove.fill));
            EXPECT_EQ(sl.figure.spec.text, "24");   // the figure is the fact, at once

            s.tick(2000.0f);
            EXPECT_NEAR(sl.groove.shown, 80.0f, 0.01f);

            Motion::setReduced(false);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(statline_test, gloss_attached_to_root)
        {
            MockLogger logger;
            StatLineFixture s;

            StatLineSpec spec; spec.glossKey = "parts/strength";
            StatLine sl = s.make(spec);
            ASSERT_TRUE(s.ecs.getEntity(sl.root.id)->has<TooltipComponent>());
            auto tc = s.ecs.getEntity(sl.root.id)->get<TooltipComponent>();
            EXPECT_EQ(tc->text, "parts/strength");
            EXPECT_EQ(tc->style, "gloss");

            StatLineSpec none; none.glossKey = "";
            StatLine sl2 = s.make(none, 100.0f, 300.0f);
            EXPECT_FALSE(s.ecs.getEntity(sl2.root.id)->has<TooltipComponent>());
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(statline_test, fed_from_gamedataview)
        {
            MockLogger logger;
            StatLineFixture s;

            StatLine sl = s.make({288.0f, "Strength", "strength", 14, 30});

            s.view->subscribe("character.parts.str", [&](const ElementType& v) {
                sl.setValue(&s.ecs, s.styles, v.get<int>(), false);
            });
            s.view->subscribe("character.parts.str.projected", [&](const ElementType& v) {
                sl.setProjected(&s.ecs, s.styles, v.get<int>());
            });
            s.view->subscribe("character.parts.str.threshold", [&](const ElementType& v) {
                sl.setThreshold(&s.ecs, v.get<int>());
            });

            s.view->set("character.parts.str", ElementType{16});
            s.settle();
            EXPECT_EQ(sl.figure.spec.text, "16");

            s.view->set("character.parts.str.projected", ElementType{19});
            s.settle();
            ASSERT_TRUE(sl.projected.has_value());
            EXPECT_EQ(sl.projected->spec.text, std::string("\xE2\x86\x92 ") + "19");

            s.view->set("character.parts.str.threshold", ElementType{18});
            s.settle();
            EXPECT_TRUE(s.pos(sl.threshold)->isVisible());
            EXPECT_NEAR(s.pos(sl.threshold)->x, s.pos(sl.groove.root)->x + 286.0f * 18.0f / 30.0f, 0.5f);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(statline_test, four_parts_stack_in_a_panel)
        {
            MockLogger logger;
            StatLineFixture s;

            // No notes: P(16) + head(51) + 4 x 34 + 3 x 12 + P(16) = 255.
            {
                Panel p = makePanel(&s.ecs, s.tokens, s.styles, {PanelFrame::Ruled, 320.0f, "Parts", "strength", "LEDGER"});
                for (int i = 0; i < 4; ++i)
                {
                    StatLineSpec ls; ls.width = 288.0f; ls.value = 14; ls.z = p.spec.contentZ;
                    p.addChild(&s.ecs, makeStatLine(&s.ecs, s.tokens, s.styles, ls).root);
                }
                s.settle();
                EXPECT_NEAR(p.height(&s.ecs), 255.0f, 1.0f);
            }

            // One note on the first line adds 4 + 15 = 19.
            {
                Panel p = makePanel(&s.ecs, s.tokens, s.styles, {PanelFrame::Ruled, 320.0f, "Parts", "strength", "LEDGER"});
                for (int i = 0; i < 4; ++i)
                {
                    StatLineSpec ls; ls.width = 288.0f; ls.value = 14; ls.z = p.spec.contentZ;
                    if (i == 0) ls.note = "WARRIOR AT 18 ASKS 18";
                    p.addChild(&s.ecs, makeStatLine(&s.ecs, s.tokens, s.styles, ls).root);
                }
                s.settle();
                EXPECT_NEAR(p.height(&s.ecs), 274.0f, 1.0f);
            }
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(statline_test, z_bands)
        {
            MockLogger logger;
            StatLineFixture s;

            StatLineSpec spec;
            spec.width = 288.0f; spec.value = 14; spec.projected = 17; spec.threshold = 18;
            spec.note = "WARRIOR AT 18 ASKS 18"; spec.z = 20;
            StatLine sl = s.make(spec);

            EXPECT_FLOAT_EQ(s.pos(sl.root)->z, 20.0f);

            EXPECT_FLOAT_EQ(s.pos(sl.name.label.entity)->z, 22.0f);
            EXPECT_FLOAT_EQ(s.pos(sl.figure.entity)->z, 22.0f);
            ASSERT_TRUE(sl.projected.has_value());
            EXPECT_FLOAT_EQ(s.pos(sl.projected->entity)->z, 22.0f);
            ASSERT_TRUE(sl.note.has_value());
            EXPECT_FLOAT_EQ(s.pos(sl.note->entity)->z, 22.0f);

            EXPECT_FLOAT_EQ(s.pos(sl.groove.root)->z, 21.0f);
            EXPECT_FLOAT_EQ(s.pos(sl.groove.fill)->z, 22.0f);
            EXPECT_FLOAT_EQ(s.pos(sl.groove.forecast)->z, 23.0f);
            EXPECT_FLOAT_EQ(s.pos(sl.groove.frame)->z, 24.0f);

            EXPECT_FLOAT_EQ(s.pos(sl.threshold)->z, 26.0f);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // Repro for the StatGallery report: hovering a line should show its gloss AT THE CURSOR,
        // with the gloss box's own content following the tooltip root (not stuck at 0,0).
        TEST(statline_test, gloss_shows_at_cursor_on_hover)
        {
            MockLogger logger;
            StatLineFixture s;

            GlossSpec g; g.title = "Strength"; g.text = "Lifting, striking, enduring.";
            g.rows = {{"Now", "14"}, {"At term", "17"}};
            s.reg->set("parts/strength", g);

            StatLineSpec spec; spec.width = 288.0f; spec.value = 14; spec.glossKey = "parts/strength";
            StatLine sl = s.make(spec, 200.0f, 150.0f);
            s.settle(); s.settle();   // let the TooltipSystem group attach hover components

            // Hover over the line, then wait past the 200 ms delay.
            s.ecs.sendEvent(OnMouseMove{Point2D{250.0f, 160.0f}, nullptr});
            s.settle();
            s.tick(250.0f);
            s.settle(); s.settle();   // let the freshly-built gloss settle its anchors

            ASSERT_TRUE(s.tip->isShowing());

            // The tooltip root lands near the cursor.
            auto rootPos = s.pos(s.reg->lastBuilt.root);
            EXPECT_GT(rootPos->x, 100.0f);
            EXPECT_GT(rootPos->y, 100.0f);

            // The gloss content must FOLLOW that root, not be stranded at the origin.
            ASSERT_TRUE(s.reg->lastBuilt.title.has_value());
            auto titlePos = s.pos(s.reg->lastBuilt.title->entity);
            EXPECT_NEAR(titlePos->x, rootPos->x + 12.0f, 1.0f);   // PAD from the tooltip's left
            EXPECT_GT(titlePos->y, 100.0f);

            // And it must sit in the tooltip z band (150), above panel content but INSIDE the
            // camera's depth range - the default camera clips world z from ~190-200 up, so a
            // 200 band drew the gloss ground on the boundary and clipped the text above it.
            EXPECT_GE(s.pos(s.reg->lastBuilt.title->entity)->z, 150.0f);
            EXPECT_GE(s.pos(s.reg->lastBuilt.title->entity)->z, 150.0f);
            EXPECT_LT(s.pos(s.reg->lastBuilt.title->entity)->z, 190.0f);   // never past the near plane

            // The visible glyphs must clear the gloss's OWN folio background, or the text is
            // occluded by its own panel. (Regression: gloss text drew behind the gloss ground.)
            ASSERT_FALSE(s.reg->lastBuilt.ground.empty());
            EXPECT_GT(s.pos(s.reg->lastBuilt.title->entity)->z, s.pos(s.reg->lastBuilt.ground)->z);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // Diagnostic: the ECS position of a mid-run tooltip is correct (proven above), but the
        // render systems CACHE a RenderCall per entity and only rebuild it on a PositionSettledEvent
        // received while the entity is already in the render group. If a freshly-built tooltip's
        // settle is dispatched before it joins the group, the cached call stays at the (0,0)
        // build-time origin even though PositionComponent is at the cursor -> "flashes at 0,0".
        TEST(statline_test, gloss_render_call_tracks_position)
        {
            MockLogger logger;
            StatLineFixture s;

            GlossSpec g; g.title = "Strength"; g.text = "Lifting, striking, enduring.";
            g.rows = {{"Now", "14"}, {"At term", "17"}};
            s.reg->set("parts/strength", g);

            StatLineSpec spec; spec.width = 288.0f; spec.value = 14; spec.glossKey = "parts/strength";
            StatLine sl = s.make(spec, 200.0f, 150.0f);
            s.settle(); s.settle();

            s.ecs.sendEvent(OnMouseMove{Point2D{250.0f, 160.0f}, nullptr});
            s.settle();
            s.tick(250.0f);
            s.settle(); s.settle();

            ASSERT_TRUE(s.tip->isShowing());

            EntityRef ground = s.reg->lastBuilt.ground;
            ASSERT_FALSE(ground.empty());

            auto* s2d = s.ecs.getSystem<Simple2DObjectSystem>();
            ASSERT_NE(s2d, nullptr);
            ASSERT_TRUE(s2d->entityRenderCalls.count(ground.id) > 0);

            // RenderCall::data = { x, y, z, w, h, ... } (see Simple2DObjectSystem::createRenderCall).
            const RenderCall& rc = s2d->entityRenderCalls.at(ground.id);
            const float posX = s.pos(ground)->x;
            const float posY = s.pos(ground)->y;

            // If this fails with rc.data[0]==0 while posX>100, the cached render call is stale:
            // the tooltip is at the cursor in the ECS but drawn at the origin.
            EXPECT_NEAR(rc.data[0], posX, 1.0f);
            EXPECT_NEAR(rc.data[1], posY, 1.0f);

            // ── The TEXT's cached glyph calls: the piece that is broken in the live app. ──
            // The title text sits two prefab levels deep (gloss root -> label box -> TTFText);
            // the ground (checked above) is one level deep and works. Verify the glyph calls
            // exist, are marked visible, and carry the settled position and tooltip-band z.
            ASSERT_TRUE(s.reg->lastBuilt.title.has_value());
            EntityRef titleText = s.reg->lastBuilt.title->entity;
            auto titleUi = s.pos(titleText);

            ASSERT_TRUE(s.ttf->entityRenderCalls.count(titleText.id) > 0) << "title text has NO cached render calls";
            const auto& glyphCalls = s.ttf->entityRenderCalls.at(titleText.id);
            ASSERT_FALSE(glyphCalls.empty()) << "title text render-call list is empty";

            // TTFText call data = { x + relX, y + relY, z, ... }; relX/relY >= 0 for the first line.
            const RenderCall& g0 = glyphCalls.front();
            EXPECT_TRUE(g0.getVisibility()) << "glyph call carries visible=false (observable/visibility propagation)";
            EXPECT_GE(g0.data[0], titleUi->x - 1.0f) << "glyph x stale (built before the box settled)";
            EXPECT_LE(g0.data[0], titleUi->x + titleUi->width + 1.0f);
            EXPECT_GE(g0.data[1], titleUi->y - 1.0f) << "glyph y stale";
            EXPECT_GE(g0.data[2], 150.0f) << "glyph z below the tooltip band";
            EXPECT_LT(g0.data[2], 190.0f) << "glyph z past the camera's near plane (would be clipped)";
        }
    }
}
