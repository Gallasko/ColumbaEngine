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

            // A boy of 7, as a fresh life starts
            ElementMap boy()
            {
                return {{"str", ElementType{6}}, {"dex", ElementType{6}}, {"int", ElementType{6}}, {"vit", ElementType{8}}, {"vitmax", ElementType{8}},
                        {"coin", ElementType{0}}, {"rations", ElementType{12}}};
            }

            // The balance's good Warrior the month he swore to the Keep, at 17.5
            ElementMap sworn()
            {
                return {{"str", ElementType{12}}, {"dex", ElementType{8}}, {"int", ElementType{6}}, {"vit", ElementType{10}}, {"vitmax", ElementType{10}},
                        {"arms", ElementType{5}}, {"discipline", ElementType{2}}, {"renown", ElementType{1}},
                        {"keep_oath", ElementType{1}}, {"watch_known", ElementType{1}}, {"edric_support", ElementType{1}},
                        {"coin", ElementType{46}}, {"rations", ElementType{3}}};
            }

            // One activity of the table, for a character at an age
            RuleActivity activityAt(Rules& rules, float age, const ElementMap& character, const std::string& id)
            {
                std::vector<RuleActivity> activities;
                EXPECT_TRUE(rules.activities(age, character, activities)) << (rules.errors.empty() ? "" : rules.errors.front());

                for (const auto& a : activities)
                {
                    if (a.fields.count("id") and a.fields.at("id").toString() == id)
                        return a;
                }

                ADD_FAILURE() << "no activity " << id;

                return RuleActivity{};
            }

            bool flag(const ElementMap& map, const std::string& key)
            {
                auto it = map.find(key);
                EXPECT_NE(it, map.end()) << key;

                return it != map.end() and it->second.type == UnionType::BOOL and it->second.get<bool>();
            }

            std::string firstError(const Rules& rules)
            {
                return rules.errors.empty() ? "" : rules.errors.front();
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
            ASSERT_TRUE(f.rules.activities(7.0f, boy(), activities)) << firstError(f.rules);
            ASSERT_GE(activities.size(), 50u);

            for (const auto& a : activities)
            {
                for (const char* key : {"id", "group", "name", "glyph", "months", "rank", "each", "path", "enters", "board", "fromAge", "finishBy", "locked", "listed", "pathOpen", "closed", "reach"})
                    EXPECT_TRUE(a.fields.count(key)) << textOf(a.fields, "id") << " has no " << key;
            }

            RuleActivity mill = activityAt(f.rules, 7.0f, boy(), "mill");

            EXPECT_EQ(intOf(mill.fields, "months"), 6);
            ASSERT_EQ(mill.gains.size(), 3u);
            EXPECT_EQ(textOf(mill.gains[0], "stat"), "str");
            EXPECT_EQ(textOf(mill.gains[0], "label"), "STR");
            EXPECT_EQ(intOf(mill.gains[0], "amount"), 1);
            EXPECT_TRUE(mill.requires.empty());
            EXPECT_TRUE(mill.costs.empty());
            EXPECT_FALSE(flag(mill.fields, "locked"));
            EXPECT_TRUE(flag(mill.fields, "listed"));
            EXPECT_FALSE(flag(mill.fields, "board"));

            // The smithy takes boys from 9 with Strength 8: the age first, then what it asks. Too
            // young, it is not listed
            RuleActivity smithy = activityAt(f.rules, 7.0f, boy(), "smithy");

            EXPECT_TRUE(flag(smithy.fields, "locked"));
            EXPECT_FALSE(flag(smithy.fields, "listed"));
            ASSERT_EQ(smithy.requires.size(), 2u);
            EXPECT_EQ(textOf(smithy.requires[0], "stat"), "age");
            EXPECT_EQ(textOf(smithy.requires[0], "label"), "Age");
            EXPECT_EQ(intOf(smithy.requires[0], "current"), 7);
            EXPECT_EQ(intOf(smithy.requires[0], "needed"), 9);
            EXPECT_EQ(textOf(smithy.requires[1], "label"), "Strength");
            EXPECT_EQ(intOf(smithy.requires[1], "current"), 6);
            EXPECT_EQ(intOf(smithy.requires[1], "needed"), 8);

            // Of age, the age is no longer asked
            EXPECT_EQ(activityAt(f.rules, 9.0f, boy(), "smithy").requires.size(), 1u);

            // The carters feed him
            RuleActivity carters = activityAt(f.rules, 7.0f, boy(), "carters");

            EXPECT_TRUE(flag(carters.fields, "board"));
            EXPECT_EQ(textOf(carters.fields, "each"), "MEALS PROVIDED");
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // The way into each path is open until he takes one; a path's own activities are listed once
        // he is one of it, and never another's.
        TEST(rules_test, activities_follow_the_path)
        {
            MockLogger logger;
            RulesFixture f;

            for (const char* id : {"keep", "collegium", "hand"})
            {
                RuleActivity way = activityAt(f.rules, 16.0f, boy(), id);

                EXPECT_TRUE(flag(way.fields, "enters")) << id;
                EXPECT_TRUE(flag(way.fields, "pathOpen")) << id;
            }

            for (const char* id : {"serve", "study", "run", "buy.reagents"})
            {
                EXPECT_FALSE(flag(activityAt(f.rules, 18.0f, boy(), id).fields, "pathOpen")) << id;
                EXPECT_FALSE(flag(activityAt(f.rules, 18.0f, boy(), id).fields, "listed")) << id;
            }

            // Sworn to the Keep: no other way in, the Keep's service his once he is of age
            for (const char* id : {"keep", "collegium", "hand", "study", "run"})
                EXPECT_FALSE(flag(activityAt(f.rules, 17.5f, sworn(), id).fields, "listed")) << id;

            RuleActivity serve = activityAt(f.rules, 17.5f, sworn(), "serve");

            EXPECT_TRUE(flag(serve.fields, "pathOpen"));
            EXPECT_FALSE(flag(serve.fields, "listed"));
            EXPECT_TRUE(flag(serve.fields, "locked"));
            ASSERT_FALSE(serve.requires.empty());
            EXPECT_EQ(textOf(serve.requires[0], "stat"), "age");
            EXPECT_EQ(intOf(serve.requires[0], "current"), 17);

            serve = activityAt(f.rules, 18.0f, sworn(), "serve");

            EXPECT_TRUE(flag(serve.fields, "listed"));
            EXPECT_FALSE(flag(serve.fields, "locked"));

            // The shared activities stay his
            EXPECT_TRUE(flag(activityAt(f.rules, 17.5f, sworn(), "carters").fields, "listed"));
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // The finish-by age is the last a term may end at: the Watch (12 months, by 13) is begun by 12 at
        // the latest, the Keep (18 months, by 20) by 18.5. A month later the row is gone.
        TEST(rules_test, activities_finish_by)
        {
            MockLogger logger;
            RulesFixture f;

            const float month = 1.0f / 12.0f;
            const ElementMap carrier = {{"str", ElementType{8}}};

            RuleActivity watch = activityAt(f.rules, 12.0f, carrier, "watch");

            EXPECT_FALSE(flag(watch.fields, "closed"));
            EXPECT_TRUE(flag(watch.fields, "listed"));
            EXPECT_EQ(intOf(watch.fields, "finishBy"), 13);

            watch = activityAt(f.rules, 12.0f + month, carrier, "watch");

            EXPECT_TRUE(flag(watch.fields, "closed"));
            EXPECT_FALSE(flag(watch.fields, "listed"));

            const ElementMap squire = {{"str", ElementType{12}}, {"arms", ElementType{3}}, {"discipline", ElementType{2}}};

            EXPECT_TRUE(flag(activityAt(f.rules, 18.5f, squire, "keep").fields, "listed"));
            EXPECT_FALSE(flag(activityAt(f.rules, 18.5f + month, squire, "keep").fields, "listed"));

            // What takes no time can be done to the last month
            EXPECT_TRUE(flag(activityAt(f.rules, 30.0f, boy(), "buy.rations").fields, "listed"));
            EXPECT_FALSE(flag(activityAt(f.rules, 30.0f, boy(), "carters").fields, "listed"));
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // A row is listed once he is of age for it and has, on average, three quarters of what it
        // asks; costs are not counted, they are bought rather than grown.
        TEST(rules_test, activities_shown_within_reach)
        {
            MockLogger logger;
            RulesFixture f;

            // The smithy at 9: Strength 6 of 8 is near enough, 5 is not
            RuleActivity smithy = activityAt(f.rules, 9.0f, boy(), "smithy");

            EXPECT_EQ(intOf(smithy.fields, "reach"), 75);
            EXPECT_TRUE(flag(smithy.fields, "listed"));
            EXPECT_TRUE(flag(smithy.fields, "locked"));

            ElementMap weak = boy();
            weak["str"] = ElementType{5};

            EXPECT_FALSE(flag(activityAt(f.rules, 9.0f, weak, "smithy").fields, "listed"));

            // The chapel comes with his letters
            EXPECT_FALSE(flag(activityAt(f.rules, 9.0f, boy(), "chapel").fields, "listed"));

            ElementMap lettered = boy();
            lettered["letters"] = ElementType{1};

            EXPECT_TRUE(flag(activityAt(f.rules, 9.0f, lettered, "chapel").fields, "listed"));

            // A strong boy at 16 sees the Keep, though he cannot pay its coin yet; not the Collegium
            const ElementMap squire = {{"str", ElementType{12}}, {"arms", ElementType{3}}, {"discipline", ElementType{2}}, {"int", ElementType{6}}};

            RuleActivity keep = activityAt(f.rules, 16.0f, squire, "keep");

            EXPECT_EQ(intOf(keep.fields, "reach"), 100);
            EXPECT_TRUE(flag(keep.fields, "listed"));
            EXPECT_TRUE(flag(keep.fields, "locked"));
            EXPECT_FALSE(flag(activityAt(f.rules, 16.0f, squire, "collegium").fields, "listed"));

            // What asks nothing is always in reach
            EXPECT_EQ(intOf(activityAt(f.rules, 7.0f, boy(), "carters").fields, "reach"), 100);
            EXPECT_TRUE(flag(activityAt(f.rules, 7.0f, boy(), "buy.rations").fields, "listed"));
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // Edric asks Arms 1 of a boy the Watch knows, the Keep Strength 11 of one Edric speaks for; the
        // Hidden Hand asks a friend on the street or one at the night market, either will do.
        TEST(rules_test, activities_eased_and_either)
        {
            MockLogger logger;
            RulesFixture f;

            ElementMap boy13 = {{"str", ElementType{10}}, {"arms", ElementType{1}}};

            RuleActivity edric = activityAt(f.rules, 13.0f, boy13, "edric");

            ASSERT_EQ(edric.requires.size(), 2u);
            EXPECT_EQ(intOf(edric.requires[1], "needed"), 2);
            EXPECT_TRUE(flag(edric.fields, "locked"));

            boy13["watch_known"] = ElementType{1};
            edric = activityAt(f.rules, 13.0f, boy13, "edric");

            EXPECT_EQ(intOf(edric.requires[1], "needed"), 1);
            EXPECT_FALSE(flag(edric.fields, "locked"));

            ElementMap squire = {{"str", ElementType{11}}, {"arms", ElementType{3}}, {"discipline", ElementType{2}}, {"coin", ElementType{10}}};

            EXPECT_TRUE(flag(activityAt(f.rules, 16.0f, squire, "keep").fields, "locked"));

            squire["edric_support"] = ElementType{1};
            RuleActivity keep = activityAt(f.rules, 16.0f, squire, "keep");

            EXPECT_EQ(intOf(keep.requires[0], "needed"), 11);
            EXPECT_FALSE(flag(keep.fields, "locked"));

            ElementMap cutpurse = {{"dex", ElementType{12}}, {"stealth", ElementType{2}}, {"guile", ElementType{2}}, {"coin", ElementType{15}}};

            RuleActivity hand = activityAt(f.rules, 16.0f, cutpurse, "hand");

            ASSERT_GE(hand.requires.size(), 4u);
            EXPECT_EQ(textOf(hand.requires[3], "label"), "Known on the street or at the night market");
            EXPECT_EQ(intOf(hand.requires[3], "current"), 0);
            EXPECT_TRUE(flag(hand.fields, "locked"));

            cutpurse["night_contacts"] = ElementType{1};
            hand = activityAt(f.rules, 16.0f, cutpurse, "hand");

            EXPECT_EQ(intOf(hand.requires[3], "current"), 1);
            EXPECT_FALSE(flag(hand.fields, "locked"));
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // The tourney pays Renown 2 and a victor's name to one who comes with Arms 7.
        TEST(rules_test, activities_bonus)
        {
            MockLogger logger;
            RulesFixture f;

            ElementMap knight = {{"keep_oath", ElementType{1}}, {"arms", ElementType{5}}};

            RuleActivity tourney = activityAt(f.rules, 19.0f, knight, "tourney");

            ASSERT_EQ(tourney.gains.size(), 2u);
            EXPECT_EQ(textOf(tourney.gains[0], "stat"), "renown");
            EXPECT_EQ(intOf(tourney.gains[0], "amount"), 1);

            knight["arms"] = ElementType{7};
            tourney = activityAt(f.rules, 19.0f, knight, "tourney");

            ASSERT_EQ(tourney.gains.size(), 3u);
            EXPECT_EQ(intOf(tourney.gains[0], "amount"), 2);
            EXPECT_EQ(textOf(tourney.gains[2], "stat"), "tourney_victor");
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
            EXPECT_EQ(textOf(next, "id"), "proving");
            EXPECT_EQ(intOf(next, "age"), 21);
            EXPECT_EQ(intOf(next, "in"), 43);

            // Passed milestones say so
            EXPECT_EQ(milestones[0].fields.at("passed").get<bool>(), true);
            EXPECT_EQ(first->fields.at("passed").get<bool>(), false);
            EXPECT_FALSE(first->asks.empty());

            // The page's head at that age
            ElementMap headline;
            ASSERT_TRUE(f.rules.milestones(17.5f, milestones, next, &headline));
            EXPECT_EQ(textOf(headline, "ageText"), "17.5");
            EXPECT_EQ(textOf(headline, "subtitle"), "THE SEVENTEENTH YEAR \xC2\xB7 SUMMER \xC2\xB7 BELLMOOR");
            EXPECT_EQ(textOf(headline, "ageNote"), "years \xC2\xB7 42 mo to the twenty-first");

            // The last one is 30, and it writes a line
            EXPECT_EQ(intOf(milestones.back().fields, "age"), 30);
            EXPECT_FALSE(textOf(milestones.back().fields, "entry").empty());

            // Past the last: empty strings and -1
            ASSERT_TRUE(f.rules.milestones(30.0f, milestones, next, &headline));
            EXPECT_EQ(textOf(headline, "ageNote"), "years");
            EXPECT_EQ(textOf(next, "id"), "");
            EXPECT_EQ(textOf(next, "label"), "");
            EXPECT_EQ(intOf(next, "age"), -1);
            EXPECT_EQ(intOf(next, "in"), -1);

            // An age summed a month at a time from 7 reaches 30 all the same
            float age = 7.0f;

            for (int i = 0; i < 276; ++i)
                age += 1.0f / 12.0f;

            ASSERT_TRUE(f.rules.milestones(age, milestones, next, &headline));
            EXPECT_EQ(textOf(next, "id"), "");
            EXPECT_EQ(textOf(headline, "ageText"), "30.0");
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(rules_test, windows_states)
        {
            MockLogger logger;
            RulesFixture f;

            RecordList windows;
            ASSERT_TRUE(f.rules.windows(12.0f, boy(), windows)) << firstError(f.rules);

            // A boy: the doors of childhood and the three ways in, no path's own
            for (const char* id : {"choir", "watch", "boys", "keep", "collegium", "hand"})
                EXPECT_NE(byId(windows, id), nullptr) << id;

            for (const char* id : {"campaign", "harrow", "rob"})
                EXPECT_EQ(byId(windows, id), nullptr) << id;

            // The choir's 12 months no longer end by 12
            EXPECT_EQ(textOf(*byId(windows, "choir"), "state"), "closed");
            EXPECT_EQ(textOf(*byId(windows, "choir"), "note"), "The choir takes boys until 12. That door is shut.");

            // The Watch: 12 months left before 13, one term of 12
            EXPECT_EQ(textOf(*byId(windows, "watch"), "state"), "open");
            EXPECT_EQ(intOf(*byId(windows, "watch"), "attempts"), 1);
            EXPECT_EQ(textOf(*byId(windows, "watch"), "note"), "Open 12 more months. One attempt fits; two do not.");

            // The Keep, from 16 to 20: two terms of 18 months fit in its four years
            EXPECT_EQ(textOf(*byId(windows, "keep"), "state"), "upcoming");
            EXPECT_EQ(intOf(*byId(windows, "keep"), "from"), 16);
            EXPECT_EQ(intOf(*byId(windows, "keep"), "to"), 20);
            EXPECT_EQ(intOf(*byId(windows, "keep"), "attempts"), 2);

            for (const auto& w : windows)
                EXPECT_FALSE(textOf(w, "note").empty()) << textOf(w, "id");

            // Sworn: the Keep is behind him, the campaign ahead, the other paths gone
            f.rules.done = {{"keep", ElementType{1}}};

            ASSERT_TRUE(f.rules.windows(17.5f, sworn(), windows));
            EXPECT_EQ(textOf(*byId(windows, "keep"), "state"), "closed");
            EXPECT_EQ(textOf(*byId(windows, "keep"), "note"), "He has been through that door.");
            ASSERT_NE(byId(windows, "campaign"), nullptr);
            EXPECT_EQ(textOf(*byId(windows, "campaign"), "state"), "upcoming");

            for (const char* id : {"collegium", "hand", "harrow", "rob"})
                EXPECT_EQ(byId(windows, id), nullptr) << id;
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(rules_test, forecast_at_term)
        {
            MockLogger logger;
            RulesFixture f;

            RuleForecast forecast;
            ASSERT_TRUE(f.rules.forecast(7.0f, boy(), "mill", 0, forecast)) << firstError(f.rules);

            EXPECT_EQ(intOf(forecast.atTerm, "str"), 7);
            EXPECT_EQ(intOf(forecast.atTerm, "vit"), 9);
            EXPECT_EQ(intOf(forecast.atTerm, "vitmax"), 9);
            EXPECT_EQ(intOf(forecast.atTerm, "coin"), 8);
            EXPECT_EQ(intOf(forecast.atTerm, "dex"), 6);   // Untouched stats carried over
            EXPECT_FLOAT_EQ(forecast.percent, 0.0f);
            EXPECT_EQ(forecast.months, 6);
            EXPECT_EQ(forecast.caption, "MONTH 0 OF 6 \xC2\xB7 STRENGTH 6 \xE2\x86\x92 7 AT TERM");
            EXPECT_EQ(forecast.error, "");

            ASSERT_EQ(forecast.entries.size(), 1u);
            EXPECT_EQ(textOf(forecast.entries[0], "text"), "Help at the Mill");
            EXPECT_EQ(textOf(forecast.entries[0], "kind"), "gain");

            ASSERT_TRUE(f.rules.forecast(7.0f, boy(), "mill", 3, forecast));
            EXPECT_FLOAT_EQ(forecast.percent, 50.0f);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // A cost is taken when the activity begins (atStart), not again at its term; an activity
        // done at once lands its gains on what is left.
        TEST(rules_test, forecast_costs_at_start)
        {
            MockLogger logger;
            RulesFixture f;

            ElementMap squire = {{"str", ElementType{12}}, {"arms", ElementType{3}}, {"discipline", ElementType{2}}, {"coin", ElementType{30}}, {"rations", ElementType{4}}};

            RuleForecast forecast;
            ASSERT_TRUE(f.rules.forecast(16.0f, squire, "keep", 0, forecast)) << firstError(f.rules);

            EXPECT_EQ(intOf(forecast.atStart, "coin"), 20);
            EXPECT_EQ(intOf(forecast.atStart, "rations"), 4);
            EXPECT_EQ(intOf(forecast.atTerm, "coin"), 30);
            EXPECT_EQ(intOf(forecast.atTerm, "keep_oath"), 1);
            EXPECT_EQ(intOf(forecast.atTerm, "arms"), 4);

            // Harrow asks coin, rations and reagents up front
            ElementMap scholar = {{"collegium", ElementType{1}}, {"harrow_fragments", ElementType{1}}, {"lore", ElementType{6}}, {"arcana", ElementType{3}},
                                  {"coin", ElementType{25}}, {"rations", ElementType{10}}, {"reagents", ElementType{4}}};

            ASSERT_TRUE(f.rules.forecast(24.0f, scholar, "harrow", 0, forecast));
            EXPECT_EQ(intOf(forecast.atStart, "coin"), 5);
            EXPECT_EQ(intOf(forecast.atStart, "rations"), 2);
            EXPECT_EQ(intOf(forecast.atStart, "reagents"), 0);

            // Done at once: what it brings, on what is left
            ASSERT_TRUE(f.rules.forecast(17.4f, {{"coin", ElementType{12}}}, "buy.rations", 0, forecast));
            EXPECT_EQ(forecast.months, 0);
            EXPECT_FLOAT_EQ(forecast.percent, 100.0f);
            EXPECT_EQ(intOf(forecast.atStart, "coin"), 7);
            EXPECT_EQ(intOf(forecast.atTerm, "coin"), 7);
            EXPECT_EQ(intOf(forecast.atTerm, "rations"), 6);
            ASSERT_EQ(forecast.entries.size(), 1u);
            EXPECT_EQ(textOf(forecast.entries[0], "figure"), "+6 rations \xE2\x88\x92" "5 coin");
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // Parts stop at 30 and skills at 10; what already stood past stays.
        TEST(rules_test, forecast_ceilings)
        {
            MockLogger logger;
            RulesFixture f;

            RuleForecast forecast;
            ASSERT_TRUE(f.rules.forecast(12.0f, {{"str", ElementType{30}}, {"arms", ElementType{10}}}, "yard", 0, forecast)) << firstError(f.rules);
            EXPECT_EQ(intOf(forecast.atTerm, "str"), 30);
            EXPECT_EQ(intOf(forecast.atTerm, "arms"), 10);

            ASSERT_TRUE(f.rules.forecast(12.0f, {{"str", ElementType{32}}, {"arms", ElementType{3}}}, "yard", 0, forecast));
            EXPECT_EQ(intOf(forecast.atTerm, "str"), 32);
            EXPECT_EQ(intOf(forecast.atTerm, "arms"), 4);

            ASSERT_TRUE(f.rules.forecast(12.0f, {{"vit", ElementType{30}}, {"vitmax", ElementType{30}}}, "mill", 0, forecast));
            EXPECT_EQ(intOf(forecast.atTerm, "vit"), 30);
            EXPECT_EQ(intOf(forecast.atTerm, "vitmax"), 30);

            // Renown has no ceiling
            ASSERT_TRUE(f.rules.forecast(19.0f, {{"keep_oath", ElementType{1}}, {"arms", ElementType{5}}, {"renown", ElementType{12}}}, "tourney", 0, forecast));
            EXPECT_EQ(intOf(forecast.atTerm, "renown"), 13);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(rules_test, forecast_gaps)
        {
            MockLogger logger;
            RulesFixture f;

            ElementMap scholar = {{"collegium", ElementType{1}}, {"arcana", ElementType{1}}, {"lore", ElementType{3}}, {"reagents", ElementType{2}}};

            RuleForecast forecast;
            ASSERT_TRUE(f.rules.forecast(18.0f, scholar, "workings", 0, forecast)) << firstError(f.rules);

            // The Mage's asks at the proving (21), against the character as he is
            ASSERT_EQ(forecast.gaps.size(), 2u);
            EXPECT_EQ(textOf(forecast.gaps[0], "stat"), "lore");
            EXPECT_EQ(intOf(forecast.gaps[0], "current"), 3);
            EXPECT_EQ(intOf(forecast.gaps[0], "needed"), 6);
            EXPECT_EQ(textOf(forecast.gaps[1], "stat"), "arcana");
            EXPECT_EQ(intOf(forecast.gaps[1], "current"), 1);
            EXPECT_EQ(intOf(forecast.gaps[1], "needed"), 3);

            // What leads toward no path has none
            ASSERT_TRUE(f.rules.forecast(18.0f, scholar, "mill", 0, forecast));
            EXPECT_TRUE(forecast.gaps.empty());
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(rules_test, forecast_unknown_activity_is_error)
        {
            MockLogger logger;
            RulesFixture f;

            RuleForecast forecast;
            EXPECT_FALSE(f.rules.forecast(17.4f, boy(), "nope", 0, forecast));
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
            ASSERT_TRUE(f.rules.forecast(7.0f, boy(), "mill", 3, first));

            // A different run in between leaves nothing behind
            RuleForecast other;
            ElementMap strong = {{"str", ElementType{30}}, {"int", ElementType{20}}};
            ASSERT_TRUE(f.rules.forecast(26.0f, strong, "captain", 5, other));
            EXPECT_NE(other.caption, first.caption);

            RuleForecast again;
            ASSERT_TRUE(f.rules.forecast(7.0f, boy(), "mill", 3, again));

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
        // An activity with a number of uses counts down as it is done, and is spent at the last one.
        TEST(rules_test, activities_limited_uses)
        {
            MockLogger logger;
            RulesFixture f;

            const ElementMap carrier = {{"str", ElementType{8}}};

            RuleActivity fresh = activityAt(f.rules, 10.0f, carrier, "watch");

            EXPECT_EQ(intOf(fresh.fields, "uses"), 1);
            EXPECT_EQ(intOf(fresh.fields, "done"), 0);
            EXPECT_EQ(intOf(fresh.fields, "left"), 1);
            EXPECT_FALSE(flag(fresh.fields, "spent"));
            EXPECT_TRUE(flag(fresh.fields, "listed"));

            f.rules.done = {{"watch", ElementType{1}}};

            RuleActivity spent = activityAt(f.rules, 10.0f, carrier, "watch");

            EXPECT_EQ(intOf(spent.fields, "done"), 1);
            EXPECT_EQ(intOf(spent.fields, "left"), 0);
            EXPECT_TRUE(flag(spent.fields, "spent"));
            EXPECT_FALSE(flag(spent.fields, "listed"));

            // No `uses`: as often as he likes
            f.rules.done = {{"carters", ElementType{40}}};

            RuleActivity carters = activityAt(f.rules, 10.0f, carrier, "carters");

            EXPECT_EQ(intOf(carters.fields, "left"), -1);
            EXPECT_FALSE(flag(carters.fields, "spent"));
            EXPECT_TRUE(flag(carters.fields, "listed"));
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // Repetition changes an activity: the mill pays less for longer after two terms, the yard
        // takes longer, and the Keep's second year of service is noted once and pays Renown.
        TEST(rules_test, activities_upgrade_after_uses)
        {
            MockLogger logger;
            RulesFixture f;

            EXPECT_EQ(activityAt(f.rules, 8.0f, boy(), "mill").gains.size(), 3u);

            f.rules.done = {{"mill", ElementType{2}}};

            RuleActivity mill = activityAt(f.rules, 8.0f, boy(), "mill");

            EXPECT_EQ(intOf(mill.fields, "months"), 9);
            ASSERT_EQ(mill.gains.size(), 1u);
            EXPECT_EQ(textOf(mill.gains[0], "stat"), "coin");
            EXPECT_EQ(intOf(mill.gains[0], "amount"), 6);

            // The last step reached wins
            f.rules.done = {{"yard", ElementType{3}}};
            EXPECT_EQ(intOf(activityAt(f.rules, 12.0f, {{"str", ElementType{9}}}, "yard").fields, "months"), 12);

            // The term that makes it two: the old gains still, and the step's line and Renown
            f.rules.done = {{"serve", ElementType{1}}};

            RuleForecast forecast;
            ASSERT_TRUE(f.rules.forecast(18.0f, sworn(), "serve", 0, forecast)) << firstError(f.rules);

            EXPECT_EQ(intOf(forecast.atTerm, "coin"), 46 + 12);
            EXPECT_EQ(intOf(forecast.atTerm, "renown"), 2);
            ASSERT_EQ(forecast.entries.size(), 2u);
            EXPECT_EQ(textOf(forecast.entries[1], "kind"), "milestone");
            EXPECT_EQ(textOf(forecast.entries[1], "figure"), "+1 renown");

            // Past the step: the new gains, and the line is not written again
            f.rules.done = {{"serve", ElementType{2}}};

            RuleActivity serve = activityAt(f.rules, 18.0f, sworn(), "serve");

            ASSERT_EQ(serve.gains.size(), 2u);
            EXPECT_EQ(intOf(serve.gains[1], "amount"), 8);

            ASSERT_TRUE(f.rules.forecast(18.0f, sworn(), "serve", 0, forecast));
            EXPECT_EQ(intOf(forecast.atTerm, "coin"), 46 + 8);
            EXPECT_EQ(intOf(forecast.atTerm, "renown"), 1);
            EXPECT_EQ(forecast.entries.size(), 1u);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // Edric's second year earns his word for the Keep, once.
        TEST(rules_test, forecast_trigger_fires_once)
        {
            MockLogger logger;
            RulesFixture f;

            const ElementMap pupil = {{"str", ElementType{11}}, {"arms", ElementType{3}}};

            RuleForecast forecast;

            ASSERT_TRUE(f.rules.forecast(14.0f, pupil, "edric", 0, forecast)) << firstError(f.rules);
            EXPECT_EQ(forecast.atTerm.count("edric_support"), 0u);
            EXPECT_EQ(forecast.entries.size(), 1u);

            f.rules.done = {{"edric", ElementType{1}}};

            ASSERT_TRUE(f.rules.forecast(14.0f, pupil, "edric", 0, forecast));
            EXPECT_EQ(intOf(forecast.atTerm, "edric_support"), 1);
            ASSERT_EQ(forecast.entries.size(), 2u);
            EXPECT_EQ(textOf(forecast.entries[1], "figure"), "+1 edric");
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // A cost is asked like a requirement and listed apart from what the activity brings.
        TEST(rules_test, activities_cost_and_at_once)
        {
            MockLogger logger;
            RulesFixture f;

            ElementMap character = boy();
            character["coin"] = ElementType{3};

            RuleActivity buy = activityAt(f.rules, 7.0f, character, "buy.rations");

            EXPECT_EQ(intOf(buy.fields, "months"), 0);
            EXPECT_TRUE(flag(buy.fields, "locked"));

            // The coin it costs, then room for what it brings
            ASSERT_EQ(buy.requires.size(), 2u);
            EXPECT_EQ(textOf(buy.requires[0], "stat"), "coin");
            EXPECT_EQ(intOf(buy.requires[0], "current"), 3);
            EXPECT_EQ(intOf(buy.requires[0], "needed"), 5);

            ASSERT_EQ(buy.gains.size(), 1u);
            EXPECT_EQ(textOf(buy.gains[0], "stat"), "rations");

            ASSERT_EQ(buy.costs.size(), 1u);
            EXPECT_EQ(textOf(buy.costs[0], "stat"), "coin");
            EXPECT_EQ(textOf(buy.costs[0], "label"), "COIN");
            EXPECT_EQ(intOf(buy.costs[0], "amount"), 5);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // A month eats a ration, and says so as a rate; a month at work that feeds him eats none, and
        // with none held costs nothing either.
        TEST(rules_test, month_decays_unless_fed)
        {
            MockLogger logger;
            RulesFixture f;

            ElementMap character = {{"vit", ElementType{12}}, {"coin", ElementType{3}}, {"rations", ElementType{5}}};

            RuleMonth month;
            ASSERT_TRUE(f.rules.month(character, false, month)) << firstError(f.rules);

            EXPECT_EQ(intOf(month.after, "coin"), 3);
            EXPECT_EQ(intOf(month.after, "rations"), 4);
            EXPECT_EQ(intOf(month.after, "vit"), 12);
            EXPECT_TRUE(month.entries.empty());

            const ElementMap* coin = byId(month.rows, "coin");
            const ElementMap* rations = byId(month.rows, "rations");

            ASSERT_NE(coin, nullptr);
            ASSERT_NE(rations, nullptr);

            EXPECT_EQ(textOf(*coin, "rate"), "");
            EXPECT_EQ(textOf(*rations, "rate"), "\xE2\x88\x92" "1 / mo");

            for (const char* key : {"id", "group", "groupLabel", "glyph", "name", "tone", "rate", "limit"})
                EXPECT_TRUE(coin->count(key)) << key;

            for (const char* id : {"reagents", "favors", "keep_oath", "collegium", "hidden_hand"})
                EXPECT_NE(byId(month.rows, id), nullptr) << id;

            // Fed by his work
            ASSERT_TRUE(f.rules.month(character, true, month));
            EXPECT_EQ(intOf(month.after, "rations"), 5);
            EXPECT_EQ(textOf(*byId(month.rows, "rations"), "rate"), "");

            // The last ration eaten is a fed month: no Vitality taken
            ASSERT_TRUE(f.rules.month({{"vit", ElementType{12}}, {"rations", ElementType{1}}}, false, month));
            EXPECT_EQ(intOf(month.after, "rations"), 0);
            EXPECT_EQ(intOf(month.after, "vit"), 12);

            // With none, fed by his work: nothing is taken, nothing is written
            ASSERT_TRUE(f.rules.month({{"vit", ElementType{12}}, {"rations", ElementType{0}}}, true, month));
            EXPECT_EQ(intOf(month.after, "vit"), 12);
            EXPECT_TRUE(month.hurt.empty());
            EXPECT_TRUE(month.entries.empty());
        }
        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // Running out is written once; staying out costs every month. Never having had any costs nothing.
        TEST(rules_test, month_empty_holding)
        {
            MockLogger logger;
            RulesFixture f;

            RuleMonth month;

            ASSERT_TRUE(f.rules.month({{"vit", ElementType{12}}, {"rations", ElementType{1}}}, false, month));
            EXPECT_EQ(intOf(month.after, "rations"), 0);
            EXPECT_EQ(intOf(month.after, "vit"), 12);
            ASSERT_EQ(month.entries.size(), 1u);
            EXPECT_EQ(textOf(month.entries[0], "kind"), "loss");

            EXPECT_TRUE(month.hurt.empty());

            ASSERT_TRUE(f.rules.month({{"vit", ElementType{12}}, {"rations", ElementType{0}}}, false, month));
            EXPECT_EQ(intOf(month.after, "rations"), 0);
            EXPECT_EQ(intOf(month.after, "vit"), 11);
            EXPECT_TRUE(month.entries.empty());

            // A month that took from what the life hangs on says so
            ASSERT_EQ(month.hurt.size(), 1u);
            EXPECT_EQ(month.hurt[0], "vit");

            // Nothing goes below zero
            ASSERT_TRUE(f.rules.month({{"vit", ElementType{0}}, {"rations", ElementType{0}}}, false, month));
            EXPECT_EQ(intOf(month.after, "vit"), 0);

            ASSERT_TRUE(f.rules.month({{"vit", ElementType{12}}}, false, month));
            EXPECT_EQ(intOf(month.after, "vit"), 12);
            EXPECT_EQ(month.after.count("rations"), 0u);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // A holding says what it is for when hovered: what he holds, its limit, what it uses.
        TEST(rules_test, month_glosses_and_limits)
        {
            MockLogger logger;
            RulesFixture f;

            auto rowOf = [](const RuleGloss& gloss, const std::string& label) -> std::string {
                for (const auto& r : gloss.rows)
                {
                    if (textOf(r, "label") == label)
                        return textOf(r, "value");
                }

                return "<none>";
            };

            auto glossOf = [](const RuleMonth& month, const std::string& id) -> const RuleGloss* {
                for (const auto& g : month.glosses)
                {
                    if (textOf(g.fields, "id") == id)
                        return &g;
                }

                return nullptr;
            };

            RuleMonth month;
            ASSERT_TRUE(f.rules.month({{"vit", ElementType{12}}, {"coin", ElementType{3}}, {"rations", ElementType{5}}}, false, month)) << firstError(f.rules);

            const RuleGloss* coin = glossOf(month, "coin");
            const RuleGloss* rations = glossOf(month, "rations");

            ASSERT_NE(coin, nullptr);
            ASSERT_NE(rations, nullptr);

            EXPECT_EQ(textOf(coin->fields, "title"), "Coin");
            EXPECT_EQ(rowOf(*coin, "Held"), "3");
            EXPECT_EQ(rowOf(*coin, "Limit"), "none");

            EXPECT_EQ(rowOf(*rations, "Held"), "5");
            EXPECT_EQ(rowOf(*rations, "Limit"), "60");
            EXPECT_EQ(rowOf(*rations, "A month uses"), "1");
            EXPECT_EQ(rowOf(*rations, "Lasts"), "5 mo");
            EXPECT_EQ(rowOf(*rations, "With none, Vitality"), "\xE2\x88\x92" "1 / mo");
            EXPECT_FALSE(textOf(rations->fields, "text").empty());

            EXPECT_EQ(intOf(*byId(month.rows, "rations"), "limit"), 60);
            EXPECT_EQ(intOf(*byId(month.rows, "coin"), "limit"), 0);

            // Fed by his work, the month uses none, and the gloss says so
            ASSERT_TRUE(f.rules.month({{"vit", ElementType{12}}, {"rations", ElementType{5}}}, true, month));
            EXPECT_NE(textOf(glossOf(month, "rations")->fields, "footnote").find("MEALS PROVIDED"), std::string::npos);

            ASSERT_TRUE(f.rules.month({{"vit", ElementType{12}}, {"rations", ElementType{60}}}, false, month));
            EXPECT_NE(textOf(glossOf(month, "rations")->fields, "footnote").find("FULL"), std::string::npos);

            // What he holds past the limit is his: the month only eats from it
            ASSERT_TRUE(f.rules.month({{"vit", ElementType{12}}, {"rations", ElementType{64}}}, false, month));
            EXPECT_EQ(intOf(month.after, "rations"), 63);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // What brings a stat with a limit is closed while he is full of it. With any room left it is
        // open, and brings all it brings: past the limit if need be.
        TEST(rules_test, activities_full_store_locks_what_fills_it)
        {
            MockLogger logger;
            RulesFixture f;

            ElementMap character = boy();
            character["coin"] = ElementType{20};
            character["rations"] = ElementType{58};

            RuleActivity buy = activityAt(f.rules, 17.5f, character, "buy.rations");

            EXPECT_FALSE(flag(buy.fields, "locked"));
            ASSERT_EQ(buy.requires.size(), 2u);
            EXPECT_EQ(textOf(buy.requires[1], "label"), "Room for Rations");
            EXPECT_EQ(intOf(buy.requires[1], "current"), 2);
            EXPECT_EQ(intOf(buy.requires[1], "needed"), 1);

            // Two short of full, six more: he ends past the limit
            RuleForecast forecast;
            ASSERT_TRUE(f.rules.forecast(17.5f, character, "buy.rations", 0, forecast));
            EXPECT_EQ(intOf(forecast.atTerm, "rations"), 64);

            for (int held : {60, 64})
            {
                character["rations"] = ElementType{held};

                buy = activityAt(f.rules, 17.5f, character, "buy.rations");

                EXPECT_TRUE(flag(buy.fields, "locked")) << held;
                EXPECT_EQ(intOf(buy.requires[1], "current"), 0) << held;
            }

            // What brings nothing with a limit asks for no room
            EXPECT_TRUE(activityAt(f.rules, 17.5f, character, "mill").requires.empty());
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // A stat with a cap mends toward it, one every two months; a month that takes from it mends
        // nothing and starts the count again. What raises it for good raises the cap with it.
        TEST(rules_test, month_mends_toward_the_cap)
        {
            MockLogger logger;
            RulesFixture f;

            RuleMonth month;

            ASSERT_TRUE(f.rules.month({{"vit", ElementType{9}}, {"vitmax", ElementType{12}}, {"rations", ElementType{5}}}, false, month)) << firstError(f.rules);
            EXPECT_EQ(intOf(month.after, "vit"), 9);
            EXPECT_EQ(intOf(month.after, "vitrest"), 1);
            EXPECT_TRUE(month.hurt.empty());

            ASSERT_EQ(month.caps.size(), 1u);
            EXPECT_EQ(textOf(month.caps[0], "stat"), "vit");
            EXPECT_EQ(intOf(month.caps[0], "most"), 12);

            ASSERT_TRUE(f.rules.month({{"vit", ElementType{9}}, {"vitmax", ElementType{12}}, {"vitrest", ElementType{1}}, {"rations", ElementType{5}}}, false, month));
            EXPECT_EQ(intOf(month.after, "vit"), 10);
            EXPECT_EQ(intOf(month.after, "vitmax"), 12);
            EXPECT_EQ(intOf(month.after, "vitrest"), 0);

            // Starving: nothing mends, the count starts again
            ASSERT_TRUE(f.rules.month({{"vit", ElementType{9}}, {"vitmax", ElementType{12}}, {"vitrest", ElementType{1}}, {"rations", ElementType{0}}}, false, month));
            EXPECT_EQ(intOf(month.after, "vit"), 8);
            EXPECT_EQ(intOf(month.after, "vitmax"), 12);
            EXPECT_EQ(intOf(month.after, "vitrest"), 0);

            // Fed by his work, it mends as on any good month
            ASSERT_TRUE(f.rules.month({{"vit", ElementType{9}}, {"vitmax", ElementType{12}}, {"vitrest", ElementType{1}}, {"rations", ElementType{0}}}, true, month));
            EXPECT_EQ(intOf(month.after, "vit"), 10);

            // Whole: it stays
            ASSERT_TRUE(f.rules.month({{"vit", ElementType{12}}, {"vitmax", ElementType{12}}, {"vitrest", ElementType{1}}}, false, month));
            EXPECT_EQ(intOf(month.after, "vit"), 12);

            // A character from before the cap takes what he has as his most
            ASSERT_TRUE(f.rules.month({{"vit", ElementType{7}}}, false, month));
            EXPECT_EQ(intOf(month.after, "vitmax"), 7);

            // No such stat, no cap
            ASSERT_TRUE(f.rules.month({{"coin", ElementType{3}}}, false, month));
            EXPECT_TRUE(month.caps.empty());

            // The mill brings Vitality: the most it can be rises with it
            RuleForecast forecast;
            ASSERT_TRUE(f.rules.forecast(17.5f, {{"str", ElementType{14}}, {"vit", ElementType{9}}, {"vitmax", ElementType{12}}}, "mill", 0, forecast));
            EXPECT_EQ(intOf(forecast.atTerm, "vit"), 10);
            EXPECT_EQ(intOf(forecast.atTerm, "vitmax"), 13);
        }
        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // A life ends when nothing is left of what it needs: the character as given, not a month later.
        TEST(rules_test, month_death)
        {
            MockLogger logger;
            RulesFixture f;

            RuleMonth month;

            ASSERT_TRUE(f.rules.month({{"vit", ElementType{1}}, {"rations", ElementType{0}}}, false, month));
            EXPECT_EQ(intOf(month.after, "vit"), 0);
            EXPECT_TRUE(month.death.empty());

            ASSERT_TRUE(f.rules.month({{"vit", ElementType{0}}, {"rations", ElementType{0}}}, false, month));
            EXPECT_FALSE(month.death.empty());

            // A character with no such stat at all is not dead of it
            ASSERT_TRUE(f.rules.month({{"coin", ElementType{3}}}, false, month));
            EXPECT_TRUE(month.death.empty());
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // How often an activity was done, and what is left of a limited one, as its row writes it.
        TEST(rules_test, activities_tally)
        {
            MockLogger logger;
            RulesFixture f;

            f.rules.done = {{"carters", ElementType{4}}};

            std::vector<RuleActivity> activities;
            ASSERT_TRUE(f.rules.activities(10.0f, boy(), activities)) << firstError(f.rules);

            for (const auto& a : activities)
            {
                const std::string id = textOf(a.fields, "id");

                if (id == "carters")
                    EXPECT_EQ(textOf(a.fields, "tally"), "DONE 4");
                else if (id == "mill")
                    EXPECT_EQ(textOf(a.fields, "tally"), "DONE 0");
                else if (id == "watch")
                    EXPECT_EQ(textOf(a.fields, "tally"), "DONE 0 \xC2\xB7 1 LEFT");
            }
        }
        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(rules_test, achievements_load_and_shape)
        {
            MockLogger logger;
            RulesFixture f;

            std::vector<RuleAchievement> achievements;
            ASSERT_TRUE(f.rules.achievements(achievements)) << (f.rules.errors.empty() ? "" : f.rules.errors.front());
            ASSERT_GE(achievements.size(), 2u);

            for (const auto& a : achievements)
            {
                for (const char* key : {"id", "name", "entry"})
                    EXPECT_TRUE(a.fields.count(key)) << textOf(a.fields, "id") << " has no " << key;

                ASSERT_FALSE(a.asks.empty()) << textOf(a.fields, "id");

                for (const auto& ask : a.asks)
                {
                    for (const char* key : {"fact", "op", "value"})
                        EXPECT_TRUE(ask.count(key)) << textOf(a.fields, "id") << " asks without " << key;
                }

                for (const auto& give : a.gives)
                {
                    for (const char* key : {"stat", "amount"})
                        EXPECT_TRUE(give.count(key)) << textOf(a.fields, "id") << " gives without " << key;
                }
            }
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // A ceiling, not a target: the forecast runs a few times per month tick. Every run rebuilds
        // the fifty activities of the table, which costs about half a millisecond.
        TEST(rules_test, timing_ceiling)
        {
            MockLogger logger;
            RulesFixture f;

            RuleForecast forecast;
            ASSERT_TRUE(f.rules.forecast(17.4f, sworn(), "mill", 0, forecast));

            const auto start = std::chrono::steady_clock::now();

            for (int i = 0; i < 100; ++i)
                ASSERT_TRUE(f.rules.forecast(17.4f, sworn(), "mill", i % 6, forecast));

            const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();

            EXPECT_LT(ms, 100) << "100 forecasts took " << ms << " ms";
        }
    }
}
