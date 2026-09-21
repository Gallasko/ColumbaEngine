#include "stdafx.h"

#include <cmath>

#include <gtest/gtest.h>

#include "UI/progressrule.h"
#include "UI/paint.h"
#include "Core/motion.h"
#include "Core/textstyle.h"

#include "ECS/entitysystem.h"
#include "UI/ttftext.h"
#include "UI/iconsystem.h"
#include "UI/sizer.h"
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
            struct ProgressFixture
            {
                Tokens tokens = Tokens::load("chronicle/tokens.json");
                EntitySystem ecs;
                MasterRenderer renderer;
                TTFTextSystem* ttf = nullptr;
                IconSystem* icons = nullptr;
                PaintSystem* paint = nullptr;
                GameDataView* view = nullptr;
                TextStyles styles;

                ProgressFixture()
                {
                    Motion::setReduced(false);
                    ecs.createSystem<PositionComponentSystem>();
                    ecs.createSystem<LayoutSystem>();
                    ecs.succeed<PositionComponentSystem, LayoutSystem>();
                    ttf = ecs.createSystem<TTFTextSystem>(&renderer);
                    ecs.createSystem<Simple2DObjectSystem>(&renderer);
                    ecs.createSystem<HatchRect2DObjectSystem>(&renderer);
                    ecs.createSystem<DottedLine2DObjectSystem>(&renderer);
                    ecs.createSystem<StrokeRect2DObjectSystem>(&renderer);
                    icons = ecs.createSystem<IconSystem>(&renderer);
                    ecs.createSystem<TweenSystem>();
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
                void tick(float ms) { ecs.sendEvent(TickEvent{ms}); ecs.executeOnce(); ecs.executeOnce(); ecs.executeOnce(); }

                CompRef<PositionComponent> pos(EntityRef e) { return ecs.getEntity(e.id)->get<PositionComponent>(); }
                CompRef<Simple2DObject> s2d(EntityRef e) { return ecs.getEntity(e.id)->get<Simple2DObject>(); }
                CompRef<HatchRect2DObject> hatchOf(EntityRef e) { return ecs.getEntity(e.id)->get<HatchRect2DObject>(); }
                CompRef<StrokeRect2DObject> strokeOf(EntityRef e) { return ecs.getEntity(e.id)->get<StrokeRect2DObject>(); }
                std::string token(EntityRef e) { return ecs.getEntity(e.id)->get<PaintComponent>()->token; }
                float palpha(EntityRef e) { return ecs.getEntity(e.id)->get<PaintComponent>()->alpha; }
                bool hasTween(EntityRef e) { return ecs.getEntity(e.id)->has<TweenComponent>(); }
            };
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(progressrule_test, track_geometry)
        {
            MockLogger logger;
            ProgressFixture s;

            ProgressRule r = makeProgressRule(&s.ecs, s.tokens, s.styles, {240.0f});
            s.settle();
            EXPECT_FLOAT_EQ(s.pos(r.root)->width, 240.0f);
            EXPECT_FLOAT_EQ(s.pos(r.root)->height, 10.0f);
            EXPECT_EQ(s.token(r.track), "progress-track");
            EXPECT_FLOAT_EQ(s.pos(r.track)->width, 240.0f);
            EXPECT_FLOAT_EQ(s.pos(r.track)->height, 10.0f);
            EXPECT_EQ(s.token(r.frame), "rule-hair");
            EXPECT_FLOAT_EQ(s.strokeOf(r.frame)->strokeWidth, 1.0f);
            EXPECT_FLOAT_EQ(s.strokeOf(r.frame)->cornerRadius, 2.0f);
            EXPECT_NEAR(s.pos(r.frame)->width, 240.0f, 0.5f);

            ProgressRuleSpec sm; sm.width = 240.0f; sm.small = true; sm.percent = 50.0f;
            ProgressRule rs = makeProgressRule(&s.ecs, s.tokens, s.styles, sm);
            s.settle();
            EXPECT_FLOAT_EQ(s.pos(rs.track)->height, 6.0f);
            EXPECT_FLOAT_EQ(s.pos(rs.fill)->height, 4.0f);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(progressrule_test, fill_is_percent_of_inner)
        {
            MockLogger logger;
            ProgressFixture s;

            ProgressRule r = makeProgressRule(&s.ecs, s.tokens, s.styles, {240.0f, false, 35.0f});
            s.settle();
            EXPECT_NEAR(s.pos(r.fill)->x, s.pos(r.root)->x + 1.0f, 0.5f);
            EXPECT_NEAR(s.pos(r.fill)->width, 238.0f * 0.35f, 0.01f);
            EXPECT_FLOAT_EQ(s.pos(r.fill)->height, 8.0f);
            EXPECT_EQ(s.token(r.fill), "progress-ink");

            r.setPercent(&s.ecs, 0.0f, false); s.settle();
            EXPECT_NEAR(s.pos(r.fill)->width, 0.0f, 0.01f);
            r.setPercent(&s.ecs, 100.0f, false); s.settle();
            EXPECT_NEAR(s.pos(r.fill)->width, 238.0f, 0.01f);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(progressrule_test, percent_is_clamped)
        {
            MockLogger logger;
            ProgressFixture s;

            ProgressRule a = makeProgressRule(&s.ecs, s.tokens, s.styles, {240.0f, false, -5.0f});
            EXPECT_FLOAT_EQ(a.spec.percent, 0.0f);
            ProgressRule b = makeProgressRule(&s.ecs, s.tokens, s.styles, {240.0f, false, 140.0f});
            EXPECT_FLOAT_EQ(b.spec.percent, 100.0f);

            ProgressRule c = makeProgressRule(&s.ecs, s.tokens, s.styles, {240.0f});
            c.setPercent(&s.ecs, 250.0f, false);
            EXPECT_FLOAT_EQ(c.shown, 100.0f);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(progressrule_test, forecast_starts_at_fill_end)
        {
            MockLogger logger;
            ProgressFixture s;

            ProgressRule r = makeProgressRule(&s.ecs, s.tokens, s.styles, {240.0f, false, 35.0f, 60.0f});
            s.settle();
            EXPECT_NEAR(s.pos(r.forecast)->x, s.pos(r.fill)->x + s.pos(r.fill)->width, 0.01f);
            EXPECT_NEAR(s.pos(r.forecast)->width, 238.0f * 0.25f, 0.01f);
            EXPECT_EQ(s.token(r.forecast), "progress-forecast");
            EXPECT_NEAR(s.palpha(r.forecast), 0.4f, 0.001f);
            EXPECT_FLOAT_EQ(s.hatchOf(r.forecast)->angle, 45.0f);
            EXPECT_FLOAT_EQ(s.hatchOf(r.forecast)->spacing, 6.0f);
            EXPECT_FLOAT_EQ(s.hatchOf(r.forecast)->lineWidth, 2.0f);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(progressrule_test, forecast_below_fill_is_empty)
        {
            MockLogger logger;
            ProgressFixture s;

            ProgressRule r = makeProgressRule(&s.ecs, s.tokens, s.styles, {240.0f, false, 60.0f, 40.0f});
            s.settle();
            EXPECT_NEAR(s.pos(r.forecast)->width, 0.0f, 0.01f);

            r.setForecast(&s.ecs, 80.0f); s.settle();
            EXPECT_NEAR(s.pos(r.forecast)->width, 238.0f * 0.2f, 0.01f);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(progressrule_test, nib_rides_the_head)
        {
            MockLogger logger;
            ProgressFixture s;

            ProgressRule r = makeProgressRule(&s.ecs, s.tokens, s.styles, {240.0f, false, 35.0f});
            s.settle();
            ASSERT_TRUE(r.nib.has_value());
            EXPECT_NEAR(s.pos(r.nib->entity)->x, s.pos(r.root)->x + 1.0f + 238.0f * 0.35f - 5.6f, 0.01f);
            EXPECT_NEAR(s.pos(r.nib->entity)->y, s.pos(r.root)->y + 5.0f - 8.4f, 0.01f);

            ProgressRuleSpec nn; nn.width = 240.0f; nn.percent = 35.0f; nn.nib = false;
            ProgressRule r2 = makeProgressRule(&s.ecs, s.tokens, s.styles, nn);
            EXPECT_FALSE(r2.nib.has_value());
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(progressrule_test, caption_below_track)
        {
            MockLogger logger;
            ProgressFixture s;

            ProgressRuleSpec cs; cs.width = 240.0f; cs.caption = "MONTH 3 OF 6";
            ProgressRule r = makeProgressRule(&s.ecs, s.tokens, s.styles, cs);
            s.settle();
            ASSERT_TRUE(r.caption.has_value());
            EXPECT_EQ(r.caption->spec.style, "caption");
            EXPECT_NEAR(s.pos(r.caption->box)->y, s.pos(r.root)->y + 10.0f + 4.0f, 0.5f);
            EXPECT_NEAR(s.pos(r.root)->height, 14.0f + s.pos(r.caption->box)->height, 0.5f);

            r.setCaption(&s.ecs, s.styles, "");
            s.settle();
            EXPECT_FLOAT_EQ(s.pos(r.root)->height, 10.0f);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(progressrule_test, set_percent_jumps_without_animation)
        {
            MockLogger logger;
            ProgressFixture s;

            ProgressRule r = makeProgressRule(&s.ecs, s.tokens, s.styles, {240.0f});
            r.setPercent(&s.ecs, 70.0f, false);
            EXPECT_FLOAT_EQ(r.shown, 70.0f);
            s.settle();
            EXPECT_NEAR(s.pos(r.fill)->width, 238.0f * 0.7f, 0.5f);
            EXPECT_FALSE(s.hasTween(r.fill));
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(progressrule_test, set_percent_animates_linearly)
        {
            MockLogger logger;
            ProgressFixture s;

            ProgressRule r = makeProgressRule(&s.ecs, s.tokens, s.styles, {240.0f});
            r.setPercent(&s.ecs, 50.0f);   // animate
            ASSERT_TRUE(s.hasTween(r.fill));
            EXPECT_FLOAT_EQ(s.ecs.getEntity(r.fill.id)->get<TweenComponent>()->duration, 300.0f);

            s.tick(150.0f);
            EXPECT_NEAR(r.shown, 25.0f, 0.5f);
            EXPECT_NEAR(s.pos(r.fill)->width, 238.0f * 0.25f, 0.5f);

            s.tick(150.0f);
            EXPECT_NEAR(r.shown, 50.0f, 0.01f);
            EXPECT_FALSE(s.ecs.getEntity(r.fill.id)->get<TweenComponent>()->active);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(progressrule_test, set_percent_during_tween_restarts_from_shown)
        {
            MockLogger logger;
            ProgressFixture s;

            ProgressRule r = makeProgressRule(&s.ecs, s.tokens, s.styles, {240.0f});
            r.setPercent(&s.ecs, 50.0f);
            s.tick(150.0f);
            ASSERT_NEAR(r.shown, 25.0f, 0.5f);

            r.setPercent(&s.ecs, 10.0f);   // from ~25 to 10, duration ~90
            EXPECT_NEAR(s.ecs.getEntity(r.fill.id)->get<TweenComponent>()->duration, 90.0f, 3.0f);
            s.tick(90.0f);
            EXPECT_NEAR(r.shown, 10.0f, 0.5f);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(progressrule_test, reduced_motion_jumps)
        {
            MockLogger logger;
            ProgressFixture s;
            Motion::setReduced(true);

            ProgressRule r = makeProgressRule(&s.ecs, s.tokens, s.styles, {240.0f});
            r.setPercent(&s.ecs, 80.0f);   // animate requested, but reduced -> jump
            EXPECT_FLOAT_EQ(r.shown, 80.0f);
            EXPECT_FALSE(s.hasTween(r.fill));

            Motion::setReduced(false);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(progressrule_test, forecast_shrinks_as_fill_grows)
        {
            MockLogger logger;
            ProgressFixture s;

            ProgressRule r = makeProgressRule(&s.ecs, s.tokens, s.styles, {240.0f, false, 0.0f, 50.0f});
            r.setPercent(&s.ecs, 50.0f);
            s.tick(150.0f);   // midpoint: shown ~25
            EXPECT_NEAR(s.pos(r.forecast)->width, 238.0f * 0.25f, 0.5f);
            EXPECT_NEAR(s.pos(r.forecast)->x, s.pos(r.fill)->x + s.pos(r.fill)->width, 0.5f);

            s.tick(150.0f);   // end: shown 50, forecast gone
            EXPECT_NEAR(s.pos(r.forecast)->width, 0.0f, 0.5f);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(progressrule_test, fed_from_gamedataview)
        {
            MockLogger logger;
            ProgressFixture s;

            ProgressRule r = makeProgressRule(&s.ecs, s.tokens, s.styles, {240.0f});
            s.view->subscribe("activity.running.percent", [&](const ElementType& v) {
                r.setPercent(&s.ecs, v.get<float>(), false);
            });
            s.view->set("activity.running.percent", ElementType{42.0f});
            EXPECT_FLOAT_EQ(r.shown, 42.0f);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(progressrule_test, set_width_relays)
        {
            MockLogger logger;
            ProgressFixture s;

            ProgressRule r = makeProgressRule(&s.ecs, s.tokens, s.styles, {240.0f, false, 50.0f, 75.0f});
            s.settle();
            r.setWidth(&s.ecs, 480.0f);
            s.settle();

            EXPECT_NEAR(s.pos(r.track)->width, 480.0f, 0.5f);
            EXPECT_NEAR(s.pos(r.frame)->width, 480.0f, 0.5f);
            EXPECT_NEAR(s.pos(r.fill)->width, 478.0f * 0.5f, 0.5f);
            EXPECT_NEAR(s.pos(r.forecast)->width, 478.0f * 0.25f, 0.5f);
            EXPECT_NEAR(s.pos(r.forecast)->x, s.pos(r.fill)->x + s.pos(r.fill)->width, 0.5f);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(progressrule_test, z_bands)
        {
            MockLogger logger;
            ProgressFixture s;

            ProgressRuleSpec zs; zs.width = 240.0f; zs.percent = 35.0f; zs.forecastPercent = 60.0f;
            zs.caption = "MONTH 3 OF 6"; zs.z = 20;
            ProgressRule r = makeProgressRule(&s.ecs, s.tokens, s.styles, zs);
            s.settle();

            EXPECT_FLOAT_EQ(s.pos(r.root)->z, 20.0f);
            EXPECT_FLOAT_EQ(s.pos(r.track)->z, 20.0f);
            EXPECT_FLOAT_EQ(s.pos(r.fill)->z, 21.0f);
            EXPECT_FLOAT_EQ(s.pos(r.forecast)->z, 22.0f);
            EXPECT_FLOAT_EQ(s.pos(r.frame)->z, 23.0f);
            EXPECT_FLOAT_EQ(s.pos(r.nib->entity)->z, 24.0f);
            EXPECT_FLOAT_EQ(s.pos(r.caption->box)->z, 21.0f);
            EXPECT_FLOAT_EQ(s.pos(r.caption->text)->z, 22.0f);
        }
    }
}
