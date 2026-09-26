#include "stdafx.h"

#include <gtest/gtest.h>

#include "Core/textmetrics.h"

#include "ECS/entitysystem.h"
#include "UI/themesystem.h"
#include "UI/ttftext.h"

#include "mocklogger.h"

using namespace chronicle;

namespace pg
{
    namespace test
    {
        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(textmetrics_test, baseline_shift_is_ascender_difference)
        {
            MockLogger logger;
            EntitySystem ecs;
            MasterRenderer renderer;
            auto* ttf = ecs.createSystem<TTFTextSystem>(&renderer);
            auto* theme = ecs.createSystem<ThemeSystem>();
            theme->loadTheme("chronicle/tokens.json", "fonts");

            const TextStyle& heading = theme->style("heading");
            const TextStyle& label = theme->style("label");
            const float ascHeading = ttf->measureText(heading.fontAlias, "H", 1.0f, 0.0f, 0.0f, heading.letterSpacingPx).ascender;
            const float ascLabel = ttf->measureText(label.fontAlias, "H", 1.0f, 0.0f, 0.0f, label.letterSpacingPx).ascender;

            EXPECT_FLOAT_EQ(ascenderOf(&ecs, "heading"), ascHeading);
            EXPECT_FLOAT_EQ(baselineShift(&ecs, "heading", "label"), ascHeading - ascLabel);

            // The cached call returns the same.
            EXPECT_FLOAT_EQ(baselineShift(&ecs, "heading", "label"), ascHeading - ascLabel);
        }
    }
}
