#include "stdafx.h"

#include <chrono>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "Core/rulevm.h"

#include "ECS/entitysystem.h"

#include "mocklogger.h"

using namespace chronicle;

// The rule bridge: inputs as globals, outputs read back by path, through the scripts in rules/
// (examples/Chronicle/rules, copied beside the test binary).
namespace pg
{
    namespace test
    {
        namespace
        {
            constexpr const char* Root = "rules";

            struct RulesFixture
            {
                EntitySystem ecs;
                Rules rules;

                RulesFixture()
                {
                    EXPECT_TRUE(rules.load(&ecs, Root)) << (rules.errors.empty() ? "" : rules.errors.front());
                }
            };

            int intOf(const ElementMap& map, const std::string& key)
            {
                auto it = map.find(key);
                EXPECT_NE(it, map.end()) << key;

                if (it == map.end())
                    return -9999;

                if (it->second.type == UnionType::FLOAT)
                    return static_cast<int>(it->second.get<float>());

                return it->second.get<int>();
            }

            float floatOf(const ElementMap& map, const std::string& key)
            {
                auto it = map.find(key);
                EXPECT_NE(it, map.end()) << key;

                if (it == map.end())
                    return -9999.0f;

                if (it->second.type == UnionType::INT)
                    return static_cast<float>(it->second.get<int>());

                return it->second.get<float>();
            }

            std::string textOf(const ElementMap& map, const std::string& key)
            {
                auto it = map.find(key);
                EXPECT_NE(it, map.end()) << key;

                return it == map.end() ? "<missing>" : it->second.toString();
            }

            const ElementMap* byId(const RecordList& list, const std::string& id)
            {
                for (const auto& rec : list)
                {
                    auto it = rec.find("id");

                    if (it != rec.end() and it->second.toString() == id)
                        return &rec;
                }

                return nullptr;
            }

            bool mentions(const std::vector<std::string>& errors, const std::string& text)
            {
                for (const auto& e : errors)
                {
                    if (e.find(text) != std::string::npos)
                        return true;
                }

                return false;
            }

            ElementMap aldren()
            {
                return {{"str", ElementType{14}}, {"dex", ElementType{11}}, {"int", ElementType{9}}, {"vit", ElementType{12}},
                        {"swd", ElementType{3}}, {"letter", ElementType{0}}};
            }
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(rules_test, activities_load_and_shape)
        {
            MockLogger logger;
            RulesFixture f;

            std::vector<RuleActivity> activities;
            ASSERT_TRUE(f.rules.activities(aldren(), activities)) << (f.rules.errors.empty() ? "" : f.rules.errors.front());
            ASSERT_GE(activities.size(), 9u);

            for (const auto& a : activities)
            {
                for (const char* key : {"id", "group", "name", "glyph", "months", "rank", "each", "locked"})
                    EXPECT_TRUE(a.fields.count(key)) << textOf(a.fields, "id") << " has no " << key;
            }

            const RuleActivity* yard = nullptr;

            for (const auto& a : activities)
            {
                if (textOf(a.fields, "id") == "train.yard")
                    yard = &a;
            }

            ASSERT_NE(yard, nullptr);
            EXPECT_EQ(intOf(yard->fields, "months"), 6);
            ASSERT_FALSE(yard->gains.empty());
            EXPECT_TRUE(yard->gains[0].count("stat"));
            EXPECT_TRUE(yard->gains[0].count("amount"));
            EXPECT_EQ(textOf(yard->gains[0], "stat"), "str");
            EXPECT_FALSE(yard->fields.at("locked").get<bool>());

            // Squiring asks STR 18 and SWD 4 of a character with 14 and 3: locked, and says so
            const RuleActivity* squire = nullptr;

            for (const auto& a : activities)
            {
                if (textOf(a.fields, "id") == "train.squire")
                    squire = &a;
            }

            ASSERT_NE(squire, nullptr);
            EXPECT_TRUE(squire->fields.at("locked").get<bool>());
            ASSERT_EQ(squire->requires.size(), 2u);
            EXPECT_EQ(textOf(squire->requires[0], "label"), "Strength");
            EXPECT_EQ(intOf(squire->requires[0], "current"), 14);
            EXPECT_EQ(intOf(squire->requires[0], "needed"), 18);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(rules_test, milestones_next)
        {
            MockLogger logger;
            RulesFixture f;

            std::vector<RuleMilestone> milestones;
            ElementMap next;
            ASSERT_TRUE(f.rules.milestones(17.4f, milestones, next));

            // The expectation is read from the same table: the first milestone after 17.4
            const RuleMilestone* first = nullptr;

            for (const auto& m : milestones)
            {
                if (floatOf(m.fields, "age") > 17.4f)
                {
                    first = &m;
                    break;
                }
            }

            ASSERT_NE(first, nullptr);
            EXPECT_EQ(textOf(next, "id"), textOf(first->fields, "id"));
            EXPECT_EQ(textOf(next, "id"), "path");
            EXPECT_EQ(intOf(next, "age"), 18);
            EXPECT_EQ(intOf(next, "in"), 7);

            // Passed milestones say so
            EXPECT_EQ(milestones[0].fields.at("passed").get<bool>(), true);
            EXPECT_EQ(first->fields.at("passed").get<bool>(), false);
            EXPECT_FALSE(first->asks.empty());

            // The page's head at that age
            ElementMap headline;
            ASSERT_TRUE(f.rules.milestones(17.5f, milestones, next, &headline));
            EXPECT_EQ(textOf(headline, "ageText"), "17.5");
            EXPECT_EQ(textOf(headline, "subtitle"), "THE SEVENTEENTH YEAR \xC2\xB7 SUMMER \xC2\xB7 BELLMOOR");
            EXPECT_EQ(textOf(headline, "ageNote"), "years \xC2\xB7 6 mo to the eighteenth");

            // Past the last: empty strings and -1
            ASSERT_TRUE(f.rules.milestones(43.0f, milestones, next, &headline));
            EXPECT_EQ(textOf(headline, "ageNote"), "years");
            EXPECT_EQ(textOf(next, "id"), "");
            EXPECT_EQ(textOf(next, "label"), "");
            EXPECT_EQ(intOf(next, "age"), -1);
            EXPECT_EQ(intOf(next, "in"), -1);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(rules_test, windows_states)
        {
            MockLogger logger;
            RulesFixture f;

            RecordList windows;
            ASSERT_TRUE(f.rules.windows(20.2f, aldren(), windows));

            ASSERT_NE(byId(windows, "squire"), nullptr);
            EXPECT_EQ(textOf(*byId(windows, "squire"), "state"), "open");
            EXPECT_EQ(textOf(*byId(windows, "choir"), "state"), "closed");
            EXPECT_EQ(textOf(*byId(windows, "academy"), "state"), "open");

            // 21 months left at 20.2, a 6-month activity: three attempts
            EXPECT_EQ(intOf(*byId(windows, "squire"), "attempts"), 3);
            EXPECT_EQ(textOf(*byId(windows, "squire"), "note"), "Open 21 more months. Three attempts fit; four do not.");

            for (const auto& w : windows)
                EXPECT_FALSE(textOf(w, "note").empty()) << textOf(w, "id");

            ASSERT_TRUE(f.rules.windows(12.0f, aldren(), windows));
            EXPECT_EQ(textOf(*byId(windows, "choir"), "state"), "open");
            EXPECT_EQ(textOf(*byId(windows, "squire"), "state"), "upcoming");
            EXPECT_EQ(intOf(*byId(windows, "squire"), "from"), 16);
            EXPECT_EQ(intOf(*byId(windows, "squire"), "to"), 22);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(rules_test, forecast_at_term)
        {
            MockLogger logger;
            RulesFixture f;

            // The gain the table gives at term
            std::vector<RuleActivity> activities;
            ASSERT_TRUE(f.rules.activities(aldren(), activities));
            int strGain = 0;

            for (const auto& a : activities)
            {
                if (textOf(a.fields, "id") == "train.yard")
                {
                    for (const auto& g : a.gains)
                    {
                        if (textOf(g, "stat") == "str")
                            strGain = intOf(g, "amount");
                    }
                }
            }

            ASSERT_GT(strGain, 0);

            RuleForecast forecast;
            ASSERT_TRUE(f.rules.forecast(17.4f, aldren(), "train.yard", 0, forecast)) << (f.rules.errors.empty() ? "" : f.rules.errors.front());

            EXPECT_EQ(intOf(forecast.atTerm, "str"), 14 + strGain);
            EXPECT_EQ(intOf(forecast.atTerm, "dex"), 11);   // Untouched stats carried over
            EXPECT_FLOAT_EQ(forecast.percent, 0.0f);
            EXPECT_EQ(forecast.months, 6);
            EXPECT_EQ(forecast.caption.rfind("MONTH 0 OF 6", 0), 0u) << forecast.caption;
            EXPECT_EQ(forecast.error, "");

            ASSERT_EQ(forecast.entries.size(), 1u);
            EXPECT_EQ(textOf(forecast.entries[0], "text"), "Train at the yard");
            EXPECT_EQ(textOf(forecast.entries[0], "kind"), "gain");

            ASSERT_TRUE(f.rules.forecast(17.4f, aldren(), "train.yard", 3, forecast));
            EXPECT_FLOAT_EQ(forecast.percent, 50.0f);
            EXPECT_EQ(forecast.caption, "MONTH 3 OF 6 \xC2\xB7 STRENGTH 14 \xE2\x86\x92 " + std::to_string(14 + strGain) + " AT TERM");
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(rules_test, forecast_gaps)
        {
            MockLogger logger;
            RulesFixture f;

            ElementMap character = {{"str", ElementType{14}}, {"swd", ElementType{3}}};

            RuleForecast forecast;
            ASSERT_TRUE(f.rules.forecast(17.4f, character, "train.yard", 0, forecast));

            // The Squire's asks at 18, against the character as he is
            ASSERT_EQ(forecast.gaps.size(), 3u);
            EXPECT_EQ(textOf(forecast.gaps[0], "stat"), "str");
            EXPECT_EQ(intOf(forecast.gaps[0], "current"), 14);
            EXPECT_EQ(intOf(forecast.gaps[0], "needed"), 18);
            EXPECT_EQ(textOf(forecast.gaps[1], "stat"), "swd");
            EXPECT_EQ(intOf(forecast.gaps[1], "current"), 3);
            EXPECT_EQ(intOf(forecast.gaps[1], "needed"), 4);
            EXPECT_EQ(textOf(forecast.gaps[2], "stat"), "letter");
            EXPECT_EQ(intOf(forecast.gaps[2], "current"), 0);
            EXPECT_EQ(intOf(forecast.gaps[2], "needed"), 1);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(rules_test, forecast_unknown_activity_is_error)
        {
            MockLogger logger;
            RulesFixture f;

            RuleForecast forecast;
            EXPECT_FALSE(f.rules.forecast(17.4f, aldren(), "nope", 0, forecast));
            EXPECT_EQ(forecast.error, "unknown activity");
            EXPECT_TRUE(mentions(f.rules.errors, "forecast.pg"));
            EXPECT_TRUE(mentions(f.rules.errors, "unknown activity"));
            EXPECT_TRUE(mentions(f.rules.errors, "nope"));
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(rules_test, forecast_is_pure)
        {
            MockLogger logger;
            RulesFixture f;

            RuleForecast first;
            ASSERT_TRUE(f.rules.forecast(17.4f, aldren(), "train.yard", 3, first));

            // A different run in between leaves nothing behind
            RuleForecast other;
            ElementMap strong = {{"str", ElementType{30}}, {"int", ElementType{20}}};
            ASSERT_TRUE(f.rules.forecast(30.0f, strong, "study.academy", 5, other));
            EXPECT_NE(other.caption, first.caption);

            RuleForecast again;
            ASSERT_TRUE(f.rules.forecast(17.4f, aldren(), "train.yard", 3, again));

            EXPECT_EQ(again.caption, first.caption);
            EXPECT_FLOAT_EQ(again.percent, first.percent);
            EXPECT_EQ(again.atTerm.size(), first.atTerm.size());
            EXPECT_FALSE(again.atTerm.count("int") and again.atTerm.at("int").get<int>() == 20);
            EXPECT_EQ(intOf(again.atTerm, "str"), intOf(first.atTerm, "str"));
            EXPECT_EQ(again.gaps.size(), first.gaps.size());
            EXPECT_EQ(again.entries.size(), first.entries.size());
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(rules_test, missing_output_is_error)
        {
            MockLogger logger;
            EntitySystem ecs;

            RuleScript script;
            ASSERT_TRUE(script.load(&ecs, "rules/bad.pg"));
            ASSERT_TRUE(script.run());

            ElementMap forecast;
            EXPECT_FALSE(script.get("forecast", forecast));
            EXPECT_TRUE(mentions(script.errors(), "rules/bad.pg"));
            EXPECT_TRUE(mentions(script.errors(), "`forecast`"));

            // A path into an output that is not there names the missing part
            ElementType value;
            EXPECT_FALSE(script.get("notTheOutput.percent", value));
            EXPECT_TRUE(mentions(script.errors(), "`percent`"));
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(rules_test, syntax_error_is_reported)
        {
            MockLogger logger;
            EntitySystem ecs;

            const unsigned int before = logger.getNbError();

            RuleScript script;
            EXPECT_FALSE(script.load(&ecs, "rules/broken.pg"));
            EXPECT_FALSE(script.loaded());
            EXPECT_TRUE(mentions(script.errors(), "rules/broken.pg"));
            EXPECT_TRUE(mentions(script.errors(), "does not compile"));

            // The VM's own message went to the log
            EXPECT_GT(logger.getNbError() - before, 1u);

            EXPECT_FALSE(script.run());
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // A script refuses its input through its `errors` list, not by throwing: the run fails.
        TEST(rules_test, errors_list_fails_the_run)
        {
            MockLogger logger;
            EntitySystem ecs;

            RuleScript script;
            ASSERT_TRUE(script.load(&ecs, "rules/refuses.pg"));
            EXPECT_FALSE(script.run());
            ASSERT_EQ(script.errors().size(), 1u);
            EXPECT_EQ(script.errors()[0], "rules/refuses.pg: character: no strength given");
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // A ceiling, not a target: the forecast runs once per month tick.
        TEST(rules_test, timing_ceiling)
        {
            MockLogger logger;
            RulesFixture f;

            RuleForecast forecast;
            ASSERT_TRUE(f.rules.forecast(17.4f, aldren(), "train.yard", 0, forecast));

            const auto start = std::chrono::steady_clock::now();

            for (int i = 0; i < 100; ++i)
                ASSERT_TRUE(f.rules.forecast(17.4f, aldren(), "train.yard", i % 6, forecast));

            const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();

            EXPECT_LT(ms, 50) << "100 forecasts took " << ms << " ms";
        }
    }
}
