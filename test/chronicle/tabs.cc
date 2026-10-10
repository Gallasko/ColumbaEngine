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

#include "ECS/entitysystem.h"
#include "UI/themesystem.h"
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
                EntitySystem ecs;
                MasterRenderer renderer;
                TTFTextSystem* ttf = nullptr;
                IconSystem* icons = nullptr;
                ThemeSystem* theme = nullptr;

                constant::Vector4D color(const std::string& token, const std::string& id = "") { return theme->theme().color(token, id.empty() ? theme->currentTheme() : id); }
                TabsSystem* tabsSys = nullptr;
                FocusOrderSystem* order = nullptr;
                TabRecorder* recorder = nullptr;

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
                    theme = ecs.createSystem<ThemeSystem>();
                    ecs.createSystem<ButtonSystem>();
                    tabsSys = ecs.createSystem<TabsSystem>();
                    ecs.succeed<MouseHoverSystem, TabsSystem>();
                    recorder = ecs.createSystem<TabRecorder>();
                    theme->loadTheme("chronicle/tokens.json", "fonts");
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
                    Tabs t = makeTabs(&ecs, spec);
                    t.root->get<PositionComponent>()->setX(x);
                    t.root->get<PositionComponent>()->setY(y);
                    pump();
                    return t;
                }

                float tabW(const std::string& text)
                {
                    const TextStyle& s = theme->style("tab");
                    return ttf->measureText(s.fontAlias, text, 1.0f, 0.0f, 0.0f, s.letterSpacingPx).width;
                }
                float capW(const std::string& text)
                {
                    const TextStyle& s = theme->style("caption");
                    return ttf->measureText(s.fontAlias, text, 1.0f, 0.0f, 0.0f, s.letterSpacingPx).width;
                }

                CompRef<PositionComponent> pos(EntityRef e) { return ecs.getEntity(e.id)->get<PositionComponent>(); }
                CompRef<PositionComponent> pos(_unique_id id) { return ecs.getEntity(id)->get<PositionComponent>(); }
                CompRef<Simple2DObject> s2d(EntityRef e) { return ecs.getEntity(e.id)->get<Simple2DObject>(); }
                std::string token(EntityRef e) { return theme->elementEntry(ecs.getEntity(e.id)->get<ThemeComponent>()->element, "color").get<std::string>(); }
                std::string token(_unique_id id) { return theme->elementEntry(ecs.getEntity(id)->get<ThemeComponent>()->element, "color").get<std::string>(); }
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

            EXPECT_EQ(s.token(t.tabs[1].label.entity.id), "ink-muted");
            EXPECT_EQ(s.token(t.tabs[0].label.entity.id), "ink");   // active

            s.hover(s.centreX(t.tabs[1]), s.centreY(t.tabs[1]));
            EXPECT_EQ(s.token(t.tabs[1].label.entity.id), "ink");
            EXPECT_EQ(s.token(t.tabs[0].label.entity.id), "ink");   // active stays ink

            s.hover(600.0f, 600.0f);
            EXPECT_EQ(s.token(t.tabs[1].label.entity.id), "ink-muted");
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
            EXPECT_EQ(s.token(t.tabs[0].badge->entity.id), "ink-muted");

            const float frameW = s.capW("3") + 8.0f;
            EXPECT_NEAR(s.pos(t.tabs[0].badgeFrame)->width, frameW, 0.5f);
            EXPECT_FLOAT_EQ(s.pos(t.tabs[0].badgeFrame)->height, 15.0f);
            EXPECT_NEAR(s.pos(t.tabs[0].face)->width, 24.0f + s.tabW("Guild") + 8.0f + frameW, 0.5f);

            t.setBadge(&s.ecs, 0, 0);
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
            makeButton(&s.ecs, {ButtonVariant::Quiet, "Go"});
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

            s.theme->setTheme("candle");
            s.pump();

            EXPECT_FLOAT_EQ(s.ecs.getEntity(t.tabs[1].label.entity.id)->get<TTFText>()->colors.x,
                            s.color("ink", "candle").x);
            EXPECT_FLOAT_EQ(s.s2d(t.rule)->colors.x, s.color("rule-ruled", "candle").x);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // A tab that is not shown: nothing of it is drawn, it takes no click, and the others keep
        // their place. Shown, it is a tab like the others.
        TEST(tabs_test, a_tab_not_shown)
        {
            MockLogger logger;
            TabsFixture s;
            TabsSpec spec; spec.tag = "life"; spec.items = {{"Life", "quill", 0}, {"Town", "town", 0}};
            Tabs t = s.place(spec);

            const float lifeX = s.pos(t.tabs[0].face)->x;
            const float townX = s.pos(t.tabs[1].face)->x;

            t.setShown(&s.ecs, 1, false);
            s.pump();

            EXPECT_FALSE(t.tabs[1].shown);
            EXPECT_TRUE(t.tabs[1].face->get<TabState>()->hidden);
            EXPECT_FALSE(t.enabled(1));
            EXPECT_FALSE(s.pos(t.tabs[1].face)->visible);
            EXPECT_FALSE(s.pos(t.tabs[1].label.entity)->visible);
            EXPECT_FALSE(s.pos(t.tabs[1].glyph->entity)->visible);
            EXPECT_FALSE(s.pos(t.tabs[1].underline)->visible);
            EXPECT_FLOAT_EQ(s.pos(t.tabs[0].face)->x, lifeX);
            EXPECT_TRUE(s.pos(t.tabs[0].face)->visible);

            // Selected all the same: nothing is sent
            s.tabsSys->select(t.tabs[1].face);
            EXPECT_EQ(s.recorder->events.size(), 0u);
            EXPECT_TRUE(t.tabs[0].face->get<TabState>()->active);

            t.setShown(&s.ecs, 1, true);
            s.pump();

            EXPECT_TRUE(t.tabs[1].shown);
            EXPECT_TRUE(t.enabled(1));
            EXPECT_TRUE(s.pos(t.tabs[1].face)->visible);
            EXPECT_TRUE(s.pos(t.tabs[1].label.entity)->visible);
            EXPECT_FLOAT_EQ(s.pos(t.tabs[1].face)->x, townX);

            s.tabsSys->select(t.tabs[1].face);
            ASSERT_EQ(s.recorder->events.size(), 1u);
            EXPECT_EQ(s.recorder->events[0].index, 1);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // A notice: a small round with a count at the tab's corner, which takes no room.
        TEST(tabs_test, a_notice_at_the_corner)
        {
            MockLogger logger;
            TabsFixture s;
            TabsSpec spec; spec.tag = "life"; spec.items = {{"Life", "quill", 0}, {"Town", "town", 0}};
            Tabs t = s.place(spec);

            const float faceW = s.pos(t.tabs[1].face)->width;
            const float townX = s.pos(t.tabs[1].face)->x;

            EXPECT_TRUE(t.tabs[1].noticeGround.empty());
            EXPECT_EQ(t.tabs[1].notice, 0);

            t.setNotice(&s.ecs, 1, 3);
            s.pump();

            ASSERT_FALSE(t.tabs[1].noticeGround.empty());
            ASSERT_TRUE(t.tabs[1].noticeText.has_value());
            EXPECT_EQ(t.tabs[1].notice, 3);
            EXPECT_EQ(t.tabs[1].noticeText->spec.text, "3");
            EXPECT_TRUE(s.pos(t.tabs[1].noticeGround)->visible);
            EXPECT_EQ(s.token(t.tabs[1].noticeGround), "vermilion");

            // Past the face's right edge, over the label's shoulder; the face keeps its size and place
            EXPECT_NEAR(s.pos(t.tabs[1].noticeGround)->x, townX + faceW + 2.0f, 0.5f);
            EXPECT_NEAR(s.pos(t.tabs[1].noticeGround)->y, s.pos(t.tabs[1].face)->y + 4.0f, 0.5f);
            EXPECT_FLOAT_EQ(s.pos(t.tabs[1].noticeGround)->width, 16.0f);
            EXPECT_FLOAT_EQ(s.pos(t.tabs[1].face)->width, faceW);
            EXPECT_FLOAT_EQ(s.pos(t.tabs[1].face)->x, townX);
            EXPECT_GT(s.pos(t.tabs[1].noticeGround)->z, s.pos(t.tabs[1].face)->z);
            EXPECT_GT(s.pos(t.tabs[1].noticeText->entity)->z, s.pos(t.tabs[1].noticeGround)->z);

            // More than nine is "9+"; none takes it away
            t.setNotice(&s.ecs, 1, 14);
            EXPECT_EQ(t.tabs[1].noticeText->spec.text, "9+");

            t.setNotice(&s.ecs, 1, 0);
            s.pump();

            EXPECT_EQ(t.tabs[1].notice, 0);
            EXPECT_FALSE(s.pos(t.tabs[1].noticeGround)->visible);
            EXPECT_FALSE(s.pos(t.tabs[1].noticeText->entity)->visible);

            // A tab that is not shown shows no notice either, and has it when it comes
            t.setShown(&s.ecs, 1, false);
            t.setNotice(&s.ecs, 1, 2);
            s.pump();
            EXPECT_FALSE(s.pos(t.tabs[1].noticeGround)->visible);

            t.setShown(&s.ecs, 1, true);
            s.pump();
            EXPECT_TRUE(s.pos(t.tabs[1].noticeGround)->visible);
            EXPECT_EQ(t.tabs[1].noticeText->spec.text, "2");
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
            EXPECT_FLOAT_EQ(s.pos(tab.label.entity)->z, 23.0f);
            EXPECT_FLOAT_EQ(s.pos(tab.glyph->entity)->z, 22.0f);
            EXPECT_FLOAT_EQ(s.pos(tab.badge->entity)->z, 23.0f);
        }
    }
}
