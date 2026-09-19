#include "stdafx.h"

#include <gtest/gtest.h>

#include "UI/iconatlas.h"
#include "Loaders/svgloader.h"

#include "mocklogger.h"

namespace pg
{
    namespace test
    {
        namespace
        {
            // The 27 Chronicle icon fixtures, copied next to the test binary from testdeps/icons.
            const std::vector<std::string> iconNames = {
                "strength", "dexterity", "intelligence", "vitality", "gold", "reputation", "time", "age",
                "training", "study", "work", "trade", "guild", "adventure", "swordsmanship", "magic",
                "town", "academy", "forge", "gate", "seal", "relic", "equipment", "quill", "skull", "check", "cross"};

            SvgCoverage solidCoverage(int width, int height)
            {
                SvgCoverage coverage;
                coverage.width = width;
                coverage.height = height;
                coverage.alpha.assign(static_cast<size_t>(width) * height, 255);
                return coverage;
            }
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(iconatlas, single_entry_uvs)
        {
            IconAtlasBuilder builder(1024, 1024);

            EXPECT_TRUE(builder.add("time", 24, solidCoverage(24, 24)));

            ASSERT_EQ(builder.entries().size(), 1u);

            const IconEntry& entry = builder.entries().front();
            EXPECT_FLOAT_EQ(entry.uvTopLeft.x,     0.0f);
            EXPECT_FLOAT_EQ(entry.uvTopLeft.y,     0.0f);
            EXPECT_FLOAT_EQ(entry.uvBottomRight.x, 24.0f / 1024.0f);
            EXPECT_FLOAT_EQ(entry.uvBottomRight.y, 24.0f / 1024.0f);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(iconatlas, pack_all_fixtures)
        {
            MockLogger logger;

            IconAtlasBuilder builder(1024, 1024);

            for (const auto& name : iconNames)
            {
                auto doc = SvgLoader::parseFile("icons/" + name + ".svg");
                ASSERT_TRUE(doc.has_value()) << "missing fixture: " << name;

                EXPECT_TRUE(builder.add(name, 24, SvgLoader::rasterizeCoverage(*doc, 24)));
                EXPECT_TRUE(builder.add(name, 48, SvgLoader::rasterizeCoverage(*doc, 48)));
            }

            const std::vector<IconEntry>& entries = builder.entries();
            ASSERT_EQ(entries.size(), 54u);

            // Every UV rectangle lies inside [0, 1].
            for (const auto& e : entries)
            {
                EXPECT_GE(e.uvTopLeft.x,     0.0f);
                EXPECT_GE(e.uvTopLeft.y,     0.0f);
                EXPECT_LE(e.uvBottomRight.x, 1.0f);
                EXPECT_LE(e.uvBottomRight.y, 1.0f);
            }

            // No two rectangles overlap (O(n^2) is fine for 54 entries).
            for (size_t i = 0; i < entries.size(); ++i)
            {
                for (size_t j = i + 1; j < entries.size(); ++j)
                {
                    const IconEntry& a = entries[i];
                    const IconEntry& b = entries[j];

                    const bool disjoint =
                        a.uvBottomRight.x <= b.uvTopLeft.x or
                        b.uvBottomRight.x <= a.uvTopLeft.x or
                        a.uvBottomRight.y <= b.uvTopLeft.y or
                        b.uvBottomRight.y <= a.uvTopLeft.y;

                    EXPECT_TRUE(disjoint) << "overlap between " << a.name << "@" << a.size
                                          << " and " << b.name << "@" << b.size;
                }
            }
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(iconatlas, reject_too_large)
        {
            MockLogger logger;

            IconAtlasBuilder builder(1024, 1024);

            EXPECT_FALSE(builder.add("huge", 1200, solidCoverage(1200, 1200)));
            EXPECT_EQ(builder.entries().size(), 0u);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(iconatlas, find_by_name_and_size)
        {
            IconAtlasBuilder builder(1024, 1024);

            builder.add("time", 24, solidCoverage(24, 24));

            const IconEntry* found = builder.find("time", 24);
            ASSERT_NE(found, nullptr);
            EXPECT_EQ(found->size, 24);

            EXPECT_EQ(builder.find("time", 32), nullptr);
        }
    }
}
