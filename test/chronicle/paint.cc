#include "stdafx.h"

#include <gtest/gtest.h>

#include "UI/paint.h"
#include "Core/textstyle.h"

#include "ECS/entitysystem.h"
#include "UI/ttftext.h"
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
            void expectColour(const constant::Vector4D& a, const constant::Vector4D& b)
            {
                EXPECT_FLOAT_EQ(a.x, b.x);
                EXPECT_FLOAT_EQ(a.y, b.y);
                EXPECT_FLOAT_EQ(a.z, b.z);
                EXPECT_FLOAT_EQ(a.w, b.w);
            }

            // Common setup: engine systems + Chronicle paint system + registered styles.
            // PaintSystem registers its groups on the first executeOnce(), so every test
            // runs one frame after building its entities.
            struct PaintFixture
            {
                Tokens tokens = Tokens::load("chronicle/tokens.json");
                EntitySystem ecs;
                MasterRenderer renderer;
                TTFTextSystem* ttf = nullptr;
                PaintSystem* paint = nullptr;
                TextStyles styles;

                PaintFixture()
                {
                    ecs.createSystem<PositionComponentSystem>();
                    ttf = ecs.createSystem<TTFTextSystem>(&renderer);
                    ecs.createSystem<Simple2DObjectSystem>(&renderer);
                    ecs.createSystem<HatchRect2DObjectSystem>(&renderer);
                    ecs.createSystem<DottedLine2DObjectSystem>(&renderer);
                    ecs.createSystem<StrokeRect2DObjectSystem>(&renderer);
                    paint = ecs.createSystem<PaintSystem>(&tokens);
                    styles = TextStyles::fromTokens(tokens);
                    styles.registerAll(ttf, "fonts");
                }

                EntityRef makeTextEntity(const std::string& initialColour = "ink")
                {
                    return styles.makeText(&ecs, "body", "x", tokens.colour(initialColour)).entity;
                }
            };
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(paint_test, attach_applies_colour)
        {
            MockLogger logger;
            PaintFixture s;

            EntityRef ent = s.makeTextEntity("vermilion");
            s.ecs.attach<PaintComponent>(ent, "ink");
            s.ecs.executeOnce();

            expectColour(ent->get<TTFText>()->colors, s.tokens.colour("ink"));
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(paint_test, paint_component_can_attach_before_the_drawable)
        {
            MockLogger logger;
            PaintFixture s;

            auto ent = s.ecs.createEntity();
            s.ecs.attach<PaintComponent>(ent, "vermilion");
            s.ecs.attach<Simple2DObject>(ent, Shape2D::Square);
            s.ecs.executeOnce();

            expectColour(ent->get<Simple2DObject>()->colors, s.tokens.colour("vermilion"));
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(paint_test, repaint_follows_theme)
        {
            MockLogger logger;
            PaintFixture s;

            EntityRef text = s.makeTextEntity("ink");
            s.ecs.attach<PaintComponent>(text, "ink");

            auto shape = makeUiSimple2DShape(&s.ecs, Shape2D::Square, 10.0f, 10.0f, s.tokens.colour("vellum"));
            s.ecs.attach<PaintComponent>(shape.entity, "vellum");
            s.ecs.executeOnce();

            s.tokens.setTheme(Theme::Candle);
            s.ecs.sendEvent(ThemeChangedEvent{Theme::Candle});
            s.ecs.executeOnce();

            expectColour(text->get<TTFText>()->colors, s.tokens.colour("ink", Theme::Candle));
            expectColour(shape.entity->get<Simple2DObject>()->colors, s.tokens.colour("vellum", Theme::Candle));
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(paint_test, repaint_resolves_aliases)
        {
            MockLogger logger;
            PaintFixture s;

            EntityRef text = s.makeTextEntity("ink");
            s.ecs.attach<PaintComponent>(text, "status-gain");
            s.ecs.executeOnce();
            expectColour(text->get<TTFText>()->colors, s.tokens.colour("verdigris", Theme::Day));

            s.tokens.setTheme(Theme::Candle);
            s.ecs.sendEvent(ThemeChangedEvent{Theme::Candle});
            s.ecs.executeOnce();

            expectColour(text->get<TTFText>()->colors, s.tokens.colour("verdigris", Theme::Candle));
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(paint_test, set_token_repaints)
        {
            MockLogger logger;
            PaintFixture s;

            EntityRef text = s.makeTextEntity("ink");
            s.ecs.attach<PaintComponent>(text, "ink");
            s.ecs.executeOnce();

            text->get<PaintComponent>()->setToken("vermilion");
            s.ecs.executeOnce();

            EXPECT_TRUE(text->has<PaintComponent>());
            EXPECT_EQ(text->get<PaintComponent>()->token, "vermilion");
            expectColour(text->get<TTFText>()->colors, s.tokens.colour("vermilion"));
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(paint_test, unpaintable_entity_is_ignored)
        {
            MockLogger logger;
            PaintFixture s;

            auto ent = s.ecs.createEntity();
            s.ecs.attach<PositionComponent>(ent);
            s.ecs.attach<PaintComponent>(ent, "ink");
            s.ecs.executeOnce();

            EXPECT_NO_THROW(s.paint->repaintAll());
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(paint_test, repaint_decorated_shapes)
        {
            MockLogger logger;
            PaintFixture s;

            auto hatch  = makeHatchRect2DShape(&s.ecs, 100.0f, 10.0f, s.tokens.colour("rule-ruled"));
            auto dotted = makeDottedLine2DShape(&s.ecs, 100.0f, s.tokens.colour("rule-ruled"));
            auto stroke = makeStrokeRect2DShape(&s.ecs, 50.0f, 50.0f, s.tokens.colour("rule-ruled"));
            s.ecs.attach<PaintComponent>(hatch.entity, "rule-ruled");
            s.ecs.attach<PaintComponent>(dotted.entity, "rule-ruled");
            s.ecs.attach<PaintComponent>(stroke.entity, "rule-ruled");
            s.ecs.executeOnce();

            s.tokens.setTheme(Theme::Candle);
            s.ecs.sendEvent(ThemeChangedEvent{Theme::Candle});
            s.ecs.executeOnce();

            const auto ruled = s.tokens.colour("rule-ruled", Theme::Candle);
            expectColour(hatch.entity->get<HatchRect2DObject>()->colors, ruled);
            expectColour(dotted.entity->get<DottedLine2DObject>()->colors, ruled);
            expectColour(stroke.entity->get<StrokeRect2DObject>()->colors, ruled);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(paint_test, alpha_scales_token)
        {
            MockLogger logger;
            PaintFixture s;

            EntityRef text = s.makeTextEntity("ink");
            s.ecs.attach<PaintComponent>(text, "ink", 0.6f);
            s.ecs.executeOnce();
            EXPECT_NEAR(text->get<TTFText>()->colors.w, s.tokens.colour("ink").w * 0.6f, 0.5f);

            // A theme switch repaints but keeps the stored alpha.
            s.tokens.setTheme(Theme::Candle);
            s.ecs.sendEvent(ThemeChangedEvent{Theme::Candle});
            s.ecs.executeOnce();
            EXPECT_NEAR(text->get<TTFText>()->colors.w, s.tokens.colour("ink", Theme::Candle).w * 0.6f, 0.5f);

            // setAlpha re-applies at a new alpha.
            text->get<PaintComponent>()->setAlpha(1.0f);
            s.ecs.executeOnce();
            EXPECT_NEAR(text->get<TTFText>()->colors.w, s.tokens.colour("ink", Theme::Candle).w, 0.5f);
        }
    }
}
