#include "stdafx.h"

#include <gtest/gtest.h>

#include "UI/ttftext.h"

#include "ECS/entitysystem.h"

#include "mocklogger.h"

namespace pg
{
    namespace test
    {
        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // Enabled in step 2, once registerFont builds the atlas synchronously.
        TEST(ttftext_test, DISABLED_colour_is_normalised)
        {
            MockLogger logger;
            EntitySystem ecs;
            MasterRenderer renderer;

            ecs.createSystem<PositionComponentSystem>();
            auto sys = ecs.createSystem<TTFTextSystem>(&renderer);
            sys->registerFont("fonts/Inter-Regular.ttf", "inter", 16);

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
    }
}
