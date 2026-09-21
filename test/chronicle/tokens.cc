#include "stdafx.h"

#include <cmath>

#include <gtest/gtest.h>

#include "Core/tokens.h"

#include "mocklogger.h"

using namespace chronicle;

namespace pg
{
    namespace test
    {
        namespace
        {
            // The shipped tokens, loaded once (read from beside the binary).
            const Tokens& shipped()
            {
                static Tokens tokens = Tokens::load("chronicle/tokens.json");
                return tokens;
            }

            void expectColour(const constant::Vector4D& c, float r, float g, float b, float a)
            {
                EXPECT_FLOAT_EQ(c.x, r);
                EXPECT_FLOAT_EQ(c.y, g);
                EXPECT_FLOAT_EQ(c.z, b);
                EXPECT_FLOAT_EQ(c.w, a);
            }

            bool isMagenta(const constant::Vector4D& c)
            {
                return c.x == 255.0f and c.y == 0.0f and c.z == 255.0f and c.w == 255.0f;
            }

            float channelLinear(float c255)
            {
                const float c = c255 / 255.0f;
                return c <= 0.03928f ? c / 12.92f : std::pow((c + 0.055f) / 1.055f, 2.4f);
            }

            float relLuminance(const constant::Vector4D& c)
            {
                return 0.2126f * channelLinear(c.x) + 0.7152f * channelLinear(c.y) + 0.0722f * channelLinear(c.z);
            }

            float contrast(const constant::Vector4D& a, const constant::Vector4D& b)
            {
                const float la = relLuminance(a);
                const float lb = relLuminance(b);
                const float hi = std::max(la, lb);
                const float lo = std::min(la, lb);
                return (hi + 0.05f) / (lo + 0.05f);
            }
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(tokens_test, loads_shipped_file)
        {
            const Tokens& t = shipped();
            EXPECT_TRUE(t.ok());
            EXPECT_TRUE(t.errors().empty());
            EXPECT_EQ(t.version(), 3);
            EXPECT_EQ(t.colourNames().size(), 29u);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(tokens_test, hex_colours)
        {
            const Tokens& t = shipped();
            expectColour(t.colour("ink", Theme::Day),    43, 34, 25, 255);
            expectColour(t.colour("ink", Theme::Candle), 234, 223, 200, 255);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(tokens_test, rgba_colours)
        {
            const Tokens& t = shipped();
            expectColour(t.colour("scrim", Theme::Day), 43, 34, 25, 148);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(tokens_test, aliases_resolve_per_theme)
        {
            const Tokens& t = shipped();
            expectColour(t.colour("status-gain", Theme::Day),
                         t.colour("verdigris", Theme::Day).x, t.colour("verdigris", Theme::Day).y,
                         t.colour("verdigris", Theme::Day).z, t.colour("verdigris", Theme::Day).w);
            expectColour(t.colour("status-gain", Theme::Candle),
                         t.colour("verdigris", Theme::Candle).x, t.colour("verdigris", Theme::Candle).y,
                         t.colour("verdigris", Theme::Candle).z, t.colour("verdigris", Theme::Candle).w);
            expectColour(t.colour("state-locked", Theme::Candle),
                         t.colour("ink-faint", Theme::Candle).x, t.colour("ink-faint", Theme::Candle).y,
                         t.colour("ink-faint", Theme::Candle).z, t.colour("ink-faint", Theme::Candle).w);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(tokens_test, alias_chain)
        {
            const std::string json = R"({"version":2,"color":{"tokens":[
                {"name":"a","value":{"day":"{b}","candle":"{b}"}},
                {"name":"b","value":{"day":"{c}","candle":"{c}"}},
                {"name":"c","value":{"day":"#000000","candle":"#000000"}}
            ]}})";
            Tokens t = Tokens::parse(json);
            EXPECT_TRUE(t.ok());
            expectColour(t.colour("a", Theme::Day), 0, 0, 0, 255);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(tokens_test, alias_cycle_is_error)
        {
            MockLogger logger;
            const std::string json = R"({"version":2,"color":{"tokens":[
                {"name":"a","value":{"day":"{b}","candle":"{b}"}},
                {"name":"b","value":{"day":"{a}","candle":"{a}"}}
            ]}})";
            Tokens t = Tokens::parse(json);
            EXPECT_FALSE(t.ok());

            bool mentionsA = false;
            for (const auto& e : t.errors())
                if (e.find("'a'") != std::string::npos)
                    mentionsA = true;
            EXPECT_TRUE(mentionsA);

            EXPECT_TRUE(isMagenta(t.colour("a", Theme::Day)));
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(tokens_test, missing_alias_target_is_error)
        {
            const std::string json = R"({"version":2,"color":{"tokens":[
                {"name":"a","value":{"day":"{ghost}","candle":"{ghost}"}}
            ]}})";
            Tokens t = Tokens::parse(json);
            EXPECT_FALSE(t.ok());
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(tokens_test, theme_switch)
        {
            Tokens t = Tokens::load("chronicle/tokens.json");
            EXPECT_EQ(t.theme(), Theme::Day);
            t.setTheme(Theme::Candle);
            expectColour(t.colour("vellum"), 23, 20, 15, 255);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(tokens_test, unknown_colour_is_magenta)
        {
            MockLogger logger;
            const Tokens& t = shipped();
            EXPECT_TRUE(isMagenta(t.colour("nope")));
            EXPECT_TRUE(isMagenta(t.colour("nope")));
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(tokens_test, spacing_scale)
        {
            const Tokens& t = shipped();
            EXPECT_FLOAT_EQ(t.space(1), 4.0f);
            EXPECT_FLOAT_EQ(t.space(2), 8.0f);
            EXPECT_FLOAT_EQ(t.space(3), 12.0f);
            EXPECT_FLOAT_EQ(t.space(4), 16.0f);
            EXPECT_FLOAT_EQ(t.space(5), 24.0f);
            EXPECT_FLOAT_EQ(t.space(6), 32.0f);
            EXPECT_FLOAT_EQ(t.space(7), 48.0f);
            EXPECT_FLOAT_EQ(t.spacing("space-5"), 24.0f);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(tokens_test, borders_radius_opacity)
        {
            const Tokens& t = shipped();
            EXPECT_FLOAT_EQ(t.border("border-hair"), 1.0f);
            EXPECT_FLOAT_EQ(t.border("border-frame"), 3.0f);
            EXPECT_FLOAT_EQ(t.radius("radius-none"), 0.0f);
            EXPECT_FLOAT_EQ(t.radius("radius-round"), 999.0f);
            EXPECT_FLOAT_EQ(t.opacity("opacity-hatch"), 0.4f);
            EXPECT_FLOAT_EQ(t.opacity("opacity-ghost"), 0.14f);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(tokens_test, bad_px_is_error)
        {
            const std::string json = R"({"version":2,"spacing":{"tokens":[
                {"name":"space-x","value":"12em"}
            ]}})";
            Tokens t = Tokens::parse(json);
            EXPECT_FALSE(t.ok());
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(tokens_test, every_colour_resolves_both_themes)
        {
            const Tokens& t = shipped();
            for (const auto& name : t.colourNames())
            {
                for (Theme theme : {Theme::Day, Theme::Candle})
                {
                    const auto c = t.colour(name, theme);
                    EXPECT_FALSE(isMagenta(c)) << name;
                    EXPECT_GT(c.w, 0.0f) << name;
                }
            }
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(tokens_test, contrast_floor_holds)
        {
            const Tokens& t = shipped();

            for (Theme theme : {Theme::Day, Theme::Candle})
            {
                const auto vellum = t.colour("vellum", theme);
                const auto folio = t.colour("folio", theme);
                const auto vellumWorn = t.colour("vellum-worn", theme);

                for (const char* ink : {"ink", "ink-muted", "ink-faint"})
                {
                    EXPECT_GE(contrast(t.colour(ink, theme), vellum), 4.5f) << ink;
                    EXPECT_GE(contrast(t.colour(ink, theme), folio), 4.5f) << ink;
                }

                EXPECT_GE(contrast(t.colour("rule-ruled", theme), vellum), 3.0f);
                EXPECT_GE(contrast(t.colour("rule-ruled", theme), folio), 3.0f);
                EXPECT_GE(contrast(t.colour("rule-ruled", theme), vellumWorn), 3.0f);

                for (const char* pigment : {"vermilion", "lapis", "verdigris", "ochre"})
                {
                    EXPECT_GE(contrast(t.colour(pigment, theme), vellum), 3.0f) << pigment;
                    EXPECT_GE(contrast(t.colour(pigment, theme), folio), 3.0f) << pigment;
                }
            }
        }
    }
}
