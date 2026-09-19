#include "stdafx.h"

#include <gtest/gtest.h>

#include "UI/ttftext.h"

#include "ECS/entitysystem.h"

#include "mocklogger.h"

namespace pg
{
    namespace test
    {
        namespace
        {
            // Registers the Inter fixture (copied beside the binary from testdeps/fonts) at 16 px.
            TTFTextSystem* makeTextSystem(EntitySystem& ecs, MasterRenderer& renderer)
            {
                ecs.createSystem<PositionComponentSystem>();
                auto* sys = ecs.createSystem<TTFTextSystem>(&renderer);
                sys->registerFont("fonts/Inter-Regular.ttf", "inter", 16);
                return sys;
            }
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(ttftext_test, colour_is_normalised)
        {
            MockLogger logger;
            EntitySystem ecs;
            MasterRenderer renderer;

            auto* sys = makeTextSystem(ecs, renderer);

            auto text = makeTTFText(&ecs, 0.0f, 0.0f, 1.0f, "inter", "Ab", 1.0f, {148.0f, 156.0f, 175.0f, 255.0f});

            ecs.executeOnce();

            const auto& templates = sys->entityGlyphTemplates[text.id];
            ASSERT_FALSE(templates.empty());

            for (const auto& glyph : templates)
            {
                EXPECT_GE(glyph.r, 0.0f);
                EXPECT_LE(glyph.r, 1.0f);
                EXPECT_GE(glyph.g, 0.0f);
                EXPECT_LE(glyph.g, 1.0f);
                EXPECT_GE(glyph.b, 0.0f);
                EXPECT_LE(glyph.b, 1.0f);
                EXPECT_GE(glyph.a, 0.0f);
                EXPECT_LE(glyph.a, 1.0f);

                EXPECT_FLOAT_EQ(glyph.r, 148.0f / 255.0f);
            }
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(ttftext_test, measure_after_register)
        {
            MockLogger logger;
            EntitySystem ecs;
            MasterRenderer renderer;

            auto* sys = makeTextSystem(ecs, renderer);

            TextMetrics metrics = sys->measureText("inter", "STR 14");

            EXPECT_GT(metrics.width, 0.0f);
            EXPECT_EQ(metrics.lineCount, 1);
            EXPECT_NEAR(metrics.height, metrics.lineHeight, 0.01f);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(ttftext_test, measure_matches_layout)
        {
            MockLogger logger;
            EntitySystem ecs;
            MasterRenderer renderer;

            auto* sys = makeTextSystem(ecs, renderer);

            auto text = makeTTFText(&ecs, 0.0f, 0.0f, 1.0f, "inter", "STR 14");
            ecs.executeOnce();

            TextMetrics metrics = sys->measureText("inter", "STR 14");

            auto obj = text.get<TTFText>();
            EXPECT_NEAR(obj->textWidth, metrics.width, 0.01f);
            EXPECT_NEAR(obj->textHeight, metrics.height, 0.01f);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(ttftext_test, measure_two_lines)
        {
            MockLogger logger;
            EntitySystem ecs;
            MasterRenderer renderer;

            auto* sys = makeTextSystem(ecs, renderer);

            TextMetrics metrics = sys->measureText("inter", "STR\n14");

            EXPECT_EQ(metrics.lineCount, 2);
            EXPECT_NEAR(metrics.height, 2.0f * metrics.lineHeight, 0.01f);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(ttftext_test, measure_wraps)
        {
            MockLogger logger;
            EntitySystem ecs;
            MasterRenderer renderer;

            auto* sys = makeTextSystem(ecs, renderer);

            TextMetrics metrics = sys->measureText("inter", "one two three four", 1.0f, 60.0f);

            EXPECT_GE(metrics.lineCount, 2);
            EXPECT_LE(metrics.width, 60.0f);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(ttftext_test, scale_is_linear)
        {
            MockLogger logger;
            EntitySystem ecs;
            MasterRenderer renderer;

            auto* sys = makeTextSystem(ecs, renderer);

            const float single = sys->measureText("inter", "STR 14", 1.0f).width;
            const float doubled = sys->measureText("inter", "STR 14", 2.0f).width;

            EXPECT_NEAR(doubled, 2.0f * single, 0.01f);
        }
    }
}
