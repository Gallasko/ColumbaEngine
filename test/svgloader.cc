#include "stdafx.h"

#include <gtest/gtest.h>

#include "Loaders/svgloader.h"

#include "mocklogger.h"

namespace pg
{
    namespace test
    {
        namespace
        {
            // A single horizontal stroke centred vertically in a 24x24 viewBox.
            const std::string horizontalLineSvg =
                "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 24 24\" "
                "width=\"24\" height=\"24\"><path d=\"M2 12h20\" stroke=\"#000\" stroke-width=\"2\"/></svg>";
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(svgloader, parse_string_valid)
        {
            MockLogger logger;

            auto doc = SvgLoader::parseString(horizontalLineSvg);

            ASSERT_TRUE(doc.has_value());
            EXPECT_TRUE(doc->valid());
            EXPECT_FLOAT_EQ(doc->width(),  24.0f);
            EXPECT_FLOAT_EQ(doc->height(), 24.0f);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(svgloader, parse_string_invalid)
        {
            MockLogger logger;

            auto doc = SvgLoader::parseString("not svg");

            EXPECT_FALSE(doc.has_value());
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(svgloader, parse_file)
        {
            MockLogger logger;

            // testdeps/ is copied next to the test binary before build.
            auto doc = SvgLoader::parseFile("icons/time.svg");

            ASSERT_TRUE(doc.has_value());
            EXPECT_TRUE(doc->valid());
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(svgloader, rasterize_coverage)
        {
            MockLogger logger;

            auto doc = SvgLoader::parseString(horizontalLineSvg);
            ASSERT_TRUE(doc.has_value());

            SvgCoverage coverage = SvgLoader::rasterizeCoverage(*doc, 24);

            EXPECT_EQ(coverage.width,        24);
            EXPECT_EQ(coverage.height,       24);
            EXPECT_EQ(coverage.alpha.size(), 576u);

            // The stroke crosses the centre; the top-left corner stays empty.
            EXPECT_GT(coverage.alpha[12 * 24 + 12], 128);
            EXPECT_EQ(coverage.alpha[0],            0);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(svgloader, rasterize_rgba)
        {
            MockLogger logger;

            auto doc = SvgLoader::parseString(horizontalLineSvg);
            ASSERT_TRUE(doc.has_value());

            SvgImage image = SvgLoader::rasterize(*doc, 48);

            EXPECT_EQ(image.width,       48);
            EXPECT_EQ(image.height,      48);
            EXPECT_EQ(image.rgba.size(), static_cast<size_t>(48 * 48 * 4));

            // Alpha of the centre pixel: the scaled stroke passes through it.
            EXPECT_GT(image.rgba[(24 * 48 + 24) * 4 + 3], 128);
        }
    }
}
