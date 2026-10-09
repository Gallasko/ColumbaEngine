#include "stdafx.h"

#include <algorithm>
#include <chrono>
#include <functional>
#include <iostream>
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

                    // A later life, whose list is whole from its first day: a first life is shown its
                    // tasks one at a time, and the tests of that say so themselves
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
                return {{"str", ElementType{6}}, {"dex", ElementType{6}}, {"int", ElementType{6}}, {"vit", ElementType{8}}, {"vitmax", ElementType{8}},
                        {"coin", ElementType{0}}, {"rations", ElementType{12}}};
            }

            // The same boy once a life has explored the town: its market is his to buy at
            ElementMap townsman()
            {
                ElementMap character = boy();
                character["town_known"] = ElementType{1};

                return character;
            }

            // A first life at its first frame: a month before 7, holding nothing
            ElementMap newcomer()
            {
                return {{"str", ElementType{6}}, {"dex", ElementType{6}}, {"int", ElementType{6}}, {"vit", ElementType{8}}, {"vitmax", ElementType{8}},
                        {"coin", ElementType{0}}};
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
            EXPECT_EQ(textOf(mill.gains[0], "name"), "Strength");
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

            for (const char* id : {"poach", "burners", "outlaws"})
            {
                RuleActivity work = activityAt(f.rules, 21.0f, outlaw, id);

                EXPECT_TRUE(flag(work.fields, "listed")) << id;
                EXPECT_FALSE(flag(work.fields, "locked")) << id;
            }

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
            EXPECT_FALSE(flag(activityAt(f.rules, 9.0f, boy(), "chapel").fields, "listed"));

            ElementMap lettered = boy();
            lettered["letters"] = ElementType{1};

            EXPECT_TRUE(flag(activityAt(f.rules, 9.0f, lettered, "chapel").fields, "listed"));

            // A strong boy at 16 sees the Keep, though he cannot pay its coin yet
            const ElementMap squire = {{"str", ElementType{12}}, {"arms", ElementType{3}}, {"discipline", ElementType{2}}, {"int", ElementType{6}}};

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
        // A month eats a ration, and says so as a rate; a month at work that feeds him eats none, and
        // with none held costs nothing either.
        TEST(rules_test, month_decays_unless_fed)
        {
            MockLogger logger;
            RulesFixture f;

            ElementMap character = {{"vit", ElementType{12}}, {"coin", ElementType{3}}, {"rations", ElementType{5}}};

            RuleMonth month;
            ASSERT_TRUE(f.rules.month(20.0f, character, false, month)) << firstError(f.rules);

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

            // Fed by his work
            ASSERT_TRUE(f.rules.month(20.0f, character, true, month));
            EXPECT_EQ(intOf(month.after, "rations"), 5);
            EXPECT_EQ(textOf(*byId(month.rows, "rations"), "rate"), "");

            // The last ration eaten is a fed month: no Vitality taken
            ASSERT_TRUE(f.rules.month(20.0f, {{"vit", ElementType{12}}, {"rations", ElementType{1}}}, false, month));
            EXPECT_EQ(intOf(month.after, "rations"), 0);
            EXPECT_EQ(intOf(month.after, "vit"), 12);

            // With none, fed by his work: nothing is taken, nothing is written
            ASSERT_TRUE(f.rules.month(20.0f, {{"vit", ElementType{12}}, {"rations", ElementType{0}}}, true, month));
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

            ASSERT_TRUE(f.rules.month(20.0f, {{"vit", ElementType{12}}, {"rations", ElementType{1}}}, false, month));
            EXPECT_EQ(intOf(month.after, "rations"), 0);
            EXPECT_EQ(intOf(month.after, "vit"), 12);
            ASSERT_EQ(month.entries.size(), 1u);
            EXPECT_EQ(textOf(month.entries[0], "kind"), "loss");

            EXPECT_TRUE(month.hurt.empty());

            ASSERT_TRUE(f.rules.month(20.0f, {{"vit", ElementType{12}}, {"rations", ElementType{0}}}, false, month));
            EXPECT_EQ(intOf(month.after, "rations"), 0);
            EXPECT_EQ(intOf(month.after, "vit"), 11);
            EXPECT_TRUE(month.entries.empty());

            // A month that took from what the life hangs on says so
            ASSERT_EQ(month.hurt.size(), 1u);
            EXPECT_EQ(month.hurt[0], "vit");

            // Nothing goes below zero
            ASSERT_TRUE(f.rules.month(20.0f, {{"vit", ElementType{0}}, {"rations", ElementType{0}}}, false, month));
            EXPECT_EQ(intOf(month.after, "vit"), 0);

            ASSERT_TRUE(f.rules.month(20.0f, {{"vit", ElementType{12}}}, false, month));
            EXPECT_EQ(intOf(month.after, "vit"), 12);
            EXPECT_EQ(month.after.count("rations"), 0u);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // What he lives on running out comes with what to do about it: one sentence while it runs
        // low, another once it is gone, none while his work feeds him.
        TEST(rules_test, month_says_what_to_do)
        {
            MockLogger logger;
            RulesFixture f;

            RuleMonth month;

            ASSERT_TRUE(f.rules.month(20.0f, {{"vit", ElementType{12}}, {"rations", ElementType{12}}}, false, month));
            EXPECT_EQ(month.warning, "");
            EXPECT_EQ(month.advice, "");

            ASSERT_TRUE(f.rules.month(20.0f, {{"vit", ElementType{12}}, {"rations", ElementType{3}}}, false, month));
            EXPECT_EQ(month.warning, "RATIONS FOR 3 MONTHS");
            EXPECT_NE(month.advice.find("running low"), std::string::npos);

            const std::string low = month.advice;

            // The same sentence down to the last one: said once, not every month
            ASSERT_TRUE(f.rules.month(20.0f, {{"vit", ElementType{12}}, {"rations", ElementType{1}}}, false, month));
            EXPECT_EQ(month.warning, "RATIONS FOR 1 MONTH");
            EXPECT_EQ(month.advice, low);

            ASSERT_TRUE(f.rules.month(20.0f, {{"vit", ElementType{12}}, {"rations", ElementType{0}}}, false, month));
            EXPECT_EQ(month.warning, "NO RATIONS LEFT");
            EXPECT_NE(month.advice.find("No rations left"), std::string::npos);
            EXPECT_NE(month.advice, low);

            // Fed by his work: nothing runs out, nothing to say
            ASSERT_TRUE(f.rules.month(20.0f, {{"vit", ElementType{12}}, {"rations", ElementType{2}}}, true, month));
            EXPECT_EQ(month.warning, "");
            EXPECT_EQ(month.advice, "");
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
            ASSERT_TRUE(f.rules.month(20.0f, {{"vit", ElementType{12}}, {"coin", ElementType{3}}, {"rations", ElementType{5}}}, false, month)) << firstError(f.rules);

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
            EXPECT_EQ(rowOf(*rations, "With none, Vitality"), "\xE2\x88\x92" "1 / mo");
            EXPECT_FALSE(textOf(rations->fields, "text").empty());

            EXPECT_EQ(intOf(*byId(month.rows, "rations"), "limit"), 60);
            EXPECT_EQ(intOf(*byId(month.rows, "coin"), "limit"), 0);

            // Fed by his work, the month uses none, and the gloss says so
            ASSERT_TRUE(f.rules.month(20.0f, {{"vit", ElementType{12}}, {"rations", ElementType{5}}}, true, month));
            EXPECT_NE(textOf(glossOf(month, "rations")->fields, "footnote").find("MEALS PROVIDED"), std::string::npos);

            ASSERT_TRUE(f.rules.month(20.0f, {{"vit", ElementType{12}}, {"rations", ElementType{60}}}, false, month));
            EXPECT_NE(textOf(glossOf(month, "rations")->fields, "footnote").find("FULL"), std::string::npos);

            // What he holds past the limit is his: the month only eats from it
            ASSERT_TRUE(f.rules.month(20.0f, {{"vit", ElementType{12}}, {"rations", ElementType{64}}}, false, month));
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
        // A stat with a cap mends toward it, one every two months; a month that takes from it mends
        // nothing and starts the count again. What raises it for good raises the cap with it.
        TEST(rules_test, month_mends_toward_the_cap)
        {
            MockLogger logger;
            RulesFixture f;

            RuleMonth month;

            ASSERT_TRUE(f.rules.month(20.0f, {{"vit", ElementType{9}}, {"vitmax", ElementType{12}}, {"rations", ElementType{5}}}, false, month)) << firstError(f.rules);
            EXPECT_EQ(intOf(month.after, "vit"), 9);
            EXPECT_EQ(intOf(month.after, "vitrest"), 1);
            EXPECT_TRUE(month.hurt.empty());

            ASSERT_EQ(month.caps.size(), 1u);
            EXPECT_EQ(textOf(month.caps[0], "stat"), "vit");
            EXPECT_EQ(intOf(month.caps[0], "most"), 12);

            ASSERT_TRUE(f.rules.month(20.0f, {{"vit", ElementType{9}}, {"vitmax", ElementType{12}}, {"vitrest", ElementType{1}}, {"rations", ElementType{5}}}, false, month));
            EXPECT_EQ(intOf(month.after, "vit"), 10);
            EXPECT_EQ(intOf(month.after, "vitmax"), 12);
            EXPECT_EQ(intOf(month.after, "vitrest"), 0);

            // Starving: nothing mends, the count starts again
            ASSERT_TRUE(f.rules.month(20.0f, {{"vit", ElementType{9}}, {"vitmax", ElementType{12}}, {"vitrest", ElementType{1}}, {"rations", ElementType{0}}}, false, month));
            EXPECT_EQ(intOf(month.after, "vit"), 8);
            EXPECT_EQ(intOf(month.after, "vitmax"), 12);
            EXPECT_EQ(intOf(month.after, "vitrest"), 0);

            // Fed by his work, it mends as on any good month
            ASSERT_TRUE(f.rules.month(20.0f, {{"vit", ElementType{9}}, {"vitmax", ElementType{12}}, {"vitrest", ElementType{1}}, {"rations", ElementType{0}}}, true, month));
            EXPECT_EQ(intOf(month.after, "vit"), 10);

            // Whole: it stays
            ASSERT_TRUE(f.rules.month(20.0f, {{"vit", ElementType{12}}, {"vitmax", ElementType{12}}, {"vitrest", ElementType{1}}}, false, month));
            EXPECT_EQ(intOf(month.after, "vit"), 12);

            // A character from before the cap takes what he has as his most
            ASSERT_TRUE(f.rules.month(20.0f, {{"vit", ElementType{7}}}, false, month));
            EXPECT_EQ(intOf(month.after, "vitmax"), 7);

            // No such stat, no cap
            ASSERT_TRUE(f.rules.month(20.0f, {{"coin", ElementType{3}}}, false, month));
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

            ASSERT_TRUE(f.rules.month(20.0f, {{"vit", ElementType{1}}, {"rations", ElementType{0}}}, false, month));
            EXPECT_EQ(intOf(month.after, "vit"), 0);
            EXPECT_TRUE(month.death.empty());

            ASSERT_TRUE(f.rules.month(20.0f, {{"vit", ElementType{0}}, {"rations", ElementType{0}}}, false, month));
            EXPECT_FALSE(month.death.empty());

            // A character with no such stat at all is not dead of it
            ASSERT_TRUE(f.rules.month(20.0f, {{"coin", ElementType{3}}}, false, month));
            EXPECT_TRUE(month.death.empty());
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // Past 30 the most his Vitality can be falls by one every two months, and his Vitality with
        // it: nothing mends it back, the page is not stopped for it, and the life that ends there
        // ended in old age.
        TEST(rules_test, old_age_takes_his_vitality)
        {
            MockLogger logger;
            RulesFixture f;

            const float twelfth = 1.0f / 12.0f;

            RuleMonth month;

            // The month that brings him to 30, and the one after: nothing yet
            ASSERT_TRUE(f.rules.month(30.0f, {{"vit", ElementType{9}}, {"vitmax", ElementType{9}}, {"rations", ElementType{5}}}, false, month)) << firstError(f.rules);
            EXPECT_EQ(intOf(month.after, "vit"), 9);
            EXPECT_EQ(intOf(month.after, "vitmax"), 9);

            ASSERT_TRUE(f.rules.month(30.0f + twelfth, {{"vit", ElementType{9}}, {"vitmax", ElementType{9}}, {"rations", ElementType{5}}}, false, month));
            EXPECT_EQ(intOf(month.after, "vit"), 9);
            EXPECT_EQ(intOf(month.after, "vitmax"), 9);

            // Every second month: the most it can be, and what he has with it. Not a hurt
            ASSERT_TRUE(f.rules.month(30.0f + 2.0f * twelfth, {{"vit", ElementType{9}}, {"vitmax", ElementType{9}}, {"rations", ElementType{5}}}, false, month));
            EXPECT_EQ(intOf(month.after, "vit"), 8);
            EXPECT_EQ(intOf(month.after, "vitmax"), 8);
            EXPECT_TRUE(month.hurt.empty());

            // Already under it: only the most falls
            ASSERT_TRUE(f.rules.month(30.0f + 4.0f * twelfth, {{"vit", ElementType{5}}, {"vitmax", ElementType{9}}, {"rations", ElementType{5}}}, false, month));
            EXPECT_EQ(intOf(month.after, "vit"), 5);
            EXPECT_EQ(intOf(month.after, "vitmax"), 8);

            // In his prime the same months take nothing
            ASSERT_TRUE(f.rules.month(20.0f + 2.0f * twelfth, {{"vit", ElementType{9}}, {"vitmax", ElementType{9}}, {"rations", ElementType{5}}}, false, month));
            EXPECT_EQ(intOf(month.after, "vit"), 9);
            EXPECT_EQ(intOf(month.after, "vitmax"), 9);

            // The last of it, and the line of a life that ended old
            ASSERT_TRUE(f.rules.month(32.0f, {{"vit", ElementType{1}}, {"vitmax", ElementType{1}}, {"rations", ElementType{5}}}, false, month));
            EXPECT_EQ(intOf(month.after, "vit"), 0);
            EXPECT_EQ(intOf(month.after, "vitmax"), 0);
            EXPECT_TRUE(month.death.empty());

            ASSERT_TRUE(f.rules.month(32.0f, {{"vit", ElementType{0}}, {"vitmax", ElementType{0}}, {"rations", ElementType{5}}}, false, month));
            EXPECT_NE(month.death.find("old age"), std::string::npos) << month.death;

            ASSERT_TRUE(f.rules.month(20.0f, {{"vit", ElementType{0}}, {"rations", ElementType{0}}}, false, month));
            EXPECT_FALSE(month.death.empty());
            EXPECT_EQ(month.death.find("old age"), std::string::npos) << month.death;
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

            // With what is bought in town, the tile that goes there, and the works for the town,
            // which no age closes
            for (const char* id : {"goto.market", "buy.rations", "tales", "garden", "teach"})
                EXPECT_NE(std::find(listed.begin(), listed.end(), id), listed.end()) << id;

            for (const auto& id : listed)
            {
                const bool old = id == "goto.market" or id == "buy.rations" or id == "tales" or id == "garden" or id == "teach";

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
        // The opening task: a month that costs nothing and brings the first rations, for a first
        // life that has done nothing yet and for no other.
        TEST(rules_test, helping_is_listed_only_in_a_first_life)
        {
            MockLogger logger;
            RulesFixture f;

            const float start = 6.0f + 11.0f / 12.0f;

            f.rules.lives = 1;

            RuleActivity helping = activityAt(f.rules, start, newcomer(), "helping");

            EXPECT_TRUE(flag(helping.fields, "listed"));
            EXPECT_FALSE(flag(helping.fields, "locked"));
            EXPECT_EQ(intOf(helping.fields, "months"), 1);
            EXPECT_TRUE(flag(helping.fields, "board"));
            EXPECT_EQ(intOf(helping.fields, "uses"), 1);
            EXPECT_TRUE(flag(helping.fields, "firstLife"));

            ASSERT_EQ(helping.gains.size(), 1u);
            EXPECT_EQ(textOf(helping.gains[0], "stat"), "rations");
            EXPECT_EQ(intOf(helping.gains[0], "amount"), 12);

            // Done, it is spent; and a first life that has done anything else no longer opens on it
            f.rules.done = {{"helping", ElementType{1}}};
            EXPECT_FALSE(flag(activityAt(f.rules, 7.0f, boy(), "helping").fields, "listed"));

            f.rules.done = {{"carters", ElementType{1}}};
            EXPECT_FALSE(flag(activityAt(f.rules, 7.25f, boy(), "helping").fields, "listed"));

            // A later life never has its row
            f.rules.done = {};
            f.rules.lives = 2;

            EXPECT_FALSE(flag(activityAt(f.rules, start, newcomer(), "helping").fields, "listed"));
            EXPECT_FALSE(flag(activityAt(f.rules, 7.0f, boy(), "helping").fields, "listed"));
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // A first life is shown its tasks one at a time, by the terms it has done; a later life has
        // them all from its first day.
        TEST(rules_test, show_after_counts_terms_in_a_first_life)
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

            auto has = [](const std::vector<std::string>& ids, const std::string& id) {
                return std::find(ids.begin(), ids.end(), id) != ids.end();
            };

            f.rules.lives = 1;

            // Nothing done: the opening task alone
            EXPECT_EQ(listed(6.0f + 11.0f / 12.0f, newcomer()), (std::vector<std::string>{"helping"}));

            f.rules.done = {{"helping", ElementType{1}}};
            EXPECT_EQ(listed(7.0f, boy()), (std::vector<std::string>{"carters"}));

            f.rules.done = {{"helping", ElementType{1}}, {"carters", ElementType{1}}};
            EXPECT_EQ(listed(7.25f, boy()), (std::vector<std::string>{"carters", "messages"}));

            // Three terms: the kitchen and the mill
            f.rules.done = {{"helping", ElementType{1}}, {"carters", ElementType{1}}, {"messages", ElementType{1}}};
            EXPECT_EQ(listed(7.5f, boy()), (std::vector<std::string>{"carters", "mill", "messages", "kitchen"}));

            // Four: his letters, and the three ways into a class, which the guide speaks of then
            f.rules.done = {{"helping", ElementType{1}}, {"carters", ElementType{1}}, {"messages", ElementType{1}}, {"kitchen", ElementType{1}}};
            EXPECT_EQ(listed(7.75f, boy()), (std::vector<std::string>{"carters", "mill", "messages", "kitchen", "letters", "explore", "keep", "collegium", "hand"}));

            // Five: whatever his age has opened meanwhile. The terms count, whichever they were
            f.rules.done = {{"helping", ElementType{1}}, {"carters", ElementType{4}}};

            const std::vector<std::string> grown = listed(8.25f, boy());

            for (const char* id : {"carters", "mill", "messages", "kitchen", "letters", "keep", "collegium", "hand", "market", "roam"})
                EXPECT_TRUE(has(grown, id)) << id;

            // A later life with nothing done: the list of its first day, whole
            f.rules.done = {};
            f.rules.lives = 2;

            EXPECT_EQ(listed(7.0f, boy()), (std::vector<std::string>{"carters", "mill", "messages", "kitchen", "letters", "explore", "keep", "collegium", "hand"}));
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
        // The kitchen feeds him while he is at it and sends him off with rations, while he has room
        // for them.
        TEST(rules_test, kitchen_feeds_and_brings_rations)
        {
            MockLogger logger;
            RulesFixture f;

            RuleActivity kitchen = activityAt(f.rules, 7.0f, boy(), "kitchen");

            EXPECT_TRUE(flag(kitchen.fields, "listed"));
            EXPECT_FALSE(flag(kitchen.fields, "locked"));
            EXPECT_TRUE(flag(kitchen.fields, "board"));
            EXPECT_EQ(intOf(kitchen.fields, "months"), 3);

            ASSERT_EQ(kitchen.gains.size(), 1u);
            EXPECT_EQ(textOf(kitchen.gains[0], "stat"), "rations");
            EXPECT_EQ(intOf(kitchen.gains[0], "amount"), 6);

            // It asks for room, as everything that brings rations does
            ASSERT_EQ(kitchen.requires.size(), 1u);
            EXPECT_EQ(textOf(kitchen.requires[0], "label"), "Room for Rations");
            EXPECT_EQ(intOf(kitchen.requires[0], "current"), 48);

            ElementMap full = boy();
            full["rations"] = ElementType{60};

            kitchen = activityAt(f.rules, 7.0f, full, "kitchen");

            EXPECT_TRUE(flag(kitchen.fields, "locked"));
            EXPECT_EQ(intOf(kitchen.requires[0], "current"), 0);

            // The term brings them
            RuleForecast forecast;
            ASSERT_TRUE(f.rules.forecast(7.0f, boy(), "kitchen", 0, forecast)) << firstError(f.rules);
            EXPECT_EQ(intOf(forecast.atTerm, "rations"), 18);
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

            const std::vector<std::string> kinds = {"button:", "tile:", "holding:", "stat:"};
            const std::vector<std::string> places = {"", "clock", "log"};

            std::vector<int> taken(11, 0);
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

                // A work is begun in two presses: a step that waits for one shows its tile, then
                // the button that begins it, and says what to press
                const std::string chosen = textOf(a.fields, "pointChosen");

                if (point.rfind("tile:", 0) == 0 and not a.until.empty())
                {
                    EXPECT_EQ(chosen, "button:begin") << id;
                    EXPECT_FALSE(textOf(a.fields, "sayChosen").empty()) << id;
                }
                else
                {
                    EXPECT_TRUE(chosen.empty()) << id;
                    EXPECT_TRUE(textOf(a.fields, "sayChosen").empty()) << id;
                }

                if (a.order == 0)
                {
                    // Outside the sequence: it waits for no step
                    EXPECT_EQ(waitsFor(a), -1) << id;
                    EXPECT_EQ(id, "guide.rations_low");
                    ++outside;

                    continue;
                }

                ASSERT_GE(a.order, 1) << id;
                ASSERT_LE(a.order, 10) << id;

                ++taken[a.order];

                EXPECT_EQ(waitsFor(a), a.order - 1) << id;
            }

            for (int order = 1; order <= 10; ++order)
                EXPECT_EQ(taken[order], 1) << "order " << order;

            EXPECT_EQ(outside, 1u);
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
            character["town.market"] = ElementType{1};
            character["town.mill"] = ElementType{3};

            RuleTown town;
            ASSERT_TRUE(f.rules.town(9.0f, character, town)) << firstError(f.rules);

            ASSERT_EQ(town.places.size(), 9u);
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
            EXPECT_EQ(intOf(yard.fields, "nextCoin"), 20);
            EXPECT_FALSE(flag(yard.fields, "nextLocked"));

            // At its first level: what it gives, and a second that asks more than he has
            const RulePlace& market = placed("market");

            EXPECT_EQ(intOf(market.fields, "level"), 1);
            EXPECT_FALSE(textOf(market.fields, "gives").empty());
            EXPECT_FALSE(textOf(market.fields, "tale").empty());
            EXPECT_EQ(textOf(market.fields, "nextWork"), "raise.market.2");
            EXPECT_TRUE(flag(market.fields, "nextLocked"));

            ASSERT_EQ(market.gaps.size(), 2u);
            EXPECT_EQ(textOf(market.gaps[0], "stat"), "coin");
            EXPECT_EQ(intOf(market.gaps[0], "current"), 30);
            EXPECT_EQ(intOf(market.gaps[0], "needed"), 60);
            EXPECT_EQ(textOf(market.gaps[1], "stat"), "guile");

            // At its last: nothing more to raise
            const RulePlace& mill = placed("mill");

            EXPECT_EQ(intOf(mill.fields, "level"), 3);
            EXPECT_EQ(textOf(mill.fields, "nextWork"), "");
            EXPECT_TRUE(mill.gaps.empty());
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // What lives left to a place at their death is counted toward its next level: the page and
        // the work that raises it ask that much less coin, never less than none.
        TEST(rules_test, town_fund_lowers_the_coin_asked)
        {
            MockLogger logger;
            RulesFixture f;

            ElementMap character = townsman();
            character["coin"] = ElementType{10};
            character["fund.mill"] = ElementType{15};
            character["fund.chapel"] = ElementType{84};

            RuleTown town;
            ASSERT_TRUE(f.rules.town(9.0f, character, town)) << firstError(f.rules);

            for (const auto& place : town.places)
            {
                const std::string id = textOf(place.fields, "id");

                if (id == "mill")
                {
                    EXPECT_EQ(intOf(place.fields, "nextCoin"), 5);
                    EXPECT_EQ(intOf(place.fields, "fund"), 15);
                    EXPECT_FALSE(flag(place.fields, "nextLocked"));
                }

                if (id == "chapel")
                    EXPECT_EQ(intOf(place.fields, "nextCoin"), 0);

                if (id == "yard")
                    EXPECT_TRUE(flag(place.fields, "nextLocked"));
            }

            RuleActivity mill = activityAt(f.rules, 9.0f, character, "raise.mill.1");

            ASSERT_EQ(mill.costs.size(), 1u);
            EXPECT_EQ(textOf(mill.costs[0], "stat"), "coin");
            EXPECT_EQ(intOf(mill.costs[0], "amount"), 5);
            EXPECT_FALSE(flag(mill.fields, "locked"));

            // Paid for whole: the work asks no coin at all
            EXPECT_TRUE(activityAt(f.rules, 9.0f, character, "raise.chapel.1").costs.empty());
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

            // What it takes of him besides coin is a cost like any other
            RuleActivity second = activityAt(f.rules, 9.0f, character, "raise.market.2");

            ASSERT_EQ(second.costs.size(), 2u);
            EXPECT_EQ(textOf(second.costs[1], "stat"), "guile");
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
        // Exploring the town is for who does not know it.
        TEST(rules_test, explore_hidden_once_known)
        {
            MockLogger logger;
            RulesFixture f;

            RuleActivity explore = activityAt(f.rules, 7.0f, boy(), "explore");

            EXPECT_TRUE(flag(explore.fields, "listed"));
            EXPECT_FALSE(flag(explore.fields, "locked"));
            EXPECT_EQ(intOf(explore.fields, "months"), 3);
            ASSERT_EQ(explore.gains.size(), 1u);
            EXPECT_EQ(textOf(explore.gains[0], "stat"), "town_known");

            EXPECT_FALSE(flag(activityAt(f.rules, 7.0f, townsman(), "explore").fields, "listed"));

            // A first life meets it with its fourth term
            f.rules.lives = 1;
            f.rules.done = {{"helping", ElementType{1}}, {"carters", ElementType{2}}};
            EXPECT_FALSE(flag(activityAt(f.rules, 7.5f, boy(), "explore").fields, "listed"));

            f.rules.done = {{"helping", ElementType{1}}, {"carters", ElementType{3}}};
            EXPECT_TRUE(flag(activityAt(f.rules, 7.75f, boy(), "explore").fields, "listed"));
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

            RuleActivity go = activityAt(f.rules, 9.0f, character, "goto.market");

            EXPECT_EQ(textOf(go.fields, "goto"), "town");
            EXPECT_EQ(textOf(go.fields, "place"), "");
            EXPECT_EQ(intOf(go.fields, "months"), 0);
            EXPECT_TRUE(flag(go.fields, "listed"));
            EXPECT_FALSE(flag(activityAt(f.rules, 9.0f, boy(), "goto.market").fields, "listed"));

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
        // What a place gives reaches a row through its `bonus` steps: its months, what it brings,
        // how often it can be done, the age it opens at.
        TEST(rules_test, bonus_steps_read_the_town)
        {
            MockLogger logger;
            RulesFixture f;

            ElementMap character = boy();

            EXPECT_EQ(intOf(activityAt(f.rules, 8.0f, character, "letters").fields, "months"), 6);
            EXPECT_EQ(intOf(activityAt(f.rules, 8.0f, character, "blades").fields, "uses"), 3);
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
            EXPECT_EQ(intOf(activityAt(f.rules, 8.0f, character, "blades").fields, "uses"), 5);
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

            EXPECT_EQ(asked(character, "keep", "str"), 12);
            EXPECT_EQ(asked(character, "collegium", "int"), 12);

            character["town.yard"] = ElementType{1};
            EXPECT_EQ(asked(character, "keep", "str"), 12);

            character["town.yard"] = ElementType{2};
            EXPECT_EQ(asked(character, "keep", "str"), 11);

            // The sergeant's word still eases it on its own
            EXPECT_EQ(asked({{"edric_support", ElementType{1}}}, "keep", "str"), 11);

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
            EXPECT_EQ(given("rations"), 6);
            EXPECT_EQ(given("str"), 1);
            EXPECT_EQ(given("watch_known"), 1);
            EXPECT_EQ(given("arms"), 0);

            // And the inn tells a new life of the one before, from its second level
            EXPECT_FALSE(town.epitaphLine.empty());
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // The market hall sends a ration a month, to every life, with no row of its own in the
        // ledger; and the family's room at the inn mends him every month.
        TEST(rules_test, stall_produces_without_a_row)
        {
            MockLogger logger;
            RulesFixture f;

            ElementMap character = townsman();
            character["vit"] = ElementType{6};
            character["vitrest"] = ElementType{0};

            RuleMonth month;
            ASSERT_TRUE(f.rules.month(9.0f, character, false, month)) << firstError(f.rules);
            EXPECT_EQ(intOf(month.after, "rations"), 11);
            EXPECT_EQ(intOf(month.after, "vit"), 6);

            const size_t rows = month.rows.size();

            character["town.market"] = ElementType{2};
            character["town.inn"] = ElementType{3};

            ASSERT_TRUE(f.rules.month(9.0f, character, false, month)) << firstError(f.rules);

            // One eaten, one sent
            EXPECT_EQ(intOf(month.after, "rations"), 12);
            EXPECT_EQ(intOf(month.after, "vit"), 7);

            // No row for it, and no gloss
            EXPECT_EQ(month.rows.size(), rows);
            EXPECT_EQ(byId(month.rows, "town.market"), nullptr);

            for (const auto& gloss : month.glosses)
                EXPECT_NE(textOf(gloss.fields, "id"), "town.market");
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // The Town page grows with the life: three places from the day the town is known, more with
        // his years, more with his class; and a place that was seen, raised or left coin stays on
        // it. A place that is not on his page has no work to show either.
        TEST(rules_test, places_come_with_age_and_class)
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

            ElementMap character = townsman();

            EXPECT_EQ(shown(7.0f, character), (Ids{"market", "mill", "smithy"}));
            EXPECT_EQ(shown(10.0f - 1.0f / 12.0f, character), (Ids{"market", "mill", "smithy"}));
            EXPECT_EQ(shown(10.0f, character), (Ids{"market", "mill", "smithy", "chapel", "yard"}));
            EXPECT_EQ(shown(13.0f, character), (Ids{"market", "mill", "smithy", "chapel", "yard", "inn"}));

            // His class brings its own
            ElementMap warrior = character;
            warrior["keep_oath"] = ElementType{1};

            EXPECT_EQ(shown(17.5f, warrior), (Ids{"market", "mill", "smithy", "chapel", "yard", "inn", "gate"}));

            ElementMap mage = character;
            mage["collegium"] = ElementType{1};

            EXPECT_EQ(shown(17.5f, mage), (Ids{"market", "mill", "smithy", "chapel", "yard", "inn", "collegium"}));
            EXPECT_EQ(shown(18.0f, mage), (Ids{"market", "mill", "smithy", "chapel", "yard", "inn", "collegium", "harrow"}));

            // Seen by a life before, raised, or left coin: on the page of a boy of 7
            character["seen.chapel"] = ElementType{1};
            character["town.gate"] = ElementType{1};
            character["fund.inn"] = ElementType{3};

            EXPECT_EQ(shown(7.0f, character), (Ids{"market", "mill", "smithy", "chapel", "inn", "gate"}));

            // The works follow the page
            EXPECT_TRUE(flag(activityAt(f.rules, 7.0f, character, "raise.chapel.1").fields, "listed"));
            EXPECT_TRUE(flag(activityAt(f.rules, 7.0f, character, "raise.gate.2").fields, "listed"));
            EXPECT_FALSE(flag(activityAt(f.rules, 7.0f, character, "raise.yard.1").fields, "listed"));
            EXPECT_FALSE(flag(activityAt(f.rules, 7.0f, character, "raise.collegium.1").fields, "listed"));
            EXPECT_TRUE(flag(activityAt(f.rules, 10.0f, character, "raise.yard.1").fields, "listed"));

            // Every place is told of all the same: the page shows the ones that are his
            RuleTown town;
            ASSERT_TRUE(f.rules.town(7.0f, townsman(), town)) << firstError(f.rules);
            EXPECT_EQ(town.places.size(), 9u);
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

            ASSERT_TRUE(f.rules.month(17.5f, stronger, false, month));
            ASSERT_TRUE(f.rules.month(17.5f, stronger, false, month));
            EXPECT_EQ(f.rules.nbRuns, 8u);

            ASSERT_TRUE(f.rules.month(17.5f, stronger, true, month));
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
            timed("month", [&](float age) { return f.rules.month(age, sworn(), false, month); });
            timed("milestones", [&](float age) { return f.rules.milestones(age, milestones, next); });

            EXPECT_EQ(f.rules.nbRuns, static_cast<size_t>(4 * runs));
        }
    }
}
