#include "stdafx.h"

#include <sstream>
#include <algorithm>
#include <chrono>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "Core/rulevm.h"

#include "ECS/entitysystem.h"

#include "mocklogger.h"

#include "compiledrules.h"

using namespace chronicle;

// The rule bridge: inputs as globals, outputs read back by path, through the scripts in rules/
// (examples/Chronicle/rules, copied beside the test binary). The fixtures read them compiled once
// a run (compiledrules.h); `Root` is the sources, for the tests of compiling itself.
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

                // The rules compiled once a run, or the sources given (`Root`), compiled here
                explicit RulesFixture(const std::string& root = compiledRules())
                {
                    EXPECT_TRUE(rules.load(&ecs, root)) << (rules.errors.empty() ? "" : rules.errors.front());

                    // A later life: the town has what a second life brings it (the Inn). The tests of
                    // a first life say so themselves
                    rules.lives = 2;
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
                return {{"str", ElementType{6}}, {"dex", ElementType{6}}, {"int", ElementType{6}}, {"vit", ElementType{40}}, {"vitmax", ElementType{40}},
                        {"coin", ElementType{0}}, {"rations", ElementType{12}}};
            }

            // A boy of six, the day a life begins: nothing held
            ElementMap child()
            {
                return {{"str", ElementType{4}}, {"dex", ElementType{4}}, {"int", ElementType{4}}, {"vit", ElementType{35}}, {"vitmax", ElementType{35}},
                        {"coin", ElementType{0}}};
            }

            // The same boy once he has been to the market: it is his to buy at
            ElementMap townsman()
            {
                ElementMap character = boy();
                character["town_known"] = ElementType{1};

                return character;
            }

            // The balance's good Warrior the month he swore to the Keep, at 17.5
            ElementMap sworn()
            {
                return {{"str", ElementType{12}}, {"dex", ElementType{8}}, {"int", ElementType{6}}, {"vit", ElementType{50}}, {"vitmax", ElementType{50}},
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
                for (const char* key : {"id", "group", "name", "glyph", "months", "rank", "each", "path", "enters", "board", "upFront", "fromAge", "finishBy", "locked", "listed", "pathOpen", "closed", "reach"})
                    EXPECT_TRUE(a.fields.count(key)) << textOf(a.fields, "id") << " has no " << key;
            }

            RuleActivity mill = activityAt(f.rules, 7.0f, boy(), "mill");

            EXPECT_EQ(intOf(mill.fields, "months"), 6);
            ASSERT_EQ(mill.gains.size(), 3u);
            EXPECT_EQ(textOf(mill.gains[0], "stat"), "str");
            EXPECT_EQ(textOf(mill.gains[0], "label"), "STR");
            EXPECT_EQ(textOf(mill.gains[0], "name"), "Strength");
            EXPECT_EQ(intOf(mill.gains[0], "amount"), 1);
            EXPECT_TRUE(mill.requires.empty());
            EXPECT_TRUE(mill.costs.empty());
            EXPECT_FALSE(flag(mill.fields, "locked"));
            EXPECT_TRUE(flag(mill.fields, "listed"));
            EXPECT_FALSE(flag(mill.fields, "board"));
            EXPECT_EQ(intOf(mill.fields, "upFront"), 0);

            // The smithy takes boys from 9 with Strength 8: the age first, then what it asks. Too
            // young, it is not listed
            RuleActivity smithy = activityAt(f.rules, 7.0f, boy(), "smithy");

            EXPECT_TRUE(flag(smithy.fields, "locked"));
            EXPECT_FALSE(flag(smithy.fields, "listed"));
            ASSERT_EQ(smithy.requires.size(), 3u);
            EXPECT_EQ(textOf(smithy.requires[0], "stat"), "age");
            EXPECT_EQ(textOf(smithy.requires[0], "label"), "Age");
            EXPECT_EQ(intOf(smithy.requires[0], "current"), 7);
            EXPECT_EQ(intOf(smithy.requires[0], "needed"), 9);
            EXPECT_EQ(textOf(smithy.requires[1], "label"), "Strength");
            EXPECT_EQ(intOf(smithy.requires[1], "current"), 6);
            EXPECT_EQ(intOf(smithy.requires[1], "needed"), 8);
            EXPECT_EQ(textOf(smithy.requires[2], "stat"), "firewood");
            EXPECT_EQ(intOf(smithy.requires[2], "needed"), 6);

            // Of age, the age is no longer asked
            EXPECT_EQ(activityAt(f.rules, 9.0f, boy(), "smithy").requires.size(), 2u);

            // The carters feed him: the rations of their three months, given as he begins
            RuleActivity carters = activityAt(f.rules, 7.0f, boy(), "carters");

            EXPECT_TRUE(flag(carters.fields, "board"));
            EXPECT_EQ(intOf(carters.fields, "upFront"), 3);
            EXPECT_EQ(textOf(carters.fields, "each"), "RATIONS UP FRONT");
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
            EXPECT_TRUE(flag(activityAt(f.rules, 30.0f, townsman(), "buy.rations").fields, "listed"));
            EXPECT_FALSE(flag(activityAt(f.rules, 30.0f, boy(), "carters").fields, "listed"));
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // An activity says when it closes once he has two years or less to begin it, as the world's
        // date (month/year) of the last month he may begin it, and says it as urgent from six months: no row
        // leaves the list without having said so. What no age closes says nothing.
        TEST(rules_test, closing_is_said_ahead)
        {
            MockLogger logger;
            RulesFixture f;

            const float month = 1.0f / 12.0f;

            // The world is at its hundredth month: Year 8, month 5
            f.rules.world = 100;

            // The carters: three months, closed with his prime at 30. Far from it, nothing
            RuleActivity carters = activityAt(f.rules, 20.0f, boy(), "carters");

            EXPECT_EQ(textOf(carters.fields, "until"), "");
            EXPECT_FALSE(flag(carters.fields, "urgent"));

            // Two years to begin it: said. Six months: urgent
            EXPECT_EQ(textOf(activityAt(f.rules, 28.0f - 4.0f * month, boy(), "carters").fields, "until"), "");

            carters = activityAt(f.rules, 28.0f - 3.0f * month, boy(), "carters");

            EXPECT_EQ(textOf(carters.fields, "until"), "CLOSES 5/10");
            EXPECT_FALSE(flag(carters.fields, "urgent"));

            carters = activityAt(f.rules, 29.0f, boy(), "carters");

            EXPECT_EQ(textOf(carters.fields, "until"), "CLOSES 2/9");
            EXPECT_FALSE(flag(carters.fields, "urgent"));

            carters = activityAt(f.rules, 29.5f, boy(), "carters");

            EXPECT_EQ(textOf(carters.fields, "until"), "CLOSES 8/8");
            EXPECT_TRUE(flag(carters.fields, "urgent"));

            // The last month he may begin it, then closed: nothing more to say
            carters = activityAt(f.rules, 29.75f, boy(), "carters");

            EXPECT_EQ(textOf(carters.fields, "until"), "CLOSES THIS MONTH");
            EXPECT_TRUE(flag(carters.fields, "urgent"));
            EXPECT_FALSE(flag(carters.fields, "closed"));

            carters = activityAt(f.rules, 29.75f + month, boy(), "carters");

            EXPECT_TRUE(flag(carters.fields, "closed"));
            EXPECT_EQ(textOf(carters.fields, "until"), "");

            // The Keep swears no one past 20, and its service is 18 months: said from 16 and a half
            EXPECT_EQ(textOf(activityAt(f.rules, 16.0f, boy(), "keep").fields, "until"), "");
            EXPECT_EQ(textOf(activityAt(f.rules, 16.5f, boy(), "keep").fields, "until"), "CLOSES 5/10");

            // From the world's first month: two years on is the first month of Year 2
            f.rules.world = 0;
            EXPECT_EQ(textOf(activityAt(f.rules, 16.5f, boy(), "keep").fields, "until"), "CLOSES 1/2");

            // An old man's work is never closed
            EXPECT_EQ(textOf(activityAt(f.rules, 29.5f, boy(), "tales").fields, "until"), "");
            EXPECT_EQ(textOf(activityAt(f.rules, 29.5f, boy(), "buy.rations").fields, "until"), "");
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // The great step of each age (the way into a path, its proving, its mastery) is listed ahead,
        // whatever he has of what it asks: locked, the age the first thing it asks, so he knows what
        // the years he is in lead to. A proving and a mastery from the milestone before; the three
        // ways into a path from his first day.
        TEST(rules_test, the_great_steps_show_ahead)
        {
            MockLogger logger;
            RulesFixture f;

            const float month = 1.0f / 12.0f;

            // An ordinary activity: listed from the age he may begin it, with three quarters of what it asks
            RuleActivity carters = activityAt(f.rules, 12.0f, boy(), "carters");

            EXPECT_EQ(intOf(carters.fields, "showAge"), 7);
            EXPECT_EQ(intOf(carters.fields, "showFrom"), 75);

            // The ways into a path: from 7, nine years before he may take one
            for (const char* id : {"keep", "collegium", "hand"})
            {
                RuleActivity way = activityAt(f.rules, 7.0f, boy(), id);

                EXPECT_EQ(intOf(way.fields, "showAge"), 7) << id;
                EXPECT_EQ(intOf(way.fields, "showFrom"), 0) << id;
                EXPECT_TRUE(flag(way.fields, "listed")) << id;
                EXPECT_TRUE(flag(way.fields, "locked")) << id;

                ASSERT_FALSE(way.requires.empty()) << id;
                EXPECT_EQ(textOf(way.requires[0], "stat"), "age") << id;
                EXPECT_EQ(intOf(way.requires[0], "needed"), 16) << id;
                EXPECT_EQ(intOf(way.requires[0], "current"), 7) << id;

                // And still there the month before he may take it
                EXPECT_TRUE(flag(activityAt(f.rules, 16.0f - month, boy(), id).fields, "listed")) << id;
            }

            // Sworn at 17: his proving shows, four years ahead; his mastery not yet, and no other path's
            EXPECT_TRUE(flag(activityAt(f.rules, 17.5f, sworn(), "campaign").fields, "listed"));
            EXPECT_TRUE(flag(activityAt(f.rules, 17.5f, sworn(), "campaign").fields, "locked"));
            EXPECT_FALSE(flag(activityAt(f.rules, 17.5f, sworn(), "captain").fields, "listed"));
            EXPECT_FALSE(flag(activityAt(f.rules, 17.5f, sworn(), "harrow").fields, "listed"));
            EXPECT_FALSE(flag(activityAt(f.rules, 17.5f, sworn(), "rob").fields, "listed"));

            // At 21 his mastery shows, five years ahead
            EXPECT_TRUE(flag(activityAt(f.rules, 21.0f, sworn(), "captain").fields, "listed"));
            EXPECT_TRUE(flag(activityAt(f.rules, 21.0f, sworn(), "captain").fields, "locked"));

            // A boy of no path sees no path's proving
            EXPECT_FALSE(flag(activityAt(f.rules, 17.5f, boy(), "campaign").fields, "listed"));
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // Who took none of the three classes in time is left a fourth, and no one else is: the way to
        // the Greenwood has no row while another way in can still be begun, nor for a man on his way
        // into one, nor for one of a class. From the month the last of them closes it asks nothing,
        // and stays open until old age. Its years have their own works, a proving and a mastery.
        TEST(rules_test, the_renegade_is_for_who_missed_the_others)
        {
            MockLogger logger;
            RulesFixture f;

            const float month = 1.0f / 12.0f;

            // The Hidden Hand's eighteen months are begun by 19 and a half at the latest: until then
            // there is still a class to take, and no Greenwood
            EXPECT_FALSE(flag(activityAt(f.rules, 16.0f, boy(), "greenwood").fields, "listed"));
            EXPECT_TRUE(flag(activityAt(f.rules, 19.5f, boy(), "hand").fields, "listed"));
            EXPECT_FALSE(flag(activityAt(f.rules, 19.5f, boy(), "greenwood").fields, "listed"));

            // The month after, the three are closed and it is there, asking nothing of a boy who has nothing
            for (const char* id : {"keep", "collegium", "hand"})
                EXPECT_FALSE(flag(activityAt(f.rules, 19.5f + month, boy(), id).fields, "listed")) << id;

            RuleActivity way = activityAt(f.rules, 19.5f + month, boy(), "greenwood");

            EXPECT_TRUE(flag(way.fields, "listed"));
            EXPECT_FALSE(flag(way.fields, "locked"));
            EXPECT_TRUE(flag(way.fields, "enters"));
            EXPECT_TRUE(way.requires.empty());
            EXPECT_EQ(textOf(way.fields, "path"), "renegade");
            EXPECT_EQ(textOf(way.fields, "group"), "A class");
            EXPECT_FALSE(textOf(way.fields, "opens").empty());

            // Not for a man who began the Hand's trust in its last month and is still earning it,
            // whatever the date; another work at hand changes nothing
            f.rules.running = "hand";
            EXPECT_FALSE(flag(activityAt(f.rules, 19.5f + month, boy(), "greenwood").fields, "listed"));

            f.rules.running = "mill";
            EXPECT_TRUE(flag(activityAt(f.rules, 19.5f + month, boy(), "greenwood").fields, "listed"));

            f.rules.running = "";

            // Never for who has a class; open until a term of it no longer fits before old age
            EXPECT_FALSE(flag(activityAt(f.rules, 20.0f, sworn(), "greenwood").fields, "listed"));
            EXPECT_TRUE(flag(activityAt(f.rules, 29.0f, boy(), "greenwood").fields, "listed"));
            EXPECT_FALSE(flag(activityAt(f.rules, 29.0f + month, boy(), "greenwood").fields, "listed"));

            // One of the Greenwood: its works are his, no other class's, and its great steps show ahead
            ElementMap outlaw = boy();
            outlaw["renegade"] = ElementType{1};

            f.rules.done = {{"greenwood", ElementType{1}}};

            for (const char* id : {"poach", "burners"})
            {
                RuleActivity work = activityAt(f.rules, 21.0f, outlaw, id);

                EXPECT_TRUE(flag(work.fields, "listed")) << id;
                EXPECT_FALSE(flag(work.fields, "locked")) << id;
            }

            // The works of a class come in an order: the outlaws once he has poached, the road
            // once he has trained with them
            EXPECT_FALSE(flag(activityAt(f.rules, 21.0f, outlaw, "outlaws").fields, "listed"));

            f.rules.done = {{"greenwood", ElementType{1}}, {"poach", ElementType{1}}};

            ElementMap poacher = outlaw;
            poacher["arms"] = ElementType{2};
            poacher["stealth"] = ElementType{3};

            EXPECT_TRUE(flag(activityAt(f.rules, 21.0f, poacher, "outlaws").fields, "listed"));
            EXPECT_FALSE(flag(activityAt(f.rules, 21.0f, poacher, "waylay").fields, "listed"));

            f.rules.done["outlaws"] = ElementType{1};

            EXPECT_TRUE(flag(activityAt(f.rules, 21.0f, poacher, "waylay").fields, "listed"));

            for (const char* id : {"greenwood", "keep", "serve", "study", "run"})
                EXPECT_FALSE(flag(activityAt(f.rules, 21.0f, outlaw, id).fields, "listed")) << id;

            EXPECT_FALSE(flag(activityAt(f.rules, 21.0f, boy(), "poach").fields, "listed"));

            EXPECT_TRUE(flag(activityAt(f.rules, 21.0f, outlaw, "gaol").fields, "listed"));
            EXPECT_TRUE(flag(activityAt(f.rules, 21.0f, outlaw, "gaol").fields, "locked"));
            EXPECT_FALSE(flag(activityAt(f.rules, 21.0f, outlaw, "woodking").fields, "listed"));
            EXPECT_TRUE(flag(activityAt(f.rules, 23.0f, outlaw, "woodking").fields, "listed"));

            // And it is what is said of him
            RuleEpitaph epitaph;

            ASSERT_TRUE(f.rules.epitaph(28.0f, {{"coin", ElementType{5}}, {"renegade", ElementType{1}}}, {}, epitaph)) << firstError(f.rules);
            EXPECT_EQ(epitaph.story[0], "He took to the Greenwood, and lived outside the town's law.");

            ASSERT_TRUE(f.rules.epitaph(28.0f, {{"coin", ElementType{5}}, {"renegade", ElementType{1}}, {"wood_king", ElementType{1}}}, {}, epitaph));
            EXPECT_EQ(epitaph.story[0], "He held the Greenwood against the Keep, and the songs are his.");
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
            EXPECT_FALSE(flag(activityAt(f.rules, 9.5f, boy(), "chapel").fields, "listed"));

            ElementMap lettered = boy();
            lettered["letters"] = ElementType{1};

            EXPECT_TRUE(flag(activityAt(f.rules, 9.5f, lettered, "chapel").fields, "listed"));

            // A strong boy at 16 sees the Keep, though he cannot pay its coin yet
            const ElementMap squire = {{"str", ElementType{14}}, {"arms", ElementType{3}}, {"discipline", ElementType{2}}, {"int", ElementType{6}}};

            RuleActivity keep = activityAt(f.rules, 16.0f, squire, "keep");

            EXPECT_EQ(intOf(keep.fields, "reach"), 100);
            EXPECT_TRUE(flag(keep.fields, "listed"));
            EXPECT_TRUE(flag(keep.fields, "locked"));

            // The Collegium is far from him, and listed all the same: a way into a path shows whatever he has
            RuleActivity collegium = activityAt(f.rules, 16.0f, squire, "collegium");

            EXPECT_LT(intOf(collegium.fields, "reach"), 75);
            EXPECT_TRUE(flag(collegium.fields, "listed"));
            EXPECT_TRUE(flag(collegium.fields, "locked"));

            // What asks nothing is always in reach
            EXPECT_EQ(intOf(activityAt(f.rules, 7.0f, boy(), "carters").fields, "reach"), 100);
            EXPECT_TRUE(flag(activityAt(f.rules, 7.0f, townsman(), "buy.rations").fields, "listed"));
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // Edric asks Arms 1 of a boy the Watch knows, the Keep Strength 13 of one Edric speaks for; the
        // Hidden Hand asks a friend on the street or one at the night market, either will do.
        TEST(rules_test, activities_eased_and_either)
        {
            MockLogger logger;
            RulesFixture f;

            ElementMap boy13 = {{"str", ElementType{10}}, {"arms", ElementType{1}}};

            RuleActivity edric = activityAt(f.rules, 13.25f, boy13, "edric");

            ASSERT_EQ(edric.requires.size(), 2u);
            EXPECT_EQ(intOf(edric.requires[1], "needed"), 2);
            EXPECT_TRUE(flag(edric.fields, "locked"));

            boy13["watch_known"] = ElementType{1};
            edric = activityAt(f.rules, 13.25f, boy13, "edric");

            EXPECT_EQ(intOf(edric.requires[1], "needed"), 1);
            EXPECT_FALSE(flag(edric.fields, "locked"));

            ElementMap squire = {{"str", ElementType{13}}, {"arms", ElementType{3}}, {"discipline", ElementType{2}}, {"coin", ElementType{10}}};

            EXPECT_TRUE(flag(activityAt(f.rules, 16.0f, squire, "keep").fields, "locked"));

            squire["edric_support"] = ElementType{1};
            RuleActivity keep = activityAt(f.rules, 16.0f, squire, "keep");

            EXPECT_EQ(intOf(keep.requires[0], "needed"), 13);
            EXPECT_FALSE(flag(keep.fields, "locked"));

            ElementMap cutpurse = {{"dex", ElementType{14}}, {"stealth", ElementType{2}}, {"guile", ElementType{2}}, {"coin", ElementType{15}}};

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

            // His age to the month, after the figure of his whole years
            EXPECT_EQ(textOf(headline, "ageUnit"), "YEARS 6 MONTHS OLD");

            ASSERT_TRUE(f.rules.milestones(9.0f + 1.0f / 12.0f, milestones, next, &headline));
            EXPECT_EQ(textOf(headline, "ageText"), "9.0");
            EXPECT_EQ(textOf(headline, "ageUnit"), "YEARS 1 MONTH OLD");

            ASSERT_TRUE(f.rules.milestones(9.0f + 10.0f / 12.0f, milestones, next, &headline));
            EXPECT_EQ(textOf(headline, "ageUnit"), "YEARS 10 MONTHS OLD");

            ASSERT_TRUE(f.rules.milestones(9.0f, milestones, next, &headline));
            EXPECT_EQ(textOf(headline, "ageUnit"), "YEARS OLD");

            // The world's date beside it: Year 0 at its first month, in the winter
            EXPECT_EQ(textOf(headline, "date"), "YEAR 0");
            EXPECT_EQ(textOf(headline, "dateNote"), "MONTH 1 \xC2\xB7 WINTER \xC2\xB7 BELLMOOR");

            // Ten years and six months on, whoever is living then
            f.rules.world = 126;
            ASSERT_TRUE(f.rules.milestones(9.0f, milestones, next, &headline));
            EXPECT_EQ(textOf(headline, "date"), "YEAR 10");
            EXPECT_EQ(textOf(headline, "dateNote"), "MONTH 7 \xC2\xB7 SUMMER \xC2\xB7 BELLMOOR");

            // The last month of a year, then the first of the next
            f.rules.world = 23;
            ASSERT_TRUE(f.rules.milestones(9.0f, milestones, next, &headline));
            EXPECT_EQ(textOf(headline, "date"), "YEAR 1");
            EXPECT_EQ(textOf(headline, "dateNote"), "MONTH 12 \xC2\xB7 AUTUMN \xC2\xB7 BELLMOOR");

            f.rules.world = 24;
            ASSERT_TRUE(f.rules.milestones(9.0f, milestones, next, &headline));
            EXPECT_EQ(textOf(headline, "date"), "YEAR 2");
            EXPECT_EQ(textOf(headline, "dateNote"), "MONTH 1 \xC2\xB7 WINTER \xC2\xB7 BELLMOOR");

            f.rules.world = 0;

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
        // The Collegium's door says what it asks now (no coin up front: the fees of its courses,
        // paid as he follows them), and is shut to who left it or was sent away, though no term
        // of it is done.
        TEST(rules_test, the_collegium_door_is_shut_to_who_left_it)
        {
            MockLogger logger;
            RulesFixture f;

            RecordList windows;
            ASSERT_TRUE(f.rules.windows(12.0f, boy(), windows)) << firstError(f.rules);

            ASSERT_NE(byId(windows, "collegium"), nullptr);
            const std::string asks = textOf(*byId(windows, "collegium"), "note");

            EXPECT_EQ(textOf(*byId(windows, "collegium"), "state"), "upcoming");
            EXPECT_EQ(asks.find("35"), std::string::npos) << asks;
            EXPECT_NE(asks.find("fees"), std::string::npos) << asks;

            // At seventeen the door stands open to a man of no class; not to one who left it
            ElementMap man = boy();
            man["int"] = ElementType{12};
            man["letters"] = ElementType{3};
            man["lore"] = ElementType{2};

            ASSERT_TRUE(f.rules.windows(17.0f, man, windows));
            ASSERT_NE(byId(windows, "collegium"), nullptr);
            EXPECT_EQ(textOf(*byId(windows, "collegium"), "state"), "open");

            man["dropout.collegium"] = ElementType{1};
            man["fire_course"] = ElementType{1};

            ASSERT_TRUE(f.rules.windows(17.0f, man, windows));
            ASSERT_NE(byId(windows, "collegium"), nullptr);
            EXPECT_EQ(textOf(*byId(windows, "collegium"), "state"), "closed");
            EXPECT_EQ(intOf(*byId(windows, "collegium"), "attempts"), 0);
            EXPECT_EQ(textOf(*byId(windows, "collegium"), "note"), "He left the Collegium, and it does not take back who left it. That door is shut.");

            // The other doors of his age stand as they did
            ASSERT_NE(byId(windows, "keep"), nullptr);
            EXPECT_EQ(textOf(*byId(windows, "keep"), "state"), "open");
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // The Collegium asks the coin of its first days (two days of fees), and takes nothing up
        // front: shut with 5, open with 6, and its window says so in words.
        TEST(rules_test, the_collegium_asks_the_coin_of_its_first_days)
        {
            MockLogger logger;
            RulesFixture f;

            ElementMap man = boy();
            man["int"] = ElementType{12};
            man["letters"] = ElementType{3};
            man["lore"] = ElementType{2};
            man["coin"] = ElementType{5};

            RuleActivity poor = activityAt(f.rules, 16.0f, man, "collegium");

            EXPECT_TRUE(flag(poor.fields, "listed"));
            EXPECT_TRUE(flag(poor.fields, "locked"));

            bool asked = false;

            for (const auto& r : poor.requires)
            {
                if (textOf(r, "stat") == "coin")
                {
                    asked = true;
                    EXPECT_EQ(intOf(r, "needed"), 6);
                }
            }

            EXPECT_TRUE(asked);
            EXPECT_TRUE(poor.costs.empty());

            man["coin"] = ElementType{6};

            RuleActivity able = activityAt(f.rules, 16.0f, man, "collegium");

            EXPECT_FALSE(flag(able.fields, "locked"));
            EXPECT_TRUE(able.costs.empty());

            RecordList windows;
            ASSERT_TRUE(f.rules.windows(12.0f, boy(), windows)) << firstError(f.rules);
            ASSERT_NE(byId(windows, "collegium"), nullptr);
            EXPECT_NE(textOf(*byId(windows, "collegium"), "note").find("coin for its first days"), std::string::npos) << textOf(*byId(windows, "collegium"), "note");
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // No event of his life opens over the Collegium's term: from 16, when it may first be begun,
        // to 22, when a term begun by 20 would end, no answer of the Flood, the Levy, the Plague
        // Year or the succession is listed (the Levy's close before he is 16, the Plague's open at
        // 23): an event never comes over a story.
        TEST(rules_test, no_life_event_opens_inside_a_collegium_term)
        {
            MockLogger logger;
            RulesFixture f;

            ElementMap man = boy();
            man["int"] = ElementType{12};
            man["letters"] = ElementType{3};
            man["lore"] = ElementType{2};
            man["coin"] = ElementType{40};

            int events = 0;

            for (int month = 16 * 12; month < 22 * 12; ++month)
            {
                const float age = static_cast<float>(month) / 12.0f;

                std::vector<RuleActivity> rows;
                ASSERT_TRUE(f.rules.activities(age, man, rows)) << firstError(f.rules);

                for (const auto& a : rows)
                {
                    if (textOf(a.fields, "event").empty())
                        continue;

                    ++events;
                    EXPECT_FALSE(flag(a.fields, "listed")) << textOf(a.fields, "id") << " at " << month / 12 << "." << month % 12;
                }
            }

            // (the rows are there, every one shut: the test looked at them)
            EXPECT_GT(events, 0);

            // And the Levy's are listed the year before
            std::vector<RuleActivity> rows;
            ASSERT_TRUE(f.rules.activities(15.0f, man, rows));

            bool levy = false;

            for (const auto& a : rows)
                levy = levy or (textOf(a.fields, "event") == "levy" and flag(a.fields, "listed"));

            EXPECT_TRUE(levy);
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
            EXPECT_EQ(intOf(forecast.atTerm, "vit"), 45);
            EXPECT_EQ(intOf(forecast.atTerm, "vitmax"), 45);
            EXPECT_EQ(intOf(forecast.atTerm, "coin"), 8);
            EXPECT_EQ(intOf(forecast.atTerm, "dex"), 6);   // Untouched stats carried over
            EXPECT_FLOAT_EQ(forecast.percent, 0.0f);
            EXPECT_EQ(forecast.months, 6);
            // Where he is in it, and no more: what it brings is its gloss's to say
            EXPECT_EQ(forecast.caption, "MONTH 0 OF 6");
            EXPECT_EQ(forecast.error, "");

            ASSERT_EQ(forecast.entries.size(), 1u);
            EXPECT_EQ(textOf(forecast.entries[0], "text"), "Help at the Mill");
            EXPECT_EQ(textOf(forecast.entries[0], "kind"), "gain");

            ASSERT_TRUE(f.rules.forecast(7.0f, boy(), "mill", 3, forecast));
            EXPECT_FLOAT_EQ(forecast.percent, 50.0f);
            EXPECT_GT(forecast.toward, forecast.percent);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // A cost is taken when the activity begins (atStart), not again at its term, with the rations
        // of its months given then by a work that feeds him; an activity done at once lands its
        // gains on what is left.
        TEST(rules_test, forecast_costs_at_start)
        {
            MockLogger logger;
            RulesFixture f;

            ElementMap squire = {{"str", ElementType{12}}, {"arms", ElementType{3}}, {"discipline", ElementType{2}}, {"coin", ElementType{30}}, {"rations", ElementType{4}}};

            RuleForecast forecast;
            ASSERT_TRUE(f.rules.forecast(16.0f, squire, "keep", 0, forecast)) << firstError(f.rules);

            EXPECT_EQ(intOf(forecast.atStart, "coin"), 20);
            EXPECT_EQ(intOf(forecast.atStart, "rations"), 4 + 18);
            EXPECT_EQ(intOf(forecast.atTerm, "coin"), 30);
            EXPECT_EQ(intOf(forecast.atTerm, "keep_oath"), 1);
            EXPECT_EQ(intOf(forecast.atTerm, "arms"), 4);

            // Harrow asks coin, rations and reagents up front, and feeds him its eighteen months
            ElementMap scholar = {{"collegium", ElementType{1}}, {"harrow_fragments", ElementType{1}}, {"lore", ElementType{6}}, {"arcana", ElementType{3}},
                                  {"coin", ElementType{25}}, {"rations", ElementType{10}}, {"reagents", ElementType{4}}};

            ASSERT_TRUE(f.rules.forecast(24.0f, scholar, "harrow", 0, forecast));
            EXPECT_EQ(intOf(forecast.atStart, "coin"), 5);
            EXPECT_EQ(intOf(forecast.atStart, "rations"), 2 + 18);
            EXPECT_EQ(intOf(forecast.atStart, "reagents"), 0);

            // Done at once: what it brings, on what is left
            ASSERT_TRUE(f.rules.forecast(17.4f, {{"coin", ElementType{12}}}, "buy.rations", 0, forecast));
            EXPECT_EQ(forecast.months, 0);
            EXPECT_FLOAT_EQ(forecast.percent, 100.0f);
            EXPECT_EQ(intOf(forecast.atStart, "coin"), 7);
            EXPECT_EQ(intOf(forecast.atTerm, "coin"), 7);
            EXPECT_EQ(intOf(forecast.atTerm, "rations"), 6);
            ASSERT_EQ(forecast.entries.size(), 1u);
            // An amount holds to its stat by a no-break space; a plain space parts the two
            EXPECT_EQ(textOf(forecast.entries[0], "figure"), "+6\xC2\xA0" "rations \xE2\x88\x92" "5\xC2\xA0" "coin");
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

            ASSERT_TRUE(f.rules.forecast(12.0f, {{"vit", ElementType{150}}, {"vitmax", ElementType{150}}}, "mill", 0, forecast));
            EXPECT_EQ(intOf(forecast.atTerm, "vit"), 150);
            EXPECT_EQ(intOf(forecast.atTerm, "vitmax"), 150);

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
            EXPECT_EQ(textOf(forecast.entries[1], "figure"), "+1\xC2\xA0" "renown");

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
            EXPECT_EQ(textOf(forecast.entries[1], "figure"), "+1\xC2\xA0" "edric");
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

            // The town to buy in, the coin it costs, then room for what it brings
            ASSERT_EQ(buy.requires.size(), 3u);
            EXPECT_EQ(textOf(buy.requires[0], "stat"), "town_known");
            EXPECT_EQ(textOf(buy.requires[0], "label"), "The town");
            EXPECT_EQ(textOf(buy.requires[1], "stat"), "coin");
            EXPECT_EQ(intOf(buy.requires[1], "current"), 3);
            EXPECT_EQ(intOf(buy.requires[1], "needed"), 5);

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
        // The basic resources are held in their own ledger group, each with a limit that a store
        // raises; a walk still finds them, and berries are still sold.
        TEST(rules_test, basic_resources_are_held_with_their_limit)
        {
            MockLogger logger;
            RulesFixture f;

            ElementMap character = {{"water", ElementType{12}}, {"stone", ElementType{3}}};

            RuleMonth month;
            ASSERT_TRUE(f.rules.month(8.0f, character, month)) << firstError(f.rules);

            const ElementMap* water = byId(month.rows, "water");
            ASSERT_NE(water, nullptr);
            EXPECT_EQ(textOf(*water, "group"), "resources");
            EXPECT_EQ(intOf(*water, "limit"), 40);

            const ElementMap* stone = byId(month.rows, "stone");
            ASSERT_NE(stone, nullptr);
            EXPECT_EQ(textOf(*stone, "group"), "resources");

            character["store.water"] = ElementType{20};

            RuleMonth stored;
            ASSERT_TRUE(f.rules.month(8.0f, character, stored)) << firstError(f.rules);

            ASSERT_NE(byId(stored.rows, "water"), nullptr);
            EXPECT_EQ(intOf(*byId(stored.rows, "water"), "limit"), 60);

            EXPECT_EQ(textOf(activityAt(f.rules, 12.0f, townsman(), "sell.berries").fields, "id"), "sell.berries");
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // A month eats a ration, and says so as a rate, whatever he is at: a work that feeds him gave
        // its rations as he began it. With none held, his Vitality pays.
        TEST(rules_test, month_decays_at_any_work)
        {
            MockLogger logger;
            RulesFixture f;

            ElementMap character = {{"vit", ElementType{12}}, {"coin", ElementType{3}}, {"rations", ElementType{5}}};

            RuleMonth month;
            ASSERT_TRUE(f.rules.month(20.0f, character, month)) << firstError(f.rules);

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

            // A standing, a title or a tie is had or not: the rules say so. What is counted is not one
            for (const char* id : {"keep_oath", "collegium", "hidden_hand", "captain", "mage_of_harrow", "master_thief", "mara_student", "watch_known", "crew_ready"})
            {
                ASSERT_NE(byId(month.rows, id), nullptr) << id;
                EXPECT_TRUE(flag(*byId(month.rows, id), "title")) << id;
            }

            for (const char* id : {"coin", "rations", "reagents", "favors"})
                EXPECT_FALSE(flag(*byId(month.rows, id), "title")) << id;

            EXPECT_EQ(textOf(*byId(month.rows, "mara_student"), "name"), "Mara's pupil");
            EXPECT_EQ(textOf(*byId(month.rows, "mara_student"), "groupLabel"), "TIES");

            // And its gloss counts nothing and states no limit: its name, and what it is
            for (const auto& gloss : month.glosses)
            {
                const std::string id = textOf(gloss.fields, "id");

                if (id == "keep_oath" or id == "mara_student")
                {
                    EXPECT_TRUE(gloss.rows.empty()) << id;
                    EXPECT_EQ(textOf(gloss.fields, "footnote"), "") << id;
                }

                if (id == "coin")
                    EXPECT_FALSE(gloss.rows.empty());
            }

            // The last ration eaten is a fed month: no Vitality taken
            ASSERT_TRUE(f.rules.month(20.0f, {{"vit", ElementType{12}}, {"rations", ElementType{1}}}, month));
            EXPECT_EQ(intOf(month.after, "rations"), 0);
            EXPECT_EQ(intOf(month.after, "vit"), 12);

            // With none, his Vitality pays, at any work: the month has nothing to spare him
            ASSERT_TRUE(f.rules.month(20.0f, {{"vit", ElementType{12}}, {"rations", ElementType{0}}}, month));
            EXPECT_EQ(intOf(month.after, "vit"), 7);
            EXPECT_EQ(month.hurt, std::vector<std::string>{"vit"});

            // A boy who never held any eats none
            ASSERT_TRUE(f.rules.month(20.0f, {{"vit", ElementType{12}}}, month));
            EXPECT_EQ(intOf(month.after, "vit"), 12);
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

            ASSERT_TRUE(f.rules.month(20.0f, {{"vit", ElementType{12}}, {"rations", ElementType{1}}}, month));
            EXPECT_EQ(intOf(month.after, "rations"), 0);
            EXPECT_EQ(intOf(month.after, "vit"), 12);
            ASSERT_EQ(month.entries.size(), 1u);
            EXPECT_EQ(textOf(month.entries[0], "kind"), "loss");

            EXPECT_TRUE(month.hurt.empty());

            ASSERT_TRUE(f.rules.month(20.0f, {{"vit", ElementType{12}}, {"rations", ElementType{0}}}, month));
            EXPECT_EQ(intOf(month.after, "rations"), 0);
            EXPECT_EQ(intOf(month.after, "vit"), 7);
            EXPECT_TRUE(month.entries.empty());

            // A month that took from what the life hangs on says so
            ASSERT_EQ(month.hurt.size(), 1u);
            EXPECT_EQ(month.hurt[0], "vit");

            // Nothing goes below zero
            ASSERT_TRUE(f.rules.month(20.0f, {{"vit", ElementType{0}}, {"rations", ElementType{0}}}, month));
            EXPECT_EQ(intOf(month.after, "vit"), 0);

            ASSERT_TRUE(f.rules.month(20.0f, {{"vit", ElementType{12}}}, month));
            EXPECT_EQ(intOf(month.after, "vit"), 12);
            EXPECT_EQ(month.after.count("rations"), 0u);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // What he lives on running out comes with what to do about it: one sentence while it runs
        // low, another once it is gone.
        TEST(rules_test, month_says_what_to_do)
        {
            MockLogger logger;
            RulesFixture f;

            RuleMonth month;

            ASSERT_TRUE(f.rules.month(20.0f, {{"vit", ElementType{12}}, {"rations", ElementType{12}}}, month));
            EXPECT_EQ(month.warning, "");
            EXPECT_EQ(month.advice, "");

            ASSERT_TRUE(f.rules.month(20.0f, {{"vit", ElementType{12}}, {"rations", ElementType{3}}}, month));
            EXPECT_EQ(month.warning, "RATIONS FOR 3 MONTHS");
            EXPECT_NE(month.advice.find("running low"), std::string::npos);

            const std::string low = month.advice;

            // The same sentence down to the last one: said once, not every month
            ASSERT_TRUE(f.rules.month(20.0f, {{"vit", ElementType{12}}, {"rations", ElementType{1}}}, month));
            EXPECT_EQ(month.warning, "RATIONS FOR 1 MONTH");
            EXPECT_EQ(month.advice, low);

            ASSERT_TRUE(f.rules.month(20.0f, {{"vit", ElementType{12}}, {"rations", ElementType{0}}}, month));
            EXPECT_EQ(month.warning, "NO RATIONS LEFT");
            EXPECT_NE(month.advice.find("No rations left"), std::string::npos);
            EXPECT_NE(month.advice, low);

            // What it says to do: the market, or work that gives its rations up front
            EXPECT_EQ(low, "Rations are running low: buy some at the market, or take work that gives rations up front");
            EXPECT_EQ(month.advice, "No rations left: his Vitality pays. Buy some at the market, or take work that gives rations up front");

            // Two left are two months, at any work: said
            ASSERT_TRUE(f.rules.month(20.0f, {{"vit", ElementType{12}}, {"rations", ElementType{2}}}, month));
            EXPECT_EQ(month.warning, "RATIONS FOR 2 MONTHS");
            EXPECT_EQ(month.advice, low);
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
            ASSERT_TRUE(f.rules.month(20.0f, {{"vit", ElementType{12}}, {"coin", ElementType{3}}, {"rations", ElementType{5}}}, month)) << firstError(f.rules);

            const RuleGloss* coin = glossOf(month, "coin");
            const RuleGloss* rations = glossOf(month, "rations");

            ASSERT_NE(coin, nullptr);
            ASSERT_NE(rations, nullptr);

            EXPECT_EQ(textOf(coin->fields, "title"), "Coin");
            // No limit: what he holds, and no row for a limit there is not
            EXPECT_EQ(rowOf(*coin, "Held"), "3");
            EXPECT_EQ(rowOf(*coin, "Limit"), "<none>");

            // A limit: what he holds over the most he can, on one row
            EXPECT_EQ(rowOf(*rations, "Held"), "5/60");
            EXPECT_EQ(rowOf(*rations, "Limit"), "<none>");
            EXPECT_EQ(rowOf(*rations, "A month uses"), "1");
            EXPECT_EQ(rowOf(*rations, "Lasts"), "5 mo");
            EXPECT_EQ(rowOf(*rations, "With none, Vitality"), "\xE2\x88\x92" "5 / mo");
            EXPECT_FALSE(textOf(rations->fields, "text").empty());

            EXPECT_EQ(intOf(*byId(month.rows, "rations"), "limit"), 60);
            EXPECT_EQ(intOf(*byId(month.rows, "coin"), "limit"), 0);

            // Written time: held with his coin, in seconds, an hour of it at most. It is the one
            // holding that runs the months faster, and a month takes none of it
            ASSERT_TRUE(f.rules.month(20.0f, {{"vit", ElementType{12}}, {"rations", ElementType{5}}, {"written", ElementType{754}}}, month));

            const ElementMap* written = byId(month.rows, "written");
            ASSERT_NE(written, nullptr);
            EXPECT_EQ(textOf(*written, "group"), "purse");
            EXPECT_EQ(intOf(*written, "limit"), 3600);
            EXPECT_EQ(intOf(*written, "haste"), 3);
            EXPECT_EQ(textOf(*written, "rate"), "");
            EXPECT_EQ(intOf(*byId(month.rows, "coin"), "haste"), 0);
            EXPECT_EQ(intOf(month.after, "written"), 754);

            ASSERT_NE(glossOf(month, "written"), nullptr);
            EXPECT_TRUE(flag(*written, "always"));
            EXPECT_FALSE(flag(*byId(month.rows, "coin"), "always"));
            EXPECT_EQ(rowOf(*glossOf(month, "written"), "Held"), "754/3600");
            EXPECT_EQ(intOf(*written, "aMonth"), 7);
            EXPECT_EQ(rowOf(*glossOf(month, "written"), "A month takes"), "7 s");
            EXPECT_EQ(rowOf(*glossOf(month, "written"), "Runs faster"), "107 mo");

            // A month uses his one ration at any work: no word of meals in the gloss
            ASSERT_TRUE(f.rules.month(20.0f, {{"vit", ElementType{12}}, {"rations", ElementType{5}}}, month));
            EXPECT_EQ(textOf(glossOf(month, "rations")->fields, "footnote").find("MEALS"), std::string::npos);
            EXPECT_EQ(rowOf(*glossOf(month, "rations"), "A month uses"), "1");

            ASSERT_TRUE(f.rules.month(20.0f, {{"vit", ElementType{12}}, {"rations", ElementType{60}}}, month));
            EXPECT_NE(textOf(glossOf(month, "rations")->fields, "footnote").find("FULL"), std::string::npos);

            // What he holds past the limit is his: the month only eats from it
            ASSERT_TRUE(f.rules.month(20.0f, {{"vit", ElementType{12}}, {"rations", ElementType{64}}}, month));
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

            ElementMap character = townsman();
            character["coin"] = ElementType{20};
            character["rations"] = ElementType{58};

            RuleActivity buy = activityAt(f.rules, 17.5f, character, "buy.rations");

            EXPECT_FALSE(flag(buy.fields, "locked"));
            ASSERT_EQ(buy.requires.size(), 3u);
            EXPECT_EQ(textOf(buy.requires[2], "label"), "Room for Rations");
            EXPECT_EQ(intOf(buy.requires[2], "current"), 2);
            EXPECT_EQ(intOf(buy.requires[2], "needed"), 1);

            // Two short of full, six more: he ends past the limit
            RuleForecast forecast;
            ASSERT_TRUE(f.rules.forecast(17.5f, character, "buy.rations", 0, forecast));
            EXPECT_EQ(intOf(forecast.atTerm, "rations"), 64);

            for (int held : {60, 64})
            {
                character["rations"] = ElementType{held};

                buy = activityAt(f.rules, 17.5f, character, "buy.rations");

                EXPECT_TRUE(flag(buy.fields, "locked")) << held;
                EXPECT_EQ(intOf(buy.requires[2], "current"), 0) << held;
            }

            // What brings nothing with a limit asks for no room
            EXPECT_TRUE(activityAt(f.rules, 17.5f, character, "mill").requires.empty());
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // A child gathers by hand: the rows bring a basic and count a scavenge, and are closed
        // while he is full of what they bring. The stones wait for eight.
        TEST(rules_test, scavenging_gathers_by_hand)
        {
            MockLogger logger;
            RulesFixture f;

            RuleActivity row = activityAt(f.rules, 7.5f, boy(), "scavenge.water");

            ASSERT_EQ(row.gains.size(), 2u);
            EXPECT_EQ(textOf(row.gains[0], "stat"), "water");
            EXPECT_EQ(intOf(row.gains[0], "amount"), 8);
            EXPECT_EQ(textOf(row.gains[1], "stat"), "scavenged");
            EXPECT_EQ(intOf(row.gains[1], "amount"), 1);
            EXPECT_EQ(intOf(row.fields, "months"), 1);
            EXPECT_FALSE(flag(row.fields, "locked"));

            ElementMap full = boy();
            full["water"] = ElementType{40};
            row = activityAt(f.rules, 7.5f, full, "scavenge.water");

            EXPECT_TRUE(flag(row.fields, "locked"));
            bool roomAsked = false;

            for (const auto& req : row.requires)
            {
                if (textOf(req, "label") == "Room for Water")
                    roomAsked = true;
            }

            EXPECT_TRUE(roomAsked);

            EXPECT_FALSE(flag(activityAt(f.rules, 7.5f, boy(), "scavenge.stone").fields, "listed"));
            EXPECT_TRUE(flag(activityAt(f.rules, 8.5f, boy(), "scavenge.stone").fields, "listed"));
            EXPECT_FALSE(flag(activityAt(f.rules, 8.5f, boy(), "scavenge.stone").fields, "locked"));
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // The first familiar waits for the second scavenge, and an unknown kind is not listed
        TEST(rules_test, the_first_familiar_comes_after_two_scavenges)
        {
            MockLogger logger;
            RulesFixture f;

            ElementMap one = boy();
            one["scavenged"] = ElementType{1};
            one["berries"] = ElementType{6};
            EXPECT_FALSE(flag(activityAt(f.rules, 7.5f, one, "bind.sprite").fields, "listed"));

            ElementMap two = one;
            two["scavenged"] = ElementType{2};
            RuleActivity row = activityAt(f.rules, 7.5f, two, "bind.sprite");
            EXPECT_TRUE(flag(row.fields, "listed"));
            EXPECT_FALSE(flag(row.fields, "locked"));
            EXPECT_EQ(textOf(row.fields, "group"), "Familiars");
            EXPECT_EQ(textOf(row.gains[0], "stat"), "fam.sprite");
        }

        TEST(rules_test, an_unknown_kind_is_not_listed)
        {
            MockLogger logger;
            RulesFixture f;

            ElementMap character = boy();
            character["scavenged"] = ElementType{2};
            EXPECT_FALSE(flag(activityAt(f.rules, 7.5f, character, "bind.imp").fields, "listed"));

            character["know.fam.imp"] = ElementType{1};
            EXPECT_TRUE(flag(activityAt(f.rules, 7.5f, character, "bind.imp").fields, "listed"));
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // A bound familiar can be taught: the row is there for a kind he holds, and raises what it makes
        TEST(rules_test, training_a_familiar_raises_what_it_makes)
        {
            MockLogger logger;
            RulesFixture f;

            ElementMap character = boy();
            EXPECT_FALSE(flag(activityAt(f.rules, 7.5f, character, "train.sprite").fields, "listed"));

            character["fam.sprite"] = ElementType{1};
            character["rations"] = ElementType{4};
            RuleActivity row = activityAt(f.rules, 7.5f, character, "train.sprite");
            EXPECT_TRUE(flag(row.fields, "listed"));
            EXPECT_EQ(textOf(row.fields, "group"), "Familiars");
            EXPECT_EQ(textOf(row.fields, "name"), "Teach the Sprite the Old Brambles");
            EXPECT_EQ(textOf(row.gains[0], "stat"), "more.berries");

            // A lesson costs a month of what the kind eats: the sprite eats nothing, and is taught for nothing
            EXPECT_TRUE(row.costs.empty());

            character["fam.mole"] = ElementType{1};
            RuleActivity mole = activityAt(f.rules, 7.5f, character, "train.mole");
            ASSERT_EQ(mole.costs.size(), 1u);
            EXPECT_EQ(textOf(mole.costs[0], "stat"), "rations");

            EXPECT_FALSE(flag(activityAt(f.rules, 7.5f, character, "train.imp").fields, "listed"));
        }

        // A kind is learned from a study row, and the row is not offered to a world that knows it
        TEST(rules_test, research_teaches_a_kind)
        {
            MockLogger logger;
            RulesFixture f;

            ElementMap character = boy();
            character["letters"] = ElementType{1};
            RuleActivity row = activityAt(f.rules, 9.5f, character, "research.imp");
            EXPECT_TRUE(flag(row.fields, "listed"));
            EXPECT_EQ(textOf(row.gains[0], "stat"), "know.fam.imp");

            character["know.fam.imp"] = ElementType{1};
            EXPECT_FALSE(flag(activityAt(f.rules, 9.5f, character, "research.imp").fields, "listed"));
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // The free slots a bind row asks, as its requirement says them
        namespace
        {
            int freeSlotsOn(const RuleActivity& row)
            {
                for (const auto& req : row.requires)
                {
                    if (textOf(req, "label") == "A free slot")
                        return intOf(req, "current");
                }

                ADD_FAILURE() << "no slot asked";

                return -1;
            }
        }

        // The mill loft gives a slot to every life of the town, counted before any class: a boy of
        // seven has two
        TEST(rules_test, the_loft_gives_a_slot)
        {
            MockLogger logger;
            RulesFixture f;

            ElementMap character = boy();
            character["scavenged"] = ElementType{2};
            EXPECT_EQ(freeSlotsOn(activityAt(f.rules, 7.0f, character, "bind.sprite")), 1);

            character["town.mill"] = ElementType{2};
            EXPECT_EQ(freeSlotsOn(activityAt(f.rules, 7.0f, character, "bind.sprite")), 2);

            // A level below it gives none
            character["town.mill"] = ElementType{1};
            EXPECT_EQ(freeSlotsOn(activityAt(f.rules, 7.0f, character, "bind.sprite")), 1);
        }

        // A level that teaches a kind makes it his to bind while it stands: the bellows teach the
        // mole. Known twice, from the town and from the world, its row is there once; and a kind
        // the town teaches is not researched again
        TEST(rules_test, the_bellows_teach_the_mole)
        {
            MockLogger logger;
            RulesFixture f;

            ElementMap character = boy();
            EXPECT_FALSE(flag(activityAt(f.rules, 7.5f, character, "bind.mole").fields, "listed"));

            character["town.smithy"] = ElementType{1};
            EXPECT_TRUE(flag(activityAt(f.rules, 7.5f, character, "bind.mole").fields, "listed"));

            character["know.fam.mole"] = ElementType{1};

            std::vector<RuleActivity> activities;
            ASSERT_TRUE(f.rules.activities(7.5f, character, activities)) << firstError(f.rules);

            size_t moles = 0;

            for (const auto& a : activities)
            {
                if (textOf(a.fields, "id") == "bind.mole")
                {
                    ++moles;
                    EXPECT_TRUE(flag(a.fields, "listed"));
                }
            }

            EXPECT_EQ(moles, 1u);

            // The chapel primer teaches the imp: no study of the well spirits for who has it
            ElementMap reader = boy();
            reader["letters"] = ElementType{1};
            EXPECT_TRUE(flag(activityAt(f.rules, 9.5f, reader, "research.imp").fields, "listed"));
            EXPECT_FALSE(flag(activityAt(f.rules, 9.5f, reader, "bind.imp").fields, "listed"));

            reader["town.chapel"] = ElementType{2};
            EXPECT_FALSE(flag(activityAt(f.rules, 9.5f, reader, "research.imp").fields, "listed"));
            EXPECT_TRUE(flag(activityAt(f.rules, 9.5f, reader, "bind.imp").fields, "listed"));
        }

        // The market hall feeds the hedge sprites: each makes one more berry
        TEST(rules_test, the_hall_feeds_the_sprites)
        {
            MockLogger logger;
            RulesFixture f;

            RuleMonth month;
            ASSERT_TRUE(f.rules.month(8.0f, {{"vit", ElementType{20}}, {"rations", ElementType{5}}, {"fam.sprite", ElementType{1}}, {"berries", ElementType{0}}, {"town.market", ElementType{2}}}, month)) << firstError(f.rules);

            EXPECT_EQ(intOf(month.after, "berries"), 5);

            // With what he has taught them, and two of them
            ASSERT_TRUE(f.rules.month(8.0f, {{"vit", ElementType{20}}, {"rations", ElementType{5}}, {"fam.sprite", ElementType{2}}, {"berries", ElementType{0}}, {"more.berries", ElementType{1}}, {"town.market", ElementType{2}}}, month)) << firstError(f.rules);

            EXPECT_EQ(intOf(month.after, "berries"), 12);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // Binding says what the creature eats and makes, before he binds it
        TEST(rules_test, binding_says_what_it_eats)
        {
            MockLogger logger;
            RulesFixture f;

            // The first kinds eat no rations: the sprite asks nothing at all
            EXPECT_EQ(textOf(activityAt(f.rules, 7.5f, boy(), "bind.sprite").fields, "each"), "EATS NOTHING \xC2\xB7 MAKES 4 BERRIES");
            EXPECT_EQ(textOf(activityAt(f.rules, 7.5f, boy(), "bind.brownie").fields, "each"), "EATS 3 BERRIES, 2 WATER, 1 FIREWOOD \xC2\xB7 MAKES 2 RATIONS");
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // A life has one slot, and its standing and calling give more; a familiar fills one
        TEST(rules_test, slots_grow_with_the_life)
        {
            MockLogger logger;
            RulesFixture f;

            auto slotCurrent = [&](const ElementMap& character)
            {
                RuleActivity row = activityAt(f.rules, 7.5f, character, "bind.sprite");

                for (const auto& req : row.requires)
                {
                    if (textOf(req, "stat") == "slot")
                        return intOf(req, "current");
                }

                ADD_FAILURE() << "no slot requirement";

                return -1;
            };

            ElementMap character = boy();
            character["scavenged"] = ElementType{2};
            character["berries"] = ElementType{6};
            EXPECT_EQ(slotCurrent(character), 1);

            EXPECT_TRUE(flag(activityAt(f.rules, 7.5f, character, "bind.sprite").fields, "listed"));

            // No slot free: no bind row is listed at all, though the one he has can still be taught
            character["fam.sprite"] = ElementType{1};
            character["rations"] = ElementType{12};
            EXPECT_EQ(slotCurrent(character), 0);
            EXPECT_FALSE(flag(activityAt(f.rules, 7.5f, character, "bind.sprite").fields, "listed"));
            EXPECT_TRUE(flag(activityAt(f.rules, 7.5f, character, "train.sprite").fields, "listed"));

            {
                std::vector<RuleActivity> rows;
                ASSERT_TRUE(f.rules.activities(7.5f, character, rows)) << firstError(f.rules);

                for (const auto& row : rows)
                {
                    if (textOf(row.fields, "id").rfind("bind.", 0) == 0)
                        EXPECT_FALSE(flag(row.fields, "listed")) << textOf(row.fields, "id");
                }
            }

            // A slot comes with his standing: the bind rows are back
            character["keep_oath"] = ElementType{1};
            EXPECT_EQ(slotCurrent(character), 1);
            EXPECT_TRUE(flag(activityAt(f.rules, 7.5f, character, "bind.sprite").fields, "listed"));

            character["castellan"] = ElementType{1};
            EXPECT_EQ(slotCurrent(character), 3);

            // More bound than the slots hold: none free, and the month leaves them alone
            ElementMap over = boy();
            over["fam.sprite"] = ElementType{2};
            EXPECT_EQ(slotCurrent(over), 0);

            RuleMonth month;
            ASSERT_TRUE(f.rules.month(7.5f, over, month)) << firstError(f.rules);
            EXPECT_EQ(intOf(month.after, "fam.sprite"), 2);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // A familiar fed eats after him and makes its month; the rates say both. (The mole: the
        // first kinds, the sprite and the imp, eat no rations)
        TEST(rules_test, a_fed_familiar_makes)
        {
            MockLogger logger;
            RulesFixture f;

            ElementMap character = {{"vit", ElementType{20}}, {"rations", ElementType{5}}, {"fam.mole", ElementType{1}}, {"stone", ElementType{0}}};

            RuleMonth month;
            ASSERT_TRUE(f.rules.month(8.0f, character, month)) << firstError(f.rules);

            EXPECT_EQ(intOf(month.after, "rations"), 2);
            EXPECT_EQ(intOf(month.after, "stone"), 2);
            EXPECT_EQ(intOf(month.after, "fam.mole"), 1);
            EXPECT_TRUE(month.entries.empty());

            ASSERT_NE(byId(month.rows, "stone"), nullptr);
            EXPECT_EQ(textOf(*byId(month.rows, "stone"), "rate"), "+2 / mo");
            ASSERT_NE(byId(month.rows, "rations"), nullptr);
            EXPECT_EQ(textOf(*byId(month.rows, "rations"), "rate"), "\xE2\x88\x92" "3 / mo");

            // Its own row, in a group of its own, and a gloss that says what it eats and makes
            const ElementMap* mole = byId(month.rows, "fam.mole");
            ASSERT_NE(mole, nullptr);
            EXPECT_EQ(textOf(*mole, "group"), "familiars");
            EXPECT_EQ(textOf(*mole, "groupLabel"), "FAMILIARS");
            EXPECT_EQ(textOf(*mole, "rate"), "");

            bool glossed = false;

            for (const auto& gloss : month.glosses)
            {
                if (textOf(gloss.fields, "id") != "fam.mole")
                    continue;

                glossed = true;

                std::string said;

                for (const auto& row : gloss.rows)
                    said += textOf(row, "label") + "=" + textOf(row, "value") + ";";

                EXPECT_NE(said.find("Each eats Rations=\xE2\x88\x92" "2 / mo"), std::string::npos) << said;
                EXPECT_NE(said.find("Each makes Stone=+2 / mo"), std::string::npos) << said;
                EXPECT_NE(said.find("For life"), std::string::npos) << said;
            }

            EXPECT_TRUE(glossed);
        }

        // The first kinds eat no rations: a sprite costs a young life nothing to keep
        TEST(rules_test, a_sprite_eats_nothing)
        {
            MockLogger logger;
            RulesFixture f;

            RuleMonth month;
            ASSERT_TRUE(f.rules.month(8.0f, {{"vit", ElementType{20}}, {"rations", ElementType{5}}, {"fam.sprite", ElementType{2}}, {"berries", ElementType{0}}}, month)) << firstError(f.rules);

            EXPECT_EQ(intOf(month.after, "rations"), 4);
            EXPECT_EQ(intOf(month.after, "berries"), 8);
            EXPECT_TRUE(month.entries.empty());
        }

        // All or nothing: a kind without enough for all of its count eats nothing and makes nothing,
        // and its hunger costs him nothing
        TEST(rules_test, a_hungry_familiar_makes_nothing)
        {
            MockLogger logger;
            RulesFixture f;

            ElementMap character = {{"vit", ElementType{20}}, {"rations", ElementType{2}}, {"fam.mole", ElementType{1}}, {"stone", ElementType{0}}};

            RuleMonth month;
            ASSERT_TRUE(f.rules.month(8.0f, character, month)) << firstError(f.rules);

            EXPECT_EQ(intOf(month.after, "rations"), 1);
            EXPECT_EQ(intOf(month.after, "stone"), 0);
            EXPECT_EQ(intOf(month.after, "vit"), 20);
            EXPECT_TRUE(month.hurt.empty());

            ASSERT_EQ(month.entries.size(), 1u);
            EXPECT_EQ(textOf(month.entries[0], "text"), "A stone mole went hungry");
            EXPECT_EQ(textOf(month.entries[0], "kind"), "loss");
        }

        // Every month he eats, and his familiars after him: one way the rations go, at any work
        TEST(rules_test, he_and_his_familiars_eat_every_month)
        {
            MockLogger logger;
            RulesFixture f;

            RuleMonth month;
            ASSERT_TRUE(f.rules.month(8.0f, {{"vit", ElementType{20}}, {"rations", ElementType{5}}, {"fam.mole", ElementType{1}}}, month)) << firstError(f.rules);

            EXPECT_EQ(intOf(month.after, "rations"), 2);
            EXPECT_EQ(intOf(month.after, "stone"), 2);
            ASSERT_NE(byId(month.rows, "rations"), nullptr);
            EXPECT_EQ(textOf(*byId(month.rows, "rations"), "rate"), "\xE2\x88\x92" "3 / mo");

            for (const auto& gloss : month.glosses)
            {
                if (textOf(gloss.fields, "id") == "rations")
                {
                    EXPECT_EQ(textOf(gloss.fields, "footnote").find("MEALS"), std::string::npos);
                }
            }
        }

        // What a familiar makes stops at the limit
        TEST(rules_test, production_stops_at_the_limit)
        {
            MockLogger logger;
            RulesFixture f;

            RuleMonth month;
            ASSERT_TRUE(f.rules.month(8.0f, {{"vit", ElementType{20}}, {"rations", ElementType{5}}, {"fam.sprite", ElementType{1}}, {"berries", ElementType{28}}}, month)) << firstError(f.rules);

            EXPECT_EQ(intOf(month.after, "berries"), 30);
            ASSERT_NE(byId(month.rows, "berries"), nullptr);
            EXPECT_EQ(textOf(*byId(month.rows, "berries"), "rate"), "+2 / mo");
        }

        // What he has learned of a thing ("more.<stat>") is more of it from each familiar
        TEST(rules_test, more_raises_the_yield)
        {
            MockLogger logger;
            RulesFixture f;

            RuleMonth month;
            ASSERT_TRUE(f.rules.month(8.0f, {{"vit", ElementType{20}}, {"rations", ElementType{5}}, {"fam.sprite", ElementType{2}}, {"berries", ElementType{0}}, {"more.berries", ElementType{2}}}, month)) << firstError(f.rules);

            EXPECT_EQ(intOf(month.after, "berries"), 12);
        }

        // The rations a coming month eats: his, and his familiars'
        TEST(rules_test, eats_counts_him_and_his_familiars)
        {
            MockLogger logger;
            RulesFixture f;

            RuleMonth month;
            ASSERT_TRUE(f.rules.month(8.0f, {{"vit", ElementType{20}}, {"rations", ElementType{5}}, {"fam.mole", ElementType{1}}}, month)) << firstError(f.rules);

            ElementType eats;
            ASSERT_TRUE(f.rules.resourcesScript.get("eats", eats)) << firstError(f.rules);
            EXPECT_EQ(eats.toString(), "3");

            // Alone, his own: the same at any work
            ASSERT_TRUE(f.rules.month(8.0f, {{"vit", ElementType{20}}, {"rations", ElementType{5}}}, month)) << firstError(f.rules);
            ASSERT_TRUE(f.rules.resourcesScript.get("eats", eats));
            EXPECT_EQ(eats.toString(), "1");
        }

        // In table order: what a brownie makes feeds a kind after it, the same month
        TEST(rules_test, familiars_feed_in_table_order)
        {
            MockLogger logger;
            RulesFixture f;

            ElementMap character = {{"vit", ElementType{20}}, {"rations", ElementType{2}}, {"fam.brownie", ElementType{1}}, {"fam.mole", ElementType{1}},
                                    {"berries", ElementType{3}}, {"water", ElementType{2}}, {"firewood", ElementType{1}}, {"stone", ElementType{0}}};

            RuleMonth month;
            ASSERT_TRUE(f.rules.month(8.0f, character, month)) << firstError(f.rules);

            // He eats his one; the brownie makes two, and the mole eats two of the three
            EXPECT_EQ(intOf(month.after, "rations"), 1);
            EXPECT_EQ(intOf(month.after, "stone"), 2);
            EXPECT_EQ(intOf(month.after, "berries"), 0);
            EXPECT_TRUE(month.entries.empty());
        }

        // Running out is foreseen with his familiars' share counted: the warning, the gloss and the
        // entry the month it is gone, written once, before a familiar's hunger
        TEST(rules_test, familiars_count_in_running_out)
        {
            MockLogger logger;
            RulesFixture f;

            auto texts = [](const RuleMonth& month) {
                std::vector<std::string> said;

                for (const auto& entry : month.entries)
                    said.push_back(textOf(entry, "text"));

                return said;
            };

            // Three rations, his one and a stone mole's two: one month of them
            RuleMonth month;
            ASSERT_TRUE(f.rules.month(8.0f, {{"vit", ElementType{20}}, {"rations", ElementType{3}}, {"fam.mole", ElementType{1}}}, month)) << firstError(f.rules);

            EXPECT_EQ(intOf(month.after, "rations"), 0);
            EXPECT_EQ(month.warning, "RATIONS FOR 1 MONTH");
            EXPECT_EQ(month.advice, "Rations are running low: buy some at the market, or take work that gives rations up front");
            EXPECT_EQ(texts(month), std::vector<std::string>{"The rations ran out"});

            for (const auto& gloss : month.glosses)
            {
                if (textOf(gloss.fields, "id") != "rations")
                    continue;

                std::string said;

                for (const auto& row : gloss.rows)
                    said += textOf(row, "label") + "=" + textOf(row, "value") + ";";

                EXPECT_NE(said.find("A month uses=3;"), std::string::npos) << said;
                EXPECT_NE(said.find("Lasts=1 mo;"), std::string::npos) << said;
            }

            // Six of them: two months, said from three
            ASSERT_TRUE(f.rules.month(8.0f, {{"vit", ElementType{20}}, {"rations", ElementType{6}}, {"fam.mole", ElementType{1}}}, month)) << firstError(f.rules);
            EXPECT_EQ(month.warning, "RATIONS FOR 2 MONTHS");
            EXPECT_TRUE(month.entries.empty());

            // His last one eaten, the mole goes hungry: the rations ran out once, then the hunger
            ASSERT_TRUE(f.rules.month(8.0f, {{"vit", ElementType{20}}, {"rations", ElementType{1}}, {"fam.mole", ElementType{1}}}, month)) << firstError(f.rules);
            EXPECT_EQ(intOf(month.after, "rations"), 0);
            EXPECT_EQ(intOf(month.after, "vit"), 20);
            EXPECT_EQ(texts(month), (std::vector<std::string>{"The rations ran out", "A stone mole went hungry"}));

            // Out of them: no second entry, his Vitality pays, the mole still hungry
            ASSERT_TRUE(f.rules.month(8.0f, {{"vit", ElementType{20}}, {"rations", ElementType{0}}, {"fam.mole", ElementType{1}}}, month)) << firstError(f.rules);
            EXPECT_EQ(intOf(month.after, "vit"), 15);
            EXPECT_EQ(month.warning, "NO RATIONS LEFT");
            EXPECT_EQ(texts(month), std::vector<std::string>{"A stone mole went hungry"});
        }

        // What a gloss of a holding says, as "label=value;" pairs
        std::string glossSaid(const RuleMonth& month, const std::string& id)
        {
            std::string said;

            for (const auto& gloss : month.glosses)
            {
                if (textOf(gloss.fields, "id") != id)
                    continue;

                for (const auto& row : gloss.rows)
                    said += textOf(row, "label") + "=" + textOf(row, "value") + ";";
            }

            return said;
        }

        // He and his familiars eat at any work: running out is foreseen on all their shares, the
        // gloss counting them as the warning does
        TEST(rules_test, the_warning_counts_every_mouth)
        {
            MockLogger logger;
            RulesFixture f;

            RuleMonth month;
            ASSERT_TRUE(f.rules.month(8.0f, {{"vit", ElementType{20}}, {"rations", ElementType{3}}, {"fam.mole", ElementType{1}}}, month)) << firstError(f.rules);

            EXPECT_EQ(intOf(month.after, "rations"), 0);
            EXPECT_EQ(month.warning, "RATIONS FOR 1 MONTH");
            EXPECT_EQ(month.advice, "Rations are running low: buy some at the market, or take work that gives rations up front");

            const std::string said = glossSaid(month, "rations");
            EXPECT_NE(said.find("A month uses=3;"), std::string::npos) << said;
            EXPECT_NE(said.find("His familiars eat=2 / mo;"), std::string::npos) << said;
            EXPECT_NE(said.find("Lasts=1 mo;"), std::string::npos) << said;

            // None bound: his own alone
            ASSERT_TRUE(f.rules.month(8.0f, {{"vit", ElementType{20}}, {"rations", ElementType{3}}}, month)) << firstError(f.rules);
            EXPECT_EQ(intOf(month.after, "rations"), 2);
            EXPECT_EQ(month.warning, "RATIONS FOR 3 MONTHS");
            EXPECT_NE(glossSaid(month, "rations").find("A month uses=1;"), std::string::npos) << glossSaid(month, "rations");
            EXPECT_NE(glossSaid(month, "rations").find("Lasts=3 mo;"), std::string::npos) << glossSaid(month, "rations");
            EXPECT_EQ(glossSaid(month, "rations").find("His familiars eat"), std::string::npos) << glossSaid(month, "rations");
        }

        // The rations last the months that feed him and his familiars both: twelve at three a month
        // feed them both four months (he eats first, and the mole is hungry in the fifth)
        TEST(rules_test, rations_last_while_all_are_fed)
        {
            MockLogger logger;
            RulesFixture f;

            RuleMonth month;
            ASSERT_TRUE(f.rules.month(8.0f, {{"vit", ElementType{20}}, {"rations", ElementType{12}}, {"fam.mole", ElementType{1}}}, month)) << firstError(f.rules);

            std::string said = glossSaid(month, "rations");
            EXPECT_NE(said.find("A month uses=3;"), std::string::npos) << said;
            EXPECT_NE(said.find("His familiars eat=2 / mo;"), std::string::npos) << said;
            EXPECT_NE(said.find("Lasts=4 mo;"), std::string::npos) << said;
            EXPECT_EQ(month.warning, "");

            // Ten: three months, and the warning says the same
            ASSERT_TRUE(f.rules.month(8.0f, {{"vit", ElementType{20}}, {"rations", ElementType{10}}, {"fam.mole", ElementType{1}}}, month)) << firstError(f.rules);
            said = glossSaid(month, "rations");
            EXPECT_NE(said.find("Lasts=3 mo;"), std::string::npos) << said;
            EXPECT_EQ(month.warning, "RATIONS FOR 3 MONTHS");

            // Alone, he eats one a month: no share of theirs to say
            ASSERT_TRUE(f.rules.month(8.0f, {{"vit", ElementType{20}}, {"rations", ElementType{9}}}, month)) << firstError(f.rules);
            said = glossSaid(month, "rations");
            EXPECT_NE(said.find("Lasts=9 mo;"), std::string::npos) << said;
            EXPECT_EQ(said.find("His familiars eat"), std::string::npos) << said;
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // What binding costs him a month is said before he binds, in the row's words; a lesson says
        // what it adds, in plain words
        TEST(rules_test, binding_says_its_upkeep_in_words)
        {
            MockLogger logger;
            RulesFixture f;

            ElementMap character = boy();
            character["fam.sprite"] = ElementType{1};

            const std::string bind = textOf(activityAt(f.rules, 7.5f, character, "bind.sprite").fields, "about");
            EXPECT_NE(bind.find("4 berries"), std::string::npos) << bind;
            EXPECT_EQ(bind.substr(bind.size() - std::string("It eats nothing, and makes 4 berries a month.").size()), "It eats nothing, and makes 4 berries a month.") << bind;

            const std::string mole = textOf(activityAt(f.rules, 7.5f, character, "bind.mole").fields, "about");
            EXPECT_NE(mole.find("It eats 2 rations a month, and makes 2 stone."), std::string::npos) << mole;

            const std::string brownie = textOf(activityAt(f.rules, 7.5f, character, "bind.brownie").fields, "about");
            EXPECT_NE(brownie.find("It eats 3 berries, 2 water and 1 firewood a month, and makes 2 rations."), std::string::npos) << brownie;

            const std::string train = textOf(activityAt(f.rules, 7.5f, character, "train.sprite").fields, "about");
            EXPECT_NE(train.find("Each sprite makes 1 berry more a month"), std::string::npos) << train;

            // A kind learned is said as what he knows
            ElementMap reader = boy();
            reader["letters"] = ElementType{1};
            RuleActivity research = activityAt(f.rules, 9.5f, reader, "research.imp");
            ASSERT_EQ(research.gains.size(), 1u);
            EXPECT_EQ(textOf(research.gains[0], "label"), "KNOWS THE IMP");
            EXPECT_EQ(textOf(research.gains[0], "name"), "How to bind a well imp");
        }

        // No tile, gain, cost or log figure of the economy's rows names a stat by its id: the
        // counter of things gathered by hand is the rules' own and is not shown at all
        TEST(rules_test, the_economy_rows_show_no_raw_id)
        {
            MockLogger logger;
            RulesFixture f;

            ElementMap character = boy();
            character["scavenged"] = ElementType{2};
            character["letters"] = ElementType{1};
            character["done.kitchen"] = ElementType{1};

            for (const char* basic : {"berries", "water", "firewood", "stone", "clay"})
            {
                character[basic] = ElementType{10};
                character[std::string("stash.") + basic] = ElementType{50};
            }

            for (const char* kind : {"sprite", "imp", "brownie", "mole", "weaver"})
                character[std::string("fam.") + kind] = ElementType{1};

            const std::vector<std::string> prefixes = {"scavenge.", "store.", "bind.", "train.", "draw.", "research.", "dry."};
            const std::vector<std::string> raw = {"scavenged", "fam.", "stash.", "know.", "more.", "store."};

            auto lower = [](std::string text) {
                std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
                return text;
            };

            auto clean = [&](const std::string& where, const std::string& text) {
                for (const auto& id : raw)
                    EXPECT_EQ(lower(text).find(id), std::string::npos) << where << ": " << text;
            };

            std::vector<RuleActivity> listed;
            ASSERT_TRUE(f.rules.activities(9.5f, character, listed)) << firstError(f.rules);

            int checked = 0;
            int hidden = 0;

            for (const auto& a : listed)
            {
                const std::string id = textOf(a.fields, "id");
                bool ours = false;

                for (const auto& prefix : prefixes)
                    ours = ours or id.rfind(prefix, 0) == 0;

                if (not ours)
                    continue;

                ++checked;
                clean(id + " name", textOf(a.fields, "name"));
                clean(id + " each", textOf(a.fields, "each"));

                for (const auto& gain : a.gains)
                {
                    if (gain.count("hidden") and flag(gain, "hidden"))
                    {
                        ++hidden;
                        continue;
                    }

                    clean(id + " gain", textOf(gain, "label"));
                    clean(id + " gain", textOf(gain, "name"));
                }

                for (const auto& cost : a.costs)
                {
                    clean(id + " cost", textOf(cost, "label"));
                    clean(id + " cost", textOf(cost, "name"));
                }

                for (const auto& req : a.requires)
                    clean(id + " requires", textOf(req, "label"));

                RuleForecast fc;
                ASSERT_TRUE(f.rules.forecast(9.5f, character, id, 0, fc)) << id << ": " << firstError(f.rules);

                for (const auto& entry : fc.entries)
                    clean(id + " log", textOf(entry, "figure"));
            }

            EXPECT_GE(checked, 20);

            // The four scavenging rows each count a thing gathered, unseen
            EXPECT_EQ(hidden, 4);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // Berries are dried at home, before the town is known: what a sprite makes feeds him
        TEST(rules_test, berries_dry_at_home)
        {
            MockLogger logger;
            RulesFixture f;

            ElementMap character = boy();
            character["berries"] = ElementType{3};

            RuleActivity dry = activityAt(f.rules, 7.0f, character, "dry.berries");
            EXPECT_TRUE(flag(dry.fields, "listed"));
            EXPECT_FALSE(flag(dry.fields, "locked"));
            EXPECT_EQ(textOf(dry.fields, "group"), "Scavenge");
            EXPECT_EQ(dry.fields.count("place") ? dry.fields.at("place").toString() : std::string(), "");
            ASSERT_EQ(dry.gains.size(), 1u);
            EXPECT_EQ(textOf(dry.gains[0], "stat"), "rations");
            EXPECT_EQ(intOf(dry.gains[0], "amount"), 1);
            ASSERT_EQ(dry.costs.size(), 1u);
            EXPECT_EQ(intOf(dry.costs[0], "amount"), 3);

            // Of berries it asks only its cost: the three it dries, and no line of its own
            int berryAsks = 0;

            for (const auto& ask : dry.requires)
            {
                EXPECT_NE(textOf(ask, "label"), "Berries to dry");

                if (textOf(ask, "stat") == "berries")
                {
                    ++berryAsks;
                    EXPECT_EQ(intOf(ask, "needed"), 3);
                }
            }

            EXPECT_EQ(berryAsks, 1);

            // Its row waits for his first berries, and stands locked until he holds three
            character["berries"] = ElementType{0};
            EXPECT_FALSE(flag(activityAt(f.rules, 7.0f, character, "dry.berries").fields, "listed"));

            character["berries"] = ElementType{1};
            EXPECT_TRUE(flag(activityAt(f.rules, 7.0f, character, "dry.berries").fields, "listed"));
            EXPECT_TRUE(flag(activityAt(f.rules, 7.0f, character, "dry.berries").fields, "locked"));
            character["berries"] = ElementType{3};

            // In the Scavenge group, right after the rows that gather by hand
            std::vector<RuleActivity> listed;
            ASSERT_TRUE(f.rules.activities(7.0f, character, listed)) << firstError(f.rules);
            int last = -1;
            int at = -1;

            for (size_t i = 0; i < listed.size(); ++i)
            {
                const std::string id = textOf(listed[i].fields, "id");

                if (id.rfind("scavenge.", 0) == 0)
                    last = static_cast<int>(i);

                if (id == "dry.berries")
                    at = static_cast<int>(i);
            }

            EXPECT_EQ(at, last + 1);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // The market buys and sells the basics by the lot, and sells no clay
        TEST(rules_test, the_market_trades_basic_resources)
        {
            MockLogger logger;
            RulesFixture f;

            ElementMap character = townsman();
            character["coin"] = ElementType{10};
            character["water"] = ElementType{10};

            RuleActivity buy = activityAt(f.rules, 8.0f, character, "buy.stone");

            ASSERT_EQ(buy.gains.size(), 1u);
            EXPECT_EQ(textOf(buy.gains[0], "stat"), "stone");
            EXPECT_EQ(intOf(buy.gains[0], "amount"), 3);
            ASSERT_EQ(buy.costs.size(), 1u);
            EXPECT_EQ(textOf(buy.costs[0], "stat"), "coin");
            EXPECT_EQ(intOf(buy.costs[0], "amount"), 3);
            EXPECT_FALSE(flag(buy.fields, "locked"));

            RuleActivity sell = activityAt(f.rules, 8.0f, character, "sell.water");

            ASSERT_EQ(sell.gains.size(), 1u);
            EXPECT_EQ(textOf(sell.gains[0], "stat"), "coin");
            EXPECT_EQ(intOf(sell.gains[0], "amount"), 1);
            EXPECT_FALSE(flag(sell.fields, "locked"));

            std::vector<RuleActivity> listed;
            ASSERT_TRUE(f.rules.activities(8.0f, character, listed)) << firstError(f.rules);
            int sellBerries = 0;

            for (const auto& a : listed)
            {
                EXPECT_NE(textOf(a.fields, "id"), "buy.clay");

                if (textOf(a.fields, "id") == "sell.berries")
                    sellBerries++;
            }

            EXPECT_EQ(sellBerries, 1);
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

            ASSERT_TRUE(f.rules.month(20.0f, {{"vit", ElementType{45}}, {"vitmax", ElementType{60}}, {"rations", ElementType{5}}}, month)) << firstError(f.rules);
            EXPECT_EQ(intOf(month.after, "vit"), 45);
            EXPECT_EQ(intOf(month.after, "vitrest"), 1);
            EXPECT_TRUE(month.hurt.empty());

            ASSERT_EQ(month.caps.size(), 1u);
            EXPECT_EQ(textOf(month.caps[0], "stat"), "vit");
            EXPECT_EQ(intOf(month.caps[0], "most"), 60);

            ASSERT_TRUE(f.rules.month(20.0f, {{"vit", ElementType{45}}, {"vitmax", ElementType{60}}, {"vitrest", ElementType{1}}, {"rations", ElementType{5}}}, month));
            EXPECT_EQ(intOf(month.after, "vit"), 50);
            EXPECT_EQ(intOf(month.after, "vitmax"), 60);
            EXPECT_EQ(intOf(month.after, "vitrest"), 0);

            // Starving: nothing mends, the count starts again
            ASSERT_TRUE(f.rules.month(20.0f, {{"vit", ElementType{45}}, {"vitmax", ElementType{60}}, {"vitrest", ElementType{1}}, {"rations", ElementType{0}}}, month));
            EXPECT_EQ(intOf(month.after, "vit"), 40);
            EXPECT_EQ(intOf(month.after, "vitmax"), 60);
            EXPECT_EQ(intOf(month.after, "vitrest"), 0);

            // His last ration eaten (one a work that feeds him gave up front), it mends as on any good month
            ASSERT_TRUE(f.rules.month(20.0f, {{"vit", ElementType{45}}, {"vitmax", ElementType{60}}, {"vitrest", ElementType{1}}, {"rations", ElementType{1}}}, month));
            EXPECT_EQ(intOf(month.after, "vit"), 50);

            // Whole: it stays
            ASSERT_TRUE(f.rules.month(20.0f, {{"vit", ElementType{60}}, {"vitmax", ElementType{60}}, {"vitrest", ElementType{1}}}, month));
            EXPECT_EQ(intOf(month.after, "vit"), 60);

            // A character from before the cap takes what he has as his most
            ASSERT_TRUE(f.rules.month(20.0f, {{"vit", ElementType{7}}}, month));
            EXPECT_EQ(intOf(month.after, "vitmax"), 7);

            // No such stat, no cap
            ASSERT_TRUE(f.rules.month(20.0f, {{"coin", ElementType{3}}}, month));
            EXPECT_TRUE(month.caps.empty());

            // The mill brings Vitality: the most it can be rises with it
            RuleForecast forecast;
            ASSERT_TRUE(f.rules.forecast(17.5f, {{"str", ElementType{14}}, {"vit", ElementType{45}}, {"vitmax", ElementType{60}}}, "mill", 0, forecast));
            EXPECT_EQ(intOf(forecast.atTerm, "vit"), 50);
            EXPECT_EQ(intOf(forecast.atTerm, "vitmax"), 65);
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

            ASSERT_TRUE(f.rules.month(20.0f, {{"vit", ElementType{1}}, {"rations", ElementType{0}}}, month));
            EXPECT_EQ(intOf(month.after, "vit"), 0);
            EXPECT_TRUE(month.death.empty());

            ASSERT_TRUE(f.rules.month(20.0f, {{"vit", ElementType{0}}, {"rations", ElementType{0}}}, month));
            EXPECT_FALSE(month.death.empty());

            // A character with no such stat at all is not dead of it
            ASSERT_TRUE(f.rules.month(20.0f, {{"coin", ElementType{3}}}, month));
            EXPECT_TRUE(month.death.empty());
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // Past 30 the most his Vitality can be falls by five every two months, and his Vitality with
        // it: nothing mends it back, the page is not stopped for it, and the life that ends there
        // ended in old age.
        TEST(rules_test, old_age_takes_his_vitality)
        {
            MockLogger logger;
            RulesFixture f;

            const float twelfth = 1.0f / 12.0f;

            RuleMonth month;

            // The month that brings him to 30, and the one after: nothing yet
            ASSERT_TRUE(f.rules.month(30.0f, {{"vit", ElementType{45}}, {"vitmax", ElementType{45}}, {"rations", ElementType{5}}}, month)) << firstError(f.rules);
            EXPECT_EQ(intOf(month.after, "vit"), 45);
            EXPECT_EQ(intOf(month.after, "vitmax"), 45);

            ASSERT_TRUE(f.rules.month(30.0f + twelfth, {{"vit", ElementType{45}}, {"vitmax", ElementType{45}}, {"rations", ElementType{5}}}, month));
            EXPECT_EQ(intOf(month.after, "vit"), 45);
            EXPECT_EQ(intOf(month.after, "vitmax"), 45);

            // Every second month: the most it can be, and what he has with it. Not a hurt
            ASSERT_TRUE(f.rules.month(30.0f + 2.0f * twelfth, {{"vit", ElementType{45}}, {"vitmax", ElementType{45}}, {"rations", ElementType{5}}}, month));
            EXPECT_EQ(intOf(month.after, "vit"), 40);
            EXPECT_EQ(intOf(month.after, "vitmax"), 40);
            EXPECT_TRUE(month.hurt.empty());

            // Already under it: only the most falls
            ASSERT_TRUE(f.rules.month(30.0f + 4.0f * twelfth, {{"vit", ElementType{25}}, {"vitmax", ElementType{45}}, {"rations", ElementType{5}}}, month));
            EXPECT_EQ(intOf(month.after, "vit"), 25);
            EXPECT_EQ(intOf(month.after, "vitmax"), 40);

            // In his prime the same months take nothing
            ASSERT_TRUE(f.rules.month(20.0f + 2.0f * twelfth, {{"vit", ElementType{45}}, {"vitmax", ElementType{45}}, {"rations", ElementType{5}}}, month));
            EXPECT_EQ(intOf(month.after, "vit"), 45);
            EXPECT_EQ(intOf(month.after, "vitmax"), 45);

            // The last of it, and the line of a life that ended old
            ASSERT_TRUE(f.rules.month(32.0f, {{"vit", ElementType{5}}, {"vitmax", ElementType{5}}, {"rations", ElementType{5}}}, month));
            EXPECT_EQ(intOf(month.after, "vit"), 0);
            EXPECT_EQ(intOf(month.after, "vitmax"), 0);
            EXPECT_TRUE(month.death.empty());

            ASSERT_TRUE(f.rules.month(32.0f, {{"vit", ElementType{0}}, {"vitmax", ElementType{0}}, {"rations", ElementType{5}}}, month));
            EXPECT_NE(month.death.find("old age"), std::string::npos) << month.death;

            ASSERT_TRUE(f.rules.month(20.0f, {{"vit", ElementType{0}}, {"rations", ElementType{0}}}, month));
            EXPECT_FALSE(month.death.empty());
            EXPECT_EQ(month.death.find("old age"), std::string::npos) << month.death;
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // A drop-out of the Collegium is said to have left it: in place of a life of no class, or
        // after the class he went to instead.
        TEST(rules_test, epitaph_says_he_left_the_collegium)
        {
            MockLogger logger;
            RulesFixture f;

            RuleEpitaph epitaph;

            ASSERT_TRUE(f.rules.epitaph(28.0f, {{"coin", ElementType{5}}, {"dropout.collegium", ElementType{1}}, {"fire_course", ElementType{1}}}, {}, epitaph)) << firstError(f.rules);
            EXPECT_EQ(epitaph.story[0], "He left the Collegium without its gown, and lived by what he had learned there.");

            ASSERT_TRUE(f.rules.epitaph(28.0f, {{"coin", ElementType{5}}, {"dropout.collegium", ElementType{1}}, {"keep_oath", ElementType{1}}}, {}, epitaph));
            ASSERT_GE(epitaph.story.size(), 2u);
            EXPECT_EQ(epitaph.story[0], "He swore himself to the Keep and served it under arms.");
            EXPECT_EQ(epitaph.story[1], "He had left the Collegium before that, and kept what he had learned there.");

            ASSERT_TRUE(f.rules.epitaph(28.0f, {{"coin", ElementType{5}}}, {}, epitaph));
            EXPECT_EQ(epitaph.story[0], "He chose no class, and lived by the work of his hands.");
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // What is said of a life when it ends: how it ended, what he became, what he worked at, what
        // he left and what is told of him, and his figures on one line.
        TEST(rules_test, epitaph_tells_the_life)
        {
            MockLogger logger;
            RulesFixture f;

            RuleEpitaph epitaph;

            // An old man of the Keep: four terms, the mill most of all; what is done at once is not a work
            f.rules.done = {{"mill", ElementType{3}}, {"carters", ElementType{1}}, {"buy.rations", ElementType{9}}};

            ASSERT_TRUE(f.rules.epitaph(33.5f, {{"vit", ElementType{0}}, {"coin", ElementType{31}}, {"keep_oath", ElementType{1}}}, {}, epitaph)) << firstError(f.rules);

            EXPECT_EQ(epitaph.cause, "He died an old man, in his thirty-third year.");
            ASSERT_EQ(epitaph.story.size(), 3u);

            // A life lived to its end: a word is asked of who played it
            EXPECT_NE(epitaph.ask.find("a comment or a rating on the game's page"), std::string::npos) << epitaph.ask;
            EXPECT_EQ(epitaph.story[0], "He swore himself to the Keep and served it under arms.");
            EXPECT_EQ(epitaph.story[1], "Of his 4 works, the one he went back to most was Help at the Mill.");
            EXPECT_EQ(epitaph.story[2], "He left 31 coin behind him.");
            EXPECT_EQ(epitaph.text, epitaph.story[0] + " " + epitaph.story[1] + " " + epitaph.story[2]);
            EXPECT_EQ(epitaph.tally, "AGE 33 \xC2\xB7 WORKS 4 \xC2\xB7 COIN 31 \xC2\xB7 DEEDS 0");

            // The highest standing is the one that is said, and a rich man is said rich
            ASSERT_TRUE(f.rules.epitaph(33.5f, {{"coin", ElementType{240}}, {"keep_oath", ElementType{1}}, {"captain", ElementType{1}}}, {}, epitaph));
            EXPECT_EQ(epitaph.story[0], "He rose to Captain of Bellmoor, and the Keep kept his name.");
            EXPECT_EQ(epitaph.story[2], "He died a rich man, with 240 coin to his name.");

            // A boy who starved: no path, no work, no coin, and one deed told of him
            f.rules.done = {};

            ASSERT_TRUE(f.rules.epitaph(12.25f, boy(), {"First wages"}, epitaph)) << firstError(f.rules);

            EXPECT_EQ(epitaph.cause, "His strength gave out in his twelfth year.");
            EXPECT_TRUE(epitaph.ask.empty()) << epitaph.ask;
            ASSERT_EQ(epitaph.story.size(), 4u);
            EXPECT_EQ(epitaph.story[0], "He did not live to choose a class.");
            EXPECT_EQ(epitaph.story[1], "Nothing he did was written down.");
            EXPECT_EQ(epitaph.story[2], "He left nothing but his name.");
            EXPECT_EQ(epitaph.story[3], "They still tell of it: First wages.");
            EXPECT_EQ(epitaph.tally, "AGE 12 \xC2\xB7 WORKS 0 \xC2\xB7 COIN 0 \xC2\xB7 DEEDS 1");

            // A man of no path, with one work to his name and two deeds
            f.rules.done = {{"smithy", ElementType{1}}};

            ASSERT_TRUE(f.rules.epitaph(24.0f, {{"coin", ElementType{5}}}, {"First wages", "Sworn"}, epitaph));

            EXPECT_EQ(epitaph.story[0], "He chose no class, and lived by the work of his hands.");
            EXPECT_EQ(epitaph.story[1], "One work is written under his name: Work the Smithy.");
            EXPECT_EQ(epitaph.story[2], "He left little: 5 coin.");
            EXPECT_EQ(epitaph.story[3], "They still tell of it: First wages, Sworn.");
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // His prime closes every door at 30; what is left to an old man is listed a year before, asks
        // nothing, and is never closed. He can still buy his rations, at the market he knows.
        TEST(rules_test, old_age_keeps_basic_work)
        {
            MockLogger logger;
            RulesFixture f;

            EXPECT_FALSE(flag(activityAt(f.rules, 28.5f, boy(), "tales").fields, "listed"));

            // A year before: listed beside the work of his prime, so its last months are not empty
            EXPECT_TRUE(flag(activityAt(f.rules, 29.0f, boy(), "tales").fields, "listed"));
            EXPECT_TRUE(flag(activityAt(f.rules, 29.0f, boy(), "carters").fields, "listed"));
            EXPECT_FALSE(flag(activityAt(f.rules, 29.0f + 11.0f / 12.0f, boy(), "carters").fields, "listed"));
            EXPECT_TRUE(flag(activityAt(f.rules, 29.0f + 11.0f / 12.0f, boy(), "tales").fields, "listed"));

            // Old: these, and nothing else
            std::vector<RuleActivity> activities;
            ASSERT_TRUE(f.rules.activities(34.0f, townsman(), activities)) << firstError(f.rules);

            std::vector<std::string> listed;

            for (const auto& a : activities)
            {
                if (flag(a.fields, "listed"))
                    listed.push_back(textOf(a.fields, "id"));
            }

            // With what is bought in town and the works for the town, which no age closes
            for (const char* id : {"buy.rations", "tales", "garden", "teach"})
                EXPECT_NE(std::find(listed.begin(), listed.end(), id), listed.end()) << id;

            for (const auto& id : listed)
            {
                // The wooden blade too: no age closes it, a man who never held one may begin there
                const bool old = id == "buy.rations" or id == "buy.water" or id == "buy.firewood" or id == "buy.stone" or id == "buy.blade" or id == "tales" or id == "garden" or id == "teach";

                EXPECT_TRUE(old or id.rfind("raise.", 0) == 0) << id;
            }

            RuleActivity tales = activityAt(f.rules, 34.0f, boy(), "tales");

            EXPECT_FALSE(flag(tales.fields, "closed"));
            EXPECT_FALSE(flag(tales.fields, "locked"));
            EXPECT_TRUE(flag(tales.fields, "board"));
            EXPECT_EQ(textOf(tales.fields, "group"), "Old age");
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
        // His sixth year: what each of its works brings, and takes.
        TEST(rules_test, the_works_of_his_sixth_year)
        {
            MockLogger logger;
            RulesFixture f;

            f.rules.lives = 2;

            auto brings = [](const RuleActivity& a) {
                std::vector<std::string> gains;

                for (const auto& g : a.gains)
                    gains.push_back(textOf(g, "stat") + " " + std::to_string(intOf(g, "amount")));

                return gains;
            };

            const float four = 6.0f + 4.0f / 12.0f;

            // What he gleans feeds him until the market; the flour and the books pay what buys the rest
            RuleActivity glean = activityAt(f.rules, 6.0f, child(), "glean");

            EXPECT_TRUE(flag(glean.fields, "listed"));
            EXPECT_FALSE(flag(glean.fields, "locked"));
            EXPECT_EQ(intOf(glean.fields, "months"), 1);
            EXPECT_FALSE(flag(glean.fields, "board"));
            EXPECT_EQ(intOf(glean.fields, "upFront"), 0);
            EXPECT_EQ(brings(glean), (std::vector<std::string>{"rations 10"}));

            EXPECT_EQ(brings(activityAt(f.rules, 6.0f, child(), "flour")), (std::vector<std::string>{"coin 6"}));
            EXPECT_EQ(brings(activityAt(f.rules, 6.0f, child(), "tome")), (std::vector<std::string>{"tome 1", "coin 4"}));

            // His body or his mind: the scribe's bench is the one that builds his Vitality
            EXPECT_EQ(brings(activityAt(f.rules, 6.0f, child(), "wheel")), (std::vector<std::string>{"str 1", "dex 1"}));
            EXPECT_EQ(brings(activityAt(f.rules, 6.0f, child(), "scribe")), (std::vector<std::string>{"int 1", "vit 5"}));

            // The three of two months: the sacks take from his Vitality as they begin
            EXPECT_EQ(brings(activityAt(f.rules, four, child(), "knots")), (std::vector<std::string>{"dex 1", "int 1"}));
            EXPECT_EQ(brings(activityAt(f.rules, four, child(), "sing")), (std::vector<std::string>{"int 1", "renown 1"}));

            RuleActivity sacks = activityAt(f.rules, four, child(), "sacks");

            EXPECT_EQ(intOf(sacks.fields, "months"), 2);
            EXPECT_EQ(brings(sacks), (std::vector<std::string>{"str 1"}));
            ASSERT_EQ(sacks.costs.size(), 1u);
            EXPECT_EQ(textOf(sacks.costs[0], "stat"), "vit");
            EXPECT_EQ(intOf(sacks.costs[0], "amount"), 5);

            // The harvest, and the month in the outskirts that brings him to seven
            RuleActivity harvest = activityAt(f.rules, 6.75f, child(), "harvest");

            EXPECT_EQ(intOf(harvest.fields, "months"), 2);
            EXPECT_EQ(brings(harvest), (std::vector<std::string>{"str 1", "dex 1", "int 1"}));

            RuleActivity outskirts = activityAt(f.rules, 6.0f + 11.0f / 12.0f, child(), "outskirts");

            EXPECT_TRUE(flag(outskirts.fields, "listed"));
            EXPECT_EQ(intOf(outskirts.fields, "months"), 1);
            EXPECT_EQ(textOf(outskirts.fields, "story"), "outskirts");
            EXPECT_EQ(textOf(outskirts.fields, "path"), "");
            // No board: its six rations are the lunch of its first day (storytable.pg), so he does
            // not go hungry the month he is there
            EXPECT_FALSE(flag(outskirts.fields, "board"));
            EXPECT_EQ(intOf(outskirts.fields, "upFront"), 0);
            EXPECT_EQ(intOf(outskirts.fields, "uses"), 1);
            EXPECT_EQ(brings(outskirts), (std::vector<std::string>{}));

            // Start Helping Out, whose month it is, has no row any more
            std::vector<RuleActivity> rows;
            ASSERT_TRUE(f.rules.activities(6.0f + 11.0f / 12.0f, child(), rows));

            for (const auto& a : rows)
                EXPECT_NE(textOf(a.fields, "id"), "helping");
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // The baker's pies ask six of the first berries he scavenges: listed from seven even with
        // none held (a cost is a requirement, and the list leaves requirements out), locked until
        // he has them
        TEST(rules_test, the_pies_ask_the_berries_he_brings)
        {
            MockLogger logger;
            RulesFixture f;

            RuleActivity bare = activityAt(f.rules, 7.0f, boy(), "pies");

            EXPECT_TRUE(flag(bare.fields, "listed"));
            EXPECT_TRUE(flag(bare.fields, "locked"));
            EXPECT_EQ(intOf(bare.fields, "months"), 2);

            ASSERT_EQ(bare.costs.size(), 1u);
            EXPECT_EQ(textOf(bare.costs[0], "stat"), "berries");
            EXPECT_EQ(intOf(bare.costs[0], "amount"), 6);

            ElementMap picked = boy();
            picked["berries"] = ElementType{6};

            RuleActivity ready = activityAt(f.rules, 7.0f, picked, "pies");

            EXPECT_TRUE(flag(ready.fields, "listed"));
            EXPECT_FALSE(flag(ready.fields, "locked"));

            RuleForecast forecast;
            ASSERT_TRUE(f.rules.forecast(7.0f, picked, "pies", 0, forecast)) << firstError(f.rules);

            EXPECT_EQ(intOf(forecast.atStart, "berries"), 0);
            EXPECT_EQ(intOf(forecast.atTerm, "coin"), 6);
            EXPECT_EQ(intOf(forecast.atTerm, "dex"), 7);

            // Each cost says what he holds of it: the gloss writes it on the cost's line, and lists
            // no requirement for it. The requirement stays the rules' own, and keeps the lock
            EXPECT_EQ(intOf(bare.costs[0], "held"), 0);
            EXPECT_EQ(intOf(ready.costs[0], "held"), 6);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // A stat makes some works pay more or go faster: steps of a row's `bonus`. Two steps on one
        // stat add up, so the higher one gives the rest of what is said of it ("in total")
        TEST(rules_test, stats_make_works_pay_more_or_go_faster)
        {
            MockLogger logger;
            RulesFixture f;

            auto berriesAt = [&](int dex) {
                ElementMap picker = boy();
                picker["dex"] = ElementType{dex};

                RuleActivity row = activityAt(f.rules, 8.0f, picker, "scavenge.berries");

                for (const auto& g : row.gains)
                {
                    if (textOf(g, "stat") == "berries")
                        return intOf(g, "amount");
                }

                return -1;
            };

            EXPECT_EQ(berriesAt(7), 6);
            EXPECT_EQ(berriesAt(8), 9);
            EXPECT_EQ(berriesAt(12), 12);

            // The carters take a strong boy for two months, not three: the pay stays, and the
            // rations up front are the two months'
            ElementMap strong = boy();
            strong["str"] = ElementType{9};

            RuleActivity carters = activityAt(f.rules, 8.0f, boy(), "carters");
            EXPECT_EQ(intOf(carters.fields, "months"), 3);
            EXPECT_EQ(intOf(carters.fields, "upFront"), 3);

            carters = activityAt(f.rules, 8.0f, strong, "carters");
            EXPECT_EQ(intOf(carters.fields, "months"), 2);
            EXPECT_EQ(intOf(carters.fields, "upFront"), 2);
            ASSERT_EQ(carters.gains.size(), 1u);
            EXPECT_EQ(intOf(carters.gains[0], "amount"), 6);

            // A quick mind learns its letters in four months
            ElementMap bright = boy();
            bright["int"] = ElementType{8};

            EXPECT_EQ(intOf(activityAt(f.rules, 8.0f, boy(), "letters").fields, "months"), 6);
            EXPECT_EQ(intOf(activityAt(f.rules, 8.0f, bright, "letters").fields, "months"), 4);

            // What each step would give is said before he has it, and marked once he has
            ElementMap deft = boy();
            deft["dex"] = ElementType{8};

            RuleActivity berries = activityAt(f.rules, 8.0f, deft, "scavenge.berries");
            ASSERT_EQ(berries.steps.size(), 2u);
            EXPECT_EQ(textOf(berries.steps[0], "label"), "With Dexterity 8: +3 berries");
            EXPECT_TRUE(flag(berries.steps[0], "reached"));
            EXPECT_EQ(textOf(berries.steps[1], "label"), "With Dexterity 12: +6 berries");
            EXPECT_FALSE(flag(berries.steps[1], "reached"));

            RuleActivity firewood = activityAt(f.rules, 8.0f, boy(), "scavenge.firewood");
            ASSERT_EQ(firewood.steps.size(), 2u);
            EXPECT_EQ(textOf(firewood.steps[0], "label"), "With Strength 8: +2 firewood");
            EXPECT_EQ(textOf(firewood.steps[1], "label"), "With Strength 12: +4 firewood");
            EXPECT_FALSE(flag(firewood.steps[0], "reached"));

            carters = activityAt(f.rules, 8.0f, strong, "carters");
            ASSERT_EQ(carters.steps.size(), 1u);
            EXPECT_EQ(textOf(carters.steps[0], "label"), "With Strength 9: 2 months instead of 3");
            EXPECT_TRUE(flag(carters.steps[0], "reached"));

            // The town's bonus, or a flag's, is not one of them: the chapel's windows on the letters
            RuleActivity letters = activityAt(f.rules, 8.0f, boy(), "letters");
            ASSERT_EQ(letters.steps.size(), 1u);
            EXPECT_EQ(textOf(letters.steps[0], "label"), "With Intelligence 8: 4 months instead of 6");

            EXPECT_TRUE(activityAt(f.rules, 8.0f, boy(), "mill").steps.empty());

            // A step that gives other gains says what it adds
            ElementMap knight = {{"keep_oath", ElementType{1}}, {"arms", ElementType{5}}};
            RuleActivity tourney = activityAt(f.rules, 19.0f, knight, "tourney");
            ASSERT_EQ(tourney.steps.size(), 1u);
            EXPECT_EQ(textOf(tourney.steps[0], "label"), "With Arms 7: +1 Renown, Victor of the tourney");
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // The year as a first life meets it: a work at a time, one of the two, two of the three,
        // the market, the harvest, the mill; then the town's work, whole, the day he is seven.
        TEST(rules_test, the_sixth_year_a_work_at_a_time)
        {
            MockLogger logger;
            RulesFixture f;

            auto listed = [&](float age, const ElementMap& character) {
                std::vector<RuleActivity> activities;
                EXPECT_TRUE(f.rules.activities(age, character, activities)) << firstError(f.rules);

                std::vector<std::string> ids;

                for (const auto& a : activities)
                {
                    if (flag(a.fields, "listed"))
                        ids.push_back(textOf(a.fields, "id"));
                }

                return ids;
            };

            auto at = [](int months) { return 6.0f + static_cast<float>(months) / 12.0f; };

            using Ids = std::vector<std::string>;

            f.rules.lives = 1;

            // Nothing done: the gleaning alone
            EXPECT_EQ(listed(at(0), child()), (Ids{"glean"}));

            f.rules.done = {{"glean", ElementType{1}}};
            EXPECT_EQ(listed(at(1), child()), (Ids{"flour"}));

            // One or the other
            f.rules.done = {{"glean", ElementType{1}}, {"flour", ElementType{1}}};
            EXPECT_EQ(listed(at(2), child()), (Ids{"wheel", "scribe"}));

            // The wheel climbed: the scribe's bench has no row any more
            f.rules.done = {{"glean", ElementType{1}}, {"flour", ElementType{1}}, {"wheel", ElementType{1}}};
            EXPECT_EQ(listed(at(3), child()), (Ids{"tome"}));

            // Three works of two months, from his fourth month
            f.rules.done["tome"] = ElementType{1};
            EXPECT_EQ(listed(at(4), child()), (Ids{"knots", "sacks", "sing"}));

            RuleActivity knots = activityAt(f.rules, at(4), child(), "knots");

            EXPECT_FALSE(flag(knots.fields, "locked"));
            EXPECT_EQ(intOf(knots.fields, "closesIn"), 3);

            f.rules.done["knots"] = ElementType{1};
            EXPECT_EQ(listed(at(6), child()), (Ids{"sacks", "sing"}));
            EXPECT_EQ(intOf(activityAt(f.rules, at(6), child(), "sing").fields, "closesIn"), 1);

            // The second done, the third has closed: there was time for two. The market is all there is
            f.rules.done["sacks"] = ElementType{1};
            EXPECT_TRUE(flag(activityAt(f.rules, at(8), child(), "sing").fields, "closed"));
            EXPECT_EQ(listed(at(8), child()), (Ids{"tomarket"}));

            // (begun a month late, the second still fits, and no later)
            EXPECT_FALSE(flag(activityAt(f.rules, at(7), child(), "sing").fields, "closed"));

            // The town known, the harvest; then the outskirts' month, which ends the day he is seven
            ElementMap known = child();
            known["town_known"] = ElementType{1};

            // (with what its market sells and the works that raise its places, on the town's own page)
            f.rules.done["tomarket"] = ElementType{1};
            EXPECT_EQ(listed(at(9), known), (Ids{"harvest", "buy.rations", "buy.water", "buy.firewood", "buy.stone", "raise.market.1", "raise.mill.1"}));

            f.rules.done["harvest"] = ElementType{1};
            EXPECT_EQ(listed(at(11), known), (Ids{"outskirts", "buy.rations", "buy.water", "buy.firewood", "buy.stone", "raise.market.1", "raise.mill.1"}));

            // The market sells to a boy of six: nothing of the town waits for him to be seven
            RuleActivity buy = activityAt(f.rules, at(9), known, "buy.rations");

            EXPECT_TRUE(flag(buy.fields, "listed"));
            EXPECT_EQ(intOf(buy.fields, "fromAge"), 6);

            // Seven: the town's work, the first of it (the rest comes over the year). The year has
            // taught him the page
            f.rules.done["outskirts"] = ElementType{1};

            ElementMap seven = boy();
            seven["town_known"] = ElementType{1};

            // (the first walk outside the town is among them, right under his work)
            // (and, the town being known, what its market sells and the works that raise its places)
            EXPECT_EQ(listed(7.0f, seven), (Ids{"carters", "mill", "pies", "scavenge.berries", "scavenge.water", "scavenge.firewood", "walk.hedgerows", "buy.rations", "letters", "keep", "collegium", "hand", "buy.water", "buy.firewood", "buy.stone", "raise.market.1", "raise.mill.1"}));
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // A later life is shown the year's first works one at a time, in the order a first life
        // is: the three still wait for his fourth month and are over by his ninth, and what he did
        // not do by seven is closed to him.
        TEST(rules_test, the_sixth_year_of_a_later_life)
        {
            MockLogger logger;
            RulesFixture f;

            auto listed = [&](float age, const ElementMap& character) {
                std::vector<RuleActivity> activities;
                EXPECT_TRUE(f.rules.activities(age, character, activities)) << firstError(f.rules);

                std::vector<std::string> ids;

                for (const auto& a : activities)
                {
                    if (flag(a.fields, "listed"))
                        ids.push_back(textOf(a.fields, "id"));
                }

                return ids;
            };

            auto at = [](int months) { return 6.0f + static_cast<float>(months) / 12.0f; };

            using Ids = std::vector<std::string>;

            f.rules.lives = 2;

            // Nothing done: the gleaning alone, as in a first life
            EXPECT_EQ(listed(at(0), child()), (Ids{"glean"}));

            f.rules.done = {{"glean", ElementType{1}}};
            EXPECT_EQ(listed(at(1), child()), (Ids{"flour"}));

            f.rules.done = {{"glean", ElementType{1}}, {"flour", ElementType{1}}};
            EXPECT_EQ(listed(at(2), child()), (Ids{"wheel", "scribe"}));

            // The scribe first: the wheel is gone, in a later life as in the first
            f.rules.done = {{"glean", ElementType{1}}, {"flour", ElementType{1}}, {"scribe", ElementType{1}}};
            EXPECT_EQ(listed(at(3), child()), (Ids{"tome"}));

            // Too young for the three, for the market and for the harvest: none has a row yet
            f.rules.done = {};

            for (const char* id : {"knots", "sacks", "sing", "tomarket", "harvest", "outskirts"})
            {
                RuleActivity early = activityAt(f.rules, at(3), child(), id);

                EXPECT_FALSE(flag(early.fields, "listed")) << id;
                EXPECT_TRUE(flag(early.fields, "locked")) << id;
            }

            // Who let the months go by is still at the first of the four, with what the months bring
            EXPECT_EQ(listed(at(4), child()), (Ids{"glean", "knots", "sacks", "sing"}));

            // The three are over by his ninth month, the market opens at his eighth and never closes
            EXPECT_EQ(listed(at(8), child()), (Ids{"glean", "tomarket"}));
            EXPECT_EQ(listed(at(9), child()), (Ids{"glean", "tomarket", "harvest"}));

            // What a short work of the year has left to begin is only said near its end
            EXPECT_EQ(intOf(activityAt(f.rules, at(0), child(), "glean").fields, "closesIn"), -1);
            EXPECT_EQ(intOf(activityAt(f.rules, at(8), child(), "glean").fields, "closesIn"), 3);
            EXPECT_EQ(intOf(activityAt(f.rules, at(9), child(), "harvest").fields, "closesIn"), -1);

            // Seven with none of it done: the year is closed, the market still waits for him
            const Ids seven = listed(7.0f, boy());

            for (const char* id : {"glean", "flour", "wheel", "scribe", "tome", "knots", "sacks", "sing", "harvest", "outskirts"})
                EXPECT_TRUE(std::find(seven.begin(), seven.end(), id) == seven.end()) << id;

            EXPECT_EQ(seven, (Ids{"tomarket", "carters", "mill", "pies", "scavenge.berries", "scavenge.water", "scavenge.firewood", "walk.hedgerows", "letters", "keep", "collegium", "hand"}));
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // Quick hands and a name for them, on the streets; and the Collegium's own drill
        TEST(rules_test, the_rooftops_and_the_night_offices)
        {
            MockLogger logger;
            RulesFixture f;

            f.rules.lives = 2;

            auto brings = [](const RuleActivity& a) {
                std::vector<std::string> stats;

                for (const auto& g : a.gains)
                    stats.push_back(g.at("stat").toString());

                return stats;
            };

            ElementMap lad = boy();
            lad["dex"] = ElementType{8};

            RuleActivity rooftops = activityAt(f.rules, 12.0f, lad, "rooftops");

            EXPECT_EQ(textOf(rooftops.fields, "group"), "The streets");
            EXPECT_TRUE(rooftops.fields.at("listed").get<bool>());
            EXPECT_FALSE(rooftops.fields.at("locked").get<bool>());
            EXPECT_EQ(brings(rooftops), (std::vector<std::string>{"dex", "renown", "rooftop_runner"}));   // and the way to step aside in a fight

            // Slow hands are not asked up there
            lad["dex"] = ElementType{6};
            EXPECT_TRUE(activityAt(f.rules, 12.0f, lad, "rooftops").fields.at("locked").get<bool>());

            // A scholar of the Collegium has its offices to keep
            ElementMap scholar = boy();
            scholar["collegium"] = ElementType{1};

            // (once he has studied and tried a first working: the Collegium's works come in an order)
            EXPECT_FALSE(activityAt(f.rules, 18.5f, scholar, "vigil").fields.at("listed").get<bool>());

            f.rules.done = {{"study", ElementType{1}}, {"workings", ElementType{1}}};

            RuleActivity vigil = activityAt(f.rules, 18.5f, scholar, "vigil");

            EXPECT_EQ(textOf(vigil.fields, "group"), "The Collegium");
            EXPECT_EQ(textOf(vigil.fields, "path"), "mage");
            EXPECT_TRUE(vigil.fields.at("listed").get<bool>());
            EXPECT_FALSE(vigil.fields.at("locked").get<bool>());
            EXPECT_EQ(brings(vigil), (std::vector<std::string>{"discipline"}));

            // And it is no one else's
            EXPECT_FALSE(activityAt(f.rules, 18.5f, boy(), "vigil").fields.at("listed").get<bool>());
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // Nobody buys at a market he does not know: the row waits for a life to have explored the town.
        TEST(rules_test, buy_rations_hidden_until_the_market_is_known)
        {
            MockLogger logger;
            RulesFixture f;

            ElementMap character = boy();
            character["coin"] = ElementType{5};

            RuleActivity buy = activityAt(f.rules, 9.0f, character, "buy.rations");

            EXPECT_FALSE(flag(buy.fields, "listed"));
            EXPECT_EQ(intOf(buy.fields, "reach"), 0);

            character["town_known"] = ElementType{1};

            buy = activityAt(f.rules, 9.0f, character, "buy.rations");

            EXPECT_TRUE(flag(buy.fields, "listed"));
            EXPECT_FALSE(flag(buy.fields, "locked"));
            EXPECT_EQ(intOf(buy.fields, "reach"), 100);
        }
        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // The kitchen takes water and firewood, and a row he cannot pay is locked.
        TEST(rules_test, kitchen_asks_water_and_firewood)
        {
            MockLogger logger;
            RulesFixture f;

            ElementMap stocked = boy();
            stocked["water"] = ElementType{3};
            stocked["firewood"] = ElementType{2};

            RuleActivity kitchen = activityAt(f.rules, 7.8f, stocked, "kitchen");

            ASSERT_EQ(kitchen.costs.size(), 2u);
            EXPECT_EQ(textOf(kitchen.costs[0], "stat"), "water");
            EXPECT_EQ(intOf(kitchen.costs[0], "amount"), 3);
            EXPECT_EQ(textOf(kitchen.costs[1], "stat"), "firewood");
            EXPECT_EQ(intOf(kitchen.costs[1], "amount"), 2);
            EXPECT_FALSE(flag(kitchen.fields, "locked"));

            EXPECT_TRUE(flag(activityAt(f.rules, 7.8f, boy(), "kitchen").fields, "locked"));
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // Whatever he holds, a child can still eat: a row that feeds him costs no resource.
        TEST(rules_test, a_child_with_nothing_can_still_eat)
        {
            MockLogger logger;
            RulesFixture f;

            ElementMap child = {{"vit", ElementType{35}}, {"vitmax", ElementType{35}}, {"coin", ElementType{0}}};
            std::vector<RuleActivity> out;
            ASSERT_TRUE(f.rules.activities(7.0f, child, out)) << firstError(f.rules);

            bool eats = false;

            for (const auto& row : out)
            {
                if (not flag(row.fields, "listed") or flag(row.fields, "locked"))
                    continue;

                bool rations = false;

                for (const auto& gain : row.gains)
                {
                    if (textOf(gain, "stat") == "rations")
                        rations = true;
                }

                if (flag(row.fields, "board") or rations)
                    eats = true;
            }

            EXPECT_TRUE(eats);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // A basket raises the berries he can hold by 20, three times in a life.
        TEST(rules_test, a_basket_raises_the_berry_limit)
        {
            MockLogger logger;
            RulesFixture f;

            ElementMap character = boy();
            character["firewood"] = ElementType{2};

            RuleForecast fc;
            ASSERT_TRUE(f.rules.forecast(9.0f, character, "store.berries", 0, fc)) << firstError(f.rules);
            EXPECT_EQ(intOf(fc.atTerm, "store.berries"), 20);

            // The player reads it in words, never as the raw id
            bool named = false;

            for (const auto& entry : fc.entries)
            {
                const std::string figure = textOf(entry, "figure");

                EXPECT_EQ(figure.find("store."), std::string::npos) << figure;

                if (figure.find("berry room") != std::string::npos)
                    named = true;
            }

            EXPECT_TRUE(named);

            ElementMap stored = {{"berries", ElementType{0}}, {"store.berries", ElementType{20}}};
            RuleMonth month;
            ASSERT_TRUE(f.rules.month(9.0f, stored, month)) << firstError(f.rules);
            ASSERT_NE(byId(month.rows, "berries"), nullptr);
            EXPECT_EQ(intOf(*byId(month.rows, "berries"), "limit"), 50);

            // Listed only once he holds some berries
            EXPECT_FALSE(flag(activityAt(f.rules, 9.0f, boy(), "store.berries").fields, "listed"));
            character["berries"] = ElementType{1};
            EXPECT_TRUE(flag(activityAt(f.rules, 9.0f, character, "store.berries").fields, "listed"));

            f.rules.done["store.berries"] = ElementType{3};
            EXPECT_FALSE(flag(activityAt(f.rules, 9.0f, character, "store.berries").fields, "listed"));
        }


        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // The kitchen feeds him while he is at it and sends him off with rations, while he has room
        // for them.
        TEST(rules_test, kitchen_feeds_and_brings_rations)
        {
            MockLogger logger;
            RulesFixture f;

            // The kitchen burns firewood and draws water, which he must hold
            ElementMap fed = boy();
            fed["water"] = ElementType{3};
            fed["firewood"] = ElementType{2};

            // The kitchen takes boys from seven and eight months
            RuleActivity kitchen = activityAt(f.rules, 7.75f, fed, "kitchen");

            EXPECT_TRUE(flag(kitchen.fields, "listed"));
            EXPECT_FALSE(flag(kitchen.fields, "locked"));
            EXPECT_TRUE(flag(kitchen.fields, "board"));
            EXPECT_EQ(intOf(kitchen.fields, "months"), 3);

            ASSERT_EQ(kitchen.gains.size(), 1u);
            EXPECT_EQ(textOf(kitchen.gains[0], "stat"), "rations");
            EXPECT_EQ(intOf(kitchen.gains[0], "amount"), 6);

            // It asks for room, as everything that brings rations does
            // and for the water and the firewood it burns
            ASSERT_EQ(kitchen.requires.size(), 3u);
            const ElementMap* room = nullptr;

            for (const auto& req : kitchen.requires)
            {
                if (textOf(req, "label") == "Room for Rations")
                    room = &req;
            }

            ASSERT_NE(room, nullptr);
            EXPECT_EQ(intOf(*room, "current"), 48);

            ElementMap full = fed;
            full["rations"] = ElementType{60};

            kitchen = activityAt(f.rules, 7.75f, full, "kitchen");

            EXPECT_TRUE(flag(kitchen.fields, "locked"));

            room = nullptr;

            for (const auto& req : kitchen.requires)
            {
                if (textOf(req, "label") == "Room for Rations")
                    room = &req;
            }

            ASSERT_NE(room, nullptr);
            EXPECT_EQ(intOf(*room, "current"), 0);

            // The term brings them
            RuleForecast forecast;
            ASSERT_TRUE(f.rules.forecast(7.75f, fed, "kitchen", 0, forecast)) << firstError(f.rules);
            EXPECT_EQ(intOf(forecast.atTerm, "rations"), 18);

            // And it feeds him: its three months of rations as he begins it, his costs taken
            EXPECT_EQ(forecast.upFront, 3);
            EXPECT_EQ(intOf(forecast.atStart, "rations"), 15);
            EXPECT_EQ(intOf(forecast.atStart, "water"), 0);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // A work that feeds him gives the rations of its months as he begins it, and the log says
        // so; what does not feed him gives none. Past the most he can hold they are his, as any
        // gain is. Left before its term, it takes back those of the months not worked, never more
        // than he holds.
        TEST(rules_test, fed_work_gives_its_rations_up_front)
        {
            MockLogger logger;
            RulesFixture f;

            RuleForecast carters;
            ASSERT_TRUE(f.rules.forecast(7.0f, boy(), "carters", 0, carters)) << firstError(f.rules);

            EXPECT_EQ(carters.upFront, 3);
            EXPECT_EQ(intOf(carters.atStart, "rations"), 15);
            EXPECT_EQ(intOf(carters.atTerm, "rations"), 12);
            EXPECT_EQ(carters.unworked, 3);
            EXPECT_EQ(carters.takenBack, 3);
            ASSERT_EQ(carters.begins.size(), 1u);
            EXPECT_EQ(textOf(carters.begins[0], "text"), "Carry for the Carters gives its rations up front");
            EXPECT_EQ(textOf(carters.begins[0], "kind"), "gain");
            EXPECT_EQ(textOf(carters.begins[0], "figure").rfind("+3", 0), 0u) << textOf(carters.begins[0], "figure");

            // A month in: two months' rations would go back; at its term, none
            RuleForecast in;
            ASSERT_TRUE(f.rules.forecast(7.0f, boy(), "carters", 1, in)) << firstError(f.rules);
            EXPECT_EQ(in.unworked, 2);
            EXPECT_EQ(in.takenBack, 2);
            EXPECT_EQ(intOf(in.atLeave, "rations"), 10);
            EXPECT_EQ(intOf(in.atOwed, "rations"), 14);
            ASSERT_EQ(in.leaves.size(), 1u);
            EXPECT_EQ(textOf(in.leaves[0], "kind"), "loss");
            EXPECT_EQ(textOf(in.leaves[0], "figure").rfind("\xE2\x88\x92" "2", 0), 0u) << textOf(in.leaves[0], "figure");

            ASSERT_TRUE(f.rules.forecast(7.0f, boy(), "carters", 3, in)) << firstError(f.rules);
            EXPECT_EQ(in.unworked, 0);
            EXPECT_EQ(in.takenBack, 0);
            EXPECT_TRUE(in.leaves.empty());

            // Never more than he holds: one ration left, one goes back
            ElementMap thin = boy();
            thin["rations"] = ElementType{1};
            ASSERT_TRUE(f.rules.forecast(7.0f, thin, "carters", 1, in)) << firstError(f.rules);
            EXPECT_EQ(in.unworked, 2);
            EXPECT_EQ(in.takenBack, 1);
            EXPECT_EQ(intOf(in.atLeave, "rations"), 0);

            // Kept past his limit of sixty: eighteen months at the Keep on fifty held
            ElementMap full = sworn();
            full["rations"] = ElementType{50};
            RuleForecast keep;
            ASSERT_TRUE(f.rules.forecast(17.5f, full, "keep", 0, keep)) << firstError(f.rules);
            EXPECT_EQ(keep.upFront, 18);
            EXPECT_EQ(intOf(keep.atStart, "rations"), 68);
            EXPECT_EQ(intOf(keep.atStart, "coin"), 36);

            // What does not feed him gives nothing up front, and leaving it takes nothing
            RuleForecast mill;
            ASSERT_TRUE(f.rules.forecast(7.0f, boy(), "mill", 2, mill)) << firstError(f.rules);
            EXPECT_EQ(mill.upFront, 0);
            EXPECT_EQ(mill.unworked, 0);
            EXPECT_EQ(mill.takenBack, 0);
            EXPECT_TRUE(mill.begins.empty());
            EXPECT_TRUE(mill.leaves.empty());

            ASSERT_TRUE(f.rules.forecast(7.0f, boy(), "mill", 0, mill)) << firstError(f.rules);
            EXPECT_EQ(intOf(mill.atStart, "rations"), 12);

            // Done at once, nothing is fed: the berries dried give their ration and no more
            ElementMap picker = boy();
            picker["berries"] = ElementType{3};
            RuleForecast dry;
            ASSERT_TRUE(f.rules.forecast(7.0f, picker, "dry.berries", 0, dry)) << firstError(f.rules);
            EXPECT_EQ(dry.upFront, 0);
            EXPECT_EQ(intOf(dry.atTerm, "rations"), 13);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // The guide's steps run 1 to n without a gap, each waiting for the one before it; the one
        // outside the sequence waits for none.
        TEST(rules_test, guide_steps_are_ordered)
        {
            MockLogger logger;
            RulesFixture f;

            std::vector<RuleAchievement> achievements;
            ASSERT_TRUE(f.rules.achievements(achievements)) << firstError(f.rules);

            auto waitsFor = [](const RuleAchievement& a) {
                for (const auto& ask : a.asks)
                {
                    if (textOf(ask, "fact") == "life.guide")
                        return ask.count("value") and textOf(ask, "op") == "==" ? intOf(ask, "value") : -2;
                }

                return -1;
            };

            // The Life page's kinds, then a walk's screens' (the outskirts' steps): a work of the
            // day, an answer of the card, a move, a beast's panel, a button of the screen
            const std::vector<std::string> kinds = {"button:", "tile:", "holding:", "stat:", "tab:", "place:", "work:", "option:", "move:", "foe:", "walkbutton:"};
            const std::vector<std::string> places = {"", "clock", "log", "order"};

            // The steps outside the sequence: the work that took from his Vitality; his rations low,
            // before and after he knows the town; and
            // the town opened, its tab, its market and its places, which are said once in a world
            const std::vector<std::string> asides = {"guide.vitality", "guide.rations_low", "guide.rations_market", "guide.town", "guide.market", "guide.places"};
            const std::vector<std::string> worlds = {"guide.town", "guide.market", "guide.places"};

            const int last = 20;

            std::vector<int> taken(last + 1, 0);
            size_t outside = 0;

            for (const auto& a : achievements)
            {
                if (a.kind != RuleKind::Guide)
                    continue;

                const std::string id = textOf(a.fields, "id");
                const std::string point = textOf(a.fields, "point");

                EXPECT_EQ(textOf(a.fields, "kind"), "guide") << id;
                EXPECT_EQ(id.rfind("guide.", 0), 0u) << id;
                EXPECT_FALSE(textOf(a.fields, "say").empty()) << id;
                EXPECT_GT(floatOf(a.fields, "hold"), 0.0f) << id;
                EXPECT_EQ(intOf(a.fields, "order"), a.order) << id;

                bool known = std::find(places.begin(), places.end(), point) != places.end();

                for (const auto& kind : kinds)
                    known = known or (point.rfind(kind, 0) == 0 and point.size() > kind.size());

                EXPECT_TRUE(known) << id << " points at " << point;

                // What is lit with it is of the same kinds: the other tiles of a choice, his other parts
                std::istringstream lights(textOf(a.fields, "light"));

                for (std::string one; std::getline(lights, one, ',');)
                {
                    bool lit = false;

                    for (const auto& kind : kinds)
                        lit = lit or (one.rfind(kind, 0) == 0 and one.size() > kind.size());

                    EXPECT_TRUE(lit) << id << " lights " << one;
                }

                // A work is begun in two presses: the first two tasks (and the market's first buy)
                // show their tile, then the button that begins it, and say what to press. Two works
                // begun, he knows the button: the hand stays on the tile
                const std::string chosen = textOf(a.fields, "pointChosen");
                const bool taught = id == "guide.begin" or id == "guide.flour" or id == "guide.market";

                if (point.rfind("tile:", 0) == 0 and not a.until.empty() and not taught)
                {
                    EXPECT_TRUE(chosen.empty()) << id;
                    EXPECT_TRUE(textOf(a.fields, "sayChosen").empty()) << id;
                }
                else if (point.rfind("tile:", 0) == 0 and not a.until.empty())
                {
                    EXPECT_EQ(chosen, "button:begin") << id;
                    EXPECT_FALSE(textOf(a.fields, "sayChosen").empty()) << id;
                }
                else if (point.rfind("work:", 0) == 0)
                {
                    // And on a story's day: the work, then the button that does it
                    EXPECT_EQ(chosen, "walkbutton:do") << id;
                    EXPECT_FALSE(textOf(a.fields, "sayChosen").empty()) << id;
                }
                else
                {
                    EXPECT_TRUE(chosen.empty()) << id;
                    EXPECT_TRUE(textOf(a.fields, "sayChosen").empty()) << id;
                }

                // Said once in a world: only a step outside the sequence, and the town's three
                EXPECT_EQ(a.world, std::find(worlds.begin(), worlds.end(), id) != worlds.end()) << id;

                if (a.order == 0)
                {
                    // Outside the sequence: it waits for no step
                    EXPECT_EQ(waitsFor(a), -1) << id;
                    EXPECT_TRUE(std::find(asides.begin(), asides.end(), id) != asides.end()) << id;
                    ++outside;

                    continue;
                }

                ASSERT_GE(a.order, 1) << id;
                ASSERT_LE(a.order, last) << id;

                ++taken[a.order];

                EXPECT_EQ(waitsFor(a), a.order - 1) << id;

                // The steps of his sixth year are for a boy under seven (its works, and the
                // outskirts' after his age); his age and the classes are said at any age
                const bool always = id == "guide.clock" or id == "guide.doors" or id == "guide.rest";

                EXPECT_EQ(intOf(a.fields, "untilAge"), always ? 0 : 7) << id;
            }

            for (int order = 1; order <= last; ++order)
                EXPECT_EQ(taken[order], 1) << "order " << order;

            EXPECT_EQ(outside, asides.size());
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // A line of lore is a line and nothing else.
        TEST(rules_test, lore_entries_have_no_gives_and_no_name)
        {
            MockLogger logger;
            RulesFixture f;

            std::vector<RuleAchievement> achievements;
            ASSERT_TRUE(f.rules.achievements(achievements)) << firstError(f.rules);

            size_t lines = 0;

            for (const auto& a : achievements)
            {
                if (a.kind != RuleKind::Lore)
                    continue;

                const std::string id = textOf(a.fields, "id");

                EXPECT_EQ(id.rfind("lore.", 0), 0u) << id;
                EXPECT_FALSE(textOf(a.fields, "entry").empty()) << id;
                EXPECT_TRUE(textOf(a.fields, "name").empty()) << id;
                EXPECT_TRUE(a.gives.empty()) << id;
                EXPECT_TRUE(a.until.empty()) << id;
                EXPECT_FALSE(a.asks.empty()) << id;

                ++lines;
            }

            EXPECT_EQ(lines, 12u);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // What gives no kind is a deed, as every entry was before there were others.
        TEST(rules_test, deeds_default_to_kind_deed)
        {
            MockLogger logger;
            RulesFixture f;

            std::vector<RuleAchievement> achievements;
            ASSERT_TRUE(f.rules.achievements(achievements)) << firstError(f.rules);

            size_t deeds = 0;

            for (const auto& a : achievements)
            {
                const std::string id = textOf(a.fields, "id");

                if (id.rfind("guide.", 0) == 0 or id.rfind("lore.", 0) == 0)
                {
                    EXPECT_NE(a.kind, RuleKind::Deed) << id;

                    continue;
                }

                EXPECT_EQ(a.kind, RuleKind::Deed) << id;
                EXPECT_EQ(textOf(a.fields, "kind"), "deed") << id;
                EXPECT_FALSE(textOf(a.fields, "name").empty()) << id;
                EXPECT_FALSE(textOf(a.fields, "entry").empty()) << id;
                EXPECT_TRUE(a.until.empty()) << id;

                ++deeds;
            }

            EXPECT_EQ(deeds, 11u);

            for (const char* id : {"first.coin", "carters.road", "sworn", "wood.king"})
            {
                const bool found = std::any_of(achievements.begin(), achievements.end(), [&](const RuleAchievement& a) {
                    return textOf(a.fields, "id") == id and a.kind == RuleKind::Deed;
                });

                EXPECT_TRUE(found) << id;
            }
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // The town as its page shows it: nine places of three levels, what each gives at the level
        // it stands at, and what its next level asks of him against what he has.
        TEST(rules_test, town_places_and_levels)
        {
            MockLogger logger;
            RulesFixture f;

            ElementMap character = townsman();
            character["coin"] = ElementType{30};
            character["firewood"] = ElementType{20};
            character["clay"] = ElementType{20};
            character["town.market"] = ElementType{1};
            character["town.mill"] = ElementType{3};

            RuleTown town;
            ASSERT_TRUE(f.rules.town(9.0f, character, town)) << firstError(f.rules);

            ASSERT_EQ(town.places.size(), 11u);
            EXPECT_FALSE(town.opened.empty());
            EXPECT_FALSE(town.raisedLine.empty());
            EXPECT_EQ(town.giftStat, "coin");

            for (const auto& place : town.places)
            {
                EXPECT_EQ(intOf(place.fields, "most"), 3) << textOf(place.fields, "id");
                EXPECT_FALSE(textOf(place.fields, "name").empty());
                EXPECT_FALSE(textOf(place.fields, "glyph").empty());
            }

            auto placed = [&](const std::string& id) -> const RulePlace& {
                for (const auto& place : town.places)
                {
                    if (textOf(place.fields, "id") == id)
                        return place;
                }

                ADD_FAILURE() << "no place " << id;

                return town.places.front();
            };

            // Not built: nothing given, its first level asked
            const RulePlace& yard = placed("yard");

            EXPECT_EQ(intOf(yard.fields, "level"), 0);
            EXPECT_EQ(textOf(yard.fields, "gives"), "");
            EXPECT_EQ(textOf(yard.fields, "line"), "NOT YET BUILT");
            EXPECT_EQ(textOf(yard.fields, "nextWork"), "raise.yard.1");
            EXPECT_EQ(intOf(yard.fields, "nextMonths"), 6);
            EXPECT_EQ(intOf(yard.fields, "nextCoin"), 5);
            EXPECT_EQ(textOf(yard.fields, "nextCosts"), "6 mo · 5 coin · 20 Firewood · 20 Clay");
            EXPECT_FALSE(flag(yard.fields, "nextLocked"));

            // At its first level: what it gives, and a second that asks more than he has
            const RulePlace& market = placed("market");

            EXPECT_EQ(intOf(market.fields, "level"), 1);
            EXPECT_FALSE(textOf(market.fields, "gives").empty());
            EXPECT_FALSE(textOf(market.fields, "tale").empty());
            ASSERT_EQ(market.given.size(), 1u);
            EXPECT_EQ(textOf(market.given[0], "gives"), textOf(market.fields, "gives"));
            EXPECT_TRUE(yard.given.empty());

            // Every level it has, reached or not: the market's first, and its two still to raise
            ASSERT_EQ(market.tiers.size(), 3u);
            EXPECT_TRUE(flag(market.tiers[0], "reached"));
            EXPECT_FALSE(flag(market.tiers[1], "reached"));
            EXPECT_FALSE(flag(market.tiers[2], "reached"));
            EXPECT_EQ(textOf(market.tiers[1], "built"), "The market hall");
            EXPECT_EQ(textOf(market.tiers[1], "gives"), textOf(market.fields, "nextGives"));
            ASSERT_EQ(yard.tiers.size(), 3u);
            EXPECT_FALSE(flag(yard.tiers[0], "reached"));
            EXPECT_EQ(textOf(market.fields, "nextWork"), "raise.market.2");
            EXPECT_TRUE(flag(market.fields, "nextLocked"));

            // The hall asks a little coin, and mostly resources: the berries and gems he lacks lock it
            ASSERT_EQ(market.gaps.size(), 5u);
            EXPECT_EQ(textOf(market.gaps[0], "stat"), "coin");
            EXPECT_EQ(intOf(market.gaps[0], "current"), 30);
            EXPECT_EQ(intOf(market.gaps[0], "needed"), 15);
            EXPECT_EQ(textOf(market.gaps[1], "stat"), "berries");
            EXPECT_EQ(intOf(market.gaps[1], "needed"), 40);
            EXPECT_EQ(textOf(market.gaps[2], "stat"), "firewood");
            EXPECT_EQ(intOf(market.gaps[2], "current"), 20);
            EXPECT_EQ(intOf(market.gaps[2], "needed"), 40);
            EXPECT_EQ(textOf(market.gaps[3], "stat"), "gems");
            EXPECT_EQ(intOf(market.gaps[3], "needed"), 2);
            EXPECT_EQ(textOf(market.gaps[4], "stat"), "guile");

            // At its last: nothing more to raise
            const RulePlace& mill = placed("mill");

            EXPECT_EQ(intOf(mill.fields, "level"), 3);
            EXPECT_EQ(textOf(mill.fields, "nextWork"), "");
            EXPECT_TRUE(mill.gaps.empty());

            // And all it gives: every level reached, the first first, the last being what its tile says
            ASSERT_EQ(mill.given.size(), 3u);
            EXPECT_EQ(intOf(mill.given[0], "level"), 1);
            EXPECT_EQ(textOf(mill.given[0], "gives"), "Help at the Mill pays 4 more coin");
            EXPECT_EQ(textOf(mill.given[1], "gives"), "Every life has a slot for one more familiar");
            EXPECT_EQ(textOf(mill.given[2], "gives"), textOf(mill.fields, "gives"));
            EXPECT_EQ(textOf(mill.given[2], "built"), "The great millstones");
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // What lives left to a place at their death is counted toward its next level: the page and
        // the work that raises it ask that much less coin, never less than none. What the level
        // asks of resources is asked as before.
        TEST(rules_test, town_fund_lowers_the_coin_asked)
        {
            MockLogger logger;
            RulesFixture f;

            // What the mill's wheel and the yard's racks ask of resources, so that only coin can lock them
            ElementMap character = townsman();
            character["coin"] = ElementType{2};
            character["stone"] = ElementType{20};
            character["water"] = ElementType{20};
            character["firewood"] = ElementType{20};
            character["clay"] = ElementType{20};
            character["fund.mill"] = ElementType{3};
            character["fund.chapel"] = ElementType{84};

            RuleTown town;
            ASSERT_TRUE(f.rules.town(9.0f, character, town)) << firstError(f.rules);

            for (const auto& place : town.places)
            {
                const std::string id = textOf(place.fields, "id");

                if (id == "mill")
                {
                    EXPECT_EQ(intOf(place.fields, "nextCoin"), 2);
                    EXPECT_EQ(intOf(place.fields, "fund"), 3);
                    EXPECT_FALSE(flag(place.fields, "nextLocked"));
                }

                if (id == "chapel")
                    EXPECT_EQ(intOf(place.fields, "nextCoin"), 0);

                if (id == "yard")
                {
                    EXPECT_EQ(intOf(place.fields, "nextCoin"), 5);
                    EXPECT_TRUE(flag(place.fields, "nextLocked"));
                }
            }

            RuleActivity mill = activityAt(f.rules, 9.0f, character, "raise.mill.1");

            ASSERT_EQ(mill.costs.size(), 3u);
            EXPECT_EQ(textOf(mill.costs[0], "stat"), "coin");
            EXPECT_EQ(intOf(mill.costs[0], "amount"), 2);
            EXPECT_EQ(textOf(mill.costs[1], "stat"), "stone");
            EXPECT_EQ(intOf(mill.costs[1], "amount"), 20);
            EXPECT_FALSE(flag(mill.fields, "locked"));

            // Paid for whole: the work asks no coin at all, and still the resources
            RuleActivity chapel = activityAt(f.rules, 9.0f, character, "raise.chapel.1");

            ASSERT_FALSE(chapel.costs.empty());

            for (const auto& cost : chapel.costs)
                EXPECT_NE(textOf(cost, "stat"), "coin");
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // A work that raises a place is listed while the place is one level below, to who knows the
        // town: hidden before, gone after.
        TEST(rules_test, raise_rows_are_listed_only_at_the_level_below)
        {
            MockLogger logger;
            RulesFixture f;

            // Nobody raises a town he does not know
            EXPECT_FALSE(flag(activityAt(f.rules, 9.0f, boy(), "raise.market.1").fields, "listed"));

            ElementMap character = townsman();

            RuleActivity first = activityAt(f.rules, 9.0f, character, "raise.market.1");

            EXPECT_TRUE(flag(first.fields, "listed"));
            EXPECT_EQ(textOf(first.fields, "group"), "The town");
            EXPECT_EQ(textOf(first.fields, "raises"), "market");
            EXPECT_EQ(intOf(first.fields, "level"), 1);
            EXPECT_EQ(intOf(first.fields, "months"), 6);
            EXPECT_FALSE(flag(activityAt(f.rules, 9.0f, character, "raise.market.2").fields, "listed"));
            EXPECT_FALSE(flag(activityAt(f.rules, 9.0f, character, "raise.market.3").fields, "listed"));

            character["town.market"] = ElementType{1};

            EXPECT_FALSE(flag(activityAt(f.rules, 9.0f, character, "raise.market.1").fields, "listed"));
            EXPECT_TRUE(flag(activityAt(f.rules, 9.0f, character, "raise.market.2").fields, "listed"));

            // What it takes of him besides coin is a cost like any other: resources, gems, a stat
            RuleActivity second = activityAt(f.rules, 9.0f, character, "raise.market.2");

            ASSERT_EQ(second.costs.size(), 5u);
            EXPECT_EQ(textOf(second.costs[0], "stat"), "coin");
            EXPECT_EQ(intOf(second.costs[0], "amount"), 15);
            EXPECT_EQ(textOf(second.costs[1], "stat"), "berries");
            EXPECT_EQ(intOf(second.costs[1], "amount"), 40);
            EXPECT_EQ(textOf(second.costs[2], "stat"), "firewood");
            EXPECT_EQ(textOf(second.costs[3], "stat"), "gems");
            EXPECT_EQ(intOf(second.costs[3], "amount"), 2);
            EXPECT_EQ(textOf(second.costs[4], "stat"), "guile");
            EXPECT_TRUE(flag(second.fields, "locked"));

            character["town.market"] = ElementType{3};

            for (const char* id : {"raise.market.1", "raise.market.2", "raise.market.3"})
                EXPECT_FALSE(flag(activityAt(f.rules, 9.0f, character, id).fields, "listed")) << id;

            // Years of his life, said on the tile and handed to the scene in months
            RuleActivity almshouse = activityAt(f.rules, 9.0f, character, "raise.chapel.3");

            EXPECT_EQ(intOf(almshouse.fields, "months"), 0);
            EXPECT_EQ(intOf(almshouse.fields, "yearMonths"), 24);
            EXPECT_EQ(textOf(almshouse.fields, "each"), "COSTS 2 YEARS");
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // A level of the town is paid mainly in resources: the first of every place asks a basic
        // resource and little coin, the second gems too.
        TEST(rules_test, a_level_asks_resources_more_than_coin)
        {
            MockLogger logger;
            RulesFixture f;

            const ElementMap character = townsman();
            const std::vector<std::string> basics = {"berries", "water", "firewood", "stone", "clay"};

            std::vector<RuleActivity> activities;
            ASSERT_TRUE(f.rules.activities(9.0f, character, activities)) << firstError(f.rules);

            for (const char* place : {"market", "mill", "smithy", "yard", "gate", "chapel", "collegium", "harrow", "inn", "lodge"})
            {
                const std::string id = std::string("raise.") + place + ".1";
                const RuleActivity* first = nullptr;

                for (const auto& a : activities)
                {
                    if (textOf(a.fields, "id") == id)
                        first = &a;
                }

                ASSERT_NE(first, nullptr) << id;

                bool basic = false;
                int coin = 0;

                for (const auto& cost : first->costs)
                {
                    const std::string stat = textOf(cost, "stat");

                    if (stat == "coin")
                        coin = intOf(cost, "amount");

                    for (const auto& b : basics)
                        basic = basic or stat == b;
                }

                EXPECT_TRUE(basic) << id;
                EXPECT_LE(coin, 10) << id;
            }

            RuleActivity loft = activityAt(f.rules, 9.0f, character, "raise.mill.2");
            int gems = 0;

            for (const auto& cost : loft.costs)
            {
                if (textOf(cost, "stat") == "gems")
                    gems = intOf(cost, "amount");
            }

            EXPECT_EQ(gems, 2);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // A world whose fund holds more coin than a level now asks: no coin is asked, never less
        // than none, and what the level asks of resources is untouched by the coin left over.
        TEST(rules_test, an_old_fund_never_goes_below_nothing)
        {
            MockLogger logger;
            RulesFixture f;

            ElementMap character = townsman();
            character["fund.mill"] = ElementType{15};

            RuleTown town;
            ASSERT_TRUE(f.rules.town(9.0f, character, town)) << firstError(f.rules);

            bool seen = false;

            for (const auto& place : town.places)
            {
                if (textOf(place.fields, "id") != "mill")
                    continue;

                seen = true;
                EXPECT_EQ(intOf(place.fields, "nextCoin"), 0);
                EXPECT_EQ(intOf(place.fields, "fund"), 15);

                // The fund pays the wheel's 5 coin and no more: the rest stays in it
                ASSERT_EQ(place.spends.size(), 1u);
                EXPECT_EQ(textOf(place.spends[0], "stat"), "coin");
                EXPECT_EQ(intOf(place.spends[0], "amount"), 5);

                for (const auto& gap : place.gaps)
                    EXPECT_NE(textOf(gap, "stat"), "coin");
            }

            EXPECT_TRUE(seen);

            RuleActivity wheel = activityAt(f.rules, 9.0f, character, "raise.mill.1");
            int stone = 0;
            int water = 0;

            for (const auto& cost : wheel.costs)
            {
                const std::string stat = textOf(cost, "stat");

                EXPECT_NE(stat, "coin");
                EXPECT_GT(intOf(cost, "amount"), 0) << stat;

                if (stat == "stone")
                    stone = intOf(cost, "amount");

                if (stat == "water")
                    water = intOf(cost, "amount");
            }

            EXPECT_EQ(stone, 20);
            EXPECT_EQ(water, 20);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // Only things can be left to a place at his death: the gate's tower asks stone, firewood
        // and Renown, and of the three he holds only the stone and the firewood are left to it.
        TEST(rules_test, a_standing_is_never_left_to_a_place)
        {
            MockLogger logger;
            RulesFixture f;

            ElementMap character = townsman();
            character["town.gate"] = ElementType{2};
            character["renown"] = ElementType{3};
            character["stone"] = ElementType{10};
            character["firewood"] = ElementType{5};

            RuleTown town;
            ASSERT_TRUE(f.rules.town(9.0f, character, town)) << firstError(f.rules);

            bool seen = false;

            for (const auto& place : town.places)
            {
                if (textOf(place.fields, "id") != "gate")
                    continue;

                seen = true;

                // Renown is asked, but not left
                bool asked = false;

                for (const auto& gap : place.gaps)
                    asked = asked or textOf(gap, "stat") == "renown";

                EXPECT_TRUE(asked);

                std::map<std::string, int> left;

                for (const auto& l : place.leaves)
                    left[textOf(l, "stat")] = intOf(l, "amount");

                EXPECT_EQ(left.count("renown"), 0u);
                EXPECT_EQ(left["stone"], 10);
                EXPECT_EQ(left["firewood"], 5);
                EXPECT_EQ(textOf(place.fields, "leaveLine").find("renown"), std::string::npos) << textOf(place.fields, "leaveLine");
            }

            EXPECT_TRUE(seen);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // Going to the market is for who has not been, in this life: it opens the town's page.
        TEST(rules_test, the_market_visit_opens_the_town_once_a_life)
        {
            MockLogger logger;
            RulesFixture f;

            const float eighth = 6.0f + 8.0f / 12.0f;

            RuleActivity visit = activityAt(f.rules, eighth, child(), "tomarket");

            EXPECT_TRUE(flag(visit.fields, "listed"));
            EXPECT_FALSE(flag(visit.fields, "locked"));
            EXPECT_EQ(intOf(visit.fields, "months"), 1);

            // Not before his eighth month, and no age closes it: no boy goes without
            EXPECT_FALSE(flag(activityAt(f.rules, 6.0f + 7.0f / 12.0f, child(), "tomarket").fields, "listed"));
            EXPECT_TRUE(flag(activityAt(f.rules, 6.0f + 7.0f / 12.0f, child(), "tomarket").fields, "locked"));
            EXPECT_TRUE(flag(activityAt(f.rules, 25.0f, boy(), "tomarket").fields, "listed"));
            EXPECT_FALSE(flag(activityAt(f.rules, 25.0f, boy(), "tomarket").fields, "closed"));
            EXPECT_EQ(intOf(activityAt(f.rules, eighth, child(), "tomarket").fields, "closesIn"), -1);

            // The page wants it seen, and no other row of a first day
            EXPECT_TRUE(flag(visit.fields, "featured"));
            EXPECT_FALSE(flag(activityAt(f.rules, 7.0f, boy(), "carters").fields, "featured"));

            // It turns a life, as a class does: such a row says so, and tells what it does
            for (const char* id : {"tomarket", "keep", "collegium", "hand", "greenwood"})
            {
                RuleActivity turning = activityAt(f.rules, 16.0f, boy(), id);

                EXPECT_TRUE(flag(turning.fields, "major")) << id;
                EXPECT_FALSE(textOf(turning.fields, "about").empty()) << id;
            }

            // A class says in red that it is a major choice, for life; the last door has none to close
            for (const char* id : {"keep", "collegium", "hand"})
                EXPECT_EQ(textOf(activityAt(f.rules, 16.0f, boy(), id).fields, "caution").rfind("This is a major choice: once you join, it's for life, and the other classes close.", 0), 0u) << id;

            EXPECT_EQ(textOf(activityAt(f.rules, 16.0f, boy(), "greenwood").fields, "caution"), "This is a major choice: once you join, it's for life.");
            EXPECT_EQ(textOf(activityAt(f.rules, 16.0f, boy(), "tomarket").fields, "caution"), "");

            EXPECT_FALSE(flag(activityAt(f.rules, 7.0f, boy(), "carters").fields, "major"));
            EXPECT_EQ(textOf(activityAt(f.rules, 7.0f, boy(), "carters").fields, "about"), "The carters always need someone to load the wagons, and they feed you while you work.");
            ASSERT_EQ(visit.gains.size(), 1u);
            EXPECT_EQ(textOf(visit.gains[0], "stat"), "town_known");

            // Known, it has no row; and nothing is left of the three months that used to open it
            EXPECT_FALSE(flag(activityAt(f.rules, 7.0f, townsman(), "tomarket").fields, "listed"));

            std::vector<RuleActivity> activities;
            ASSERT_TRUE(f.rules.activities(7.0f, boy(), activities)) << firstError(f.rules);

            for (const auto& a : activities)
                EXPECT_NE(textOf(a.fields, "id"), "explore");
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // What is bought on the spot is the market's: its rows say so, and the Life page keeps the
        // tile that goes there.
        TEST(rules_test, market_rows_carry_their_place)
        {
            MockLogger logger;
            RulesFixture f;

            ElementMap character = townsman();
            character["coin"] = ElementType{5};

            RuleActivity buy = activityAt(f.rules, 9.0f, character, "buy.rations");

            EXPECT_EQ(textOf(buy.fields, "place"), "market");
            EXPECT_TRUE(flag(buy.fields, "listed"));
            EXPECT_FALSE(flag(buy.fields, "locked"));

            // No tile of the Life page goes to the market: its tab does
            {
                std::vector<RuleActivity> all;
                ASSERT_TRUE(f.rules.activities(9.0f, character, all));

                for (const auto& a : all)
                    EXPECT_NE(textOf(a.fields, "id"), "goto.market");
            }

            // What the charter brings to the market is not there before it
            EXPECT_FALSE(flag(activityAt(f.rules, 9.0f, character, "buy.sword").fields, "listed"));

            character["town.market"] = ElementType{3};

            RuleActivity sword = activityAt(f.rules, 9.0f, character, "buy.sword");

            EXPECT_TRUE(flag(sword.fields, "listed"));
            EXPECT_EQ(textOf(sword.fields, "place"), "market");

            // Reagents are a mage's, until the Collegium sells them at its gate
            EXPECT_FALSE(flag(activityAt(f.rules, 9.0f, character, "buy.reagents").fields, "listed"));

            character["town.collegium"] = ElementType{1};
            EXPECT_TRUE(flag(activityAt(f.rules, 9.0f, character, "buy.reagents").fields, "listed"));
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // The wooden blade is three short steps: bought at the market from seven and two months,
        // then two months of practice that has no row before he holds one, then the edge or the
        // point, one of the two. A yard that has its racks lends the blade, and none is sold.
        TEST(rules_test, the_wooden_blade_is_bought_practiced_then_chosen)
        {
            MockLogger logger;
            RulesFixture f;

            auto listed = [&](float age, const ElementMap& character, const char* id) {
                return flag(activityAt(f.rules, age, character, id).fields, "listed");
            };

            ElementMap character = townsman();
            character["coin"] = ElementType{8};

            // Too young for the stall to show it; two months past seven it is there, and nothing of
            // the rest
            EXPECT_FALSE(listed(7.0f, character, "buy.blade"));
            EXPECT_TRUE(listed(7.25f, character, "buy.blade"));

            RuleActivity buy = activityAt(f.rules, 8.0f, character, "buy.blade");

            EXPECT_TRUE(flag(buy.fields, "listed"));
            EXPECT_FALSE(flag(buy.fields, "locked"));
            EXPECT_EQ(textOf(buy.fields, "place"), "market");
            EXPECT_EQ(intOf(buy.fields, "months"), 0);
            EXPECT_EQ(intOf(buy.fields, "uses"), 1);

            EXPECT_FALSE(listed(8.0f, character, "blades"));
            EXPECT_FALSE(listed(8.0f, character, "slashes"));
            EXPECT_FALSE(listed(8.0f, character, "stabs"));

            // Short of its price, it is there and shut
            character["coin"] = ElementType{7};
            EXPECT_TRUE(flag(activityAt(f.rules, 8.0f, character, "buy.blade").fields, "locked"));

            // Bought: the stall has no other, and the practice is his
            character["wooden_blade"] = ElementType{1};
            f.rules.done = {{"buy.blade", ElementType{1}}};

            EXPECT_FALSE(listed(8.0f, character, "buy.blade"));

            RuleActivity practice = activityAt(f.rules, 8.0f, character, "blades");

            EXPECT_TRUE(flag(practice.fields, "listed"));
            EXPECT_FALSE(flag(practice.fields, "locked"));
            EXPECT_EQ(intOf(practice.fields, "months"), 2);
            EXPECT_EQ(intOf(practice.fields, "uses"), 1);
            EXPECT_FALSE(listed(8.0f, character, "slashes"));

            // Practiced: a point of Arms and the basics, which open the two
            RuleForecast forecast;
            ASSERT_TRUE(f.rules.forecast(8.0f, character, "blades", 0, forecast)) << firstError(f.rules);

            EXPECT_EQ(intOf(forecast.atTerm, "arms"), 1);
            EXPECT_EQ(intOf(forecast.atTerm, "blade_basics"), 1);

            character["arms"] = ElementType{1};
            character["blade_basics"] = ElementType{1};
            f.rules.done = {{"buy.blade", ElementType{1}}, {"blades", ElementType{1}}};

            EXPECT_FALSE(listed(8.2f, character, "blades"));
            EXPECT_TRUE(listed(8.2f, character, "slashes"));
            EXPECT_TRUE(listed(8.2f, character, "stabs"));

            // The edge: his Strength, his Arms and the slash. The point is gone
            ASSERT_TRUE(f.rules.forecast(8.2f, character, "slashes", 0, forecast)) << firstError(f.rules);

            EXPECT_EQ(intOf(forecast.atTerm, "str"), 7);
            EXPECT_EQ(intOf(forecast.atTerm, "arms"), 2);
            EXPECT_EQ(intOf(forecast.atTerm, "slash_learned"), 1);

            ASSERT_TRUE(f.rules.forecast(8.2f, character, "stabs", 0, forecast)) << firstError(f.rules);

            EXPECT_EQ(intOf(forecast.atTerm, "dex"), 7);
            EXPECT_EQ(intOf(forecast.atTerm, "thrust_learned"), 1);

            // Each says before it is taken what it gives up, and its term writes it as a loss
            EXPECT_EQ(textOf(activityAt(f.rules, 8.2f, character, "slashes").fields, "caution"),
                      "One or the other: once this is done, Practice Thrusts is closed to you for this life.");
            EXPECT_EQ(textOf(activityAt(f.rules, 8.2f, character, "stabs").fields, "caution"),
                      "One or the other: once this is done, Practice Slashes is closed to you for this life.");
            EXPECT_EQ(textOf(activityAt(f.rules, 8.2f, character, "blades").fields, "caution"), "");

            ASSERT_TRUE(f.rules.forecast(8.2f, character, "slashes", 0, forecast)) << firstError(f.rules);

            ASSERT_EQ(forecast.entries.size(), 2u);
            EXPECT_EQ(textOf(forecast.entries[0], "text"), "Practice Slashes");
            EXPECT_EQ(textOf(forecast.entries[1], "text"), "One or the other: Practice Thrusts is closed to you");
            EXPECT_EQ(textOf(forecast.entries[1], "kind"), "loss");

            // What closes nothing writes its own line and no more
            f.rules.done = {{"buy.blade", ElementType{1}}};
            ASSERT_TRUE(f.rules.forecast(8.0f, character, "blades", 0, forecast)) << firstError(f.rules);
            EXPECT_EQ(forecast.entries.size(), 1u);

            f.rules.done = {{"buy.blade", ElementType{1}}, {"blades", ElementType{1}}, {"slashes", ElementType{1}}};

            EXPECT_FALSE(listed(8.5f, character, "slashes"));
            EXPECT_FALSE(listed(8.5f, character, "stabs"));

            // No age closes any of it: a grown man who never held one may still begin there
            f.rules.done = {};
            EXPECT_TRUE(listed(20.0f, townsman(), "buy.blade"));
            EXPECT_EQ(intOf(activityAt(f.rules, 20.0f, townsman(), "buy.blade").fields, "closesIn"), -1);

            ElementMap late = townsman();
            late["wooden_blade"] = ElementType{1};
            f.rules.done = {{"buy.blade", ElementType{1}}};

            EXPECT_TRUE(listed(20.0f, late, "blades"));
            EXPECT_EQ(intOf(activityAt(f.rules, 20.0f, late, "blades").fields, "closesIn"), -1);

            // The basics he already has (the yard gives them too) are not his twice: the point of
            // Arms is all the practice brings then
            late["blade_basics"] = ElementType{1};

            ASSERT_TRUE(f.rules.forecast(20.0f, late, "blades", 0, forecast)) << firstError(f.rules);

            EXPECT_EQ(intOf(forecast.atTerm, "arms"), 1);
            EXPECT_EQ(intOf(forecast.atTerm, "blade_basics"), 1);
            EXPECT_EQ(activityAt(f.rules, 20.0f, late, "blades").gains.size(), 1u);

            f.rules.done = {};

            // The yard's racks: the practice is his with no blade of his own, and the stall sells none
            ElementMap lent = townsman();
            lent["town.yard"] = ElementType{1};

            EXPECT_FALSE(listed(8.0f, lent, "buy.blade"));
            EXPECT_TRUE(listed(8.0f, lent, "blades"));
            EXPECT_FALSE(flag(activityAt(f.rules, 8.0f, lent, "blades").fields, "locked"));
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // The Flood comes the month he is eleven, in every life: the year to answer it, and one
        // answer of three. What he answers is his for the life: who stood in the mill's water
        // mends its wheel for half, and who looted is not given the sacristan's keys.
        TEST(rules_test, the_flood_is_answered_once_at_eleven)
        {
            MockLogger logger;
            RulesFixture f;

            auto listed = [&](float age, const ElementMap& character, const char* id) {
                return flag(activityAt(f.rules, age, character, id).fields, "listed");
            };

            ElementMap character = townsman();
            character["str"] = ElementType{7};
            character["dex"] = ElementType{7};
            character["letters"] = ElementType{2};
            character["coin"] = ElementType{30};

            const float month = 1.0f / 12.0f;

            // Not before, and not after its year: time enough for who was at a long work
            for (const char* id : {"flood.sandbags", "flood.rescue", "flood.salvage"})
            {
                EXPECT_FALSE(listed(11.0f - month, character, id)) << id;
                EXPECT_TRUE(listed(11.0f, character, id)) << id;
                EXPECT_TRUE(listed(11.0f + 10.0f * month, character, id)) << id;
                EXPECT_FALSE(listed(11.0f + 11.0f * month, character, id)) << id;
            }

            // A row whatever he has: what he cannot do he sees he cannot
            ElementMap weak = townsman();
            weak["str"] = ElementType{4};

            EXPECT_TRUE(listed(11.0f, weak, "flood.sandbags"));
            EXPECT_TRUE(flag(activityAt(f.rules, 11.0f, weak, "flood.sandbags").fields, "locked"));

            // Each says what it closes; the looting closes the keys besides
            EXPECT_EQ(textOf(activityAt(f.rules, 11.0f, character, "flood.sandbags").fields, "caution"),
                      "One or the other: once this is done, Pull Folk from the Water and Salvage the Wreck are closed to you for this life. "
                      "Leave the Flood unanswered, and Carry for the Watch, Take the Sacristan's Keys and Run with the Bellmoor Boys are closed to you for this life.");
            EXPECT_NE(textOf(activityAt(f.rules, 11.0f, character, "flood.salvage").fields, "caution").find("Take the Sacristan's Keys"), std::string::npos);

            // The coin the mill's wheel asks: 0 when it asks none
            auto wheelCoin = [&](float age, const ElementMap& who) {
                int coin = 0;

                for (const auto& cost : activityAt(f.rules, age, who, "raise.mill.1").costs)
                {
                    if (textOf(cost, "stat") == "coin")
                        coin = intOf(cost, "amount");
                }

                return coin;
            };

            // The mill's wheel, before: five coin
            EXPECT_EQ(wheelCoin(11.0f, character), 5);

            // The sandbags: the others are gone, and the wheel asks no coin
            RuleForecast forecast;
            ASSERT_TRUE(f.rules.forecast(11.0f, character, "flood.sandbags", 0, forecast)) << firstError(f.rules);

            EXPECT_EQ(intOf(forecast.atTerm, "str"), 8);
            EXPECT_EQ(intOf(forecast.atTerm, "mill_saved"), 1);
            ASSERT_EQ(forecast.entries.size(), 3u);

            ElementMap saved = character;
            saved["mill_saved"] = ElementType{1};
            f.rules.done = {{"flood.sandbags", ElementType{1}}};

            EXPECT_FALSE(listed(11.0f + month, saved, "flood.rescue"));
            EXPECT_FALSE(listed(11.0f + month, saved, "flood.salvage"));
            EXPECT_EQ(wheelCoin(11.5f, saved), 0);
            EXPECT_TRUE(listed(12.0f, saved, "keys"));

            // The looting: the keys are not his, the forbidden shelf is
            f.rules.done = {{"flood.salvage", ElementType{1}}};

            EXPECT_FALSE(listed(12.0f, character, "keys"));
            EXPECT_TRUE(listed(12.0f, character, "shelf"));
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // A lead shows the next thing and gives no stat: the forge to who worked the smithy, the
        // tunnel to who roamed the town twice. What a task closes besides its own choice is
        // written plainly: the Boys' oath shuts the Watch, turning them in shuts the Hidden Hand.
        TEST(rules_test, a_childhood_of_leads_and_forks)
        {
            MockLogger logger;
            RulesFixture f;

            auto listed = [&](float age, const ElementMap& character, const char* id) {
                return flag(activityAt(f.rules, age, character, id).fields, "listed");
            };

            ElementMap character = townsman();
            character["str"] = ElementType{9};
            character["dex"] = ElementType{8};

            // Nothing leads to them yet
            EXPECT_FALSE(listed(10.0f, character, "forge"));
            EXPECT_FALSE(listed(11.0f, character, "tunnel"));
            EXPECT_FALSE(listed(10.5f, character, "temper"));
            EXPECT_TRUE(listed(10.5f, character, "drive"));

            f.rules.done = {{"smithy", ElementType{1}}, {"roam", ElementType{1}}};

            EXPECT_TRUE(listed(10.0f, character, "forge"));
            EXPECT_FALSE(listed(11.0f, character, "tunnel"));

            f.rules.done = {{"smithy", ElementType{1}}, {"roam", ElementType{2}}};

            EXPECT_TRUE(listed(11.0f, character, "tunnel"));

            // The forge seen: the tempering, or the carters' reins
            RuleForecast forecast;
            ASSERT_TRUE(f.rules.forecast(10.0f, character, "forge", 0, forecast)) << firstError(f.rules);
            EXPECT_EQ(intOf(forecast.atTerm, "forge_seen"), 1);
            EXPECT_EQ(intOf(forecast.atTerm, "str"), 9);

            character["forge_seen"] = ElementType{1};
            character["tunnel_found"] = ElementType{1};

            EXPECT_TRUE(listed(10.5f, character, "temper"));
            EXPECT_FALSE(listed(10.5f, character, "smuggler"));
            EXPECT_TRUE(listed(11.0f, character, "smuggler"));

            f.rules.done = {{"smithy", ElementType{1}}, {"roam", ElementType{2}}, {"forge", ElementType{1}}, {"temper", ElementType{1}}};

            EXPECT_FALSE(listed(10.5f, character, "drive"));

            // The Boys: an oath, or a word to the sergeant
            f.rules.done = {{"boys", ElementType{1}}};

            EXPECT_TRUE(listed(12.0f, character, "oath"));
            EXPECT_TRUE(listed(12.0f, character, "turnin"));
            EXPECT_EQ(textOf(activityAt(f.rules, 12.0f, character, "oath").fields, "caution"),
                      "One or the other: once this is done, Carry for the Watch and Turn the Boys In are closed to you for this life.");

            ASSERT_TRUE(f.rules.forecast(12.0f, character, "oath", 0, forecast)) << firstError(f.rules);

            ASSERT_EQ(forecast.entries.size(), 3u);
            EXPECT_EQ(textOf(forecast.entries[1], "text"), "Carry for the Watch is closed to you");
            EXPECT_EQ(textOf(forecast.entries[2], "text"), "One or the other: Turn the Boys In is closed to you");

            f.rules.done = {{"boys", ElementType{1}}, {"oath", ElementType{1}}};

            EXPECT_FALSE(listed(12.0f, character, "watch"));
            EXPECT_FALSE(listed(12.5f, character, "turnin"));
            EXPECT_TRUE(listed(12.5f, character, "hand"));

            f.rules.done = {{"boys", ElementType{1}}, {"turnin", ElementType{1}}};

            EXPECT_FALSE(listed(12.5f, character, "hand"));
            EXPECT_FALSE(listed(12.5f, character, "oath"));
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // The Lord's Levy comes the month he is fifteen: six months to answer, one answer. Who
        // marched is asked less Arms by the Keep; who hid is a deserter, shut out of the Keep, and
        // the Greenwood is a door like the others for him at sixteen.
        TEST(rules_test, the_levy_at_fifteen_turns_the_classes)
        {
            MockLogger logger;
            RulesFixture f;

            auto listed = [&](float age, const ElementMap& character, const char* id) {
                return flag(activityAt(f.rules, age, character, id).fields, "listed");
            };

            ElementMap character = townsman();
            character["str"] = ElementType{14};
            character["arms"] = ElementType{2};
            character["discipline"] = ElementType{2};
            character["coin"] = ElementType{30};

            const float month = 1.0f / 12.0f;

            // Called the spring before he is fifteen: nine months to begin an answer
            for (const char* id : {"levy.march", "levy.pay", "levy.hide"})
            {
                EXPECT_FALSE(listed(14.75f - month, character, id)) << id;
                EXPECT_TRUE(listed(14.75f, character, id)) << id;
                EXPECT_TRUE(listed(15.0f + 5.0f * month, character, id)) << id;
                EXPECT_FALSE(listed(15.0f + 6.0f * month, character, id)) << id;
            }

            // The scutage is paid at once, and closes the other two
            EXPECT_EQ(intOf(activityAt(f.rules, 15.0f, character, "levy.pay").fields, "months"), 0);
            EXPECT_EQ(intOf(activityAt(f.rules, 15.0f, character, "levy.pay").costs[0], "amount"), 20);

            // Two of Arms are not the Keep's three, nor is the Greenwood a door while others stand
            EXPECT_TRUE(flag(activityAt(f.rules, 16.0f, character, "keep").fields, "locked"));
            EXPECT_FALSE(listed(16.0f, character, "greenwood"));

            // Marched: the Keep asks two
            ElementMap marched = character;
            marched["levy_served"] = ElementType{1};
            f.rules.done = {{"levy.march", ElementType{1}}};

            EXPECT_TRUE(listed(16.0f, marched, "keep"));
            EXPECT_FALSE(flag(activityAt(f.rules, 16.0f, marched, "keep").fields, "locked"));

            // Hid: no Keep, and the Greenwood at sixteen with the other doors still open
            ElementMap hid = character;
            hid["deserter"] = ElementType{1};
            f.rules.done = {{"levy.hide", ElementType{1}}};

            EXPECT_FALSE(listed(16.0f, hid, "keep"));
            EXPECT_TRUE(listed(16.0f, hid, "collegium"));
            EXPECT_FALSE(listed(15.5f, hid, "greenwood"));
            EXPECT_TRUE(listed(16.0f, hid, "greenwood"));
            EXPECT_FALSE(flag(activityAt(f.rules, 16.0f, hid, "greenwood").fields, "locked"));
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // An event left unanswered is paid for. The month it is settled, who holds none of its
        // answers has stood by: he holds that, the log says so, and the rows the event names are
        // shut to him. Nothing else is: a second one shuts its own rows, and no class.
        // A player's report (2026-10-20): answering the Lord's Pardon left the Tithe Barn to burn.
        // The two are one or the other, as the Pardon's own `unless` says
        TEST(rules_test, the_pardon_and_the_barn_shut_each_other)
        {
            MockLogger logger;
            RulesFixture f;

            ElementMap outlaw = boy();
            outlaw["renegade"] = ElementType{1};
            outlaw["arms"] = ElementType{4};
            outlaw["stealth"] = ElementType{4};
            outlaw["renown"] = ElementType{3};

            const float age = 24.5f;

            f.rules.done = {{"greenwood", ElementType{1}}};

            EXPECT_TRUE(flag(activityAt(f.rules, age, outlaw, "barn").fields, "listed"));
            EXPECT_TRUE(flag(activityAt(f.rules, age, outlaw, "pardon").fields, "listed"));

            f.rules.done["pardon"] = ElementType{1};

            EXPECT_FALSE(flag(activityAt(f.rules, age, outlaw, "barn").fields, "listed"));

            f.rules.done = {{"greenwood", ElementType{1}}, {"barn", ElementType{1}}};

            EXPECT_FALSE(flag(activityAt(f.rules, age, outlaw, "pardon").fields, "listed"));
        }

        TEST(rules_test, an_event_stood_by_shuts_what_follows)
        {
            MockLogger logger;
            RulesFixture f;

            auto listed = [&](float age, const ElementMap& character, const char* id) {
                return flag(activityAt(f.rules, age, character, id).fields, "listed");
            };

            auto barred = [&](float age, const ElementMap& character, const char* id) {
                return textOf(activityAt(f.rules, age, character, id).fields, "barred");
            };

            auto wrote = [](const RuleMonth& month, const std::string& text) {
                for (const auto& e : month.entries)
                {
                    if (textOf(e, "text").find(text) != std::string::npos and textOf(e, "kind") == "loss")
                        return true;
                }

                return false;
            };

            const float month = 1.0f / 12.0f;
            const float settled = 12.0f;

            ElementMap character = townsman();
            character["str"] = ElementType{9};
            character["dex"] = ElementType{9};
            character["letters"] = ElementType{2};

            // The Flood is settled the month after its last answer could end: nothing held, he stood by
            RuleMonth passed;
            ASSERT_TRUE(f.rules.month(settled, character, passed)) << firstError(f.rules);

            EXPECT_EQ(intOf(passed.after, "stood_flood"), 1);
            EXPECT_EQ(intOf(passed.after, "stood_by"), 1);
            EXPECT_TRUE(wrote(passed, "The Flood came and went, and he stood by"));
            EXPECT_FALSE(wrote(passed, "no class will have him"));

            // Not the month before, when an answer begun in the last month is still to end; not who
            // answered; and a life that was never that month old has stood by at nothing
            ASSERT_TRUE(f.rules.month(settled - month, character, passed)) << firstError(f.rules);
            EXPECT_EQ(passed.after.count("stood_flood"), 0u);

            ASSERT_TRUE(f.rules.month(settled + month, character, passed)) << firstError(f.rules);
            EXPECT_EQ(passed.after.count("stood_flood"), 0u);
            EXPECT_TRUE(passed.entries.empty());

            ElementMap looter = character;
            looter["flood_looted"] = ElementType{1};

            ASSERT_TRUE(f.rules.month(settled, looter, passed)) << firstError(f.rules);
            EXPECT_EQ(passed.after.count("stood_flood"), 0u);
            EXPECT_TRUE(passed.entries.empty());

            // What it shuts: one row each way a life may go, and their doors on the clock
            ElementMap stood = character;
            stood["stood_flood"] = ElementType{1};
            stood["stood_by"] = ElementType{1};

            // (the Boys are for who has roamed the town)
            f.rules.done = {{"roam", ElementType{1}}};

            for (const char* id : {"watch", "keys", "boys"})
            {
                EXPECT_TRUE(listed(settled, character, id)) << id;
                EXPECT_EQ(barred(settled, character, id), "") << id;

                EXPECT_FALSE(listed(settled, stood, id)) << id;
                EXPECT_NE(barred(settled, stood, id).find("The Flood came and went"), std::string::npos) << id;
            }

            for (const char* id : {"carters", "letters", "roam", "keep", "collegium", "hand"})
                EXPECT_TRUE(listed(settled, stood, id)) << id;

            RecordList windows;
            ASSERT_TRUE(f.rules.windows(12.0f, stood, windows)) << firstError(f.rules);

            const ElementMap* watch = byId(windows, "watch");
            ASSERT_NE(watch, nullptr);
            EXPECT_EQ(textOf(*watch, "state"), "closed");
            EXPECT_NE(textOf(*watch, "note").find("he stood by"), std::string::npos);

            // Every answer of an event says what leaving it unanswered shuts, and no more once he
            // has stood by already
            const std::string first = textOf(activityAt(f.rules, 15.0f, character, "levy.pay").fields, "caution");
            const std::string second = textOf(activityAt(f.rules, 15.0f, stood, "levy.pay").fields, "caution");

            EXPECT_NE(first.find("Leave the Levy unanswered, and Train under Sergeant Edric, Lessons with Mara and Work the Night Market are closed to you for this life."), std::string::npos) << first;
            EXPECT_EQ(second, first);

            // The Levy is settled at sixteen, once a march begun in its last month is over. Who
            // marched has answered; who did nothing has stood by a second time
            ElementMap marched = stood;
            marched["levy_served"] = ElementType{1};

            ASSERT_TRUE(f.rules.month(16.0f, marched, passed)) << firstError(f.rules);
            EXPECT_EQ(passed.after.count("stood_levy"), 0u);
            EXPECT_EQ(intOf(passed.after, "stood_by"), 1);

            ASSERT_TRUE(f.rules.month(16.0f, stood, passed)) << firstError(f.rules);
            EXPECT_EQ(intOf(passed.after, "stood_levy"), 1);
            EXPECT_EQ(intOf(passed.after, "stood_by"), 2);
            EXPECT_TRUE(wrote(passed, "The Levy mustered without him"));
            EXPECT_FALSE(wrote(passed, "no class will have him"));

            // Twice: the Levy's rows are shut too, and the classes are still his, so the Greenwood
            // waits for them to close by their age
            ElementMap twice = stood;
            twice["stood_levy"] = ElementType{1};
            twice["stood_by"] = ElementType{2};

            for (const char* id : {"keep", "collegium", "hand"})
            {
                EXPECT_TRUE(listed(16.0f, twice, id)) << id;
                EXPECT_EQ(barred(16.0f, twice, id), "") << id;
            }

            for (const char* id : {"edric", "mara", "night"})
                EXPECT_NE(barred(16.0f, twice, id).find("The Levy mustered without him"), std::string::npos) << id;

            EXPECT_FALSE(listed(16.0f, twice, "greenwood"));

            ASSERT_TRUE(f.rules.windows(16.0f, twice, windows)) << firstError(f.rules);

            const ElementMap* keep = byId(windows, "keep");
            ASSERT_NE(keep, nullptr);
            EXPECT_NE(textOf(*keep, "state"), "closed");
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // A man of a class sees his two callings from the day he is of it, where the Inn stands, as
        // a child sees the classes. No number of events stood by shuts them. The old Lord's death
        // has an answer that asks nothing, so that standing by is his own doing.
        TEST(rules_test, the_callings_show_ahead_and_events_stood_by_never_shut_them)
        {
            MockLogger logger;
            RulesFixture f;

            auto listed = [&](float age, const ElementMap& character, const char* id) {
                return flag(activityAt(f.rules, age, character, id).fields, "listed");
            };

            auto wrote = [](const RuleMonth& month, const std::string& text) {
                for (const auto& e : month.entries)
                {
                    if (textOf(e, "text").find(text) != std::string::npos)
                        return true;
                }

                return false;
            };

            f.rules.done = {{"keep", ElementType{1}}};

            // No Inn, no calling to see
            EXPECT_FALSE(listed(17.5f, sworn(), "castellan"));

            ElementMap man = sworn();
            man["town.inn"] = ElementType{1};
            f.rules.inn = 1;

            for (const char* id : {"castellan", "cross"})
            {
                RuleActivity calling = activityAt(f.rules, 17.5f, man, id);

                EXPECT_TRUE(flag(calling.fields, "listed")) << id;
                EXPECT_TRUE(flag(calling.fields, "locked")) << id;
                EXPECT_EQ(textOf(calling.fields, "barred"), "") << id;
            }

            // (his own class's, and no other's)
            EXPECT_FALSE(listed(17.5f, man, "chair"));
            EXPECT_FALSE(listed(17.5f, man, "contract"));

            // Every one of the four events has an answer he can always give
            RuleActivity kneel = activityAt(f.rules, 27.0f, man, "lord.kneel");

            EXPECT_TRUE(flag(kneel.fields, "listed"));
            EXPECT_FALSE(flag(kneel.fields, "locked"));
            EXPECT_EQ(intOf(kneel.fields, "months"), 0);
            EXPECT_EQ(textOf(kneel.fields, "event"), "lord");

            // The plague year settled at twenty-four on a man who stood by at the Flood: the
            // second time, and its own rows are all it costs him
            ElementMap once = man;
            once["stood_flood"] = ElementType{1};
            once["stood_by"] = ElementType{1};

            RuleMonth passed;
            ASSERT_TRUE(f.rules.month(24.0f, once, passed)) << firstError(f.rules);

            EXPECT_EQ(intOf(passed.after, "stood_plague"), 1);
            EXPECT_EQ(intOf(passed.after, "stood_by"), 2);
            EXPECT_TRUE(wrote(passed, "The plague year went by"));
            EXPECT_FALSE(wrote(passed, "no second calling"));

            // (who shut his door has answered)
            ElementMap shut = once;
            shut["plague_shut"] = ElementType{1};

            ASSERT_TRUE(f.rules.month(24.0f, shut, passed)) << firstError(f.rules);
            EXPECT_EQ(passed.after.count("stood_plague"), 0u);

            ElementMap twice = once;
            twice["stood_plague"] = ElementType{1};
            twice["stood_by"] = ElementType{2};

            for (const char* id : {"castellan", "cross"})
            {
                EXPECT_TRUE(listed(29.0f, twice, id)) << id;
                EXPECT_EQ(textOf(activityAt(f.rules, 29.0f, twice, id).fields, "barred"), "") << id;
            }

            EXPECT_NE(textOf(activityAt(f.rules, 24.0f, twice, "raid").fields, "barred").find("The plague year went by"), std::string::npos);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // The works of a class come a few at a time and in an order: its bread the day he is of it,
        // each of the others once the one before is behind him. What he missed holds nothing up.
        TEST(rules_test, the_works_of_a_class_come_in_an_order)
        {
            MockLogger logger;
            RulesFixture f;

            auto shown = [&](float age, const ElementMap& character, std::initializer_list<const char*> ids) {
                std::vector<std::string> out;

                for (const char* id : ids)
                {
                    if (flag(activityAt(f.rules, age, character, id).fields, "listed"))
                        out.push_back(id);
                }

                return out;
            };

            using Ids = std::vector<std::string>;

            // The Keep: its service, then the patrols, then the tourney, and from the tourney the
            // Master-at-Arms and the duel; the merchant road after the duel
            ElementMap knight = sworn();
            knight["discipline"] = ElementType{3};
            knight["renown"] = ElementType{2};

            const auto keep = {"serve", "patrol", "tourney", "master", "duel", "escort"};

            f.rules.done = {{"keep", ElementType{1}}};
            EXPECT_EQ(shown(20.0f, knight, keep), (Ids{"serve"}));

            f.rules.done["serve"] = ElementType{1};
            EXPECT_EQ(shown(20.0f, knight, keep), (Ids{"serve", "patrol"}));

            f.rules.done["patrol"] = ElementType{1};
            EXPECT_EQ(shown(20.0f, knight, keep), (Ids{"serve", "patrol", "tourney"}));

            f.rules.done["tourney"] = ElementType{1};
            EXPECT_EQ(shown(20.0f, knight, keep), (Ids{"serve", "patrol", "master"}));
            EXPECT_EQ(shown(22.0f, knight, keep), (Ids{"serve", "patrol", "master", "duel"}));

            f.rules.done["duel"] = ElementType{1};
            EXPECT_EQ(shown(22.0f, knight, keep), (Ids{"serve", "patrol", "master", "escort"}));

            // The tourney he never fought closes at twenty-three: what came after it does not wait
            f.rules.done = {{"keep", ElementType{1}}, {"serve", ElementType{1}}, {"patrol", ElementType{1}}};
            EXPECT_EQ(shown(22.5f, knight, keep), (Ids{"serve", "patrol", "tourney"}));
            EXPECT_EQ(shown(22.5f + 1.0f / 12.0f, knight, keep), (Ids{"serve", "patrol", "master", "duel"}));

            // The great step of his years shows from the first day, locked
            f.rules.done = {{"keep", ElementType{1}}};
            EXPECT_TRUE(flag(activityAt(f.rules, 17.5f, knight, "campaign").fields, "listed"));

            // The Collegium: its study, then the workings and the treatise, then the offices and Orin
            ElementMap scholar = boy();
            scholar["collegium"] = ElementType{1};
            scholar["int"] = ElementType{12};
            scholar["letters"] = ElementType{4};
            scholar["lore"] = ElementType{4};
            scholar["arcana"] = ElementType{2};

            const auto collegium = {"study", "workings", "treatise", "vigil", "orin", "fragments", "lecture"};

            f.rules.done = {{"collegium", ElementType{1}}};
            EXPECT_EQ(shown(21.0f, scholar, collegium), (Ids{"study"}));

            f.rules.done["study"] = ElementType{1};
            EXPECT_EQ(shown(21.0f, scholar, collegium), (Ids{"study", "workings", "treatise"}));

            f.rules.done["workings"] = ElementType{1};
            EXPECT_EQ(shown(21.0f, scholar, collegium), (Ids{"study", "workings", "treatise", "vigil", "orin"}));

            scholar["lore"] = ElementType{5};
            f.rules.done["treatise"] = ElementType{1};
            EXPECT_EQ(shown(21.0f, scholar, collegium), (Ids{"study", "workings", "vigil", "orin", "fragments"}));

            f.rules.done["fragments"] = ElementType{1};
            EXPECT_EQ(shown(21.0f, scholar, collegium), (Ids{"study", "workings", "vigil", "orin", "lecture"}));

            // The Hidden Hand: its errands, then the informants and the hidden ways, then Old Vey and
            // the mark, then the gate
            ElementMap thief = boy();
            thief["hidden_hand"] = ElementType{1};
            thief["favors"] = ElementType{2};
            thief["stealth"] = ElementType{4};
            thief["guile"] = ElementType{4};

            const auto hand = {"run", "network", "drill", "mark", "vey", "smuggle"};

            f.rules.done = {{"hand", ElementType{1}}};
            EXPECT_EQ(shown(19.0f, thief, hand), (Ids{"run"}));

            f.rules.done["run"] = ElementType{1};
            EXPECT_EQ(shown(19.0f, thief, hand), (Ids{"run", "network", "drill"}));

            f.rules.done["network"] = ElementType{1};
            EXPECT_EQ(shown(19.0f, thief, hand), (Ids{"run", "network", "drill", "mark", "vey"}));

            f.rules.done["drill"] = ElementType{1};
            EXPECT_EQ(shown(19.0f, thief, hand), (Ids{"run", "network", "drill", "mark", "vey", "smuggle"}));
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // The town's work a child never took is cleared from his page at ten or so; what he has
        // done once stays his, and the carters take anyone at any age. The month at the forge
        // closes when the tempering it leads to can no longer follow.
        TEST(rules_test, a_work_never_taken_leaves_the_list)
        {
            MockLogger logger;
            RulesFixture f;

            auto listed = [&](float age, const ElementMap& character, const char* id) {
                return flag(activityAt(f.rules, age, character, id).fields, "listed");
            };

            const float month = 1.0f / 12.0f;

            ElementMap character = boy();
            character["str"] = ElementType{8};

            for (const char* id : {"mill", "messages", "kitchen"})
            {
                EXPECT_TRUE(listed(10.0f - month, character, id)) << id;
                EXPECT_FALSE(listed(10.0f, character, id)) << id;
            }

            EXPECT_TRUE(listed(11.0f - month, character, "market"));
            EXPECT_FALSE(listed(11.0f, character, "market"));

            EXPECT_TRUE(listed(13.0f - month, character, "smithy"));
            EXPECT_FALSE(listed(13.0f, character, "smithy"));

            EXPECT_TRUE(listed(10.0f, character, "carters"));
            EXPECT_TRUE(listed(25.0f, character, "carters"));

            // Nothing is said of it: no countdown to have him do it once
            EXPECT_EQ(intOf(activityAt(f.rules, 10.0f - month, character, "mill").fields, "closesIn"), -1);
            EXPECT_EQ(textOf(activityAt(f.rules, 10.0f - month, character, "mill").fields, "until"), "");

            // Done once, it is his for good; and so is the one he is at for the first time
            f.rules.done = {{"mill", ElementType{1}}, {"smithy", ElementType{1}}};

            EXPECT_TRUE(listed(10.0f, character, "mill"));
            EXPECT_TRUE(listed(25.0f, character, "mill"));
            EXPECT_TRUE(listed(13.0f, character, "smithy"));
            EXPECT_FALSE(listed(10.0f, character, "messages"));

            f.rules.running = "kitchen";
            EXPECT_TRUE(listed(10.0f, character, "kitchen"));
            f.rules.running = "";
            EXPECT_FALSE(listed(10.0f, character, "kitchen"));

            // The forge: a month, and three of tempering that must be over by thirteen
            EXPECT_TRUE(listed(12.0f + 8.0f * month, character, "forge"));
            EXPECT_FALSE(listed(12.0f + 9.0f * month, character, "forge"));
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // In a town whose Inn stands a man of thirty is not old: his prime runs to fifty, old age
        // begins there, and thirty is where a second calling does. Without the Inn nothing moves.
        TEST(rules_test, the_inn_gives_a_life_a_second_calling)
        {
            MockLogger logger;
            RulesFixture f;

            auto listed = [&](float age, const ElementMap& character, const char* id) {
                return flag(activityAt(f.rules, age, character, id).fields, "listed");
            };

            auto ages = [&]() {
                std::vector<RuleMilestone> milestones;
                ElementMap next;

                EXPECT_TRUE(f.rules.milestones(29.0f, milestones, next)) << firstError(f.rules);

                std::vector<std::string> out;

                for (const auto& m : milestones)
                    out.push_back(textOf(m.fields, "id") + " " + std::to_string(intOf(m.fields, "age")));

                return out;
            };

            // A veteran of the Keep at twenty-nine
            ElementMap veteran = sworn();
            veteran["veteran"] = ElementType{1};
            veteran["arms"] = ElementType{7};
            veteran["discipline"] = ElementType{5};
            veteran["renown"] = ElementType{4};
            veteran["coin"] = ElementType{100};
            f.rules.done = {{"keep", ElementType{1}}, {"campaign", ElementType{1}}};

            // No Inn: old at thirty, the old man's works at twenty-nine, and no calling
            EXPECT_EQ(ages().back(), "old 30");
            EXPECT_TRUE(listed(29.5f, veteran, "garden"));
            EXPECT_FALSE(listed(29.0f, veteran, "castellan"));
            EXPECT_FALSE(listed(29.0f, veteran, "cross"));

            // The Inn stands: a calling at thirty, old at fifty
            veteran["town.inn"] = ElementType{1};
            f.rules.inn = 1;

            const std::vector<std::string> long_ = ages();

            ASSERT_GE(long_.size(), 2u);
            EXPECT_EQ(long_[long_.size() - 2], "calling 30");
            EXPECT_EQ(long_.back(), "old 50");

            EXPECT_FALSE(listed(29.5f, veteran, "garden"));
            EXPECT_TRUE(listed(49.5f, veteran, "garden"));

            // What no age of its own closes is his past thirty
            EXPECT_TRUE(listed(35.0f, veteran, "serve"));
            EXPECT_EQ(intOf(activityAt(f.rules, 35.0f, veteran, "serve").fields, "closesIn"), -1);

            // His two callings, a warrior's, and no other class's; one or the other
            EXPECT_TRUE(listed(29.0f, veteran, "castellan"));
            EXPECT_TRUE(listed(29.0f, veteran, "cross"));
            EXPECT_FALSE(listed(29.0f, veteran, "contract"));
            EXPECT_FALSE(flag(activityAt(f.rules, 29.0f, veteran, "castellan").fields, "locked"));
            EXPECT_NE(textOf(activityAt(f.rules, 29.0f, veteran, "castellan").fields, "caution").find("Take the Cross"), std::string::npos);

            // The keys taken: the cross is gone, the militia is his at thirty-one, and the siege
            // shows him what the calling leads to
            veteran["castellan"] = ElementType{1};
            f.rules.done = {{"keep", ElementType{1}}, {"campaign", ElementType{1}}, {"castellan", ElementType{1}}};

            EXPECT_FALSE(listed(31.0f, veteran, "cross"));
            EXPECT_TRUE(listed(31.0f, veteran, "militia"));
            EXPECT_FALSE(listed(31.0f, veteran, "marches"));
            EXPECT_TRUE(listed(31.0f, veteran, "siege"));
            EXPECT_TRUE(flag(activityAt(f.rules, 31.0f, veteran, "siege").fields, "locked"));
            EXPECT_FALSE(listed(31.0f, veteran, "crusade"));
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // The month he reaches thirty settles where his old age begins, for the rest of that life
        // (the stat "prime_end", resources.pg): thirty without the Inn, fifty with it. An Inn raised
        // after that takes nothing back: his old age goes on, the milestones and the works keep
        // it, and the chair says it is for the lives after his.
        TEST(rules_test, an_inn_raised_past_thirty_is_for_the_lives_after_his)
        {
            MockLogger logger;
            RulesFixture f;

            const float twelfth = 1.0f / 12.0f;

            RuleMonth month;

            const ElementMap man = {{"vit", ElementType{45}}, {"vitmax", ElementType{45}}, {"rations", ElementType{5}}};

            // Not yet thirty: nothing is settled
            ASSERT_TRUE(f.rules.month(30.0f - twelfth, man, month)) << firstError(f.rules);
            EXPECT_EQ(month.after.count("prime_end"), 0u);

            // Thirty without the Inn: old at thirty. With it, from before: old at fifty
            ASSERT_TRUE(f.rules.month(30.0f, man, month)) << firstError(f.rules);
            EXPECT_EQ(intOf(month.after, "prime_end"), 30);

            ElementMap withInn = man;
            withInn["town.inn"] = ElementType{1};

            ASSERT_TRUE(f.rules.month(30.0f, withInn, month));
            EXPECT_EQ(intOf(month.after, "prime_end"), 50);

            // Settled at thirty, the Inn raised at thirty-one: the years still take from him
            ElementMap old = withInn;
            old["prime_end"] = ElementType{30};

            ASSERT_TRUE(f.rules.month(31.0f, old, month));
            EXPECT_EQ(intOf(month.after, "vitmax"), 40);
            EXPECT_EQ(intOf(month.after, "vit"), 40);
            EXPECT_EQ(intOf(month.after, "prime_end"), 30);

            // Settled at fifty: nothing is taken at forty
            ElementMap prime = withInn;
            prime["prime_end"] = ElementType{50};

            ASSERT_TRUE(f.rules.month(40.0f, prime, month));
            EXPECT_EQ(intOf(month.after, "vitmax"), 45);
            EXPECT_EQ(intOf(month.after, "prime_end"), 50);

            // The milestones keep his old age at thirty, and give him no second calling
            auto ages = [&]() {
                std::vector<RuleMilestone> milestones;
                ElementMap next;

                EXPECT_TRUE(f.rules.milestones(31.0f, milestones, next)) << firstError(f.rules);

                std::vector<std::string> out;

                for (const auto& m : milestones)
                    out.push_back(textOf(m.fields, "id") + " " + std::to_string(intOf(m.fields, "age")));

                return out;
            };

            f.rules.inn = 1;
            f.rules.prime = 30;

            const std::vector<std::string> settled = ages();

            ASSERT_GE(settled.size(), 2u);
            EXPECT_EQ(settled.back(), "old 30");
            EXPECT_NE(settled[settled.size() - 2], "calling 30");

            // And the works old age closed stay closed: what no age of its own closes was his to thirty
            ElementMap veteran = sworn();
            veteran["veteran"] = ElementType{1};
            veteran["arms"] = ElementType{7};
            veteran["discipline"] = ElementType{5};
            veteran["renown"] = ElementType{4};
            veteran["town.inn"] = ElementType{1};
            veteran["prime_end"] = ElementType{30};
            f.rules.done = {{"keep", ElementType{1}}, {"campaign", ElementType{1}}};

            EXPECT_FALSE(flag(activityAt(f.rules, 35.0f, veteran, "serve").fields, "listed"));
            EXPECT_TRUE(flag(activityAt(f.rules, 35.0f, veteran, "garden").fields, "listed"));
            EXPECT_FALSE(flag(activityAt(f.rules, 31.0f, veteran, "castellan").fields, "listed"));

            // The chair says what it does: old age at fifty, not thirty. To a man whose old age has
            // begun, that it is for the lives after his
            auto inn = [&](float age, const ElementMap& character) {
                RuleTown town;
                EXPECT_TRUE(f.rules.town(age, character, town)) << firstError(f.rules);

                for (const auto& p : town.places)
                {
                    if (textOf(p.fields, "id") == "inn")
                        return p.fields;
                }

                ADD_FAILURE() << "no inn";
                return ElementMap{};
            };

            ElementMap townsfolk = townsman();

            const ElementMap young = inn(20.0f, townsfolk);
            EXPECT_NE(textOf(young, "nextGives").find("Old age begins at 50 instead of 30"), std::string::npos) << textOf(young, "nextGives");
            EXPECT_EQ(textOf(young, "nextGives").find("lives after yours"), std::string::npos) << textOf(young, "nextGives");

            townsfolk["prime_end"] = ElementType{30};

            const ElementMap aged = inn(31.0f, townsfolk);
            EXPECT_NE(textOf(aged, "nextGives").find("Old age begins at 50 instead of 30"), std::string::npos) << textOf(aged, "nextGives");
            EXPECT_NE(textOf(aged, "nextGives").find("lives after yours"), std::string::npos) << textOf(aged, "nextGives");

            townsfolk["town.inn"] = ElementType{1};

            const ElementMap raised = inn(31.0f, townsfolk);
            EXPECT_NE(textOf(raised, "gives").find("lives after yours"), std::string::npos) << textOf(raised, "gives");
            EXPECT_NE(textOf(raised, "line").find("LIVES AFTER HIS"), std::string::npos) << textOf(raised, "line");

            // A man who had the Inn at thirty has it for himself
            townsfolk["prime_end"] = ElementType{50};

            const ElementMap his = inn(31.0f, townsfolk);
            EXPECT_EQ(textOf(his, "gives").find("lives after yours"), std::string::npos) << textOf(his, "gives");
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // What kind of man of his class he is: a fork a class, whose flag changes what the class's
        // own works bring or ask. The Hand's mark makes every favor one more and silence free; his
        // own name pays more and earns none. An outlaw the roads fear is asked more Renown.
        TEST(rules_test, a_class_has_its_fork)
        {
            MockLogger logger;
            RulesFixture f;

            auto brought = [&](float age, const ElementMap& character, const char* id, const char* stat) {
                for (const auto& gain : activityAt(f.rules, age, character, id).gains)
                {
                    if (textOf(gain, "stat") == stat)
                        return intOf(gain, "amount");
                }

                return 0;
            };

            ElementMap thief = {{"hidden_hand", ElementType{1}}, {"favors", ElementType{3}}, {"stealth", ElementType{5}}, {"guile", ElementType{5}},
                                {"guild_enemy", ElementType{1}}, {"coin", ElementType{100}}};

            EXPECT_EQ(brought(21.0f, thief, "run", "favors"), 1);
            EXPECT_EQ(brought(21.0f, thief, "blackmail", "coin"), 45);
            EXPECT_EQ(activityAt(f.rules, 22.0f, thief, "silence").costs.size(), 1u);

            ElementMap marked = thief;
            marked["hand_marked"] = ElementType{1};

            EXPECT_EQ(brought(21.0f, marked, "run", "favors"), 2);
            EXPECT_EQ(brought(21.0f, marked, "smuggle", "favors"), 3);
            EXPECT_TRUE(activityAt(f.rules, 22.0f, marked, "silence").costs.empty());

            ElementMap own = thief;
            own["freelance"] = ElementType{1};

            EXPECT_EQ(brought(21.0f, own, "run", "favors"), 0);
            EXPECT_EQ(brought(21.0f, own, "smuggle", "favors"), 0);
            EXPECT_EQ(brought(21.0f, own, "network", "favors"), 2);
            EXPECT_EQ(brought(21.0f, own, "blackmail", "coin"), 65);

            // The Greenwood: who the band robs
            ElementMap outlaw = {{"renegade", ElementType{1}}, {"band", ElementType{1}}, {"arms", ElementType{5}}, {"stealth", ElementType{5}}, {"renown", ElementType{4}}};

            auto renownAsked = [&](const ElementMap& character) {
                for (const auto& r : activityAt(f.rules, 26.0f, character, "woodking").requires)
                {
                    if (textOf(r, "stat") == "renown")
                        return intOf(r, "needed");
                }

                return 0;
            };

            EXPECT_EQ(renownAsked(outlaw), 4);
            EXPECT_EQ(brought(22.0f, outlaw, "waylay", "coin"), 40);

            ElementMap sung = outlaw;
            sung["peoples_outlaw"] = ElementType{1};

            EXPECT_EQ(renownAsked(sung), 3);
            EXPECT_EQ(brought(22.0f, sung, "waylay", "coin"), 30);
            EXPECT_EQ(brought(22.0f, sung, "waylay", "renown"), 2);

            ElementMap feared = outlaw;
            feared["feared"] = ElementType{1};

            EXPECT_EQ(renownAsked(feared), 5);
            EXPECT_EQ(brought(22.0f, feared, "waylay", "coin"), 55);
            EXPECT_EQ(brought(22.0f, feared, "waylay", "renown"), 0);

            // The pardon: the Keep's service is his at twenty-five, and the Greenwood is not his to hold
            ElementMap pardoned = outlaw;
            pardoned["pardoned"] = ElementType{1};
            f.rules.done = {{"pardon", ElementType{1}}};

            EXPECT_FALSE(flag(activityAt(f.rules, 26.0f, pardoned, "woodking").fields, "listed"));
            EXPECT_TRUE(flag(activityAt(f.rules, 25.0f, pardoned, "serve").fields, "listed"));
            EXPECT_FALSE(flag(activityAt(f.rules, 25.0f, outlaw, "serve").fields, "listed"));
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // The yard takes a boy who has watched its tourney from the rail, a month at nine, or one
        // who comes with the basics of the blade: either, and the tourney is for who has no blade.
        // What the tourney leaves him is a thing he holds, an enthusiasm, that his first term at
        // the yard takes; and the yard gives the basics to who came without them.
        TEST(rules_test, the_yard_asks_the_tourney_or_the_basics)
        {
            MockLogger logger;
            RulesFixture f;

            auto listed = [&](float age, const ElementMap& character, const char* id) {
                return flag(activityAt(f.rules, age, character, id).fields, "listed");
            };

            auto brings = [&](float age, const ElementMap& character, const char* id, const char* stat) {
                for (const auto& gain : activityAt(f.rules, age, character, id).gains)
                {
                    if (textOf(gain, "stat") == stat)
                        return true;
                }

                return false;
            };

            ElementMap character = boy();
            character["str"] = ElementType{9};

            // Strong enough and of age, and no row: he has never been near the yard
            EXPECT_FALSE(listed(8.5f, character, "spectate"));
            EXPECT_TRUE(listed(9.25f, character, "spectate"));
            EXPECT_FALSE(flag(activityAt(f.rules, 9.25f, character, "spectate").fields, "locked"));
            EXPECT_EQ(intOf(activityAt(f.rules, 9.25f, character, "spectate").fields, "months"), 1);
            EXPECT_FALSE(listed(10.0f, character, "yard"));
            EXPECT_TRUE(flag(activityAt(f.rules, 10.0f, character, "yard").fields, "locked"));

            // The tourney watched: an enthusiasm he holds, and the yard has its row at nine, his at ten
            RuleForecast forecast;
            ASSERT_TRUE(f.rules.forecast(9.25f, character, "spectate", 0, forecast)) << firstError(f.rules);
            EXPECT_EQ(intOf(forecast.atTerm, "yard_enthusiasm"), 1);

            ElementMap watched = character;
            watched["yard_enthusiasm"] = ElementType{1};
            f.rules.done = {{"spectate", ElementType{1}}};

            EXPECT_FALSE(listed(9.2f, watched, "spectate"));
            EXPECT_TRUE(listed(9.2f, watched, "yard"));
            EXPECT_TRUE(flag(activityAt(f.rules, 9.2f, watched, "yard").fields, "locked"));
            EXPECT_FALSE(flag(activityAt(f.rules, 10.0f, watched, "yard").fields, "locked"));

            // His first term takes the enthusiasm when it begins, and gives him the basics
            RuleActivity yard = activityAt(f.rules, 10.0f, watched, "yard");

            ASSERT_EQ(yard.costs.size(), 1u);
            EXPECT_EQ(textOf(yard.costs[0], "stat"), "yard_enthusiasm");
            EXPECT_TRUE(brings(10.0f, watched, "yard", "blade_basics"));

            ASSERT_TRUE(f.rules.forecast(10.0f, watched, "yard", 0, forecast)) << firstError(f.rules);

            EXPECT_EQ(intOf(forecast.atStart, "yard_enthusiasm"), 0);
            EXPECT_EQ(intOf(forecast.atTerm, "blade_basics"), 1);
            EXPECT_EQ(intOf(forecast.atTerm, "arms"), 1);

            // From then on the basics are what lets him in: nothing more is taken, and they are
            // not given twice
            ElementMap trained = character;
            trained["blade_basics"] = ElementType{1};
            trained["arms"] = ElementType{1};
            f.rules.done = {{"spectate", ElementType{1}}, {"yard", ElementType{1}}};

            yard = activityAt(f.rules, 11.0f, trained, "yard");

            EXPECT_TRUE(flag(yard.fields, "listed"));
            EXPECT_FALSE(flag(yard.fields, "locked"));
            EXPECT_TRUE(yard.costs.empty());
            EXPECT_FALSE(brings(11.0f, trained, "yard", "blade_basics"));

            ASSERT_TRUE(f.rules.forecast(11.0f, trained, "yard", 0, forecast)) << firstError(f.rules);
            EXPECT_EQ(intOf(forecast.atTerm, "blade_basics"), 1);

            // Who comes with the basics has no tourney to watch, and nothing to pay
            f.rules.done = {};

            ElementMap drilled = character;
            drilled["blade_basics"] = ElementType{1};

            EXPECT_FALSE(listed(9.0f, drilled, "spectate"));
            EXPECT_TRUE(listed(10.0f, drilled, "yard"));
            EXPECT_FALSE(flag(activityAt(f.rules, 10.0f, drilled, "yard").fields, "locked"));
            EXPECT_TRUE(activityAt(f.rules, 10.0f, drilled, "yard").costs.empty());
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // The yard's breathing is for who has done both: practiced the edge or the point, either,
        // and a term at the yard. One of the two is not enough, nor are the basics alone.
        TEST(rules_test, the_yards_breathing_asks_the_edge_or_the_point_and_the_yard)
        {
            MockLogger logger;
            RulesFixture f;

            ElementMap character = boy();
            character["str"] = ElementType{9};
            character["blade_basics"] = ElementType{1};

            auto listed = [&](float age) { return flag(activityAt(f.rules, age, character, "breathing").fields, "listed"); };

            EXPECT_FALSE(listed(10.0f));

            // The basics and the yard, and no edge nor point yet
            f.rules.done = {{"blades", ElementType{1}}, {"yard", ElementType{2}}};
            EXPECT_FALSE(listed(10.0f));

            // The point learned, and never a term at the yard
            character["thrust_learned"] = ElementType{1};
            f.rules.done = {{"blades", ElementType{1}}, {"stabs", ElementType{1}}};
            EXPECT_FALSE(listed(10.0f));

            // Both: the point does as well as the edge
            f.rules.done = {{"stabs", ElementType{1}}, {"yard", ElementType{1}}};
            EXPECT_TRUE(listed(10.5f));

            character.erase("thrust_learned");
            character["slash_learned"] = ElementType{1};
            f.rules.done = {{"slashes", ElementType{1}}, {"yard", ElementType{1}}};

            RuleActivity breathing = activityAt(f.rules, 10.5f, character, "breathing");

            EXPECT_TRUE(flag(breathing.fields, "listed"));
            EXPECT_FALSE(flag(breathing.fields, "locked"));
            EXPECT_EQ(intOf(breathing.fields, "uses"), 1);

            // What it asks is said as what he did
            ASSERT_EQ(breathing.requires.size(), 2u);
            EXPECT_EQ(textOf(breathing.requires[0], "label"), "Practiced slashes, or thrusts");
            EXPECT_EQ(intOf(breathing.requires[0], "current"), 1);
            EXPECT_EQ(textOf(breathing.requires[1], "label"), "Trained at the yard");
            EXPECT_EQ(intOf(breathing.requires[1], "current"), 1);

            RuleForecast forecast;
            ASSERT_TRUE(f.rules.forecast(10.5f, character, "breathing", 0, forecast)) << firstError(f.rules);

            EXPECT_EQ(intOf(forecast.atTerm, "discipline"), 1);
            EXPECT_EQ(intOf(forecast.atTerm, "yard_breathing"), 1);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // What a place gives reaches a row through its `bonus` steps: its months, what it brings,
        // how often it can be done, the age it opens at.
        TEST(rules_test, bonus_steps_read_the_town)
        {
            MockLogger logger;
            RulesFixture f;

            ElementMap character = boy();

            EXPECT_EQ(intOf(activityAt(f.rules, 8.0f, character, "letters").fields, "months"), 6);
            EXPECT_EQ(intOf(activityAt(f.rules, 8.0f, character, "smithy").fields, "fromAge"), 9);

            auto coinOf = [&](const RuleActivity& a) {
                for (const auto& gain : a.gains)
                {
                    if (textOf(gain, "stat") == "coin")
                        return intOf(gain, "amount");
                }

                return 0;
            };

            EXPECT_EQ(coinOf(activityAt(f.rules, 8.0f, character, "mill")), 8);
            EXPECT_EQ(intOf(activityAt(f.rules, 8.0f, townsman(), "buy.rations").gains[0], "amount"), 6);

            character = townsman();
            character["town.chapel"] = ElementType{1};
            character["town.yard"] = ElementType{1};
            character["town.smithy"] = ElementType{1};
            character["town.mill"] = ElementType{1};
            character["town.market"] = ElementType{1};

            EXPECT_EQ(intOf(activityAt(f.rules, 8.0f, character, "letters").fields, "months"), 4);
            EXPECT_EQ(intOf(activityAt(f.rules, 8.0f, character, "smithy").fields, "fromAge"), 8);
            EXPECT_EQ(intOf(activityAt(f.rules, 8.0f, character, "buy.rations").gains[0], "amount"), 8);

            // What it adds is added to what the row brings as it stands: its other gains are his still
            RuleActivity mill = activityAt(f.rules, 8.0f, character, "mill");

            EXPECT_EQ(coinOf(mill), 12);
            EXPECT_EQ(mill.gains.size(), 3u);

            // And to what repetition has made of it
            f.rules.done = {{"mill", ElementType{2}}};
            EXPECT_EQ(coinOf(activityAt(f.rules, 8.0f, character, "mill")), 10);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // A place eases what a class asks from the level that says so, and not before.
        TEST(rules_test, eased_at_level)
        {
            MockLogger logger;
            RulesFixture f;

            auto asked = [&](const ElementMap& character, const std::string& id, const std::string& stat) {
                RuleActivity a = activityAt(f.rules, 16.0f, character, id);

                for (const auto& r : a.requires)
                {
                    if (textOf(r, "stat") == stat)
                        return intOf(r, "needed");
                }

                return -1;
            };

            ElementMap character = townsman();

            EXPECT_EQ(asked(character, "keep", "str"), 14);
            EXPECT_EQ(asked(character, "collegium", "int"), 12);

            character["town.yard"] = ElementType{1};
            EXPECT_EQ(asked(character, "keep", "str"), 14);

            character["town.yard"] = ElementType{2};
            EXPECT_EQ(asked(character, "keep", "str"), 13);

            // The sergeant's word still eases it on its own
            EXPECT_EQ(asked({{"edric_support", ElementType{1}}}, "keep", "str"), 13);

            character["town.collegium"] = ElementType{2};
            EXPECT_EQ(asked(character, "collegium", "int"), 12);

            character["town.collegium"] = ElementType{3};
            EXPECT_EQ(asked(character, "collegium", "int"), 11);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // What a new life is born with for the town: every level reached, not the last alone.
        TEST(rules_test, start_from_the_town)
        {
            MockLogger logger;
            RulesFixture f;

            RuleTown town;
            ASSERT_TRUE(f.rules.town(7.0f, townsman(), town)) << firstError(f.rules);
            EXPECT_TRUE(town.start.empty());
            EXPECT_TRUE(town.epitaphLine.empty());

            ElementMap character = townsman();
            character["town.chapel"] = ElementType{2};
            character["town.mill"] = ElementType{3};
            character["town.gate"] = ElementType{1};
            character["town.inn"] = ElementType{2};

            ASSERT_TRUE(f.rules.town(7.0f, character, town)) << firstError(f.rules);

            auto given = [&](const std::string& stat) {
                int amount = 0;

                for (const auto& s : town.start)
                {
                    if (textOf(s, "stat") == stat)
                        amount += intOf(s, "amount");
                }

                return amount;
            };

            EXPECT_EQ(given("letters"), 1);
            EXPECT_EQ(given("rations"), 0);     // The mill loft gives a familiar slot now, not rations
            EXPECT_EQ(given("str"), 1);
            EXPECT_EQ(given("watch_known"), 1);
            EXPECT_EQ(given("arms"), 0);

            // And the inn tells a new life of the one before, from its second level
            EXPECT_FALSE(town.epitaphLine.empty());
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // The market hall sends a ration every three months, to every life, with no row of its own
        // in the ledger; and the family's room at the inn mends him every month.
        TEST(rules_test, stall_produces_without_a_row)
        {
            MockLogger logger;
            RulesFixture f;

            ElementMap character = townsman();
            character["vit"] = ElementType{30};
            character["vitrest"] = ElementType{0};

            RuleMonth month;
            ASSERT_TRUE(f.rules.month(9.0f, character, month)) << firstError(f.rules);
            EXPECT_EQ(intOf(month.after, "rations"), 11);
            EXPECT_EQ(intOf(month.after, "vit"), 30);

            const size_t rows = month.rows.size();

            character["town.market"] = ElementType{2};
            character["town.inn"] = ElementType{3};

            ASSERT_TRUE(f.rules.month(9.0f, character, month)) << firstError(f.rules);

            // The hall's month: one eaten, one sent
            EXPECT_EQ(intOf(month.after, "rations"), 12);
            EXPECT_EQ(intOf(month.after, "vit"), 35);

            // And its gloss says where the ration comes from, and how often
            const RuleGloss* rations = nullptr;

            for (const auto& gloss : month.glosses)
            {
                if (textOf(gloss.fields, "id") == "rations")
                    rations = &gloss;
            }

            ASSERT_NE(rations, nullptr);

            bool told = false;

            for (const auto& row : rations->rows)
            {
                if (textOf(row, "label") == "From the market hall")
                {
                    told = true;

                    EXPECT_EQ(textOf(row, "value"), "+1 / 3 mo");
                }
            }

            EXPECT_TRUE(told);

            // No row for it, and no gloss
            EXPECT_EQ(month.rows.size(), rows);
            EXPECT_EQ(byId(month.rows, "town.market"), nullptr);

            for (const auto& gloss : month.glosses)
                EXPECT_NE(textOf(gloss.fields, "id"), "town.market");

            // The two months between: he eats, and the hall sends nothing
            for (float age : {9.0f + 1.0f / 12.0f, 9.0f + 2.0f / 12.0f})
            {
                ASSERT_TRUE(f.rules.month(age, character, month)) << firstError(f.rules);

                EXPECT_EQ(intOf(month.after, "rations"), 11) << age;
            }

            // And the next one is its month again
            ASSERT_TRUE(f.rules.month(9.25f, character, month)) << firstError(f.rules);
            EXPECT_EQ(intOf(month.after, "rations"), 12);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // The Town page grows with what he does: two places from the day the town is known, the smithy
        // with a term at the forge, the Keep's two with its border and its chain, the Collegium's
        // three with its door, its Magister and Harrow, the inn with the second life of a world. A
        // place that was seen, raised or left coin stays on it, and a place that is not on his page
        // has no work to show either.
        TEST(rules_test, places_come_with_what_he_does)
        {
            MockLogger logger;
            RulesFixture f;

            auto shown = [&](float age, const ElementMap& character) {
                RuleTown town;
                EXPECT_TRUE(f.rules.town(age, character, town)) << firstError(f.rules);

                std::vector<std::string> ids;

                for (const auto& place : town.places)
                {
                    if (flag(place.fields, "shown"))
                        ids.push_back(textOf(place.fields, "id"));
                }

                return ids;
            };

            using Ids = std::vector<std::string>;

            // A first life: the market and the mill, and his years alone bring nothing more
            f.rules.lives = 1;

            ElementMap character = townsman();

            EXPECT_EQ(shown(7.0f, character), (Ids{"market", "mill"}));
            EXPECT_EQ(shown(29.0f, character), (Ids{"market", "mill"}));

            // A term at the forge brings the smithy
            f.rules.done = {{"smithy", ElementType{1}}};

            EXPECT_EQ(shown(10.0f, character), (Ids{"market", "mill", "smithy"}));

            f.rules.done.clear();

            // The Keep: nothing for the oath, the yard to a veteran of the border, the gate to its captain
            ElementMap warrior = character;
            warrior["keep_oath"] = ElementType{1};

            EXPECT_EQ(shown(17.5f, warrior), (Ids{"market", "mill"}));

            warrior["veteran"] = ElementType{1};

            EXPECT_EQ(shown(23.0f, warrior), (Ids{"market", "mill", "yard"}));

            warrior["captain"] = ElementType{1};

            EXPECT_EQ(shown(28.0f, warrior), (Ids{"market", "mill", "yard", "gate"}));

            // The Collegium: the chapel the day he is taken in, its gate after Magister Orin, the
            // road after Harrow
            ElementMap mage = character;
            mage["collegium"] = ElementType{1};

            EXPECT_EQ(shown(18.0f, mage), (Ids{"market", "mill", "chapel"}));

            mage["orin_known"] = ElementType{1};

            EXPECT_EQ(shown(21.0f, mage), (Ids{"market", "mill", "chapel", "collegium"}));

            mage["harrow_truth"] = ElementType{1};

            EXPECT_EQ(shown(23.0f, mage), (Ids{"market", "mill", "chapel", "collegium", "harrow"}));

            // The inn comes when a life has ended: from the second of a world
            f.rules.lives = 2;

            EXPECT_EQ(shown(7.0f, character), (Ids{"market", "mill", "inn"}));

            f.rules.lives = 1;

            // Seen by a life before, raised, or left coin: on the page of a boy of 7
            character["seen.chapel"] = ElementType{1};
            character["town.gate"] = ElementType{1};
            character["fund.inn"] = ElementType{3};

            EXPECT_EQ(shown(7.0f, character), (Ids{"market", "mill", "gate", "chapel", "inn"}));

            // The works follow the page
            EXPECT_TRUE(flag(activityAt(f.rules, 7.0f, character, "raise.chapel.1").fields, "listed"));
            EXPECT_TRUE(flag(activityAt(f.rules, 7.0f, character, "raise.gate.2").fields, "listed"));
            EXPECT_FALSE(flag(activityAt(f.rules, 7.0f, character, "raise.yard.1").fields, "listed"));
            EXPECT_FALSE(flag(activityAt(f.rules, 7.0f, character, "raise.smithy.1").fields, "listed"));
            EXPECT_FALSE(flag(activityAt(f.rules, 7.0f, character, "raise.collegium.1").fields, "listed"));

            character["veteran"] = ElementType{1};

            EXPECT_TRUE(flag(activityAt(f.rules, 7.0f, character, "raise.yard.1").fields, "listed"));

            f.rules.done = {{"smithy", ElementType{2}}};

            EXPECT_TRUE(flag(activityAt(f.rules, 7.0f, character, "raise.smithy.1").fields, "listed"));

            f.rules.done.clear();

            // Every place is told of all the same: the page shows the ones that are his
            RuleTown town;
            ASSERT_TRUE(f.rules.town(7.0f, townsman(), town)) << firstError(f.rules);
            EXPECT_EQ(town.places.size(), 11u);
            EXPECT_FALSE(town.foundLine.empty());
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // An answer is kept while what it was asked with stands: the scene asks the same thing from
        // several places in one month. One run of windows.pg answers the activities and the doors.
        TEST(rules_test, answers_are_kept_while_the_inputs_stand)
        {
            MockLogger logger;
            RulesFixture f;

            std::vector<RuleActivity> activities;
            RecordList windows;

            ASSERT_TRUE(f.rules.activities(17.4f, sworn(), activities));
            ASSERT_TRUE(f.rules.windows(17.4f, sworn(), windows));

            EXPECT_EQ(f.rules.nbRuns, 1u);
            EXPECT_FALSE(activities.empty());
            EXPECT_FALSE(windows.empty());

            std::vector<RuleActivity> again;

            ASSERT_TRUE(f.rules.activities(17.4f, sworn(), again));

            EXPECT_EQ(f.rules.nbRuns, 1u);
            ASSERT_EQ(again.size(), activities.size());
            EXPECT_EQ(textOf(again.front().fields, "id"), textOf(activities.front().fields, "id"));
            EXPECT_EQ(again.front().requires.size(), activities.front().requires.size());

            // Another age, another character, another count of terms, another date: each asks again
            ASSERT_TRUE(f.rules.activities(17.5f, sworn(), again));
            EXPECT_EQ(f.rules.nbRuns, 2u);

            ElementMap stronger = sworn();
            stronger["str"] = ElementType{intOf(stronger, "str") + 1};

            ASSERT_TRUE(f.rules.windows(17.5f, stronger, windows));
            EXPECT_EQ(f.rules.nbRuns, 3u);

            f.rules.done["mill"] = ElementType{1};

            ASSERT_TRUE(f.rules.activities(17.5f, stronger, again));
            EXPECT_EQ(f.rules.nbRuns, 4u);

            f.rules.world = f.rules.world + 1;

            ASSERT_TRUE(f.rules.activities(17.5f, stronger, again));
            EXPECT_EQ(f.rules.nbRuns, 5u);

            // The forecasts of one moment are kept together: the work at hand, its next month, a tile
            RuleForecast first;
            RuleForecast other;
            RuleForecast back;

            ASSERT_TRUE(f.rules.forecast(17.5f, stronger, "mill", 0, first));
            ASSERT_TRUE(f.rules.forecast(17.5f, stronger, "mill", 1, other));
            ASSERT_TRUE(f.rules.forecast(17.5f, stronger, "mill", 0, back));

            EXPECT_EQ(f.rules.nbRuns, 7u);
            EXPECT_EQ(back.caption, first.caption);
            EXPECT_EQ(back.months, first.months);
            EXPECT_NE(other.caption, first.caption);

            // A month, the milestones and the deeds
            RuleMonth month;

            ASSERT_TRUE(f.rules.month(17.5f, stronger, month));
            ASSERT_TRUE(f.rules.month(17.5f, stronger, month));
            EXPECT_EQ(f.rules.nbRuns, 8u);

            // Another character is another run
            ElementMap hungrier = stronger;
            hungrier["rations"] = ElementType{intOf(stronger, "rations") + 1};
            ASSERT_TRUE(f.rules.month(17.5f, hungrier, month));
            EXPECT_EQ(f.rules.nbRuns, 9u);

            std::vector<RuleMilestone> milestones;
            ElementMap next;
            ElementMap headline;

            ASSERT_TRUE(f.rules.milestones(17.5f, milestones, next));
            ASSERT_TRUE(f.rules.milestones(17.5f, milestones, next, &headline));
            EXPECT_EQ(f.rules.nbRuns, 10u);
            EXPECT_FALSE(headline.empty());

            std::vector<RuleAchievement> deeds;

            ASSERT_TRUE(f.rules.achievements(deeds));
            ASSERT_TRUE(f.rules.achievements(deeds));
            EXPECT_EQ(f.rules.nbRuns, 11u);
            EXPECT_FALSE(deeds.empty());
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

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // What a run of each script costs, printed for whoever works on them: nothing is expected of
        // the figures. Every call has its own age, so that none is answered from what is kept.
        TEST(rules_test, run_costs_are_printed)
        {
            MockLogger logger;
            RulesFixture f;

            const int runs = 200;

            auto timed = [&](const char* what, const std::function<bool(float)>& call) {
                const auto start = std::chrono::steady_clock::now();

                for (int i = 0; i < runs; ++i)
                    ASSERT_TRUE(call(17.0f + static_cast<float>(i) / 1000.0f)) << what;

                const auto us = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - start).count();

                std::cout << "[ rules    ] " << what << ": " << us / runs << " us a run" << std::endl;
            };

            std::vector<RuleActivity> activities;
            RecordList windows;
            RuleForecast forecast;
            RuleMonth month;
            std::vector<RuleMilestone> milestones;
            ElementMap next;

            timed("activities and windows", [&](float age) { return f.rules.activities(age, sworn(), activities) and f.rules.windows(age, sworn(), windows); });
            timed("forecast", [&](float age) { return f.rules.forecast(age, sworn(), "mill", 0, forecast); });
            timed("month", [&](float age) { return f.rules.month(age, sworn(), month); });
            timed("milestones", [&](float age) { return f.rules.milestones(age, milestones, next); });

            EXPECT_EQ(f.rules.nbRuns, static_cast<size_t>(4 * runs));
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // The rules as a web build ships them: compiled once, the .pgc alone where they are read,
        // and they answer as their sources do.
        TEST(rules_test, shipped_compiled)
        {
            MockLogger logger;

            {
                EntitySystem ecs;
                std::vector<std::string> errors;

                ASSERT_TRUE(precompileRules(&ecs, Root, "rules-shipped", errors)) << (errors.empty() ? "" : errors.front());
            }

            std::ifstream source("rules-shipped/windows.pg");
            EXPECT_FALSE(source.good()) << "a source was shipped with the compiled rules";

            std::ifstream compiled("rules-shipped/windows.pgc");
            EXPECT_TRUE(compiled.good());

            RulesFixture fromSource(Root);   // compiled from the sources here, not the run's .pgc

            EntitySystem ecs;
            Rules shipped;
            ASSERT_TRUE(shipped.load(&ecs, "rules-shipped")) << firstError(shipped);
            shipped.lives = fromSource.rules.lives;

            std::vector<RuleActivity> expected, got;
            ASSERT_TRUE(fromSource.rules.activities(7.0f, boy(), expected)) << firstError(fromSource.rules);
            ASSERT_TRUE(shipped.activities(7.0f, boy(), got)) << firstError(shipped);

            ASSERT_EQ(got.size(), expected.size());

            for (size_t i = 0; i < got.size(); ++i)
            {
                EXPECT_EQ(textOf(got[i].fields, "id"), textOf(expected[i].fields, "id"));
                EXPECT_EQ(intOf(got[i].fields, "months"), intOf(expected[i].fields, "months"));
                EXPECT_EQ(got[i].gains.size(), expected[i].gains.size());
            }

            std::vector<RuleMilestone> expectedSteps, gotSteps;
            ElementMap expectedNext, gotNext;
            ASSERT_TRUE(fromSource.rules.milestones(12.0f, expectedSteps, expectedNext)) << firstError(fromSource.rules);
            ASSERT_TRUE(shipped.milestones(12.0f, gotSteps, gotNext)) << firstError(shipped);
            EXPECT_EQ(gotSteps.size(), expectedSteps.size());
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // What a boy learns racing the rooftops and following the hunters is his on a walk: the
        // rows leave the flags that teach a way to step aside in a fight, and to track and skin.
        TEST(rules_test, the_rooftops_and_the_hunters_teach_a_way)
        {
            MockLogger logger;
            RulesFixture f;

            auto gives = [](const RuleActivity& a, const std::string& stat) {
                for (const auto& g : a.gains)
                {
                    if (textOf(g, "stat") == stat)
                        return true;
                }

                return false;
            };

            ElementMap who = boy();
            who["dex"] = ElementType{9};

            EXPECT_TRUE(gives(activityAt(f.rules, 12.0f, who, "rooftops"), "rooftop_runner"));
            EXPECT_TRUE(gives(activityAt(f.rules, 12.0f, who, "hunters"), "hunter_trained"));
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // The market mixes a fire flask out of firewood a walk brought home, and sells blessed oil.
        TEST(rules_test, the_market_sells_what_a_fight_spends)
        {
            MockLogger logger;
            RulesFixture f;

            ElementMap who = boy();
            who["town_known"] = ElementType{1};
            who["firewood"] = ElementType{2};
            who["walked.millpond"] = ElementType{1};   // (the oil is sold to who has walked where beasts are)
            who["coin"] = ElementType{10};

            RuleActivity flask = activityAt(f.rules, 12.0f, who, "brew.flask");

            ASSERT_EQ(flask.gains.size(), 1u);
            EXPECT_EQ(textOf(flask.gains[0], "stat"), "flask");

            int firewood = 0;

            for (const auto& c : flask.costs)
            {
                if (textOf(c, "stat") == "firewood")
                    firewood = intOf(c, "amount");
            }

            EXPECT_EQ(firewood, 2);

            RuleActivity oil = activityAt(f.rules, 12.0f, who, "buy.oil");

            ASSERT_EQ(oil.gains.size(), 1u);
            EXPECT_EQ(textOf(oil.gains[0], "stat"), "oil");
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // Who fell to a boss on a walk is dead, and the next life opens with how.
        TEST(rules_test, a_fall_on_a_walk_is_a_death)
        {
            MockLogger logger;
            RulesFixture f;

            ElementMap fallen = boy();
            fallen["vit"] = ElementType{0};
            fallen["fell"] = ElementType{1};

            RuleMonth month;
            ASSERT_TRUE(f.rules.month(15.0f, fallen, month)) << firstError(f.rules);
            EXPECT_NE(month.death.find("a fight he chose"), std::string::npos) << month.death;
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // What a fight spends is in the Inventory, and so is what fights have taught him: the
        // ranks of his techniques and the lessons of his first kills.
        TEST(rules_test, what_fights_teach_and_spend_is_held)
        {
            MockLogger logger;
            RulesFixture f;

            ElementMap fighter = boy();
            fighter["flask"] = ElementType{2};
            fighter["oil"] = ElementType{1};
            fighter["rank.slash"] = ElementType{1};
            fighter["lesson.rat"] = ElementType{1};

            RuleMonth month;
            ASSERT_TRUE(f.rules.month(15.0f, fighter, month)) << firstError(f.rules);

            for (const char* id : {"flask", "oil", "rank.slash", "lesson.rat"})
            {
                bool shown = false;

                for (const auto& r : month.rows)
                {
                    if (textOf(r, "id") == id)
                        shown = true;
                }

                EXPECT_TRUE(shown) << id;
            }
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // What the stash keeps is drawn as far as he has room for it: 100 water kept and 30 held
        // of 40 he can hold draws 10. Full, the row is closed by its room.
        TEST(rules_test, stash_rows_draw_what_room_allows)
        {
            MockLogger logger;
            RulesFixture f;

            ElementMap character = boy();
            character["stash.water"] = ElementType{100};
            character["water"] = ElementType{30};

            RuleActivity draw = activityAt(f.rules, 9.0f, character, "draw.water");

            EXPECT_EQ(textOf(draw.fields, "group"), "The stash");
            EXPECT_EQ(intOf(draw.fields, "months"), 0);
            EXPECT_EQ(textOf(draw.fields, "each"), "100 IN THE STASH");
            EXPECT_TRUE(flag(draw.fields, "listed"));
            EXPECT_FALSE(flag(draw.fields, "locked"));

            ASSERT_EQ(draw.gains.size(), 1u);
            EXPECT_EQ(textOf(draw.gains[0], "stat"), "water");
            EXPECT_EQ(intOf(draw.gains[0], "amount"), 10);

            ASSERT_EQ(draw.costs.size(), 1u);
            EXPECT_EQ(textOf(draw.costs[0], "stat"), "stash.water");
            EXPECT_EQ(intOf(draw.costs[0], "amount"), 10);

            // The player reads it in words, never as the raw id
            // (named as taken, so its price on the tile is not read as what the stash holds)
            EXPECT_EQ(textOf(draw.costs[0], "name"), "Water from the stash");

            // Full, the room he has not closes it
            character["water"] = ElementType{40};
            RuleActivity full = activityAt(f.rules, 9.0f, character, "draw.water");
            EXPECT_TRUE(flag(full.fields, "listed"));
            EXPECT_TRUE(flag(full.fields, "locked"));

            // With nothing kept, there is no row
            character.erase("stash.water");
            EXPECT_FALSE(flag(activityAt(f.rules, 9.0f, character, "draw.water").fields, "listed"));
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // The town says what the stash keeps between lives: each basic, up to 100 of it.
        TEST(rules_test, the_town_says_what_the_stash_keeps)
        {
            MockLogger logger;
            RulesFixture f;

            RuleTown town;
            ASSERT_TRUE(f.rules.town(7.0f, townsman(), town)) << firstError(f.rules);

            ASSERT_EQ(town.stash.size(), 5u);

            for (const char* id : {"berries", "water", "firewood", "stone", "clay"})
            {
                bool kept = false;

                for (const auto& s : town.stash)
                {
                    if (textOf(s, "stat") == id)
                    {
                        kept = true;
                        EXPECT_EQ(intOf(s, "most"), 100) << id;
                    }
                }

                EXPECT_TRUE(kept) << id;
            }
        }

        // The inn's chair makes the stash keep more of each thing: 150, not 100
        TEST(rules_test, the_inn_makes_the_stash_bigger)
        {
            MockLogger logger;
            RulesFixture f;

            ElementMap character = townsman();
            character["town.inn"] = ElementType{1};

            RuleTown town;
            ASSERT_TRUE(f.rules.town(20.0f, character, town)) << firstError(f.rules);

            ASSERT_EQ(town.stash.size(), 5u);

            for (const auto& s : town.stash)
                EXPECT_EQ(intOf(s, "most"), 150) << textOf(s, "stat");
        }

        // The storehouse comes onto the Town page with the stash: not while it is empty, and once
        // it holds anything. Its levels make the stash bigger, and its draws stand there once the
        // town is known (on the Life page before)
        TEST(rules_test, the_storehouse_comes_with_the_stash)
        {
            MockLogger logger;
            RulesFixture f;

            auto storehouse = [](const RuleTown& town) -> const RulePlace* {
                for (const auto& place : town.places)
                {
                    if (textOf(place.fields, "id") == "storehouse")
                        return &place;
                }

                return nullptr;
            };

            RuleTown town;
            ASSERT_TRUE(f.rules.town(20.0f, townsman(), town)) << firstError(f.rules);
            ASSERT_NE(storehouse(town), nullptr);
            EXPECT_FALSE(flag(storehouse(town)->fields, "shown"));
            EXPECT_EQ(intOf(storehouse(town)->fields, "most"), 3);

            ElementMap character = townsman();
            character["stash.stone"] = ElementType{5};

            ASSERT_TRUE(f.rules.town(20.0f, character, town)) << firstError(f.rules);
            ASSERT_NE(storehouse(town), nullptr);
            EXPECT_TRUE(flag(storehouse(town)->fields, "shown"));
            EXPECT_EQ(textOf(storehouse(town)->fields, "nextWork"), "raise.storehouse.1");

            // Each level keeps more: 100 more, then 200, then 400
            character["town.storehouse"] = ElementType{2};
            ASSERT_TRUE(f.rules.town(20.0f, character, town)) << firstError(f.rules);

            for (const auto& s : town.stash)
                EXPECT_EQ(intOf(s, "most"), 400) << textOf(s, "stat");

            // Its first work is listed, now that it is on his page
            character["town.storehouse"] = ElementType{0};
            EXPECT_TRUE(flag(activityAt(f.rules, 20.0f, character, "raise.storehouse.1").fields, "listed"));

            // What is drawn from the stash is drawn at the storehouse once the town is his
            RuleActivity draw = activityAt(f.rules, 20.0f, character, "draw.stone");
            EXPECT_TRUE(flag(draw.fields, "listed"));
            EXPECT_EQ(textOf(draw.fields, "place"), "storehouse");

            // Before, on the Life page: a boy of six of a later life draws from the stash
            ElementMap young = child();
            young["stash.stone"] = ElementType{5};
            RuleActivity early = activityAt(f.rules, 6.0f, young, "draw.stone");
            EXPECT_TRUE(flag(early.fields, "listed"));
            EXPECT_EQ(textOf(early.fields, "place"), "");
        }

        // The ending says what his death puts into the stash: what he holds of the basics, less what
        // a gift leaves to a place, and no more than the stash has room for
        TEST(rules_test, the_town_says_what_goes_into_the_stash)
        {
            MockLogger logger;
            RulesFixture f;

            ElementMap character = townsman();
            character["water"] = ElementType{25};
            character["firewood"] = ElementType{32};
            character["stone"] = ElementType{12};
            character["berries"] = ElementType{1};
            character["stash.stone"] = ElementType{95};
            character["walk.hollow"] = ElementType{1};
            character["seen.lodge"] = ElementType{1};

            RuleTown town;
            ASSERT_TRUE(f.rules.town(20.0f, character, town)) << firstError(f.rules);

            EXPECT_EQ(town.stashLine, "His stores go to the family's stash: 1 berry, 25 water, 32 firewood, 5 stone.");

            // Left to the lodge, its 20 firewood are not in the stash
            const RulePlace* lodge = nullptr;

            for (const auto& place : town.places)
            {
                if (textOf(place.fields, "id") == "lodge")
                    lodge = &place;
            }

            ASSERT_NE(lodge, nullptr);
            EXPECT_EQ(textOf(lodge->fields, "stashLine"), "His stores go to the family's stash: 1 berry, 25 water, 12 firewood, 5 stone.");

            // Nothing of the kind held: nothing said
            ASSERT_TRUE(f.rules.town(20.0f, townsman(), town)) << firstError(f.rules);
            EXPECT_EQ(town.stashLine, "");
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // The smithy forges gear of resources, a piece once a life: the iron blade and the jerkin
        // once it has its first level. It stands on the Town page, at the smithy, and is lost at
        // his death: never left to a place, never in the stash.
        TEST(rules_test, the_smithy_forges_gear)
        {
            MockLogger logger;
            RulesFixture f;

            ElementMap character = townsman();
            EXPECT_FALSE(flag(activityAt(f.rules, 20.0f, character, "forge.iron_blade").fields, "listed"));

            character["town.smithy"] = ElementType{1};

            RuleActivity blade = activityAt(f.rules, 20.0f, character, "forge.iron_blade");

            EXPECT_TRUE(flag(blade.fields, "listed"));
            EXPECT_EQ(textOf(blade.fields, "place"), "smithy");
            EXPECT_EQ(textOf(blade.fields, "group"), "Market");
            EXPECT_EQ(intOf(blade.fields, "uses"), 1);

            bool smithy = false;

            for (const auto& r : blade.requires)
            {
                if (textOf(r, "stat") == "town.smithy")
                {
                    smithy = true;
                    EXPECT_EQ(intOf(r, "needed"), 1);
                }
            }

            EXPECT_TRUE(smithy);
            ASSERT_EQ(blade.gains.size(), 1u);
            EXPECT_EQ(textOf(blade.gains[0], "stat"), "gear.iron_blade");
            EXPECT_EQ(intOf(blade.gains[0], "amount"), 1);

            // What it is forged of: shut while he has none of it
            ASSERT_EQ(blade.costs.size(), 3u);
            EXPECT_EQ(textOf(blade.costs[0], "stat"), "stone");
            EXPECT_EQ(intOf(blade.costs[0], "amount"), 10);
            EXPECT_EQ(textOf(blade.costs[1], "stat"), "firewood");
            EXPECT_EQ(intOf(blade.costs[1], "amount"), 10);
            EXPECT_EQ(textOf(blade.costs[2], "stat"), "gems");
            EXPECT_EQ(intOf(blade.costs[2], "amount"), 1);
            EXPECT_TRUE(flag(blade.fields, "locked"));

            character["stone"] = ElementType{10};
            character["firewood"] = ElementType{10};
            character["gems"] = ElementType{1};
            EXPECT_FALSE(flag(activityAt(f.rules, 20.0f, character, "forge.iron_blade").fields, "locked"));

            // The jerkin at the same level
            EXPECT_TRUE(flag(activityAt(f.rules, 20.0f, character, "forge.jerkin").fields, "listed"));

            // Forged once, it is his for the life: the smithy makes him no second one
            character["gear.iron_blade"] = ElementType{1};
            f.rules.done = {{"forge.iron_blade", ElementType{1}}};
            EXPECT_FALSE(flag(activityAt(f.rules, 20.0f, character, "forge.iron_blade").fields, "listed"));

            // And he holds it: a row of the ledger, in a group of its own, that says he has it
            RuleMonth month;
            ASSERT_TRUE(f.rules.month(20.0f, character, month)) << firstError(f.rules);

            const ElementMap* held = byId(month.rows, "gear.iron_blade");

            ASSERT_NE(held, nullptr);
            EXPECT_EQ(textOf(*held, "groupLabel"), "GEAR");
            EXPECT_TRUE(flag(*held, "title"));

            // Lost at his death: what a place is left and what the stash keeps hold stone, and
            // never a piece of gear
            character["gear.jerkin"] = ElementType{1};
            character["stone"] = ElementType{30};
            character["firewood"] = ElementType{30};

            RuleTown town;
            ASSERT_TRUE(f.rules.town(20.0f, character, town)) << firstError(f.rules);

            bool leftAny = false;

            for (const auto& place : town.places)
            {
                for (const auto& l : place.leaves)
                {
                    leftAny = true;
                    EXPECT_NE(textOf(l, "stat").rfind("gear.", 0), 0u) << textOf(place.fields, "id") << " is left " << textOf(l, "stat");
                }
            }

            EXPECT_TRUE(leftAny);

            bool stone = false;

            for (const auto& s : town.stash)
            {
                stone = stone or textOf(s, "stat") == "stone";
                EXPECT_NE(textOf(s, "stat").rfind("gear.", 0), 0u) << "the stash keeps " << textOf(s, "stat");
            }

            EXPECT_TRUE(stone);
            EXPECT_EQ(town.stashLine.find("blade"), std::string::npos) << town.stashLine;
            EXPECT_EQ(town.stashLine.find("jerkin"), std::string::npos) << town.stashLine;
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // The warded charm waits for the Marsh: its row at the smithy is listed only once the
        // world has met a marsh light's working, and at the smithy's second level.
        TEST(rules_test, the_charm_waits_for_the_marsh_workings)
        {
            MockLogger logger;
            RulesFixture f;

            ElementMap character = townsman();
            character["town.smithy"] = ElementType{2};
            EXPECT_FALSE(flag(activityAt(f.rules, 20.0f, character, "forge.charm").fields, "listed"));

            character["know.move.light.bite"] = ElementType{1};

            RuleActivity charm = activityAt(f.rules, 20.0f, character, "forge.charm");

            EXPECT_TRUE(flag(charm.fields, "listed"));
            EXPECT_EQ(textOf(charm.fields, "place"), "smithy");
            ASSERT_EQ(charm.gains.size(), 1u);
            EXPECT_EQ(textOf(charm.gains[0], "stat"), "gear.charm");

            ASSERT_EQ(charm.costs.size(), 2u);
            EXPECT_EQ(textOf(charm.costs[0], "stat"), "gems");
            EXPECT_EQ(intOf(charm.costs[0], "amount"), 3);
            EXPECT_EQ(textOf(charm.costs[1], "stat"), "water");
            EXPECT_EQ(intOf(charm.costs[1], "amount"), 10);

            // A smithy of one level does not make it
            character["town.smithy"] = ElementType{1};
            EXPECT_FALSE(flag(activityAt(f.rules, 20.0f, character, "forge.charm").fields, "listed"));

            // Held, it is a row of GEAR
            character["gear.charm"] = ElementType{1};

            RuleMonth month;
            ASSERT_TRUE(f.rules.month(20.0f, character, month)) << firstError(f.rules);

            const ElementMap* held = byId(month.rows, "gear.charm");

            ASSERT_NE(held, nullptr);
            EXPECT_EQ(textOf(*held, "groupLabel"), "GEAR");
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // What only the Marsh gives (bog iron, pitch, wisp-glass) is held among his stores and is
        // not sold: it is what the last levels of the Collegium Gate, the Harrow Road and the
        // storehouse ask, and what of it a place still asks is left to it at his death.
        TEST(rules_test, what_the_marsh_finds_is_kept_and_left)
        {
            MockLogger logger;
            RulesFixture f;

            ElementMap character = townsman();
            character["bog_iron"] = ElementType{1};
            character["pitch"] = ElementType{3};
            character["wisp_glass"] = ElementType{1};

            RuleMonth month;
            ASSERT_TRUE(f.rules.month(20.0f, character, month)) << firstError(f.rules);

            for (const char* id : {"bog_iron", "pitch", "wisp_glass"})
            {
                const ElementMap* held = byId(month.rows, id);

                ASSERT_NE(held, nullptr) << id;
                EXPECT_EQ(textOf(*held, "groupLabel"), "STORES") << id;
            }

            std::vector<RuleActivity> activities;
            ASSERT_TRUE(f.rules.activities(20.0f, character, activities)) << firstError(f.rules);

            auto asks = [&activities](const std::string& id, const std::string& stat) {
                for (const auto& a : activities)
                {
                    if (textOf(a.fields, "id") != id)
                        continue;

                    for (const auto& c : a.costs)
                    {
                        if (textOf(c, "stat") == stat)
                            return true;
                    }
                }

                return false;
            };

            for (const auto& a : activities)
            {
                for (const char* id : {"sell.bog_iron", "sell.pitch", "sell.wisp_glass"})
                    EXPECT_NE(textOf(a.fields, "id"), id);
            }

            EXPECT_TRUE(asks("raise.collegium.3", "wisp_glass"));
            EXPECT_FALSE(asks("raise.collegium.3", "antler"));
            EXPECT_TRUE(asks("raise.harrow.3", "bog_iron"));
            EXPECT_FALSE(asks("raise.harrow.3", "shell_plate"));
            EXPECT_TRUE(asks("raise.storehouse.3", "bog_iron"));
            EXPECT_FALSE(asks("raise.storehouse.3", "shell_plate"));

            // The Harrow Road at its second level: his bog iron is left to it
            character["seen.harrow"] = ElementType{1};
            character["town.harrow"] = ElementType{2};

            RuleTown town;
            ASSERT_TRUE(f.rules.town(20.0f, character, town)) << firstError(f.rules);

            int left = 0;

            for (const auto& place : town.places)
            {
                if (textOf(place.fields, "id") != "harrow")
                    continue;

                for (const auto& l : place.leaves)
                {
                    if (textOf(l, "stat") == "bog_iron")
                        left = intOf(l, "amount");
                }
            }

            EXPECT_EQ(left, 1);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // The market's rows say what kind of trade they are (`trade`): "buy" for what he pays coin
        // for, "sell" for what he is paid for, nothing for what is made there (a remedy, a flask).
        // The Town page puts them on lines of their own by it, and the button names the act
        TEST(rules_test, the_market_rows_say_their_trade)
        {
            MockLogger logger;
            RulesFixture f;

            std::vector<RuleActivity> activities;
            ASSERT_TRUE(f.rules.activities(20.0f, townsman(), activities)) << firstError(f.rules);

            int buys = 0;
            int sells = 0;

            for (const auto& a : activities)
            {
                const std::string id = textOf(a.fields, "id");

                ASSERT_TRUE(a.fields.count("trade")) << id << " has no trade";

                const std::string trade = textOf(a.fields, "trade");

                if (id.rfind("buy.", 0) == 0)
                {
                    EXPECT_EQ(trade, "buy") << id;
                    ++buys;
                }
                else if (id.rfind("sell.", 0) == 0)
                {
                    EXPECT_EQ(trade, "sell") << id;
                    ++sells;
                }
                else if (id.rfind("brew.", 0) == 0)
                {
                    EXPECT_EQ(trade, "make") << id;
                }
                else if (id == "dry.berries")
                {
                    EXPECT_EQ(trade, "make") << id;
                }
                else
                {
                    EXPECT_EQ(trade, "") << id;
                }
            }

            // The hand-written buys (rations to the horse, and the chapel's oil), the basics bought,
            // and what a walk brings home sold with the basics
            EXPECT_GE(buys, 11);
            EXPECT_GE(sells, 5);

            for (const char* id : {"buy.rations", "buy.reagents", "buy.blade", "buy.wire", "buy.primer", "buy.sword", "buy.horse", "buy.oil", "buy.water", "buy.firewood", "buy.stone"})
                EXPECT_EQ(textOf(activityAt(f.rules, 20.0f, townsman(), id).fields, "trade"), "buy") << id;

            for (const char* id : {"sell.berries", "sell.water", "sell.firewood", "sell.stone", "sell.clay"})
                EXPECT_EQ(textOf(activityAt(f.rules, 20.0f, townsman(), id).fields, "trade"), "sell") << id;

            EXPECT_EQ(textOf(activityAt(f.rules, 20.0f, townsman(), "brew.remedy").fields, "trade"), "make");
            EXPECT_EQ(textOf(activityAt(f.rules, 20.0f, townsman(), "brew.flask").fields, "trade"), "make");
            EXPECT_EQ(textOf(activityAt(f.rules, 20.0f, townsman(), "dry.berries").fields, "trade"), "make");
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // The gem weaver is studied, as the imp and the brownie are, so that the gems of the town's
        // higher levels wait on no boss: from fourteen, with Letters 2 and some Lore, for a little
        // stone and water. The row is gone once the world knows the kind, and the kind is bindable
        TEST(rules_test, the_stone_weavers_craft_teaches_the_weaver)
        {
            MockLogger logger;
            RulesFixture f;

            ElementMap reader = boy();
            reader["letters"] = ElementType{2};
            reader["lore"] = ElementType{1};
            reader["stone"] = ElementType{5};
            reader["water"] = ElementType{5};

            EXPECT_FALSE(flag(activityAt(f.rules, 13.5f, reader, "research.weaver").fields, "listed"));

            RuleActivity row = activityAt(f.rules, 14.5f, reader, "research.weaver");
            EXPECT_TRUE(flag(row.fields, "listed"));
            EXPECT_FALSE(flag(row.fields, "locked"));
            EXPECT_EQ(textOf(row.fields, "group"), "Study");
            EXPECT_EQ(textOf(row.fields, "name"), "Study the Stone-Weavers' Craft");
            ASSERT_EQ(row.gains.size(), 1u);
            EXPECT_EQ(textOf(row.gains[0], "stat"), "know.fam.weaver");

            std::map<std::string, int> costs;

            for (const auto& c : row.costs)
                costs[textOf(c, "stat")] = intOf(c, "amount");

            EXPECT_EQ(costs, (std::map<std::string, int>{{"stone", 5}, {"water", 5}}));

            // No lore, no study: the row stands shut
            ElementMap unread = reader;
            unread.erase("lore");
            EXPECT_TRUE(flag(activityAt(f.rules, 14.5f, unread, "research.weaver").fields, "locked"));

            // Not before he can bind it, and gone once it is known
            EXPECT_FALSE(flag(activityAt(f.rules, 14.5f, reader, "bind.weaver").fields, "listed"));

            reader["know.fam.weaver"] = ElementType{1};
            EXPECT_FALSE(flag(activityAt(f.rules, 14.5f, reader, "research.weaver").fields, "listed"));
            EXPECT_TRUE(flag(activityAt(f.rules, 14.5f, reader, "bind.weaver").fields, "listed"));
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // His familiars eat a fed work's up-front rations with him: the row says how long they last
        // as things stand (`upFrontLasts`), when that is short of the work, and nothing otherwise
        TEST(rules_test, a_fed_work_says_how_long_its_rations_last)
        {
            MockLogger logger;
            RulesFixture f;

            // Alone, what the carters give lasts their three months
            ElementMap alone = boy();
            alone["rations"] = ElementType{0};

            RuleActivity carters = activityAt(f.rules, 8.0f, alone, "carters");
            EXPECT_EQ(intOf(carters.fields, "upFront"), 3);
            EXPECT_EQ(intOf(carters.fields, "upFrontLasts"), 0);

            // A stone mole eats two rations: three a month, and the three last one
            ElementMap kept = alone;
            kept["fam.mole"] = ElementType{1};
            EXPECT_EQ(intOf(activityAt(f.rules, 8.0f, kept, "carters").fields, "upFrontLasts"), 1);

            // What he holds counts with them: 3 more last two months, 6 more the three
            kept["rations"] = ElementType{3};
            EXPECT_EQ(intOf(activityAt(f.rules, 8.0f, kept, "carters").fields, "upFrontLasts"), 2);

            kept["rations"] = ElementType{6};
            EXPECT_EQ(intOf(activityAt(f.rules, 8.0f, kept, "carters").fields, "upFrontLasts"), 0);

            // A work that does not feed him says nothing
            EXPECT_EQ(intOf(activityAt(f.rules, 8.0f, kept, "mill").fields, "upFrontLasts"), 0);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // A thing counted once is named once: the found thing a level asks ("1 Pelt", not "1
        // Pelts") on its costs line and on what a death would leave, and the wisp's one reagent
        TEST(rules_test, one_thing_is_named_once)
        {
            MockLogger logger;
            RulesFixture f;

            ElementMap character = townsman();
            character["town.market"] = ElementType{2};
            character["town.yard"] = ElementType{2};
            character["town.chapel"] = ElementType{2};
            character["pelts"] = ElementType{1};
            character["hides"] = ElementType{1};
            character["antler"] = ElementType{1};
            character["gems"] = ElementType{1};

            RuleTown town;
            ASSERT_TRUE(f.rules.town(20.0f, character, town)) << firstError(f.rules);

            std::map<std::string, const RulePlace*> places;

            for (const auto& place : town.places)
                places[textOf(place.fields, "id")] = &place;

            ASSERT_EQ(places.count("market"), 1u);
            ASSERT_EQ(places.count("yard"), 1u);
            ASSERT_EQ(places.count("chapel"), 1u);

            const std::string market = textOf(places["market"]->fields, "nextCosts");
            EXPECT_NE(market.find(" · 1 Pelt"), std::string::npos) << market;
            EXPECT_EQ(market.find("1 Pelts"), std::string::npos) << market;
            EXPECT_NE(market.find(" · 4 Gems"), std::string::npos) << market;

            EXPECT_NE(textOf(places["yard"]->fields, "nextCosts").find(" · 1 Hide"), std::string::npos) << textOf(places["yard"]->fields, "nextCosts");
            EXPECT_NE(textOf(places["chapel"]->fields, "nextCosts").find(" · 1 White antler"), std::string::npos) << textOf(places["chapel"]->fields, "nextCosts");

            EXPECT_EQ(textOf(places["market"]->fields, "leaveLine"), "1 gem, 1 pelt");
            EXPECT_EQ(textOf(places["yard"]->fields, "leaveLine"), "1 gem, 1 hide");
            EXPECT_EQ(textOf(places["chapel"]->fields, "leaveLine"), "1 gem, 1 white antler");

            // What a wisp makes: one reagent
            EXPECT_EQ(textOf(activityAt(f.rules, 18.0f, boy(), "bind.wisp").fields, "each"), "EATS 1 RATION, 2 WATER \xC2\xB7 MAKES 1 REAGENT");
        }
    }
}
