#include "stdafx.h"

#ifdef __linux__
#include <SDL2/SDL.h>
#elif _WIN32
#include <SDL.h>
#endif

#include <cmath>

#include <gtest/gtest.h>

#include "UI/tabs.h"
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
            struct TabRecorder : public System<Listener<TabSelectedEvent>, StoragePolicy>
            {
                std::string getSystemName() const override { return "Tab Recorder"; }
                void onEvent(const TabSelectedEvent& e) override { events.push_back(e); }
                std::vector<TabSelectedEvent> events;
            };

            struct TabsFixture
            {
                Tokens tokens = Tokens::load("chronicle/tokens.json");
                EntitySystem ecs;
                MasterRenderer renderer;
                TTFTextSystem* ttf = nullptr;
                IconSystem* icons = nullptr;
                PaintSystem* paint = nullptr;
                TabsSystem* tabsSys = nullptr;
                FocusOrderSystem* order = nullptr;
                TabRecorder* recorder = nullptr;
                TextStyles styles;

                TabsFixture()
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
                    ecs.createSystem<FocusableSystem>();
                    order = ecs.createSystem<FocusOrderSystem>();
                    paint = ecs.createSystem<PaintSystem>(&tokens);
                    ecs.createSystem<ButtonSystem>(&tokens);
                    tabsSys = ecs.createSystem<TabsSystem>(&tokens);
                    ecs.succeed<MouseHoverSystem, TabsSystem>();
                    recorder = ecs.createSystem<TabRecorder>();
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

                Tabs place(const TabsSpec& spec, float x = 100.0f, float y = 100.0f)
                {
                    Tabs t = makeTabs(&ecs, tokens, styles, spec);
                    t.root->get<PositionComponent>()->setX(x);
                    t.root->get<PositionComponent>()->setY(y);
                    pump();
                    return t;
                }

                float tabW(const std::string& text)
                {
                    const TextStyle& s = styles.get("tab");
                    return ttf->measureText(s.fontAlias, text, 1.0f, 0.0f, 0.0f, s.letterSpacingPx).width;
                }
                float capW(const std::string& text)
                {
                    const TextStyle& s = styles.get("caption");
                    return ttf->measureText(s.fontAlias, text, 1.0f, 0.0f, 0.0f, s.letterSpacingPx).width;
                }

                CompRef<PositionComponent> pos(EntityRef e) { return ecs.getEntity(e.id)->get<PositionComponent>(); }
                CompRef<PositionComponent> pos(_unique_id id) { return ecs.getEntity(id)->get<PositionComponent>(); }
                CompRef<Simple2DObject> s2d(EntityRef e) { return ecs.getEntity(e.id)->get<Simple2DObject>(); }
                std::string token(EntityRef e) { return ecs.getEntity(e.id)->get<PaintComponent>()->token; }
                std::string token(_unique_id id) { return ecs.getEntity(id)->get<PaintComponent>()->token; }
                float centreX(const Tabs::Tab& t) { return pos(t.face)->x + pos(t.face)->width / 2.0f; }
                float centreY(const Tabs::Tab& t) { return pos(t.face)->y + 22.0f; }

                TabsSpec sixTabs()
                {
                    TabsSpec spec; spec.tag = "life"; spec.active = 0;
                    spec.items = {
                        {"Life", "quill", 0}, {"Kit", "equipment", 0}, {"Town", "town", 0},
                        {"Guild", "guild", 0}, {"Adventure", "adventure", 0}, {"Chronicle", "study", 0}};
                    return spec;
                }
            };
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(tabs_test, row_geometry)
        {
            MockLogger logger;
            TabsFixture s;
            Tabs t = s.place(s.sixTabs());

            EXPECT_FLOAT_EQ(s.pos(t.root)->height, 44.0f);
            EXPECT_EQ(s.token(t.rule), "rule-ruled");
            EXPECT_NEAR(s.pos(t.rule)->width, s.pos(t.root)->width, 0.5f);

            for (size_t k = 1; k < t.tabs.size(); ++k)
                EXPECT_NEAR(s.pos(t.tabs[k].face)->x, s.pos(t.tabs[k - 1].face)->x + s.pos(t.tabs[k - 1].face)->width + 16.0f, 0.5f);

            EXPECT_NEAR(s.pos(t.tabs[0].face)->width, 24.0f + s.tabW("Life"), 0.5f);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(tabs_test, underline_only_on_active)
        {
            MockLogger logger;
            TabsFixture s;
            Tabs t = s.place(s.sixTabs());

            EXPECT_TRUE(s.pos(t.tabs[0].underline)->visible);
            EXPECT_FLOAT_EQ(s.pos(t.tabs[0].underline)->height, 3.0f);
            EXPECT_EQ(s.token(t.tabs[0].underline), "vermilion");
            EXPECT_FALSE(s.pos(t.tabs[1].underline)->visible);

            t.setActive(&s.ecs, 2);
            s.pump();
            EXPECT_FALSE(s.pos(t.tabs[0].underline)->visible);
            EXPECT_TRUE(s.pos(t.tabs[2].underline)->visible);
            EXPECT_EQ(s.recorder->events.size(), 0u);   // programmatic, no event
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(tabs_test, inks_at_rest_and_hover)
        {
            MockLogger logger;
            TabsFixture s;
            Tabs t = s.place(s.sixTabs());

            EXPECT_EQ(s.token(t.tabs[1].label.text.id), "ink-muted");
            EXPECT_EQ(s.token(t.tabs[0].label.text.id), "ink");   // active

            s.hover(s.centreX(t.tabs[1]), s.centreY(t.tabs[1]));
            EXPECT_EQ(s.token(t.tabs[1].label.text.id), "ink");
            EXPECT_EQ(s.token(t.tabs[0].label.text.id), "ink");   // active stays ink

            s.hover(600.0f, 600.0f);
            EXPECT_EQ(s.token(t.tabs[1].label.text.id), "ink-muted");
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(tabs_test, badge_geometry)
        {
            MockLogger logger;
            TabsFixture s;
            TabsSpec spec; spec.items = {{"Guild", "guild", 3}};
            Tabs t = s.place(spec);

            ASSERT_TRUE(t.tabs[0].badge.has_value());
            EXPECT_EQ(t.tabs[0].badge->spec.text, "3");
            EXPECT_EQ(s.token(t.tabs[0].badge->text.id), "ink-muted");

            const float frameW = s.capW("3") + 8.0f;
            EXPECT_NEAR(s.pos(t.tabs[0].badgeFrame)->width, frameW, 0.5f);
            EXPECT_FLOAT_EQ(s.pos(t.tabs[0].badgeFrame)->height, 15.0f);
            EXPECT_NEAR(s.pos(t.tabs[0].face)->width, 24.0f + s.tabW("Guild") + 8.0f + frameW, 0.5f);

            t.setBadge(&s.ecs, s.styles, 0, 0);
            s.pump();
            EXPECT_FALSE(t.tabs[0].badge.has_value());
            EXPECT_NEAR(s.pos(t.tabs[0].face)->width, 24.0f + s.tabW("Guild"), 0.5f);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(tabs_test, click_selects_on_release_inside)
        {
            MockLogger logger;
            TabsFixture s;
            Tabs t = s.place(s.sixTabs());

            s.hover(s.centreX(t.tabs[3]), s.centreY(t.tabs[3]));
            s.press(s.centreX(t.tabs[3]), s.centreY(t.tabs[3]));
            s.release(s.centreX(t.tabs[3]), s.centreY(t.tabs[3]));

            ASSERT_EQ(s.recorder->events.size(), 1u);
            EXPECT_EQ(s.recorder->events[0].index, 3);
            EXPECT_TRUE(t.tabs[3].face->get<TabState>()->active);
            EXPECT_FALSE(t.tabs[0].face->get<TabState>()->active);

            s.hover(s.centreX(t.tabs[3]), s.centreY(t.tabs[3]));
            s.press(s.centreX(t.tabs[3]), s.centreY(t.tabs[3]));
            s.release(s.centreX(t.tabs[3]), s.centreY(t.tabs[3]));
            EXPECT_EQ(s.recorder->events.size(), 1u);   // already active, no second event
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(tabs_test, release_outside_cancels)
        {
            MockLogger logger;
            TabsFixture s;
            Tabs t = s.place(s.sixTabs());

            s.hover(s.centreX(t.tabs[2]), s.centreY(t.tabs[2]));
            s.press(s.centreX(t.tabs[2]), s.centreY(t.tabs[2]));
            s.hover(600.0f, 600.0f);
            s.release(600.0f, 600.0f);
            EXPECT_EQ(s.recorder->events.size(), 0u);
            EXPECT_TRUE(t.tabs[0].face->get<TabState>()->active);   // unchanged
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(tabs_test, arrow_keys_move_selection)
        {
            MockLogger logger;
            TabsFixture s;

            // A button first, so the shared Tab order is button -> tab0 -> tab1 ...
            makeButton(&s.ecs, s.tokens, s.styles, {ButtonVariant::Quiet, "Go"});
            Tabs t = s.place(s.sixTabs(), 100.0f, 200.0f);

            s.key(SDL_SCANCODE_TAB);   // button
            s.key(SDL_SCANCODE_TAB);   // tab0
            EXPECT_EQ(s.order->current(), t.tabs[0].face.id);

            s.key(SDL_SCANCODE_LEFT);  // index 0 -> nothing
            EXPECT_EQ(s.recorder->events.size(), 0u);

            s.key(SDL_SCANCODE_RIGHT);
            ASSERT_EQ(s.recorder->events.size(), 1u);
            EXPECT_EQ(s.recorder->events[0].index, 1);
            EXPECT_EQ(s.order->current(), t.tabs[1].face.id);
            EXPECT_TRUE(s.pos(t.tabs[1].ring)->visible);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(tabs_test, enter_selects_focused)
        {
            MockLogger logger;
            TabsFixture s;
            Tabs t = s.place(s.sixTabs());

            s.key(SDL_SCANCODE_TAB);   // tab0
            s.key(SDL_SCANCODE_TAB);   // tab1
            s.key(SDL_SCANCODE_TAB);   // tab2
            s.key(SDL_SCANCODE_TAB);   // tab3
            s.key(SDL_SCANCODE_TAB);   // tab4
            s.key(SDL_SCANCODE_RETURN);
            ASSERT_EQ(s.recorder->events.size(), 1u);
            EXPECT_EQ(s.recorder->events[0].index, 4);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(tabs_test, too_many_tabs_warns)
        {
            MockLogger logger;
            TabsFixture s;
            TabsSpec spec;
            for (int i = 0; i < 7; ++i)
                spec.items.push_back({"T" + std::to_string(i), "", 0});
            Tabs t = s.place(spec);
            EXPECT_EQ(t.tabs.size(), 7u);
            EXPECT_GE(logger.getNbWarning(), 1u);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(tabs_test, fixed_width_row)
        {
            MockLogger logger;
            TabsFixture s;
            TabsSpec spec = s.sixTabs();
            spec.width = 800.0f;
            Tabs t = s.place(spec);

            EXPECT_NEAR(s.pos(t.rule)->width, 800.0f, 0.5f);
            EXPECT_NEAR(s.pos(t.tabs[0].face)->width, 24.0f + s.tabW("Life"), 0.5f);   // faces unchanged
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(tabs_test, theme_repaints)
        {
            MockLogger logger;
            TabsFixture s;
            Tabs t = s.place(s.sixTabs());

            s.hover(s.centreX(t.tabs[1]), s.centreY(t.tabs[1]));

            s.tokens.setTheme(Theme::Candle);
            s.ecs.sendEvent(ThemeChangedEvent{Theme::Candle});
            s.pump();

            EXPECT_FLOAT_EQ(s.ecs.getEntity(t.tabs[1].label.text.id)->get<TTFText>()->colors.x,
                            s.tokens.colour("ink", Theme::Candle).x);
            EXPECT_FLOAT_EQ(s.s2d(t.rule)->colors.x, s.tokens.colour("rule-ruled", Theme::Candle).x);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(tabs_test, z_bands)
        {
            MockLogger logger;
            TabsFixture s;
            TabsSpec spec; spec.items = {{"Guild", "guild", 3}}; spec.z = 20;
            Tabs t = s.place(spec);
            const Tabs::Tab& tab = t.tabs[0];

            EXPECT_FLOAT_EQ(s.pos(t.root)->z, 20.0f);
            EXPECT_FLOAT_EQ(s.pos(t.rule)->z, 20.0f);
            EXPECT_FLOAT_EQ(s.pos(tab.face)->z, 20.0f);
            EXPECT_FLOAT_EQ(s.pos(tab.underline)->z, 21.0f);
            EXPECT_FLOAT_EQ(s.pos(tab.ring)->z, 21.0f);
            EXPECT_FLOAT_EQ(s.pos(tab.label.box)->z, 22.0f);
            EXPECT_FLOAT_EQ(s.pos(tab.label.text)->z, 23.0f);
            EXPECT_FLOAT_EQ(s.pos(tab.glyph->entity)->z, 22.0f);
            EXPECT_FLOAT_EQ(s.pos(tab.badge->box)->z, 22.0f);
        }
    }
}
