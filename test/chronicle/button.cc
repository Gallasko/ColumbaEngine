#include "stdafx.h"

#ifdef __linux__
#include <SDL2/SDL.h>
#elif _WIN32
#include <SDL.h>
#endif

#include <cmath>

#include <gtest/gtest.h>

#include "UI/button.h"
#include "UI/paint.h"
#include "Core/textstyle.h"

#include "ECS/entitysystem.h"
#include "UI/ttftext.h"
#include "UI/iconsystem.h"
#include "UI/sizer.h"
#include "UI/focusable.h"
#include "Input/inputcomponent.h"
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
            struct ActivationRecorder : public System<Listener<ButtonActivatedEvent>, StoragePolicy>
            {
                std::string getSystemName() const override { return "Button Recorder"; }
                void onEvent(const ButtonActivatedEvent& e) override { events.push_back(e); }
                std::vector<ButtonActivatedEvent> events;
            };

            struct ButtonFixture
            {
                Tokens tokens = Tokens::load("chronicle/tokens.json");
                EntitySystem ecs;
                MasterRenderer renderer;
                TTFTextSystem* ttf = nullptr;
                IconSystem* icons = nullptr;
                PaintSystem* paint = nullptr;
                ButtonSystem* buttons = nullptr;
                FocusableSystem* focus = nullptr;
                ActivationRecorder* recorder = nullptr;
                TextStyles styles;

                ButtonFixture()
                {
                    ecs.createSystem<PositionComponentSystem>();
                    ecs.createSystem<LayoutSystem>();
                    ecs.succeed<PositionComponentSystem, LayoutSystem>();
                    ttf = ecs.createSystem<TTFTextSystem>(&renderer);
                    ecs.createSystem<Simple2DObjectSystem>(&renderer);
                    ecs.createSystem<RoundedRect2DObjectSystem>(&renderer);
                    ecs.createSystem<HatchRect2DObjectSystem>(&renderer);
                    ecs.createSystem<DottedLine2DObjectSystem>(&renderer);
                    ecs.createSystem<StrokeRect2DObjectSystem>(&renderer);
                    icons = ecs.createSystem<IconSystem>(&renderer);
                    ecs.createSystem<MouseHoverSystem>();
                    focus = ecs.createSystem<FocusableSystem>();
                    paint = ecs.createSystem<PaintSystem>(&tokens);
                    buttons = ecs.createSystem<ButtonSystem>(&tokens);
                    ecs.succeed<MouseHoverSystem, ButtonSystem>();
                    recorder = ecs.createSystem<ActivationRecorder>();
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

                void pump() { ecs.executeOnce(); ecs.executeOnce(); ecs.executeOnce(); }
                void hover(float x, float y) { ecs.sendEvent(OnMouseMove{Point2D{x, y}, nullptr}); pump(); }
                void press(float x, float y) { ecs.sendEvent(OnMouseClick{Point2D{x, y}, static_cast<MouseButton>(1)}); pump(); }
                void release(float x, float y) { ecs.sendEvent(OnMouseRelease{Point2D{x, y}, static_cast<MouseButton>(1)}); pump(); }
                void key(SDL_Scancode k, Uint16 mod = 0) { ecs.sendEvent(OnSDLScanCode{k, mod}); pump(); }

                Button place(const ButtonSpec& spec, float x = 100.0f, float y = 100.0f)
                {
                    Button b = makeButton(&ecs, tokens, styles, spec);
                    b.root->get<PositionComponent>()->setX(x);
                    b.root->get<PositionComponent>()->setY(y);
                    pump();
                    return b;
                }

                float controlWidth(const std::string& text)
                {
                    const TextStyle& s = styles.get("control");
                    return ttf->measureText(s.fontAlias, text, 1.0f, 0.0f, 0.0f, s.letterSpacingPx).width;
                }
                float tickWidth(const std::string& text)
                {
                    const TextStyle& s = styles.get("tick");
                    return ttf->measureText(s.fontAlias, text, 1.0f, 0.0f, 0.0f, s.letterSpacingPx).width;
                }

                CompRef<PositionComponent> pos(EntityRef e) { return ecs.getEntity(e.id)->get<PositionComponent>(); }
                CompRef<PositionComponent> pos(_unique_id id) { return ecs.getEntity(id)->get<PositionComponent>(); }
                std::string token(_unique_id id) { return ecs.getEntity(id)->get<PaintComponent>()->token; }
                float palpha(_unique_id id) { return ecs.getEntity(id)->get<PaintComponent>()->alpha; }
                ButtonState* state(const Button& b) { return ecs.getEntity(b.face.id)->get<ButtonState>().component; }
            };
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(button_test, face_geometry)
        {
            MockLogger logger;
            ButtonFixture s;

            Button plain = s.place({ButtonVariant::Quiet, "Train"});
            EXPECT_FLOAT_EQ(s.pos(plain.face)->height, 36.0f);
            EXPECT_NEAR(s.pos(plain.face)->width, 12.0f + s.controlWidth("Train") + 12.0f, 0.5f);
            EXPECT_FLOAT_EQ(s.pos(plain.root)->height, 36.0f);

            ButtonSpec gs; gs.label = "Train"; gs.glyph = "training";
            Button withGlyph = s.place(gs);
            EXPECT_NEAR(s.pos(withGlyph.face)->width, 12.0f + 24.0f + s.controlWidth("Train") + 12.0f, 0.5f);

            ButtonSpec cs; cs.label = "Train"; cs.glyph = "training"; cs.months = 3;
            Button withCost = s.place(cs);
            const float expected = 12.0f + 24.0f + s.controlWidth("Train") + 8.0f + 14.0f + 4.0f + s.tickWidth("3 mo") + 12.0f;
            EXPECT_NEAR(s.pos(withCost.face)->width, expected, 0.5f);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(button_test, variant_tokens)
        {
            MockLogger logger;
            ButtonFixture s;

            ButtonSpec qs; qs.variant = ButtonVariant::Quiet; qs.label = "Train"; qs.glyph = "training"; qs.months = 3;
            Button q = s.place(qs);
            EXPECT_EQ(s.token(s.state(q)->ground), "folio");
            EXPECT_EQ(s.token(s.state(q)->frame), "rule-ruled");
            EXPECT_EQ(s.token(q.label.text.id), "ink");
            EXPECT_EQ(s.token(q.costMark->entity.id), "status-time");

            ButtonSpec ds; ds.variant = ButtonVariant::Study; ds.label = "Study"; ds.glyph = "study"; ds.months = 9;
            Button d = s.place(ds);
            EXPECT_EQ(s.token(s.state(d)->ground), "lapis");
            EXPECT_EQ(s.token(s.state(d)->frame), "lapis");
            EXPECT_EQ(s.token(d.label.text.id), "on-lapis");
            EXPECT_EQ(s.token(d.costMark->entity.id), "on-lapis");

            ButtonSpec es; es.variant = ButtonVariant::Seal; es.label = "Take"; es.glyph = "seal"; es.months = 3;
            Button e = s.place(es);
            EXPECT_EQ(s.token(s.state(e)->ground), "vermilion");
            EXPECT_EQ(s.token(s.state(e)->frame), "vermilion");
            EXPECT_EQ(s.token(e.label.text.id), "on-vermilion");
            EXPECT_EQ(s.token(e.costMark->entity.id), "on-vermilion");
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(button_test, hover_tints_quiet_only_the_fill)
        {
            MockLogger logger;
            ButtonFixture s;

            Button q = s.place({ButtonVariant::Quiet, "Train"});
            s.hover(110.0f, 118.0f);
            EXPECT_EQ(s.token(s.state(q)->ground), "vellum-tint");
            EXPECT_EQ(s.token(s.state(q)->frame), "rule-ruled");
            EXPECT_EQ(s.token(q.label.text.id), "ink");

            s.hover(600.0f, 600.0f);
            EXPECT_EQ(s.token(s.state(q)->ground), "folio");
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(button_test, hover_sheens_tonal)
        {
            MockLogger logger;
            ButtonFixture s;

            Button seal = s.place({ButtonVariant::Seal, "Take the path", "seal"});
            s.hover(110.0f, 118.0f);
            EXPECT_NEAR(s.palpha(s.state(seal)->sheen), 0.14f, 0.001f);
            EXPECT_EQ(s.token(s.state(seal)->ground), "vermilion");

            s.hover(600.0f, 600.0f);
            EXPECT_NEAR(s.palpha(s.state(seal)->sheen), 0.0f, 0.001f);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(button_test, click_activates_on_release_inside)
        {
            MockLogger logger;
            ButtonFixture s;

            ButtonSpec spec; spec.label = "Train"; spec.tag = "train";
            Button b = s.place(spec);

            s.hover(110.0f, 118.0f);
            s.press(110.0f, 118.0f);
            EXPECT_EQ(s.recorder->events.size(), 0u);   // press alone does nothing

            s.release(110.0f, 118.0f);
            ASSERT_EQ(s.recorder->events.size(), 1u);
            EXPECT_EQ(s.recorder->events[0].tag, "train");
            EXPECT_EQ(s.recorder->events[0].face, b.face.id);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(button_test, release_outside_cancels)
        {
            MockLogger logger;
            ButtonFixture s;

            ButtonSpec spec; spec.label = "Train"; spec.tag = "train";
            Button b = s.place(spec);

            s.hover(110.0f, 118.0f);
            s.press(110.0f, 118.0f);
            s.hover(600.0f, 600.0f);       // drag off
            s.release(600.0f, 600.0f);
            EXPECT_EQ(s.recorder->events.size(), 0u);
            EXPECT_EQ(s.token(s.state(b)->ground), "folio");
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(button_test, topmost_button_wins)
        {
            MockLogger logger;
            ButtonFixture s;

            ButtonSpec low; low.label = "Low"; low.tag = "low"; low.z = 20;
            ButtonSpec high; high.label = "High"; high.tag = "high"; high.z = 30;
            Button b1 = s.place(low, 100.0f, 100.0f);
            Button b2 = s.place(high, 100.0f, 100.0f);   // overlapping

            s.hover(110.0f, 118.0f);
            s.press(110.0f, 118.0f);
            s.release(110.0f, 118.0f);

            ASSERT_EQ(s.recorder->events.size(), 1u);
            EXPECT_EQ(s.recorder->events[0].tag, "high");
            EXPECT_EQ(s.recorder->events[0].face, b2.face.id);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(button_test, disabled_swallows_and_states_reason)
        {
            MockLogger logger;
            ButtonFixture s;

            ButtonSpec spec; spec.label = "Squire"; spec.tag = "squire";
            spec.disabled = true; spec.reason = "SWORDSMANSHIP 3 OF 4";
            Button b = s.place(spec);

            s.hover(110.0f, 118.0f);
            s.press(110.0f, 118.0f);
            s.release(110.0f, 118.0f);
            EXPECT_EQ(s.recorder->events.size(), 0u);
            EXPECT_EQ(s.token(s.state(b)->ground), "folio");

            EXPECT_NEAR(s.palpha(s.state(b)->ground), 0.6f, 0.001f);
            EXPECT_NEAR(s.palpha(b.label.text.id), 0.6f, 0.001f);

            ASSERT_TRUE(b.reason.has_value());
            EXPECT_EQ(b.reason->spec.style, "caption");
            EXPECT_TRUE(s.pos(b.reason->box)->visible);
            EXPECT_NEAR(s.pos(b.reason->box)->y, s.pos(b.face)->y + 36.0f + 4.0f, 0.5f);
            EXPECT_NEAR(s.pos(b.root)->height, 55.0f, 0.5f);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(button_test, set_disabled_toggles)
        {
            MockLogger logger;
            ButtonFixture s;

            ButtonSpec spec; spec.label = "Squire"; spec.tag = "squire";
            spec.disabled = true; spec.reason = "SWORDSMANSHIP 3 OF 4";
            Button b = s.place(spec);

            b.setDisabled(&s.ecs, false);
            s.pump();
            EXPECT_NEAR(s.palpha(s.state(b)->ground), 1.0f, 0.001f);
            EXPECT_FALSE(s.pos(b.reason->box)->visible);
            EXPECT_NEAR(s.pos(b.root)->height, 36.0f, 0.5f);

            s.hover(110.0f, 118.0f);
            s.press(110.0f, 118.0f);
            s.release(110.0f, 118.0f);
            EXPECT_EQ(s.recorder->events.size(), 1u);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(button_test, tab_focuses_in_order_and_draws_ring)
        {
            MockLogger logger;
            ButtonFixture s;

            Button b1 = s.place({ButtonVariant::Quiet, "One"}, 100.0f, 100.0f);
            Button b2 = s.place({ButtonVariant::Quiet, "Two"}, 100.0f, 160.0f);
            Button b3 = s.place({ButtonVariant::Quiet, "Three"}, 100.0f, 220.0f);

            s.key(SDL_SCANCODE_TAB);
            EXPECT_TRUE(s.state(b1)->keyboardFocus);
            EXPECT_TRUE(s.pos(s.state(b1)->ring)->visible);
            EXPECT_EQ(s.focus->currentFocus, b1.face.id);

            s.key(SDL_SCANCODE_TAB);
            s.key(SDL_SCANCODE_TAB);
            EXPECT_TRUE(s.state(b3)->keyboardFocus);

            s.key(SDL_SCANCODE_TAB);   // wraps
            EXPECT_TRUE(s.state(b1)->keyboardFocus);

            s.key(SDL_SCANCODE_TAB, KMOD_SHIFT);   // back -> third
            EXPECT_TRUE(s.state(b3)->keyboardFocus);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(button_test, ring_geometry)
        {
            MockLogger logger;
            ButtonFixture s;

            Button b = s.place({ButtonVariant::Quiet, "Train"});
            const _unique_id ring = s.state(b)->ring;

            EXPECT_FALSE(s.pos(ring)->visible);   // hidden until keyboard focus
            EXPECT_EQ(s.token(ring), "focus-ink");
            EXPECT_FLOAT_EQ(s.ecs.getEntity(ring)->get<StrokeRect2DObject>()->strokeWidth, 2.0f);

            s.key(SDL_SCANCODE_TAB);
            EXPECT_TRUE(s.pos(ring)->visible);
            EXPECT_NEAR(s.pos(ring)->width, s.pos(b.face)->width + 8.0f, 0.5f);
            EXPECT_NEAR(s.pos(ring)->x, s.pos(b.face)->x - 4.0f, 0.5f);
            EXPECT_NEAR(s.pos(ring)->y, s.pos(b.face)->y - 4.0f, 0.5f);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(button_test, enter_and_space_activate_focused)
        {
            MockLogger logger;
            ButtonFixture s;

            ButtonSpec spec; spec.label = "Train"; spec.tag = "train";
            Button b = s.place(spec);

            s.key(SDL_SCANCODE_TAB);
            s.key(SDL_SCANCODE_RETURN);
            ASSERT_EQ(s.recorder->events.size(), 1u);
            EXPECT_EQ(s.recorder->events[0].tag, "train");

            s.key(SDL_SCANCODE_SPACE);
            EXPECT_EQ(s.recorder->events.size(), 2u);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(button_test, mouse_click_hides_every_ring)
        {
            MockLogger logger;
            ButtonFixture s;

            Button b1 = s.place({ButtonVariant::Quiet, "One"}, 100.0f, 100.0f);
            Button b2 = s.place({ButtonVariant::Quiet, "Two"}, 100.0f, 160.0f);

            s.key(SDL_SCANCODE_TAB);
            EXPECT_TRUE(s.pos(s.state(b1)->ring)->visible);

            s.press(600.0f, 600.0f);   // click empty space
            EXPECT_FALSE(s.state(b1)->keyboardFocus);
            EXPECT_FALSE(s.pos(s.state(b1)->ring)->visible);
            EXPECT_FALSE(s.state(b2)->keyboardFocus);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(button_test, theme_change_keeps_state)
        {
            MockLogger logger;
            ButtonFixture s;

            Button b = s.place({ButtonVariant::Quiet, "Train"});
            s.hover(110.0f, 118.0f);
            EXPECT_EQ(s.token(s.state(b)->ground), "vellum-tint");

            s.tokens.setTheme(Theme::Candle);
            s.ecs.sendEvent(ThemeChangedEvent{Theme::Candle});
            s.pump();

            EXPECT_EQ(s.token(s.state(b)->ground), "vellum-tint");
            EXPECT_FLOAT_EQ(s.ecs.getEntity(s.state(b)->ground)->get<RoundedRect2DObject>()->colors.x,
                            s.tokens.colour("vellum-tint", Theme::Candle).x);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(button_test, set_label_regrows)
        {
            MockLogger logger;
            ButtonFixture s;

            Button b = s.place({ButtonVariant::Quiet, "Go"});
            const float before = s.pos(b.face)->width;

            b.setLabel(&s.ecs, s.styles, "Enter the Ruins");
            s.pump();

            const float after = s.pos(b.face)->width;
            EXPECT_GT(after, before);
            EXPECT_NEAR(s.pos(s.state(b)->ground)->width, after, 0.5f);   // ground follows by fillIn
            EXPECT_NEAR(s.pos(s.state(b)->frame)->width, after, 0.5f);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(button_test, z_bands)
        {
            MockLogger logger;
            ButtonFixture s;

            ButtonSpec spec; spec.variant = ButtonVariant::Seal; spec.label = "Take"; spec.glyph = "seal"; spec.months = 3;
            spec.disabled = true; spec.reason = "STRENGTH 14 OF 18"; spec.z = 20;
            Button b = s.place(spec);
            ButtonState* st = s.state(b);

            EXPECT_FLOAT_EQ(s.pos(st->ground)->z, 20.0f);
            EXPECT_FLOAT_EQ(s.pos(st->frame)->z, 21.0f);
            EXPECT_FLOAT_EQ(s.pos(st->ring)->z, 21.0f);
            EXPECT_FLOAT_EQ(s.pos(st->sheen)->z, 21.0f);
            EXPECT_FLOAT_EQ(s.pos(b.label.box)->z, 22.0f);
            EXPECT_FLOAT_EQ(s.pos(b.label.text)->z, 23.0f);
            EXPECT_FLOAT_EQ(s.pos(b.glyph->entity)->z, 22.0f);
            EXPECT_FLOAT_EQ(s.pos(b.reason->box)->z, 22.0f);

            for (float z : {s.pos(st->ground)->z, s.pos(st->frame)->z, s.pos(b.label.box)->z, s.pos(b.label.text)->z})
                EXPECT_FLOAT_EQ(z, std::floor(z));
        }
    }
}
