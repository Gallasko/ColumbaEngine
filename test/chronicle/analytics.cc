#include "stdafx.h"

#include <gtest/gtest.h>

#include "Core/analytics.h"
#include "Scenes/lifesave.h"

#include "mocklogger.h"

using namespace chronicle;

namespace pg
{
    namespace test
    {
        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // A row is the JSON array of the six values the proxy's query names, the digest as a string
        // (its own quotes escaped) or null when the scene has not given one yet.
        TEST(analytics_test, a_row_is_the_six_values_of_the_query)
        {
            MockLogger logger;

            EXPECT_EQ(Analytics::row("abc", "chronicle.session_end", 1500, 9000, 123456789, "{\"age\":7.00}"),
                      "[\"abc\",\"chronicle.session_end\",1500,9000,123456789,\"{\\\"age\\\":7.00}\"]");

            EXPECT_EQ(Analytics::row("abc", "chronicle.session_start", 0, 0, 5, ""),
                      "[\"abc\",\"chronicle.session_start\",0,0,5,null]");

            EXPECT_EQ(Analytics::jsonEscape("a\"b\\c\nd"), "a\\\"b\\\\c\\nd");
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // What the page is shown on rides at the end of the digest, with the first row too, which has
        // no digest yet: sizes, a ratio and a flag, and nothing else.
        TEST(analytics_test, a_digest_carries_the_screen)
        {
            MockLogger logger;

            const std::string screen = Analytics::screenFields(390, 844, 390, 844, 3.0, true);

            EXPECT_EQ(screen, "\"viewW\":390,\"viewH\":844,\"screenW\":390,\"screenH\":844,\"dpr\":3.00,\"touch\":1");
            EXPECT_EQ(Analytics::screenFields(1320, 1020, 1920, 1080, 1.25, false), "\"viewW\":1320,\"viewH\":1020,\"screenW\":1920,\"screenH\":1080,\"dpr\":1.25,\"touch\":0");

            // After what the life says
            EXPECT_EQ(Analytics::withFields("{\"age\":7.00,\"lives\":1}", "\"page\":\"phone\""), "{\"age\":7.00,\"lives\":1,\"page\":\"phone\"}");
            EXPECT_EQ(Analytics::withFields("{\"age\":7.00}", screen), "{\"age\":7.00," + screen + "}");

            // Alone, when the scene has not spoken yet
            EXPECT_EQ(Analytics::withFields("", screen), "{" + screen + "}");
            EXPECT_EQ(Analytics::withFields("{}", screen), "{" + screen + "}");

            // Nothing to add: the digest as it was
            EXPECT_EQ(Analytics::withFields("{\"age\":7.00}", ""), "{\"age\":7.00}");
            EXPECT_EQ(Analytics::withFields("", ""), "");
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // The digest says where a life stands and nothing of who he is.
        TEST(analytics_test, a_digest_says_where_the_life_stands)
        {
            MockLogger logger;

            LifeSave life = freshLife();

            life.running = "carters";
            life.monthsIn = 2;
            life.done["mill"] = 2;
            life.done["messages"] = 1;
            life.lives = 3;
            life.picks = 4;
            life.begun = 2;
            life.atOnce = 1;
            life.skips = 5;
            life.guide = 4;

            // A deed, a line of lore he read and a word of the guide: one deed
            life.achieved = {"first.coin", "lore.bell", "guide.rations_low"};

            ASSERT_FALSE(life.name.empty());

            const std::string digest = life.digest();

            EXPECT_EQ(digest.find("{\"age\":7.00,\"world\":0,"), 0u);
            EXPECT_NE(digest.find("\"running\":\"carters\",\"monthsIn\":2,\"terms\":3,\"deeds\":1,"), std::string::npos);
            EXPECT_NE(digest.find(",\"lives\":3,\"picks\":4,\"begun\":2,\"atOnce\":1,\"skips\":5,\"guide\":4,\"guideSkipped\":0,\"town\":0,\"townKnown\":0}"), std::string::npos);
            EXPECT_EQ(digest.find(life.name), std::string::npos);

            // A first life starts a month before 7, its guide at its first step
            EXPECT_EQ(freshLife(true).digest().find("{\"age\":6.92,\"world\":0,"), 0u);
            EXPECT_NE(freshLife(true).digest().find(",\"guide\":0,\"guideSkipped\":0,\"town\":0,\"townKnown\":0}"), std::string::npos);

            // Skipped at its third step: past the last, and where he left it
            life.guide = 10;
            life.guideSkipped = 3;

            EXPECT_NE(life.digest().find(",\"guide\":10,\"guideSkipped\":3,\"town\":0,\"townKnown\":0}"), std::string::npos);

            // The town: whether a life has explored it, and the levels of its places added up
            life.townKnown = true;
            life.town = {{"market", 2}, {"mill", 1}};

            EXPECT_NE(life.digest().find(",\"town\":3,\"townKnown\":1}"), std::string::npos);

            // The rules read it with his stats, and it is never written among them
            EXPECT_EQ(life.character().at("town_known").get<int>(), 1);
            EXPECT_EQ(life.character().at("town.market").get<int>(), 2);
            EXPECT_TRUE(life.takeTown("town.market", 3));
            EXPECT_EQ(life.town["market"], 2);
            EXPECT_FALSE(life.takeTown("coin", 3));
        }
    }
}
