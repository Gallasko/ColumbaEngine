#include "stdafx.h"

#include <gtest/gtest.h>

#include "UI/ttftext.h"
#include "UI/utf8.h"

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

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(ttftext_test, line_height_is_font_wide)
        {
            MockLogger logger;
            EntitySystem ecs;
            MasterRenderer renderer;

            auto* sys = makeTextSystem(ecs, renderer);

            const float shortGlyphs = sys->measureText("inter", "acemnorsuvwxz").height;
            const float tallGlyphs = sys->measureText("inter", "AjgQ|").height;

            EXPECT_NEAR(shortGlyphs, tallGlyphs, 0.01f);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(ttftext_test, fractional_advance)
        {
            MockLogger logger;
            EntitySystem ecs;
            MasterRenderer renderer;

            auto* sys = makeTextSystem(ecs, renderer);

            const GlyphInfo* iGlyph = sys->fonts.at("inter").glyph('i');
            ASSERT_NE(iGlyph, nullptr);

            const float width = sys->measureText("inter", "iiiiiiiiii").width;

            // Ten advances, no per-glyph truncation to whole pixels.
            EXPECT_NEAR(width, 10.0f * iGlyph->advance, 0.01f);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(ttftext_test, kerning_does_not_widen)
        {
            MockLogger logger;
            EntitySystem ecs;
            MasterRenderer renderer;

            auto* sys = makeTextSystem(ecs, renderer);

            const FontAtlas& atlas = sys->fonts.at("inter");
            const GlyphInfo* a = atlas.glyph('A');
            const GlyphInfo* v = atlas.glyph('V');
            ASSERT_NE(a, nullptr);
            ASSERT_NE(v, nullptr);

            const float width = sys->measureText("inter", "AV").width;

            // Kerning may pull the pair closer but never wider (equal with no kern table).
            EXPECT_LE(width, a->advance + v->advance + 0.01f);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(ttftext_test, glyph_origins_are_integral)
        {
            MockLogger logger;
            EntitySystem ecs;
            MasterRenderer renderer;

            auto* sys = makeTextSystem(ecs, renderer);

            auto text = makeTTFText(&ecs, 0.0f, 0.0f, 1.0f, "inter", "Wave form Ajg");
            ecs.executeOnce();

            const float uiX = text.get<PositionComponent>()->x;

            const auto& templates = sys->entityGlyphTemplates[text.id];
            ASSERT_FALSE(templates.empty());

            for (const auto& glyph : templates)
            {
                const float origin = uiX + glyph.relX;
                EXPECT_NEAR(origin, std::round(origin), 1e-4f);
            }
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(ttftext_test, utf8_decode)
        {
            const std::vector<uint32_t> decoded = utf8::decode("\xE2\x86\x92 \xE2\x80\x94 \xC3\x97");
            const std::vector<uint32_t> expected = {0x2192, 0x20, 0x2014, 0x20, 0xD7};
            EXPECT_EQ(decoded, expected);

            const std::vector<uint32_t> invalid = utf8::decode("\xFF");
            const std::vector<uint32_t> replacement = {0xFFFD};
            EXPECT_EQ(invalid, replacement);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(ttftext_test, arrows_have_glyphs)
        {
            MockLogger logger;
            EntitySystem ecs;
            MasterRenderer renderer;

            auto* sys = makeTextSystem(ecs, renderer);

            EXPECT_NE(sys->fonts.at("inter").glyph(0x2192), nullptr);

            const float withArrow = sys->measureText("inter", "3 \xE2\x86\x92 4").width;
            const float withSpace = sys->measureText("inter", "3  4").width;

            EXPECT_GT(withArrow, withSpace);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(ttftext_test, wrap_keeps_box_width)
        {
            MockLogger logger;
            EntitySystem ecs;
            MasterRenderer renderer;

            auto* sys = makeTextSystem(ecs, renderer);

            auto text = makeTTFText(&ecs, 0.0f, 0.0f, 1.0f, "inter", "one two three four five six seven eight nine ten");
            text.get<TTFText>()->setWrap(true);
            text.get<PositionComponent>()->setWidth(80.0f);

            ecs.executeOnce();

            EXPECT_FLOAT_EQ(text.get<PositionComponent>()->width, 80.0f);
            EXPECT_LE(text.get<TTFText>()->textWidth, 80.0f);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(ttftext_test, atlas_overflow_is_reported)
        {
            MockLogger logger;

            FT_Library ft;
            ASSERT_EQ(FT_Init_FreeType(&ft), 0);

            // The atlas keeps its FT_Face open, so it must be destroyed before the library.
            {
                FontAtlas atlas;
                const bool built = atlas.build(ft, "fonts/Inter-Regular.ttf", 400, defaultCharset());

                EXPECT_FALSE(built);
            }

            FT_Done_FreeType(ft);
        }
    }
}
