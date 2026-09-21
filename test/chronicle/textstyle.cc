#include "stdafx.h"

#include <gtest/gtest.h>

#include "Core/textstyle.h"

#include "ECS/entitysystem.h"

#include "mocklogger.h"

using namespace chronicle;

namespace pg
{
    namespace test
    {
        namespace
        {
            TextStyles shippedStyles()
            {
                Tokens tokens = Tokens::load("chronicle/tokens.json");
                return TextStyles::fromTokens(tokens);
            }

            bool endsWith(const std::string& s, const std::string& suffix)
            {
                return s.size() >= suffix.size() and s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
            }
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(textstyle_test, sixteen_styles_in_order)
        {
            TextStyles styles = shippedStyles();
            EXPECT_TRUE(styles.ok());
            ASSERT_EQ(styles.all().size(), 16u);

            const std::vector<std::string> expected = {
                "versal", "chapter", "title", "heading", "tab", "gloss-title", "body", "body-sm",
                "gloss", "label", "caption", "control", "figure-xl", "figure", "figure-sm", "tick"};

            for (size_t i = 0; i < expected.size(); ++i)
                EXPECT_EQ(styles.all()[i].name, expected[i]);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(textstyle_test, sizes_and_line_heights)
        {
            TextStyles styles = shippedStyles();

            EXPECT_EQ(styles.get("body").sizePx, 16);
            EXPECT_EQ(styles.get("body").lineHeightPx, 24);
            EXPECT_EQ(styles.get("body").weight, 400);

            EXPECT_EQ(styles.get("figure-xl").sizePx, 32);
            EXPECT_EQ(styles.get("figure-xl").lineHeightPx, 34);
            EXPECT_EQ(styles.get("figure-xl").weight, 600);

            EXPECT_EQ(styles.get("versal").sizePx, 64);
            EXPECT_EQ(styles.get("versal").lineHeightPx, 56);
            EXPECT_EQ(styles.get("versal").weight, 600);

            EXPECT_TRUE(styles.get("gloss").italic);
            EXPECT_EQ(styles.get("label").weight, 600);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(textstyle_test, letter_spacing_px)
        {
            TextStyles styles = shippedStyles();
            EXPECT_FLOAT_EQ(styles.get("heading").letterSpacingPx, 0.5f);
            EXPECT_FLOAT_EQ(styles.get("label").letterSpacingPx, 1.0f);
            EXPECT_FLOAT_EQ(styles.get("caption").letterSpacingPx, 0.5f);
            EXPECT_FLOAT_EQ(styles.get("body").letterSpacingPx, 0.0f);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(textstyle_test, font_files_resolve)
        {
            TextStyles styles = shippedStyles();
            for (const auto& s : styles.all())
                EXPECT_FALSE(s.fontFile.empty()) << s.name;

            EXPECT_TRUE(endsWith(styles.get("chapter").fontFile, "CormorantGaramond-SemiBold.ttf"));
            EXPECT_TRUE(endsWith(styles.get("gloss").fontFile, "EBGaramond-Italic.ttf"));
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(textstyle_test, unknown_weight_is_error)
        {
            const std::string json = R"({"version":2,"type":{"groups":[
                {"name":"Text","family":"text","styles":[
                    {"name":"body","fontSize":"16px","lineHeight":"24px","fontWeight":700}
                ]}
            ]}})";
            Tokens tokens = Tokens::parse(json);
            TextStyles styles = TextStyles::fromTokens(tokens);

            EXPECT_FALSE(styles.ok());

            bool namesBodyAnd700 = false;
            for (const auto& e : styles.errors())
                if (e.find("body") != std::string::npos and e.find("700") != std::string::npos)
                    namesBodyAnd700 = true;
            EXPECT_TRUE(namesBodyAnd700);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(textstyle_test, register_all_skips_missing_files)
        {
            MockLogger logger;
            EntitySystem ecs;
            MasterRenderer renderer;
            ecs.createSystem<PositionComponentSystem>();
            auto* ttf = ecs.createSystem<TTFTextSystem>(&renderer);

            TextStyles styles = shippedStyles();
            const size_t registered = styles.registerAll(ttf, "fonts");

            EXPECT_EQ(registered, 13u);  // 16 minus Italic (gloss) and Medium (caption, tick)
            EXPECT_GT(ttf->fonts.count("chr-body"), 0u);
            EXPECT_GT(ttf->fonts.count("chr-figure"), 0u);
            EXPECT_GT(ttf->fonts.count("chr-chapter"), 0u);
            EXPECT_EQ(ttf->fonts.count("chr-gloss"), 0u);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(textstyle_test, exact_pixel_size)
        {
            MockLogger logger;
            EntitySystem ecs;
            MasterRenderer renderer;
            ecs.createSystem<PositionComponentSystem>();
            auto* ttf = ecs.createSystem<TTFTextSystem>(&renderer);

            TextStyles styles = shippedStyles();
            styles.registerAll(ttf, "fonts");

            EXPECT_GT(ttf->measureText("chr-figure-xl", "17").height, ttf->measureText("chr-figure", "17").height);

            const float body = ttf->measureText("chr-body", "x").height;
            const float bodySm = ttf->measureText("chr-body-sm", "x").height;
            EXPECT_NEAR(body, bodySm * (16.0f / 14.0f), body * 0.08f);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(textstyle_test, line_spacing_gives_token_line_height)
        {
            MockLogger logger;
            EntitySystem ecs;
            MasterRenderer renderer;
            ecs.createSystem<PositionComponentSystem>();
            auto* ttf = ecs.createSystem<TTFTextSystem>(&renderer);

            TextStyles styles = shippedStyles();
            styles.registerAll(ttf, "fonts");

            EXPECT_NEAR(styles.get("body").lineSpacingPx + ttf->measureText("chr-body", "Hg").lineHeight, 24.0f, 0.01f);
            EXPECT_NEAR(styles.get("heading").lineSpacingPx + ttf->measureText("chr-heading", "Hg").lineHeight, 26.0f, 0.01f);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(textstyle_test, make_text_applies_style)
        {
            MockLogger logger;
            EntitySystem ecs;
            MasterRenderer renderer;
            ecs.createSystem<PositionComponentSystem>();
            auto* ttf = ecs.createSystem<TTFTextSystem>(&renderer);

            TextStyles styles = shippedStyles();
            styles.registerAll(ttf, "fonts");

            const constant::Vector4D ink{43.0f, 34.0f, 25.0f, 255.0f};
            auto text = styles.makeText(&ecs, "label", "REQUIRES", ink);

            auto obj = text.get<TTFText>();
            EXPECT_EQ(obj->fontPath, "chr-label");
            EXPECT_FLOAT_EQ(obj->scale, 1.0f);
            EXPECT_FLOAT_EQ(obj->letterSpacing, 1.0f);
            EXPECT_FLOAT_EQ(obj->spacing, styles.get("label").lineSpacingPx);
            EXPECT_FLOAT_EQ(obj->colors.x, ink.x);
            EXPECT_FLOAT_EQ(obj->colors.w, ink.w);

            ecs.executeOnce();

            const float expected = ttf->measureText("chr-label", "REQUIRES", 1.0f, 0.0f, 0.0f, 1.0f).width;
            EXPECT_NEAR(obj->textWidth, expected, 0.01f);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(textstyle_test, figures_are_tabular)
        {
            MockLogger logger;
            EntitySystem ecs;
            MasterRenderer renderer;
            ecs.createSystem<PositionComponentSystem>();
            auto* ttf = ecs.createSystem<TTFTextSystem>(&renderer);

            TextStyles styles = shippedStyles();
            styles.registerAll(ttf, "fonts");

            // EB Garamond's lining figures are tabular: every digit shares one advance.
            for (const char* alias : {"chr-figure", "chr-figure-xl", "chr-figure-sm", "chr-tick"})
            {
                const float zero = ttf->measureText(alias, "0").width;
                for (char d = '1'; d <= '9'; ++d)
                    EXPECT_NEAR(ttf->measureText(alias, std::string(1, d)).width, zero, 0.01f) << alias << " digit " << d;
            }
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(textstyle_test, display_figures_are_not_tabular_so_never_used_for_figures)
        {
            MockLogger logger;
            EntitySystem ecs;
            MasterRenderer renderer;
            ecs.createSystem<PositionComponentSystem>();
            auto* ttf = ecs.createSystem<TTFTextSystem>(&renderer);

            TextStyles styles = shippedStyles();
            styles.registerAll(ttf, "fonts");

            // Cormorant (display) digits are proportional; the kit never sets figures in it.
            const float zero = ttf->measureText("chr-title", "0").width;
            bool anyDiffer = false;
            for (char d = '1'; d <= '9'; ++d)
                if (std::abs(ttf->measureText("chr-title", std::string(1, d)).width - zero) > 0.01f)
                    anyDiffer = true;
            EXPECT_TRUE(anyDiffer);
        }
    }
}
