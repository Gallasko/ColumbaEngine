#include "stdafx.h"

#include <gtest/gtest.h>

#include <map>
#include <set>

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
            text.get<TTFText>()->setOverflow(pg::TextOverflow::Wrap);
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

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(ttftext_test, letter_spacing_widens)
        {
            MockLogger logger;
            EntitySystem ecs;
            MasterRenderer renderer;

            auto* sys = makeTextSystem(ecs, renderer);

            const float base = sys->measureText("inter", "STRENGTH").width;
            const float tracked = sys->measureText("inter", "STRENGTH", 1.0f, 0.0f, 0.0f, 1.5f).width;

            // "STRENGTH" is 8 glyphs, each widened by one tracking step.
            EXPECT_NEAR(tracked, base + 8.0f * 1.5f, 0.01f);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(ttftext_test, letter_spacing_scales)
        {
            MockLogger logger;
            EntitySystem ecs;
            MasterRenderer renderer;

            auto* sys = makeTextSystem(ecs, renderer);

            const float base = sys->measureText("inter", "STRENGTH", 2.0f).width;
            const float tracked = sys->measureText("inter", "STRENGTH", 2.0f, 0.0f, 0.0f, 1.5f).width;

            EXPECT_NEAR(tracked, base + 8.0f * 1.5f * 2.0f, 0.01f);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(ttftext_test, letter_spacing_matches_layout)
        {
            MockLogger logger;
            EntitySystem ecs;
            MasterRenderer renderer;

            auto* sys = makeTextSystem(ecs, renderer);

            auto text = makeTTFText(&ecs, 0.0f, 0.0f, 1.0f, "inter", "STRENGTH");
            text.get<TTFText>()->setLetterSpacing(1.5f);
            ecs.executeOnce();

            const float measured = sys->measureText("inter", "STRENGTH", 1.0f, 0.0f, 0.0f, 1.5f).width;
            EXPECT_NEAR(text.get<TTFText>()->textWidth, measured, 0.01f);
        }

        namespace
        {
            const std::string ELLIPSIS = "\xE2\x80\xA6";   // U+2026
            const std::string FOX = "The quick brown fox jumps over the lazy dog";

            TextLayoutParams ellipsisAt(float maxWidth, float letterSpacing = 0.0f)
            {
                TextLayoutParams params;
                params.maxWidth = maxWidth;
                params.letterSpacing = letterSpacing;
                params.overflow = TextOverflow::Ellipsis;
                return params;
            }
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(ttftext_test, ellipsis_elides_to_width)
        {
            MockLogger logger;
            EntitySystem ecs;
            MasterRenderer renderer;

            auto* sys = makeTextSystem(ecs, renderer);

            TextMetrics metrics = sys->measureText("inter", FOX, ellipsisAt(120.0f));

            EXPECT_TRUE(metrics.elided);
            EXPECT_EQ(metrics.lineCount, 1);
            EXPECT_GT(metrics.width, 0.0f);
            EXPECT_LE(metrics.width, 120.0f);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(ttftext_test, ellipsis_whole_text_fits_untouched)
        {
            MockLogger logger;
            EntitySystem ecs;
            MasterRenderer renderer;

            auto* sys = makeTextSystem(ecs, renderer);

            const float plain = sys->measureText("inter", "STR 14").width;
            TextMetrics metrics = sys->measureText("inter", "STR 14", ellipsisAt(plain + 20.0f));

            EXPECT_FALSE(metrics.elided);
            EXPECT_NEAR(metrics.width, plain, 0.01f);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(ttftext_test, ellipsis_no_trailing_space)
        {
            MockLogger logger;
            EntitySystem ecs;
            MasterRenderer renderer;

            auto* sys = makeTextSystem(ecs, renderer);

            // A width that fits "one two…" but not "one two t…": the cut lands inside
            // "three" and the elided line must not keep the space before the ellipsis.
            const float fits = sys->measureText("inter", "one two" + ELLIPSIS).width;
            const float overflows = sys->measureText("inter", "one two t" + ELLIPSIS).width;
            ASSERT_LT(fits, overflows);

            TextMetrics metrics = sys->measureText("inter", "one two three", ellipsisAt((fits + overflows) * 0.5f));

            EXPECT_TRUE(metrics.elided);
            EXPECT_NEAR(metrics.width, fits, 0.01f);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(ttftext_test, ellipsis_too_narrow_is_empty)
        {
            MockLogger logger;
            EntitySystem ecs;
            MasterRenderer renderer;

            auto* sys = makeTextSystem(ecs, renderer);

            TextMetrics metrics = sys->measureText("inter", FOX, ellipsisAt(2.0f));

            EXPECT_TRUE(metrics.elided);
            EXPECT_FLOAT_EQ(metrics.width, 0.0f);

            auto text = makeTTFText(&ecs, 0.0f, 0.0f, 1.0f, "inter", FOX);
            text.get<TTFText>()->setOverflow(TextOverflow::Ellipsis);
            text.get<PositionComponent>()->setWidth(2.0f);
            ecs.executeOnce();

            EXPECT_TRUE(sys->entityGlyphTemplates[text.id].empty());
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(ttftext_test, ellipsis_respects_letter_spacing)
        {
            MockLogger logger;
            EntitySystem ecs;
            MasterRenderer renderer;

            auto* sys = makeTextSystem(ecs, renderer);

            // The box fits the untracked text exactly, so tracking must force an elision.
            const float untracked = sys->measureText("inter", "STRENGTH").width;
            TextMetrics metrics = sys->measureText("inter", "STRENGTH", ellipsisAt(untracked, 1.5f));

            EXPECT_TRUE(metrics.elided);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(ttftext_test, wrap_max_lines_truncates)
        {
            MockLogger logger;
            EntitySystem ecs;
            MasterRenderer renderer;

            auto* sys = makeTextSystem(ecs, renderer);

            TextLayoutParams params;
            params.maxWidth = 80.0f;
            params.overflow = TextOverflow::Wrap;
            params.maxLines = 2;

            TextMetrics unlimited = sys->measureText("inter", FOX, 1.0f, 80.0f);
            ASSERT_GT(unlimited.lineCount, 2);

            TextMetrics metrics = sys->measureText("inter", FOX, params);

            EXPECT_TRUE(metrics.elided);
            EXPECT_EQ(metrics.lineCount, 2);
            EXPECT_NEAR(metrics.height, 2.0f * metrics.lineHeight, 0.01f);
            EXPECT_LE(metrics.width, 80.0f);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(ttftext_test, align_right_offsets_glyphs)
        {
            MockLogger logger;
            EntitySystem ecs;
            MasterRenderer renderer;

            auto* sys = makeTextSystem(ecs, renderer);

            auto makeAligned = [&](TextAlign align)
            {
                auto text = makeTTFText(&ecs, 0.0f, 0.0f, 1.0f, "inter", "STR 14");
                text.get<TTFText>()->setOverflow(TextOverflow::Ellipsis);
                text.get<TTFText>()->setAlign(align);
                text.get<PositionComponent>()->setWidth(200.0f);
                return text;
            };

            auto left = makeAligned(TextAlign::Left);
            auto right = makeAligned(TextAlign::Right);
            ecs.executeOnce();

            const float w = sys->measureText("inter", "STR 14").width;
            const float expected = std::round(200.0f - w);

            const auto& leftGlyphs = sys->entityGlyphTemplates[left.id];
            const auto& rightGlyphs = sys->entityGlyphTemplates[right.id];
            ASSERT_FALSE(leftGlyphs.empty());
            ASSERT_EQ(leftGlyphs.size(), rightGlyphs.size());

            for (size_t i = 0; i < leftGlyphs.size(); ++i)
            {
                EXPECT_NEAR(rightGlyphs[i].relX - leftGlyphs[i].relX, expected, 0.01f);
                EXPECT_NEAR(rightGlyphs[i].relX, std::round(rightGlyphs[i].relX), 1e-4f);
            }
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(ttftext_test, align_centre_offsets_glyphs)
        {
            MockLogger logger;
            EntitySystem ecs;
            MasterRenderer renderer;

            auto* sys = makeTextSystem(ecs, renderer);

            auto makeAligned = [&](TextAlign align)
            {
                auto text = makeTTFText(&ecs, 0.0f, 0.0f, 1.0f, "inter", "STR 14");
                text.get<TTFText>()->setOverflow(TextOverflow::Ellipsis);
                text.get<TTFText>()->setAlign(align);
                text.get<PositionComponent>()->setWidth(200.0f);
                return text;
            };

            auto left = makeAligned(TextAlign::Left);
            auto centre = makeAligned(TextAlign::Centre);
            ecs.executeOnce();

            const float w = sys->measureText("inter", "STR 14").width;
            const float expected = std::round((200.0f - w) * 0.5f);

            const auto& leftGlyphs = sys->entityGlyphTemplates[left.id];
            const auto& centreGlyphs = sys->entityGlyphTemplates[centre.id];
            ASSERT_FALSE(leftGlyphs.empty());
            ASSERT_EQ(leftGlyphs.size(), centreGlyphs.size());

            for (size_t i = 0; i < leftGlyphs.size(); ++i)
                EXPECT_NEAR(centreGlyphs[i].relX - leftGlyphs[i].relX, expected, 0.01f);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(ttftext_test, wrap_aligns_per_line_right)
        {
            MockLogger logger;
            EntitySystem ecs;
            MasterRenderer renderer;

            auto* sys = makeTextSystem(ecs, renderer);

            auto makeWrapped = [&](TextAlign align)
            {
                auto text = makeTTFText(&ecs, 0.0f, 0.0f, 1.0f, "inter", FOX);
                text.get<TTFText>()->setOverflow(TextOverflow::Wrap);
                text.get<TTFText>()->setAlign(align);
                text.get<PositionComponent>()->setWidth(100.0f);
                return text;
            };

            auto left = makeWrapped(TextAlign::Left);
            auto right = makeWrapped(TextAlign::Right);
            ecs.executeOnce();

            const auto& leftGlyphs = sys->entityGlyphTemplates[left.id];
            const auto& rightGlyphs = sys->entityGlyphTemplates[right.id];
            ASSERT_FALSE(leftGlyphs.empty());
            ASSERT_EQ(leftGlyphs.size(), rightGlyphs.size());

            // Every glyph on one line shares one offset; ragged lines get different ones.
            std::map<float, float> offsetPerLine;
            for (size_t i = 0; i < leftGlyphs.size(); ++i)
            {
                const float delta = rightGlyphs[i].relX - leftGlyphs[i].relX;
                EXPECT_GE(delta, 0.0f);

                auto it = offsetPerLine.find(leftGlyphs[i].relY);
                if (it == offsetPerLine.end())
                    offsetPerLine[leftGlyphs[i].relY] = delta;
                else
                    EXPECT_NEAR(it->second, delta, 0.01f);
            }

            ASSERT_GE(offsetPerLine.size(), 2u);

            std::set<float> distinct;
            for (const auto& [relY, delta] : offsetPerLine)
                distinct.insert(delta);
            EXPECT_GE(distinct.size(), 2u);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(ttftext_test, width_change_triggers_rewrap)
        {
            MockLogger logger;
            EntitySystem ecs;
            MasterRenderer renderer;

            auto* sys = makeTextSystem(ecs, renderer);

            auto text = makeTTFText(&ecs, 0.0f, 0.0f, 1.0f, "inter", FOX);
            text.get<TTFText>()->setOverflow(TextOverflow::Wrap);
            text.get<PositionComponent>()->setWidth(300.0f);
            ecs.executeOnce();

            const float tallAt300 = text.get<TTFText>()->textHeight;

            text.get<PositionComponent>()->setWidth(80.0f);
            ecs.executeOnce();
            ecs.executeOnce();

            EXPECT_GT(text.get<TTFText>()->textHeight, tallAt300);
            EXPECT_LE(text.get<TTFText>()->textWidth, 80.0f);

            // The rebuild's own height settle must not retrigger a rebuild.
            const float settled = text.get<TTFText>()->textHeight;
            ecs.executeOnce();
            ecs.executeOnce();
            EXPECT_FLOAT_EQ(text.get<TTFText>()->textHeight, settled);
            EXPECT_FLOAT_EQ(text.get<PositionComponent>()->width, 80.0f);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(ttftext_test, measure_matches_layout_ellipsis)
        {
            MockLogger logger;
            EntitySystem ecs;
            MasterRenderer renderer;

            auto* sys = makeTextSystem(ecs, renderer);

            auto text = makeTTFText(&ecs, 0.0f, 0.0f, 1.0f, "inter", FOX);
            text.get<TTFText>()->setOverflow(TextOverflow::Ellipsis);
            text.get<PositionComponent>()->setWidth(120.0f);
            ecs.executeOnce();

            TextMetrics metrics = sys->measureText("inter", FOX, ellipsisAt(120.0f));

            EXPECT_TRUE(metrics.elided);
            EXPECT_NEAR(text.get<TTFText>()->textWidth, metrics.width, 0.01f);
            EXPECT_NEAR(text.get<TTFText>()->textHeight, metrics.height, 0.01f);
        }
    }
}
