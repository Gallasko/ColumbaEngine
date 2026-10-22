#include "stdafx.h"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "Core/rulevm.h"
#include "Core/walk.h"

#include "ECS/entitysystem.h"

#include "mocklogger.h"

#include "compiledrules.h"

using namespace chronicle;

// The walks outside the town, as the rules play them: rules/expedition.pg asked through
// Rules::walk, with the walk as it stands handed in and taken back at every step, the way the
// Life screen does it (the scripts are copied beside the test binary, and compiled once a run:
// compiledrules.h).
namespace pg
{
    namespace test
    {
        namespace
        {
            int intOf(const ElementMap& map, const std::string& key)
            {
                auto it = map.find(key);

                if (it == map.end())
                    return 0;

                switch (it->second.type)
                {
                case UnionType::INT:    return it->second.get<int>();
                case UnionType::FLOAT:  return static_cast<int>(it->second.get<float>());
                case UnionType::DOUBLE: return static_cast<int>(it->second.get<double>());
                case UnionType::SIZE_T: return static_cast<int>(it->second.get<size_t>());
                case UnionType::BOOL:   return it->second.get<bool>() ? 1 : 0;
                default:                return 0;
                }
            }

            std::string textOf(const ElementMap& map, const std::string& key)
            {
                auto it = map.find(key);

                return it == map.end() ? std::string() : it->second.toString();
            }

            const ElementMap* byId(const RecordList& list, const std::string& id)
            {
                for (const auto& rec : list)
                {
                    if (textOf(rec, "id") == id)
                        return &rec;
                }

                return nullptr;
            }

            // The marks of a kind for a thing, added up
            int marked(const RecordList& marks, const std::string& kind, const std::string& id)
            {
                int sum = 0;

                for (const auto& mark : marks)
                {
                    if (textOf(mark, "kind") == kind and textOf(mark, "id") == id)
                        sum += intOf(mark, "n");
                }

                return sum;
            }

            // A boy who has helped at the hedges: the Hedgerows are open to him
            ElementMap gatherer()
            {
                return {{"str", ElementType{6}}, {"dex", ElementType{6}}, {"int", ElementType{6}}, {"vit", ElementType{40}}, {"vitmax", ElementType{40}},
                        {"coin", ElementType{0}}, {"rations", ElementType{12}}};
            }

            // A youth who can fight: the Hollow Woods are open to him
            ElementMap hunter()
            {
                return {{"str", ElementType{11}}, {"dex", ElementType{10}}, {"int", ElementType{9}}, {"vit", ElementType{70}}, {"vitmax", ElementType{70}},
                        {"arms", ElementType{2}}, {"coin", ElementType{5}}, {"rations", ElementType{10}}, {"remedy", ElementType{2}}, {"bow_learned", ElementType{1}}};
            }

            // Someone whose world has studied a place that often (3 is its knowledge 2, 6 its 3,
            // 10 its 4): what the place opens a level at a time is open to him
            ElementMap knowing(ElementMap who, const std::string& zone, int studies)
            {
                who["know.visit." + zone] = ElementType{1};
                who["know.study." + zone] = ElementType{studies};

                return who;
            }

            // One who walks: the rules, his character and his walk, each answer taken as the Life
            // screen takes it (the stats become his, the walk becomes the walk as it stands)
            struct Walker
            {
                EntitySystem ecs;
                Rules rules;
                ElementMap character;
                float age = 14.0f;
                WalkState state;
                std::vector<WalkRecipe> recipes;
                WalkQueue queue;
                RecordList walks;
                RuleWalk last;
                int seed = 100;

                explicit Walker(ElementMap who) : character(std::move(who))
                {
                    EXPECT_TRUE(rules.load(&ecs, compiledRules())) << (rules.errors.empty() ? "" : rules.errors.front());

                    rules.lives = 2;
                }

                bool ask(const std::string& what, const WalkAct& act = {}, const std::string& replay = "")
                {
                    const bool ok = rules.walk(age, character, "Aldren", what, ++seed, state, act, recipes, walks, queue, replay, last);

                    EXPECT_TRUE(ok) << (rules.errors.empty() ? "" : rules.errors.front());

                    if (not ok)
                        return false;

                    for (const auto& [key, value] : last.after)
                        character[key] = value;

                    if (what == "enter" or what == "act")
                        state = last.state;

                    return true;
                }

                bool act(const std::string& kind, const std::string& target = "", int count = 0)
                {
                    return ask("act", WalkAct{kind, target, count});
                }

                std::string screen() const { return textOf(last.site, "screen"); }

                // An event that comes up is answered with its first option that can be taken
                void answer()
                {
                    for (int guard = 0; guard < 4 and screen() == "event"; ++guard)
                    {
                        std::string option;

                        for (const auto& o : last.options)
                        {
                            if (option.empty() and intOf(o, "locked") == 0)
                                option = textOf(o, "id");
                        }

                        ASSERT_FALSE(option.empty());
                        act("option", option);
                    }
                }

                // A fight handed to his stance, to its end and back to the woods
                void fight()
                {
                    answer();

                    ASSERT_EQ(screen(), "fight");

                    for (int turn = 0; turn < 80 and textOf(last.duel, "over").empty(); ++turn)
                        act("step");

                    ASSERT_FALSE(textOf(last.duel, "over").empty());

                    act("back");
                    answer();
                }

                // The walk just ended, kept as the Life screen keeps it
                void keep(const std::string& id)
                {
                    WalkRecipe recipe;
                    recipe.id = id;
                    recipe.fields = last.recipe;
                    recipe.steps = last.state.record;
                    recipe.marks = last.state.marks;

                    recipes.push_back(recipe);
                    state = WalkState{};
                }
            };
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // He arrives: a week of days before him, the field work of the place, and no beast.
        TEST(expedition_test, a_walk_begins_with_its_days)
        {
            MockLogger logger;
            Walker w(gatherer());

            ASSERT_TRUE(w.ask("enter", WalkAct{"", "hedgerows", 0}));

            EXPECT_TRUE(w.state.under());
            EXPECT_EQ(w.screen(), "site");
            EXPECT_EQ(intOf(w.state.run, "ap"), 7);
            EXPECT_EQ(intOf(w.state.run, "months"), 1);

            // His Vitality counts five in the field
            EXPECT_EQ(intOf(w.state.run, "hp"), 40);
            EXPECT_EQ(intOf(w.state.run, "hpMax"), 40);

            // The place is visited from his first step: the world knows it at one of four
            EXPECT_EQ(intOf(w.character, "know.visit.hedgerows"), 1);
            EXPECT_EQ(textOf(w.last.site, "knowledge"), "ZONE KNOWLEDGE 1 OF 4");
            EXPECT_EQ(textOf(w.last.site, "daysLeft"), "7");

            // Three things to do on a first visit, and no more
            EXPECT_TRUE(w.last.seeks.empty());
            EXPECT_EQ(w.last.works.size(), 3u);
            ASSERT_NE(byId(w.last.works, "gather.berries"), nullptr);
            ASSERT_NE(byId(w.last.works, "gather.firewood"), nullptr);
            ASSERT_NE(byId(w.last.works, "study"), nullptr);

            // A cost is said before it is paid, with its unit
            EXPECT_EQ(textOf(*byId(w.last.works, "gather.berries"), "costText"), "1 AP");
            EXPECT_EQ(textOf(*byId(w.last.works, "study"), "costText"), "2 AP");

            // No walk under an unknown name
            Walker lost(gatherer());
            MockLogger quiet;

            EXPECT_FALSE(lost.rules.walk(14.0f, lost.character, "Aldren", "enter", 1, WalkState{}, WalkAct{"", "nowhere", 0}, {}, {}, WalkQueue{}, "", lost.last));
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // What he learned as a boy is his in the field, where he can camp: a snare is a day for a
        // ration, the herbal makes a remedy of two wortleaf where he stands, who says the office
        // rests a little better, and who knows the roads is a day sooner there. Who learned none
        // of it has none of it, and nobody camps at the Hedgerows.
        TEST(expedition_test, a_boys_trades_are_his_in_the_field)
        {
            MockLogger logger;

            // Never taught: no tile for it, and asking is refused
            {
                Walker w(knowing(gatherer(), "millpond", 3));

                ASSERT_TRUE(w.ask("enter", WalkAct{"", "millpond", 0}));

                ASSERT_NE(byId(w.last.works, "rest"), nullptr);
                EXPECT_EQ(byId(w.last.works, "snare"), nullptr);
                EXPECT_EQ(byId(w.last.works, "brew"), nullptr);

                const size_t steps = w.state.record.size();

                ASSERT_TRUE(w.act("snare"));
                EXPECT_EQ(w.state.record.size(), steps);
                EXPECT_FALSE(textOf(w.last.site, "refused").empty());

                ASSERT_TRUE(w.act("brew"));
                EXPECT_EQ(w.state.record.size(), steps);
                EXPECT_FALSE(textOf(w.last.site, "refused").empty());
            }

            // Taught, but at the Hedgerows: no camp, so no rest and no trade, whatever is known of them
            {
                ElementMap boy = knowing(gatherer(), "hedgerows", 10);
                boy["snare_set"] = ElementType{1};
                boy["herbal"] = ElementType{1};

                Walker w(boy);

                ASSERT_TRUE(w.ask("enter", WalkAct{"", "hedgerows", 0}));

                EXPECT_EQ(byId(w.last.works, "rest"), nullptr);
                EXPECT_EQ(byId(w.last.works, "snare"), nullptr);
                EXPECT_EQ(byId(w.last.works, "brew"), nullptr);

                const size_t steps = w.state.record.size();

                ASSERT_TRUE(w.act("rest"));
                EXPECT_EQ(w.state.record.size(), steps);
                EXPECT_FALSE(textOf(w.last.site, "refused").empty());

                ASSERT_TRUE(w.act("snare"));
                EXPECT_EQ(w.state.record.size(), steps);
            }

            // A snare: a day of the stay, a ration
            {
                ElementMap trapper = knowing(gatherer(), "millpond", 3);
                trapper["snare_set"] = ElementType{1};

                Walker w(trapper);

                ASSERT_TRUE(w.ask("enter", WalkAct{"", "millpond", 0}));
                ASSERT_NE(byId(w.last.works, "snare"), nullptr);
                EXPECT_EQ(textOf(*byId(w.last.works, "snare"), "costText"), "1 AP");

                const int rations = intOf(w.character, "rations");
                const int days = intOf(w.state.run, "ap");

                ASSERT_TRUE(w.act("snare"));

                EXPECT_EQ(intOf(w.character, "rations"), rations + 1);
                EXPECT_EQ(intOf(w.state.run, "ap"), days - 1);
                ASSERT_FALSE(w.state.record.empty());
                EXPECT_EQ(textOf(w.state.record.back(), "kind"), "snare");
            }

            // The herbal: shut without the leaf, then two wortleaf for a remedy, and no day spent
            {
                ElementMap herbalist = knowing(gatherer(), "millpond", 3);
                herbalist["herbal"] = ElementType{1};

                Walker w(herbalist);

                ASSERT_TRUE(w.ask("enter", WalkAct{"", "millpond", 0}));
                ASSERT_NE(byId(w.last.works, "brew"), nullptr);
                EXPECT_EQ(intOf(*byId(w.last.works, "brew"), "locked"), 1);

                const size_t steps = w.state.record.size();

                ASSERT_TRUE(w.act("brew"));
                EXPECT_EQ(w.state.record.size(), steps);
                EXPECT_FALSE(textOf(w.last.site, "refused").empty());

                w.character["wortleaf"] = ElementType{3};

                const int days = intOf(w.state.run, "ap");

                ASSERT_TRUE(w.act("brew"));

                EXPECT_EQ(intOf(w.character, "wortleaf"), 1);
                EXPECT_EQ(intOf(w.character, "remedy"), 1);
                EXPECT_EQ(intOf(w.state.run, "ap"), days);
                EXPECT_EQ(w.state.record.size(), steps + 1);
            }

            // A rest eats a ration; the office said over his fire gives two more
            {
                auto rested = [](ElementMap who) {
                    who["vit"] = ElementType{10};

                    Walker w(knowing(std::move(who), "millpond", 3));

                    EXPECT_TRUE(w.ask("enter", WalkAct{"", "millpond", 0}));

                    const int before = intOf(w.state.run, "hp");
                    const int rations = intOf(w.character, "rations");

                    EXPECT_TRUE(w.act("rest"));
                    EXPECT_EQ(intOf(w.character, "rations"), rations - 1);

                    return intOf(w.state.run, "hp") - before;
                };

                ElementMap sacristan = gatherer();
                sacristan["anselm_trust"] = ElementType{1};

                EXPECT_EQ(rested(sacristan), rested(gatherer()) + 2);
            }

            // The roads known: a day off the way there, and so off the way home
            {
                Walker plain(hunter());
                ASSERT_TRUE(plain.ask("enter", WalkAct{"", "hollow", 0}));

                ElementMap carter = hunter();
                carter["roads_known"] = ElementType{1};

                Walker w(carter);
                ASSERT_TRUE(w.ask("enter", WalkAct{"", "hollow", 0}));

                EXPECT_EQ(intOf(w.state.run, "travel"), intOf(plain.state.run, "travel") - 1);
            }
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // Every day spent is a line of the record, with what it cost and what it gave.
        TEST(expedition_test, a_day_is_written_to_the_record)
        {
            MockLogger logger;
            Walker w(gatherer());

            ASSERT_TRUE(w.ask("enter", WalkAct{"", "hedgerows", 0}));
            ASSERT_TRUE(w.act("gather", "berries"));
            w.answer();

            const int berries = marked(w.state.marks, "gain", "berries");

            EXPECT_GE(berries, 1);
            EXPECT_EQ(intOf(w.character, "berries"), berries);
            EXPECT_LE(intOf(w.state.run, "ap"), 6);

            ASSERT_FALSE(w.state.record.empty());
            EXPECT_EQ(textOf(w.state.record[0], "kind"), "gather");
            EXPECT_EQ(intOf(w.state.record[0], "ap"), 1);
            EXPECT_NE(textOf(w.state.record[0], "figure").find("1 AP"), std::string::npos);
            EXPECT_NE(textOf(w.state.record[0], "figure").find("BERRIES"), std::string::npos);

            // What is for the town is his to hold like the rest, until a work takes it
            ASSERT_TRUE(w.act("gather", "firewood"));
            w.answer();

            EXPECT_GE(intOf(w.character, "firewood"), 1);
            EXPECT_EQ(w.character.count("store.firewood"), 0u);

            // A study teaches the world
            ASSERT_TRUE(w.act("study"));
            w.answer();

            EXPECT_GE(intOf(w.character, "know.study.hedgerows"), 1);

            // What cannot be paid is refused, and nothing is written
            const size_t steps = w.state.record.size();

            while (intOf(w.state.run, "ap") >= 2 and w.screen() == "site")
            {
                ASSERT_TRUE(w.act("study"));
                w.answer();
            }

            EXPECT_GT(w.state.record.size(), steps);

            if (w.screen() == "site" and intOf(w.state.run, "ap") == 1)
            {
                const size_t before = w.state.record.size();

                ASSERT_TRUE(w.act("study"));
                EXPECT_EQ(w.state.record.size(), before);
                EXPECT_FALSE(textOf(w.last.site, "refused").empty());
            }
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // A deft boy picks more berries at the hedge, a strong one carries more wood: each step of
        // a spot's bonus he has reached adds to every gathering, and its work says so. Each
        // gathering of them counts a scavenge
        TEST(expedition_test, a_deft_or_strong_boy_gathers_more_at_the_hedge)
        {
            MockLogger logger;

            ElementMap able = gatherer();
            able["dex"] = ElementType{12};
            able["str"] = ElementType{8};

            Walker plain(gatherer());
            Walker w(able);

            ASSERT_TRUE(plain.ask("enter", WalkAct{"", "hedgerows", 0}));
            ASSERT_TRUE(w.ask("enter", WalkAct{"", "hedgerows", 0}));

            const ElementMap* berries = byId(w.last.works, "gather.berries");
            const ElementMap* firewood = byId(w.last.works, "gather.firewood");
            ASSERT_NE(berries, nullptr);
            ASSERT_NE(firewood, nullptr);

            EXPECT_EQ(textOf(*berries, "gives"), "3\xE2\x80\x93" "5 berries, to sell");
            EXPECT_EQ(textOf(*berries, "bonus"), "With Dexterity 8: +1 berry\nWith Dexterity 12: +2 berries");
            EXPECT_EQ(intOf(*berries, "bonusReached"), 2);
            EXPECT_EQ(textOf(*firewood, "gives"), "2\xE2\x80\x93" "5 firewood, for the town");
            EXPECT_EQ(intOf(*firewood, "bonusReached"), 1);

            EXPECT_EQ(intOf(*byId(plain.last.works, "gather.berries"), "bonusReached"), 0);
            EXPECT_EQ(textOf(*byId(plain.last.works, "gather.berries"), "gives"), "1\xE2\x80\x93" "3 berries, to sell");

            // What he picks is the roll and the steps (marked before any event of the gathering)
            ASSERT_TRUE(w.act("gather", "berries"));
            const int picked = marked(w.state.marks, "gain", "berries");
            EXPECT_GE(picked, 3);
            EXPECT_LE(picked, 5);
            w.answer();

            ASSERT_TRUE(w.act("gather", "firewood"));
            const int carried = marked(w.state.marks, "gain", "firewood");
            EXPECT_GE(carried, 2);
            EXPECT_LE(carried, 5);

            // Each gathering of them is a scavenge, as a month of the Scavenge rows is: two open the
            // hedge sprite. A study is none
            EXPECT_EQ(intOf(w.character, "scavenged"), 2);
            EXPECT_EQ(marked(w.state.marks, "stat", "scavenged"), 2);
            w.answer();

            ASSERT_TRUE(w.act("study"));
            w.answer();
            EXPECT_EQ(intOf(w.character, "scavenged"), 2);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // Leaving is asked twice; the walk is then over, and what it was is what a record keeps.
        TEST(expedition_test, turning_home_is_asked_twice)
        {
            MockLogger logger;
            Walker w(gatherer());

            ASSERT_TRUE(w.ask("enter", WalkAct{"", "hedgerows", 0}));
            ASSERT_TRUE(w.act("gather", "berries"));
            w.answer();

            ASSERT_TRUE(w.act("leave"));

            // The first press says what is lost, and nothing is done
            EXPECT_EQ(w.screen(), "site");
            EXPECT_EQ(intOf(w.state.run, "leaving"), 1);
            EXPECT_NE(textOf(w.last.site, "leave").find("lost"), std::string::npos);
            EXPECT_TRUE(textOf(w.last.recipe, "zone").empty());

            // Something else done lets go of it
            ASSERT_TRUE(w.act("gather", "berries"));
            w.answer();
            EXPECT_EQ(intOf(w.state.run, "leaving"), 0);

            ASSERT_TRUE(w.act("leave"));
            ASSERT_TRUE(w.act("leave"));

            EXPECT_EQ(w.screen(), "done");
            EXPECT_EQ(textOf(w.last.recipe, "zone"), "hedgerows");
            EXPECT_EQ(intOf(w.last.recipe, "steps"), static_cast<int>(w.state.record.size()));
            EXPECT_EQ(intOf(w.last.recipe, "months"), 1);
            EXPECT_EQ(intOf(w.character, "walked.hedgerows"), 1);
            EXPECT_EQ(textOf(w.state.record.back(), "kind"), "leave");

            // And the walk does nothing more
            const size_t steps = w.state.record.size();

            ASSERT_TRUE(w.act("gather", "berries"));
            EXPECT_EQ(w.state.record.size(), steps);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // The stay ends with its last day, whether or not he turned for home.
        TEST(expedition_test, the_last_day_ends_the_stay)
        {
            MockLogger logger;
            Walker w(gatherer());

            ASSERT_TRUE(w.ask("enter", WalkAct{"", "hedgerows", 0}));

            for (int day = 0; day < 20 and w.screen() != "done"; ++day)
            {
                ASSERT_TRUE(w.act("gather", "berries"));
                w.answer();
            }

            EXPECT_EQ(w.screen(), "done");
            EXPECT_EQ(intOf(w.state.run, "ap"), 0);
            EXPECT_EQ(intOf(w.last.recipe, "ap"), 7);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // A beast sought is fought: the fight costs days by its turns, and its line says what it took.
        TEST(expedition_test, a_fight_costs_days_by_its_turns)
        {
            MockLogger logger;
            Walker w(hunter());

            ASSERT_TRUE(w.ask("enter", WalkAct{"", "hollow", 0}));

            // On a first visit one wolf alone is all the wood shows him: the pack, the boar and
            // the rare beast come with the place known better
            const ElementMap* lone = byId(w.last.seeks, "wolf");

            ASSERT_NE(lone, nullptr);
            EXPECT_EQ(w.last.seeks.size(), 1u);
            EXPECT_EQ(byId(w.last.seeks, "wolf*pack"), nullptr);
            EXPECT_EQ(byId(w.last.seeks, "boar"), nullptr);
            EXPECT_EQ(byId(w.last.seeks, "stag"), nullptr);

            // Known 4 of 4: the pack is cheaper to find than one wolf alone, and the rare beast is listed
            {
                Walker known(knowing(hunter(), "hollow", 10));

                ASSERT_TRUE(known.ask("enter", WalkAct{"", "hollow", 0}));

                const ElementMap* pack = byId(known.last.seeks, "wolf*pack");
                const ElementMap* alone = byId(known.last.seeks, "wolf");
                const ElementMap* stag = byId(known.last.seeks, "stag");

                ASSERT_NE(pack, nullptr);
                ASSERT_NE(alone, nullptr);
                ASSERT_NE(stag, nullptr);

                EXPECT_LT(intOf(*pack, "cost"), intOf(*alone, "cost"));
                EXPECT_EQ(intOf(*pack, "count"), 3);
                EXPECT_EQ(intOf(*stag, "locked"), 0);
            }

            const int before = intOf(w.state.run, "ap");
            const int seekCost = intOf(*lone, "cost");

            ASSERT_TRUE(w.act("seek", "wolf", 1));
            w.answer();

            ASSERT_EQ(w.screen(), "fight");
            EXPECT_EQ(w.state.foes.size(), 1u);
            EXPECT_EQ(textOf(w.last.duel, "turn"), "TURN 1");

            // What he may do is what anyone can: strike, defend, take a remedy while he holds
            // one, flee; then what he has learned, the bow shot of the yard, and nothing else
            ASSERT_EQ(w.last.menu.size(), 5u);
            EXPECT_EQ(textOf(w.last.menu[4], "id"), "tech.bow");
            EXPECT_EQ(textOf(w.last.menu[0], "id"), "strike");
            EXPECT_EQ(textOf(w.last.menu[1], "name"), "Defend");
            EXPECT_EQ(textOf(w.last.menu[2], "id"), "remedy");
            EXPECT_EQ(textOf(w.last.menu[3], "name"), "Flee");
            EXPECT_EQ(textOf(w.last.preview, "id"), "strike");
            EXPECT_FALSE(textOf(w.last.preview, "deals").empty());

            // What he fights is said of each beast: how it stands, and what it means to do to him
            ASSERT_EQ(w.last.targets.size(), 1u);
            EXPECT_EQ(textOf(w.last.targets[0], "life"), "UNHURT");
            EXPECT_EQ(intOf(w.last.targets[0], "percent"), 100);
            EXPECT_FALSE(textOf(w.last.targets[0], "intent").empty());
            EXPECT_FALSE(textOf(w.last.duel, "about").empty());

            // Looking at another choice changes nothing but what is said
            ASSERT_TRUE(w.act("pick", "guard"));
            EXPECT_EQ(textOf(w.last.preview, "id"), "guard");
            EXPECT_EQ(textOf(w.last.preview, "title"), "Defend");
            EXPECT_EQ(textOf(w.last.duel, "turn"), "TURN 1");

            // What he has not learned cannot be done, whoever asks
            ASSERT_TRUE(w.act("pick", "tech.heavy"));
            ASSERT_TRUE(w.act("do"));
            EXPECT_EQ(textOf(w.last.duel, "turn"), "TURN 1");
            EXPECT_EQ(textOf(w.last.duel, "refused"), "LEARNED AT THE KEEP");

            ASSERT_TRUE(w.act("pick", "strike"));

            w.fight();

            // The fight's line: after the seek's, with the days it cost
            ASSERT_GE(w.state.record.size(), 2u);

            const ElementMap* line = nullptr;

            for (const auto& step : w.state.record)
            {
                if (textOf(step, "kind") == "fight")
                    line = &step;
            }

            ASSERT_NE(line, nullptr);
            EXPECT_GE(intOf(*line, "ap"), 1);
            EXPECT_EQ(textOf(*line, "target"), "wolf");
            EXPECT_EQ(intOf(w.character, "know.fights.wolf"), 1);
            EXPECT_EQ(intOf(w.character, "fought.wolf"), 1);

            EXPECT_LE(intOf(w.state.run, "ap"), before - seekCost - intOf(*line, "ap"));

            // His Vitality in the field is his own: the same points at home, and never nothing
            EXPECT_GE(intOf(w.character, "vit"), 1);
            EXPECT_EQ(intOf(w.character, "vit"), intOf(w.state.run, "hp"));
            EXPECT_EQ(intOf(w.state.run, "hpMax"), intOf(w.character, "vitmax"));
            EXPECT_EQ(w.character.count("scratches"), 0u);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // What a life has learned is what its fights offer: the Keep's blows to its sworn men, the
        // Collegium's workings to its scholars, a trick of the streets to who knows them.
        TEST(expedition_test, a_fight_offers_what_he_has_learned)
        {
            MockLogger logger;

            auto learned = [](ElementMap who) {
                Walker w(std::move(who));

                EXPECT_TRUE(w.ask("enter", WalkAct{"", "hollow", 0}));
                w.act("seek", "wolf", 1);
                w.answer();

                std::vector<std::string> ids;

                for (const auto& m : w.last.menu)
                {
                    if (not textOf(m, "group").empty())
                        ids.push_back(textOf(m, "id"));
                }

                return ids;
            };

            // (the hunter learned the bow at the yard: its shot is with him in every fight)
            EXPECT_EQ(learned(hunter()), (std::vector<std::string>{"tech.bow"}));

            ElementMap knight = hunter();
            knight["keep_oath"] = ElementType{1};
            EXPECT_EQ(learned(knight), (std::vector<std::string>{"tech.bow", "tech.heavy"}));

            knight["veteran"] = ElementType{1};
            EXPECT_EQ(learned(knight), (std::vector<std::string>{"tech.bow", "tech.heavy", "tech.sweep"}));

            // The Collegium's workings come from its courses, not from the class: its fire course
            // teaches Firebolt (a drop-out keeps it), and Orin his binding word
            ElementMap mage = hunter();
            mage["collegium"] = ElementType{1};
            mage["orin_known"] = ElementType{1};
            EXPECT_EQ(learned(mage), (std::vector<std::string>{"tech.bow", "tech.binding"}));

            mage["fire_course"] = ElementType{1};
            EXPECT_EQ(learned(mage), (std::vector<std::string>{"tech.bow", "tech.firebolt", "tech.binding"}));

            ElementMap frost = hunter();
            frost["frost_course"] = ElementType{1};
            EXPECT_EQ(learned(frost), (std::vector<std::string>{"tech.bow", "tech.frostbolt"}));

            ElementMap storm = hunter();
            storm["storm_course"] = ElementType{1};
            EXPECT_EQ(learned(storm), (std::vector<std::string>{"tech.bow", "tech.stormbolt"}));

            // The basics' three cantrips
            ElementMap basics = hunter();
            basics["spark_taught"] = ElementType{1};
            basics["rime_taught"] = ElementType{1};
            basics["static_taught"] = ElementType{1};
            EXPECT_EQ(learned(basics), (std::vector<std::string>{"tech.bow", "tech.spark", "tech.rime", "tech.static"}));

            ElementMap urchin = hunter();
            urchin["street_known"] = ElementType{1};
            EXPECT_EQ(learned(urchin), (std::vector<std::string>{"tech.bow", "tech.feint"}));

            // What the wooden blade taught a boy, the edge or the point, comes first: it was first
            ElementMap edge = hunter();
            edge["slash_learned"] = ElementType{1};
            EXPECT_EQ(learned(edge), (std::vector<std::string>{"tech.slash", "tech.bow"}));

            // It is a better strike and no more: it costs no breath, and he is told of none
            {
                Walker w(edge);

                EXPECT_TRUE(w.ask("enter", WalkAct{"", "hollow", 0}));
                w.act("seek", "wolf", 1);
                w.answer();

                for (const auto& m : w.last.menu)
                {
                    if (textOf(m, "id") == "tech.slash")
                        EXPECT_EQ(textOf(m, "cost"), "");
                }

                EXPECT_EQ(textOf(w.last.duel, "breath"), "");

                // Used a turn, it is his again the next: nothing to wait for
                ASSERT_TRUE(w.act("pick", "tech.slash"));
                ASSERT_TRUE(w.act("do"));
                EXPECT_EQ(textOf(w.last.duel, "refused"), "");

                ASSERT_TRUE(w.act("pick", "tech.slash"));
                ASSERT_TRUE(w.act("do"));
                EXPECT_EQ(textOf(w.last.duel, "refused"), "");
            }

            ElementMap point = hunter();
            point["thrust_learned"] = ElementType{1};
            point["keep_oath"] = ElementType{1};
            EXPECT_EQ(learned(point), (std::vector<std::string>{"tech.thrust", "tech.bow", "tech.heavy"}));

            // The levy teaches its boys the heavy cut before any oath, and the smuggler his one trick
            ElementMap levied = hunter();
            levied["levy_served"] = ElementType{1};
            EXPECT_EQ(learned(levied), (std::vector<std::string>{"tech.bow", "tech.heavy"}));

            ElementMap runner = hunter();
            runner["smuggler_runner"] = ElementType{1};
            EXPECT_EQ(learned(runner), (std::vector<std::string>{"tech.bow", "tech.sand"}));

            // A working is counted from his Intelligence: a clever man's firebolt is worth his study
            mage["int"] = ElementType{16};

            Walker w(mage);

            ASSERT_TRUE(w.ask("enter", WalkAct{"", "hollow", 0}));
            ASSERT_TRUE(w.act("seek", "wolf", 1));
            w.answer();
            ASSERT_EQ(w.screen(), "fight");

            ASSERT_TRUE(w.act("pick", "tech.firebolt"));
            EXPECT_EQ(textOf(w.last.preview, "title"), "Firebolt");
            EXPECT_EQ(textOf(w.last.preview, "deals"), "13\xE2\x80\x93" "16");   // 5-8, and half of 16: a working is not stopped by the armour of a wolf

            // The binding word deals nothing: the beast loses the move it meant to make
            ASSERT_TRUE(w.act("pick", "tech.binding"));
            EXPECT_EQ(textOf(w.last.preview, "deals"), "Nothing");

            const int hp = intOf(w.state.run, "hp");
            const int breath = intOf(w.state.fight, "breath");

            ASSERT_TRUE(w.act("do"));

            EXPECT_EQ(intOf(w.state.run, "hp"), hp);
            EXPECT_EQ(intOf(w.state.fight, "breath"), breath - 2);
            EXPECT_EQ(textOf(w.last.duel, "turn"), "TURN 2");
            EXPECT_EQ(intOf(w.state.foes[0], "hp"), intOf(w.state.foes[0], "hpMax"));
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // What a beast means to do is said by the move's name alone. What the move does to him is
        // said once it has been used on him, or once he has watched the beast: not before.
        TEST(expedition_test, a_move_is_known_once_it_was_used_on_him)
        {
            MockLogger logger;

            ElementMap boy = knowing(gatherer(), "hedgerows", 10);

            Walker w(boy);
            w.age = 10.0f;

            ASSERT_TRUE(w.ask("enter", WalkAct{"", "millpond", 0}));
            ASSERT_TRUE(w.act("seek", "rat", 1));
            w.answer();
            ASSERT_EQ(w.screen(), "fight");

            // No rat is quicker than a boy: over the coming six actions he acts at least as often
            // as it does (the line used to say who acts first in a round; there are no rounds now)
            EXPECT_EQ(textOf(w.last.duel, "quick").find("HE ACTS ONCE TO"), std::string::npos);
            EXPECT_NE(textOf(w.last.duel, "quick").find("YOU ACT"), std::string::npos);

            // He defends until it has bitten at him once: the bite is a name and nothing else till then
            bool sawUnknown = false;

            for (int turn = 0; turn < 12 and intOf(w.character, "know.move.rat.bite") == 0 and textOf(w.last.duel, "over").empty(); ++turn)
            {
                ASSERT_EQ(w.last.targets.size(), 1u);

                if (textOf(w.last.targets[0], "intent") == "BITE")
                {
                    sawUnknown = true;
                    EXPECT_EQ(textOf(w.last.targets[0], "moveName"), "Bite");
                    EXPECT_FALSE(textOf(w.last.targets[0], "moveLine").empty());
                    EXPECT_EQ(intOf(w.last.targets[0], "moveKnown"), 0);
                    EXPECT_TRUE(textOf(w.last.targets[0], "moveEffect").empty());

                    // And what is coming is counted, not weighed
                    EXPECT_EQ(textOf(w.last.duel, "warn"), "1 BLOW COMING");
                }
                else
                {
                    // What draws no blood is plain to see
                    EXPECT_EQ(intOf(w.last.targets[0], "moveKnown"), 1);
                }

                ASSERT_TRUE(w.act("move", "guard"));
            }

            EXPECT_TRUE(sawUnknown);
            ASSERT_EQ(intOf(w.character, "know.move.rat.bite"), 1);

            // From here on it says what it takes, in this fight and in the next: the world knows it
            for (int turn = 0; turn < 6 and textOf(w.last.duel, "over").empty(); ++turn)
            {
                if (textOf(w.last.targets[0], "intent") == "BITE")
                {
                    EXPECT_EQ(intOf(w.last.targets[0], "moveKnown"), 1);
                    EXPECT_NE(textOf(w.last.targets[0], "moveEffect").find("vitality if it lands"), std::string::npos);
                    EXPECT_NE(textOf(w.last.duel, "warn").find("IF ALL LAND"), std::string::npos);
                }

                ASSERT_TRUE(w.act("move", "guard"));
            }

            // A button pressed is the move done: every row of the menu carries what its gloss says
            for (const auto& m : w.last.menu)
                EXPECT_FALSE(textOf(m, "text").empty()) << textOf(m, "id");
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // A field work says what its gloss needs: a line on it, what it costs in words, what it gives
        TEST(expedition_test, a_field_work_says_what_it_costs_and_gives)
        {
            MockLogger logger;

            Walker w(gatherer());

            ASSERT_TRUE(w.ask("enter", WalkAct{"", "hedgerows", 0}));
            ASSERT_FALSE(w.last.works.empty());

            auto work = [&](const std::string& id) -> ElementMap {
                for (const auto& row : w.last.works)
                {
                    if (textOf(row, "id") == id)
                        return row;
                }

                ADD_FAILURE() << "no work " << id;

                return {};
            };

            for (const auto& row : w.last.works)
            {
                EXPECT_FALSE(textOf(row, "about").empty()) << textOf(row, "id");
                EXPECT_FALSE(textOf(row, "costs").empty()) << textOf(row, "id");
                EXPECT_FALSE(textOf(row, "gives").empty()) << textOf(row, "id");
            }

            EXPECT_EQ(textOf(work("gather.berries"), "costs"), "1 day of the stay");
            EXPECT_EQ(textOf(work("gather.berries"), "gives"), "1\xE2\x80\x93" "3 berries, to sell");
            EXPECT_EQ(textOf(work("gather.firewood"), "costs"), "2 days of the stay");
            EXPECT_EQ(textOf(work("gather.firewood"), "gives"), "1\xE2\x80\x93" "4 firewood, for the town");

            // A study says what it gives, alone; what the next level opens is the head's note's to say
            EXPECT_EQ(textOf(work("study"), "gives"), "+1 zone knowledge");
            EXPECT_NE(textOf(w.last.site, "knowledgeNote").find("exploring the nearby pond"), std::string::npos);

            // The site's head carries the place's ladder, for the gloss of his panel: what each
            // level brings, the studies it asks in all, and none made yet toward the second
            EXPECT_EQ(textOf(w.last.site, "brings2"), "Exploring the nearby pond, a task of your life");
            EXPECT_EQ(textOf(w.last.site, "brings3"), "Gathering wortleaf, a longer stay");
            EXPECT_EQ(intOf(w.last.site, "needs2"), 3);
            EXPECT_EQ(intOf(w.last.site, "needs4"), 10);
            EXPECT_EQ(intOf(w.last.site, "studyPercent"), 0);

            // And so does a rest, where he can camp
            Walker camper(knowing(gatherer(), "millpond", 3));

            ASSERT_TRUE(camper.ask("enter", WalkAct{"", "millpond", 0}));

            const ElementMap* rest = byId(camper.last.works, "rest");

            ASSERT_NE(rest, nullptr);
            EXPECT_NE(textOf(*rest, "costs").find("1 ration of"), std::string::npos);
            EXPECT_NE(textOf(*rest, "gives").find("Vitality"), std::string::npos);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // A place opens a little at a time: three things to do on a first visit, one more at each
        // level of its knowledge, and at the fourth the next place. What is not open is refused.
        TEST(expedition_test, a_place_opens_a_level_at_a_time)
        {
            MockLogger logger;

            auto works = [](int studies) {
                Walker w(knowing(gatherer(), "hedgerows", studies));
                std::vector<std::string> ids;

                EXPECT_TRUE(w.ask("enter", WalkAct{"", "hedgerows", 0}));

                for (const auto& row : w.last.works)
                    ids.push_back(textOf(row, "id"));

                return ids;
            };

            using Ids = std::vector<std::string>;

            EXPECT_EQ(works(0), (Ids{"gather.berries", "gather.firewood", "study"}));
            EXPECT_EQ(works(3), (Ids{"gather.berries", "gather.firewood", "study"}));
            EXPECT_EQ(works(6), (Ids{"gather.berries", "gather.firewood", "gather.wortleaf", "study"}));
            EXPECT_EQ(works(10), (Ids{"gather.berries", "gather.firewood", "gather.wortleaf", "gather.hazel", "study"}));

            // Known 3 of 4, the note under how well it is known says the fourth opens the next place
            // (the study itself gives its point of knowledge, and nothing more)
            {
                Walker w(knowing(gatherer(), "hedgerows", 6));

                ASSERT_TRUE(w.ask("enter", WalkAct{"", "hedgerows", 0}));
                ASSERT_NE(byId(w.last.works, "study"), nullptr);
                EXPECT_EQ(textOf(*byId(w.last.works, "study"), "gives"), "+1 zone knowledge");
                EXPECT_NE(textOf(w.last.site, "knowledgeNote").find("the way to the Hollow Woods"), std::string::npos);
            }

            // What is not open is refused, and nothing is written
            {
                Walker w(gatherer());

                ASSERT_TRUE(w.ask("enter", WalkAct{"", "hedgerows", 0}));
                ASSERT_TRUE(w.act("gather", "wortleaf"));

                EXPECT_TRUE(w.state.record.empty());
                EXPECT_FALSE(textOf(w.last.site, "refused").empty());
                EXPECT_EQ(intOf(w.state.run, "ap"), 7);
            }

            // A pack is not sought before the wood is known 4 of 4
            {
                Walker w(hunter());

                ASSERT_TRUE(w.ask("enter", WalkAct{"", "hollow", 0}));

                const int days = intOf(w.state.run, "ap");

                ASSERT_TRUE(w.act("seek", "wolf", 3));

                EXPECT_EQ(w.screen(), "site");
                EXPECT_FALSE(textOf(w.last.site, "refused").empty());
                EXPECT_EQ(intOf(w.state.run, "ap"), days);
            }
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // A blow that found nothing is marked in the account, his or the beast's, and no other line is
        TEST(expedition_test, a_missed_blow_is_marked_in_the_account)
        {
            MockLogger logger;

            int his = 0;
            int theirs = 0;

            for (int seed = 100; seed < 1300 and (his == 0 or theirs == 0); seed += 100)
            {
                ElementMap boy = knowing(gatherer(), "hedgerows", 10);

                Walker w(boy);
                w.age = 10.0f;
                w.seed = seed;

                ASSERT_TRUE(w.ask("enter", WalkAct{"", "millpond", 0}));
                ASSERT_TRUE(w.act("seek", "rat", 1));
                w.answer();

                if (w.screen() != "fight")
                    continue;

                for (int turn = 0; turn < 12 and textOf(w.last.duel, "over").empty(); ++turn)
                    ASSERT_TRUE(w.act("move", "strike"));

                for (const auto& line : w.state.account)
                {
                    const std::string text = textOf(line, "text");
                    const bool miss = text.find(": a miss") != std::string::npos or text.find("step clear") != std::string::npos;

                    EXPECT_EQ(intOf(line, "missed") > 0, miss) << text;

                    if (miss and textOf(line, "by") == "hero")
                        ++his;

                    if (miss and textOf(line, "by") == "foe")
                        ++theirs;
                }
            }

            EXPECT_GT(his, 0);
            EXPECT_GT(theirs, 0);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // A retreat ends the fight at once: the days stay written, and nothing is carried off.
        TEST(expedition_test, a_retreat_keeps_the_days_and_leaves_the_loot)
        {
            MockLogger logger;
            Walker w(knowing(hunter(), "hollow", 10));   // The wood known 4 of 4: its packs are found

            ASSERT_TRUE(w.ask("enter", WalkAct{"", "hollow", 0}));
            ASSERT_TRUE(w.act("seek", "wolf", 3));
            w.answer();

            ASSERT_EQ(w.screen(), "fight");
            EXPECT_EQ(w.state.foes.size(), 3u);

            // Of a pack of three, one hangs back while two stand
            int holding = 0;

            for (const auto& t : w.last.targets)
            {
                if (textOf(t, "intent") == "HANG BACK")
                    ++holding;
            }

            EXPECT_EQ(holding, 1);

            ASSERT_TRUE(w.act("step"));
            ASSERT_TRUE(w.act("pick", "retreat"));
            ASSERT_TRUE(w.act("do"));

            EXPECT_EQ(textOf(w.last.duel, "over"), "fled");

            ASSERT_TRUE(w.act("back"));
            w.answer();

            const ElementMap* line = nullptr;

            for (const auto& step : w.state.record)
            {
                if (textOf(step, "kind") == "fight")
                    line = &step;
            }

            ASSERT_NE(line, nullptr);
            EXPECT_EQ(textOf(*line, "tone"), "loss");
            EXPECT_GE(intOf(*line, "ap"), 1);
            EXPECT_EQ(marked(w.state.marks, "kill", "wolf"), 0);
            EXPECT_EQ(marked(w.state.marks, "loot", "pelts"), 0);
            EXPECT_EQ(intOf(w.character, "pelts"), 0);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // The same dice make the same walk: a walk is asked with its own.
        TEST(expedition_test, the_same_dice_make_the_same_walk)
        {
            MockLogger logger;

            auto walked = [](int seed) {
                Walker w(knowing(hunter(), "hollow", 10));
                w.seed = seed;

                EXPECT_TRUE(w.ask("enter", WalkAct{"", "hollow", 0}));
                w.act("seek", "wolf", 3);
                w.fight();
                w.act("gather", "ashwood");
                w.answer();

                std::string told;

                for (const auto& step : w.state.record)
                    told += textOf(step, "text") + "|" + textOf(step, "figure") + ";";

                return told;
            };

            EXPECT_EQ(walked(500), walked(500));
            EXPECT_FALSE(walked(500).empty());
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // A record walked again is a fold over its steps: what it took is taken, what it gathered
        // is gathered, what its beasts drop is rolled anew, and no fight is fought.
        TEST(expedition_test, a_record_is_walked_again_by_its_steps)
        {
            MockLogger logger;
            Walker w(knowing(hunter(), "hollow", 3));   // The wood known 2 of 4: he camps there

            ASSERT_TRUE(w.ask("enter", WalkAct{"", "hollow", 0}));
            ASSERT_TRUE(w.act("seek", "wolf", 1));
            w.fight();
            ASSERT_TRUE(w.act("gather", "ashwood"));
            w.answer();
            ASSERT_TRUE(w.act("rest"));
            w.answer();
            ASSERT_TRUE(w.act("leave"));
            ASSERT_TRUE(w.act("leave"));
            ASSERT_EQ(w.screen(), "done");

            const int gathered = marked(w.state.marks, "gain", "ashwood");
            const int eaten = marked(w.state.marks, "use", "rations");
            const int total = intOf(w.last.recipe, "hp");

            ASSERT_GE(gathered, 1);
            ASSERT_GE(eaten, 1);

            w.keep("r1");

            // The records page: his record, with what a queue of it would take and bring
            w.walks = {{{"zone", ElementType{std::string("hollow")}}, {"listed", ElementType{1}}, {"locked", ElementType{0}}, {"months", ElementType{3}}, {"until", ElementType{std::string()}}}};
            w.queue.recipe = "r1";
            w.queue.count = 3;

            ASSERT_TRUE(w.ask("records"));

            ASSERT_EQ(w.last.shelf.size(), 1u);
            EXPECT_EQ(textOf(w.last.shelf[0], "id"), "r1");
            EXPECT_EQ(textOf(w.last.shelf[0], "state"), "kept");
            EXPECT_EQ(textOf(w.last.form, "recipe"), "r1");
            EXPECT_EQ(intOf(w.last.form, "months"), 9);
            EXPECT_EQ(textOf(w.last.form, "total"), "3 TIMES \xC2\xB7 9 MO");
            EXPECT_TRUE(textOf(w.last.form, "blocked").empty());
            EXPECT_FALSE(w.last.stops.empty());

            // He sets out: what the record eats is taken as he leaves
            const int rations = intOf(w.character, "rations");
            const int ash = intOf(w.character, "ashwood");
            const int vit = intOf(w.character, "vit");

            ASSERT_TRUE(w.ask("begin", {}, "r1"));
            EXPECT_TRUE(w.last.blocked.empty());
            EXPECT_EQ(intOf(w.character, "rations"), rations - eaten);

            // He is home: the fold
            ASSERT_TRUE(w.ask("term", {}, "r1"));

            EXPECT_EQ(intOf(w.character, "ashwood"), ash + gathered);
            EXPECT_EQ(intOf(w.character, "walked.hollow"), 2);
            EXPECT_EQ(intOf(w.character, "know.fights.wolf"), 2);
            EXPECT_GE(intOf(w.character, "pelts"), 1);   // A wolf always leaves its pelt
            ASSERT_FALSE(w.last.entries.empty());

            // What the walk cost him it costs again, and never his life
            EXPECT_GE(intOf(w.character, "vit"), 1);
            EXPECT_LE(intOf(w.character, "vit"), 70);

            if (total < 0)
                EXPECT_LE(intOf(w.character, "vit"), vit);

            // With nothing to eat the record cannot be walked: the bag says what it lacks
            w.character["rations"] = ElementType{0};

            ASSERT_TRUE(w.ask("begin", {}, "r1"));
            EXPECT_FALSE(w.last.blocked.empty());
            EXPECT_NE(w.last.blocked.find("ration"), std::string::npos);
            EXPECT_EQ(intOf(w.character, "rations"), 0);

            ASSERT_TRUE(w.ask("records"));
            EXPECT_FALSE(textOf(w.last.form, "blocked").empty());
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // What is known is the world's, and it changes what a walk costs and shows.
        TEST(expedition_test, knowledge_makes_a_beast_cheaper_to_find_and_easier_to_read)
        {
            MockLogger logger;
            Walker w(hunter());

            ASSERT_TRUE(w.ask("enter", WalkAct{"", "hollow", 0}));

            const int unknown = intOf(*byId(w.last.seeks, "wolf"), "cost");

            EXPECT_EQ(byId(w.last.works, "rest"), nullptr);

            // Three studies: the zone is known two of four, a wolf is found a day sooner, and
            // what the second level opens is open (a camp); the rare beast waits for the fourth
            for (int i = 0; i < 3; ++i)
            {
                ASSERT_TRUE(w.act("study"));
                w.answer();
            }

            ASSERT_TRUE(w.ask("look"));

            EXPECT_EQ(textOf(w.last.site, "knowledge"), "ZONE KNOWLEDGE 2 OF 4");
            EXPECT_NE(byId(w.last.works, "rest"), nullptr);

            // Its rule fills with the studies toward the third level, 3 of 6, not with the level
            EXPECT_EQ(intOf(w.last.site, "studies"), 3);
            EXPECT_EQ(intOf(w.last.site, "studyPercent"), 50);
            EXPECT_NE(textOf(w.last.site, "brings4").find("the white stag"), std::string::npos);
            EXPECT_EQ(byId(w.last.seeks, "stag"), nullptr);
            EXPECT_LT(intOf(*byId(w.last.seeks, "wolf"), "cost"), unknown);

            // Watched twice, a beast is known two of four
            ASSERT_TRUE(w.act("observe", "wolf"));
            ASSERT_TRUE(w.act("observe", "wolf"));

            EXPECT_EQ(intOf(w.character, "know.observe.wolf"), 2);
            EXPECT_NE(textOf(*byId(w.last.seeks, "wolf"), "rank").find("KNOWN 2 OF 4"), std::string::npos);

            // And what is known of it is shown in a fight: its vitality in figures
            ASSERT_TRUE(w.act("seek", "wolf", 1));
            w.answer();

            if (w.screen() == "fight")
            {
                ASSERT_FALSE(w.last.targets.empty());
                EXPECT_NE(textOf(w.last.targets[0], "life").find(" OF "), std::string::npos);
            }
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // A walk is a row of the Life page: the Adventure group, its months up front, locked until
        // what opens it is done.
        TEST(expedition_test, a_walk_is_a_row_of_the_life_page)
        {
            MockLogger logger;
            Walker w(gatherer());

            std::vector<RuleActivity> activities;

            ASSERT_TRUE(w.rules.activities(8.0f, w.character, activities));

            const RuleActivity* hedgerows = nullptr;
            const RuleActivity* pond = nullptr;

            for (const auto& a : activities)
            {
                if (textOf(a.fields, "id") == "walk.hedgerows")
                    hedgerows = &a;

                if (textOf(a.fields, "id") == "walk.millpond")
                    pond = &a;

                // No task stands before the first walk: its tile takes him there
                EXPECT_NE(textOf(a.fields, "id"), "hedges");
            }

            ASSERT_NE(hedgerows, nullptr);
            ASSERT_NE(pond, nullptr);

            EXPECT_EQ(textOf(hedgerows->fields, "group"), "Adventure");
            EXPECT_EQ(textOf(hedgerows->fields, "zone"), "hedgerows");
            EXPECT_EQ(intOf(hedgerows->fields, "months"), 1);

            // The first walk asks nothing of him: listed, and open
            EXPECT_EQ(intOf(hedgerows->fields, "listed"), 1);
            EXPECT_EQ(intOf(hedgerows->fields, "locked"), 0);
            EXPECT_TRUE(hedgerows->requires.empty());

            // The next one has no row until he has explored the pond, a task of his life: one place
            // at a time. Walked, or known 4 of 4 without it, is not enough
            EXPECT_EQ(intOf(pond->fields, "listed"), 0);

            auto row = [&](const std::string& id) -> const RuleActivity* {
                for (const auto& a : activities)
                {
                    if (textOf(a.fields, "id") == id)
                        return &a;
                }

                return nullptr;
            };

            // That task has no row before the Hedgerows are known 2 of 4
            ASSERT_NE(row("pond"), nullptr);
            EXPECT_EQ(intOf(row("pond")->fields, "listed"), 0);

            w.character["walked.hedgerows"] = ElementType{1};
            w.character["know.study.hedgerows"] = ElementType{10};
            ASSERT_TRUE(w.rules.activities(8.0f, w.character, activities));

            ASSERT_NE(row("walk.millpond"), nullptr);
            EXPECT_EQ(intOf(row("walk.millpond")->fields, "listed"), 0);

            // Known 2 of 4 (and more): the task stands, a month on the Life page, and it is not a walk;
            // what it gives is the world's. It holds the pond's age: at eight it waits for ten
            ASSERT_NE(row("pond"), nullptr);
            EXPECT_EQ(intOf(row("pond")->fields, "listed"), 1);
            EXPECT_EQ(intOf(row("pond")->fields, "locked"), 1);
            EXPECT_EQ(intOf(row("pond")->fields, "fromAge"), 10);
            EXPECT_EQ(intOf(row("pond")->fields, "months"), 1);
            EXPECT_EQ(textOf(row("pond")->fields, "group"), "Work");
            EXPECT_TRUE(textOf(row("pond")->fields, "zone").empty());
            ASSERT_EQ(row("pond")->gains.size(), 1u);
            EXPECT_EQ(textOf(row("pond")->gains[0], "stat"), "know.found.millpond");

            // The bargemen's tale makes it his at nine
            w.character["bargemen_tale"] = ElementType{1};
            ASSERT_TRUE(w.rules.activities(9.0f, w.character, activities));

            ASSERT_NE(row("pond"), nullptr);
            EXPECT_EQ(intOf(row("pond")->fields, "fromAge"), 9);
            EXPECT_EQ(intOf(row("pond")->fields, "locked"), 0);

            // Done, the world knows the pond (`know.found.millpond`): the task is offered no more
            w.character["know.found.millpond"] = ElementType{1};
            ASSERT_TRUE(w.rules.activities(9.0f, w.character, activities));

            ASSERT_NE(row("pond"), nullptr);
            EXPECT_EQ(intOf(row("pond")->fields, "listed"), 0);

            // Explored, Mill Pond is his at once: the task asked its age, the tale's year sooner too
            ASSERT_NE(row("walk.millpond"), nullptr);
            EXPECT_EQ(intOf(row("walk.millpond")->fields, "listed"), 1);
            EXPECT_EQ(intOf(row("walk.millpond")->fields, "locked"), 0);

            for (const auto& a : activities)
            {
                if (textOf(a.fields, "id") == "walk.hedgerows")
                    EXPECT_EQ(intOf(a.fields, "locked"), 0);

                // The Hollow Woods are three months: the way there, the stay and the way back
                if (textOf(a.fields, "id") == "walk.hollow")
                    EXPECT_EQ(intOf(a.fields, "months"), 3);
            }
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // What a walk brings for the town is his, and ends with him: at his death only what the
        // place he chooses still asks of it is left there, and counts toward its next level.
        TEST(expedition_test, what_a_place_asks_can_be_left_to_it)
        {
            MockLogger logger;
            Walker w(hunter());

            w.character["town_known"] = ElementType{1};
            w.character["seen.lodge"] = ElementType{1};
            w.character["coin"] = ElementType{30};
            w.character["firewood"] = ElementType{32};
            w.character["ashwood"] = ElementType{4};
            w.character["stone"] = ElementType{9};

            auto lodge = [&](RuleTown& town) -> const RulePlace* {
                EXPECT_TRUE(w.rules.town(14.0f, w.character, town)) << (w.rules.errors.empty() ? "" : w.rules.errors.front());

                for (const auto& place : town.places)
                {
                    if (textOf(place.fields, "id") == "lodge")
                        return &place;
                }

                return nullptr;
            };

            auto left = [](const RulePlace* place, const std::string& stat) {
                for (const auto& l : place->leaves)
                {
                    if (textOf(l, "stat") == stat)
                        return intOf(l, "amount");
                }

                return 0;
            };

            // Its first level asks 20 firewood and 10 ash wood: no more than it asks, no more than
            // he holds, and nothing of what it does not ask
            RuleTown town;
            const RulePlace* place = lodge(town);
            ASSERT_NE(place, nullptr);

            EXPECT_EQ(left(place, "firewood"), 20);
            EXPECT_EQ(left(place, "ashwood"), 4);
            EXPECT_EQ(left(place, "stone"), 0);
            EXPECT_EQ(textOf(place->fields, "leaveLine"), "20 firewood, 4 ash wood");
            EXPECT_TRUE(intOf(place->fields, "nextLocked"));   // Six ash wood short

            // What a life before left it counts toward what it asks, as coin does
            w.character["fund.lodge.firewood"] = ElementType{15};
            w.character["fund.lodge.ashwood"] = ElementType{10};

            RuleTown after;
            place = lodge(after);
            ASSERT_NE(place, nullptr);

            EXPECT_EQ(left(place, "firewood"), 5);
            EXPECT_EQ(left(place, "ashwood"), 0);
            EXPECT_FALSE(intOf(place->fields, "nextLocked"));

            bool firewoodAsked = false;

            for (const auto& gap : place->gaps)
            {
                if (textOf(gap, "stat") == "firewood")
                {
                    firewoodAsked = true;
                    EXPECT_EQ(intOf(gap, "needed"), 5);
                }

                EXPECT_NE(textOf(gap, "stat"), "ashwood");
            }

            EXPECT_TRUE(firewoodAsked);

            // And the work that raises it takes the same from him
            std::vector<RuleActivity> activities;
            ASSERT_TRUE(w.rules.activities(14.0f, w.character, activities));

            for (const auto& a : activities)
            {
                if (textOf(a.fields, "id") != "raise.lodge.1")
                    continue;

                int firewood = -1;
                int ashwood = -1;

                for (const auto& c : a.costs)
                {
                    if (textOf(c, "stat") == "firewood")
                        firewood = intOf(c, "amount");

                    if (textOf(c, "stat") == "ashwood")
                        ashwood = intOf(c, "amount");
                }

                EXPECT_EQ(firewood, 5);
                EXPECT_EQ(ashwood, -1);
            }
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // An encounter is beasts of several kinds in one fight: the Old Grey and two of his wolves.
        // Each beast is its own, with its own name and its own panel, and the fight's line in the
        // record names the encounter.
        TEST(expedition_test, a_fight_may_hold_beasts_of_several_kinds)
        {
            MockLogger logger;
            Walker w(knowing(hunter(), "hollow", 10));

            ASSERT_TRUE(w.ask("enter", WalkAct{"", "hollow", 0}));
            ASSERT_NE(byId(w.last.seeks, "enc.grey"), nullptr);

            ASSERT_TRUE(w.act("seek", "enc.grey", 0));
            w.answer();

            ASSERT_EQ(w.screen(), "fight");
            ASSERT_EQ(w.state.foes.size(), 3u);
            EXPECT_EQ(textOf(w.state.foes[0], "id"), "oldgrey");
            EXPECT_EQ(textOf(w.state.foes[1], "id"), "wolf");
            EXPECT_EQ(textOf(w.state.foes[2], "id"), "wolf");
            ASSERT_EQ(w.last.targets.size(), 3u);
            EXPECT_EQ(textOf(w.last.targets[0], "name"), "The Old Grey");

            w.fight();

            const ElementMap* line = nullptr;

            for (const auto& step : w.state.record)
            {
                if (textOf(step, "kind") == "fight")
                    line = &step;
            }

            ASSERT_NE(line, nullptr);
            EXPECT_EQ(textOf(*line, "target"), "enc.grey");
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // A fight is a timeline, not rounds: who is quicker acts more often. A boy as quick as
        // twenty acts twice before a water rat acts once; the order of the turn shows the six
        // coming actions, his first.
        TEST(expedition_test, a_quicker_man_acts_more_often)
        {
            MockLogger logger;

            ElementMap boy = knowing(gatherer(), "hedgerows", 10);
            boy["dex"] = ElementType{20};

            Walker w(boy);
            w.age = 10.0f;

            ASSERT_TRUE(w.ask("enter", WalkAct{"", "millpond", 0}));
            ASSERT_TRUE(w.act("seek", "rat", 1));
            w.answer();
            ASSERT_EQ(w.screen(), "fight");

            auto foeLines = [&w]() {
                int n = 0;

                for (const auto& a : w.state.account)
                {
                    if (textOf(a, "by") == "foe")
                        ++n;
                }

                return n;
            };

            ASSERT_EQ(w.last.order.size(), 6u);
            EXPECT_EQ(textOf(w.last.order[0], "note"), "NOW");

            // (he defends: nothing of his ends the rat before it has had its turn)
            ASSERT_TRUE(w.act("move", "guard"));
            EXPECT_EQ(foeLines(), 0);

            ASSERT_TRUE(w.act("move", "guard"));
            EXPECT_EQ(foeLines(), 1);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // A walk saved in the middle of a fight before the timeline came has no waits: the fight
        // goes on from where it stood.
        TEST(expedition_test, a_fight_saved_before_the_timeline_plays_on)
        {
            MockLogger logger;

            Walker w(knowing(gatherer(), "hedgerows", 10));
            w.age = 10.0f;

            ASSERT_TRUE(w.ask("enter", WalkAct{"", "millpond", 0}));
            ASSERT_TRUE(w.act("seek", "rat", 1));
            w.answer();
            ASSERT_EQ(w.screen(), "fight");

            w.state.fight.erase("heroWait");

            for (auto& foe : w.state.foes)
            {
                foe.erase("wait");
                foe.erase("delays");
            }

            ASSERT_TRUE(w.act("move", "guard"));
            EXPECT_EQ(textOf(w.last.duel, "turn"), "TURN 2");
            EXPECT_EQ(textOf(w.state.run, "refused"), "");
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // A blow has a kind: the boar's hide turns an edge and takes a hammer badly. What it
        // resists is said on the move once the beast is known well enough, and Lore knows it sooner
        // and finds the weakness deeper.
        TEST(expedition_test, a_beast_resists_some_blows_and_not_others)
        {
            MockLogger logger;

            ElementMap who = knowing(hunter(), "hollow", 10);
            who["slash_learned"] = ElementType{1};
            who["smith_apprentice"] = ElementType{1};

            auto fightBoar = [](const ElementMap& character) {
                auto w = std::make_unique<Walker>(character);

                EXPECT_TRUE(w->ask("enter", WalkAct{"", "hollow", 0}));
                EXPECT_TRUE(w->act("seek", "boar", 1));
                w->answer();
                EXPECT_EQ(w->screen(), "fight");

                return w;
            };

            auto row = [](const Walker& w, const std::string& id) {
                const ElementMap* found = byId(w.last.menu, id);

                EXPECT_NE(found, nullptr) << id;

                return found ? *found : ElementMap{};
            };

            auto endsWith = [](const std::string& text, const std::string& end) {
                return text.size() >= end.size() and text.compare(text.size() - end.size(), end.size(), end) == 0;
            };

            {
                // Str 11, Arms 2: 11-14 before the move. The edge: 130 %, half of it turned by the
                // hide, less 2 of armour. The hammer: 110 %, a quarter more, less 2
                auto w = fightBoar(who);

                EXPECT_EQ(textOf(row(*w, "tech.slash"), "deals"), "5\xE2\x80\x93" "7");
                EXPECT_EQ(textOf(row(*w, "tech.hammer"), "deals"), "13\xE2\x80\x93" "16");

                // Not known well enough: nothing said of what it resists
                EXPECT_FALSE(endsWith(textOf(row(*w, "tech.slash"), "note"), "RESISTED"));
            }

            {
                ElementMap watched = who;
                watched["know.observe.boar"] = ElementType{5};

                auto w = fightBoar(watched);

                EXPECT_TRUE(endsWith(textOf(row(*w, "tech.slash"), "note"), "RESISTED"));
                EXPECT_TRUE(endsWith(textOf(row(*w, "tech.hammer"), "note"), "WEAK"));
            }

            {
                // Lore 3: the boar is known a level higher, and its weakness is deeper (-40)
                ElementMap learned = who;
                learned["lore"] = ElementType{3};

                Walker plain(who);
                Walker lettered(learned);

                ASSERT_TRUE(plain.ask("enter", WalkAct{"", "hollow", 0}));
                ASSERT_TRUE(lettered.ask("enter", WalkAct{"", "hollow", 0}));

                ASSERT_NE(byId(plain.last.seeks, "boar"), nullptr);
                ASSERT_NE(byId(lettered.last.seeks, "boar"), nullptr);
                EXPECT_NE(textOf(*byId(plain.last.seeks, "boar"), "rank").find("KNOWN 1 OF 4"), std::string::npos);
                EXPECT_NE(textOf(*byId(lettered.last.seeks, "boar"), "rank").find("KNOWN 2 OF 4"), std::string::npos);

                auto w = fightBoar(learned);

                EXPECT_EQ(textOf(row(*w, "tech.hammer"), "deals"), "14\xE2\x80\x93" "19");
            }
        }

        namespace
        {
            // A hunter of the Hollow, known 4 of 4, in a fight with one boar, hard to kill
            std::unique_ptr<Walker> boarFight(ElementMap who)
            {
                who = knowing(std::move(who), "hollow", 10);
                who["vit"] = ElementType{200};
                who["vitmax"] = ElementType{200};

                auto w = std::make_unique<Walker>(who);

                EXPECT_TRUE(w->ask("enter", WalkAct{"", "hollow", 0}));
                EXPECT_TRUE(w->act("seek", "boar", 1));
                w->answer();
                EXPECT_EQ(w->screen(), "fight");

                return w;
            }

            bool accountSays(const Walker& w, const std::string& part)
            {
                for (const auto& a : w.state.account)
                {
                    if (textOf(a, "text").find(part) != std::string::npos)
                        return true;
                }

                return false;
            }
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // A working of fire leaves the beast burning: it hurts at its own turn, for a few of them.
        TEST(expedition_test, a_burning_beast_hurts_at_its_turn)
        {
            MockLogger logger;

            ElementMap mage = hunter();
            mage["collegium"] = ElementType{1};
            mage["fire_course"] = ElementType{1};
            mage["int"] = ElementType{16};

            auto w = boarFight(mage);

            for (int i = 0; i < 4 and textOf(w->state.foes[0], "mark") != "fire"; ++i)
            {
                ASSERT_TRUE(w->act("move", "tech.firebolt"));
                ASSERT_TRUE(w->act("move", "guard"));
            }

            ASSERT_TRUE(accountSays(*w, "burns"));
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // A beast with poise wears it down under his blows, a hammer's twice as fast: at nothing
        // it staggers, its next turn far off and its guard open, and its poise comes back whole.
        TEST(expedition_test, poise_broken_staggers_a_beast)
        {
            MockLogger logger;

            ElementMap smith = hunter();
            smith["smith_apprentice"] = ElementType{1};
            smith["dex"] = ElementType{30};

            auto w = boarFight(smith);

            ASSERT_EQ(intOf(w->state.foes[0], "poise"), 20);

            for (int i = 0; i < 5 and not accountSays(*w, "staggers"); ++i)
                ASSERT_TRUE(w->act("move", "tech.hammer"));

            ASSERT_TRUE(accountSays(*w, "staggers"));
            EXPECT_EQ(intOf(w->state.foes[0], "exposed"), 1);
            EXPECT_EQ(intOf(w->state.foes[0], "poise"), 20);
            EXPECT_NE(textOf(w->last.targets[0], "life").find("EXPOSED"), std::string::npos);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // Holds and delays thrown at one beast again and again count for less each time: it is
        // never kept from acting for good.
        TEST(expedition_test, no_beast_is_held_or_delayed_for_good)
        {
            MockLogger logger;

            ElementMap binder = hunter();
            binder["smuggler_runner"] = ElementType{1};
            binder["collegium"] = ElementType{1};
            binder["orin_known"] = ElementType{1};
            binder["discipline"] = ElementType{20};

            auto w = boarFight(binder);

            for (int i = 0; i < 12 and textOf(w->last.duel, "over").empty(); ++i)
            {
                std::string move = "guard";

                for (const char* hold : {"tech.sand", "tech.binding"})
                {
                    const ElementMap* row = byId(w->last.menu, hold);

                    if (move == "guard" and row and intOf(*row, "locked") == 0)
                        move = hold;
                }

                ASSERT_TRUE(w->act("move", move));
            }

            int acted = 0;

            for (const auto& a : w->state.account)
            {
                if (textOf(a, "by") == "foe" or textOf(a, "text").find("paws") != std::string::npos or textOf(a, "text").find("circles") != std::string::npos)
                    ++acted;
            }

            EXPECT_GE(acted, 2);
        }

        namespace
        {
            const ElementMap* menuRowOf(const Walker& w, const std::string& id)
            {
                return byId(w.last.menu, id);
            }

            // Presses a move until `done` holds, at most `most` times: whether it came to hold
            template <typename Done>
            bool pressUntil(Walker& w, const std::string& move, int most, Done done)
            {
                for (int i = 0; i < most and not done() and textOf(w.last.duel, "over").empty(); ++i)
                {
                    if (not w.act("move", move))
                        return false;
                }

                return done();
            }
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // The Keep's rhythm: every blow that lands builds his momentum, and at three a finisher
        // spends it all.
        TEST(expedition_test, three_blows_open_a_finisher)
        {
            MockLogger logger;

            ElementMap levy = hunter();
            levy["slash_learned"] = ElementType{1};
            levy["levy_served"] = ElementType{1};
            levy["dex"] = ElementType{30};

            auto w = boarFight(levy);

            ASSERT_NE(menuRowOf(*w, "tech.heavy"), nullptr);
            EXPECT_EQ(intOf(*menuRowOf(*w, "tech.heavy"), "locked"), 1);
            EXPECT_EQ(textOf(*menuRowOf(*w, "tech.heavy"), "asks"), "ASKS 3 MOMENTUM");

            ASSERT_TRUE(pressUntil(*w, "tech.slash", 10, [&w]() { return intOf(w->state.fight, "momentum") >= 3; }));
            EXPECT_EQ(intOf(*menuRowOf(*w, "tech.heavy"), "locked"), 0);

            ASSERT_TRUE(w->act("move", "tech.heavy"));
            EXPECT_EQ(intOf(w->state.fight, "momentum"), 0);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // Defending on the big blow costs a point of momentum; bracing, the Watch's lesson, keeps it.
        TEST(expedition_test, defending_costs_momentum_and_bracing_keeps_it)
        {
            MockLogger logger;

            ElementMap edge = hunter();
            edge["slash_learned"] = ElementType{1};
            edge["dex"] = ElementType{30};

            {
                auto w = boarFight(edge);

                ASSERT_TRUE(pressUntil(*w, "tech.slash", 10, [&w]() { return intOf(w->state.fight, "momentum") >= 2; }));
                ASSERT_EQ(intOf(w->state.fight, "momentum"), 2);
                ASSERT_TRUE(w->act("move", "guard"));
                EXPECT_EQ(intOf(w->state.fight, "momentum"), 1);
            }

            edge["watch_known"] = ElementType{1};

            {
                auto w = boarFight(edge);

                ASSERT_TRUE(pressUntil(*w, "tech.slash", 10, [&w]() { return intOf(w->state.fight, "momentum") >= 2; }));
                ASSERT_EQ(intOf(w->state.fight, "momentum"), 2);
                ASSERT_TRUE(w->act("move", "tech.brace"));
                EXPECT_EQ(intOf(w->state.fight, "momentum"), 2);
            }
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // The Collegium's rhythm: a working of frost on a beast still burning shatters, and the
        // marks are spent.
        TEST(expedition_test, frost_on_a_burning_beast_shatters)
        {
            MockLogger logger;

            ElementMap scholar = hunter();
            scholar["forbidden_read"] = ElementType{1};
            scholar["mara_student"] = ElementType{1};
            scholar["int"] = ElementType{14};

            auto w = boarFight(scholar);

            ASSERT_TRUE(pressUntil(*w, "tech.spark", 3, [&w]() { return textOf(w->state.foes[0], "mark") == "fire"; }));
            ASSERT_TRUE(pressUntil(*w, "tech.rime", 3, [&w]() { return accountSays(*w, "Shatter"); }));
            EXPECT_EQ(textOf(w->state.foes[0], "mark"), "");
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // The Hidden Hand's rhythm: every hit before the beast's own turn bites a little harder.
        TEST(expedition_test, hits_before_its_turn_bite_harder)
        {
            MockLogger logger;

            ElementMap quick = hunter();
            quick["thrust_learned"] = ElementType{1};
            quick["dex"] = ElementType{20};

            auto w = boarFight(quick);

            const std::string before = textOf(*menuRowOf(*w, "tech.thrust"), "deals");

            ASSERT_TRUE(pressUntil(*w, "tech.thrust", 3, [&w]() { return intOf(w->state.foes[0], "hits") >= 1; }));
            ASSERT_EQ(intOf(w->state.foes[0], "hits"), 1);

            EXPECT_NE(textOf(*menuRowOf(*w, "tech.thrust"), "deals"), before);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // The Greenwood's opening: a bow shot at the start of a fight costs no time, and he acts
        // again before anything moves.
        TEST(expedition_test, a_bow_shot_opens_the_fight_for_free)
        {
            MockLogger logger;

            auto w = boarFight(hunter());

            auto foeLines = [&w]() {
                int n = 0;

                for (const auto& a : w->state.account)
                {
                    if (textOf(a, "by") == "foe")
                        ++n;
                }

                return n;
            };

            const int before = foeLines();

            ASSERT_TRUE(w->act("move", "tech.bow"));
            EXPECT_EQ(foeLines(), before);
            EXPECT_EQ(textOf(w->last.duel, "turn"), "TURN 2");
            EXPECT_EQ(textOf(w->last.order[0], "note"), "NOW");
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // Who has followed the hunters finds a beast a day sooner.
        TEST(expedition_test, a_hunter_trained_finds_beasts_sooner)
        {
            MockLogger logger;

            ElementMap trained = hunter();
            trained["hunter_trained"] = ElementType{1};

            Walker plain(hunter());
            Walker tracker(trained);

            ASSERT_TRUE(plain.ask("enter", WalkAct{"", "hollow", 0}));
            ASSERT_TRUE(tracker.ask("enter", WalkAct{"", "hollow", 0}));
            ASSERT_NE(byId(plain.last.seeks, "wolf"), nullptr);
            ASSERT_NE(byId(tracker.last.seeks, "wolf"), nullptr);

            EXPECT_EQ(intOf(*byId(tracker.last.seeks, "wolf"), "cost"), intOf(*byId(plain.last.seeks, "wolf"), "cost") - 1);
        }

        namespace
        {
            // The place's own events on the way are answered (their first option): the card of
            // how to meet the beast, if there is one, is left up
            void answerZoneEvents(Walker& w)
            {
                for (int guard = 0; guard < 4 and w.screen() == "event" and textOf(w.last.card, "id") != "approach"; ++guard)
                    w.act("option", textOf(w.last.options[0], "id"));
            }
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // A beast found, who can hide or has set a trap chooses how to meet it: in the open (always
        // first), lying in wait, or not at all. Who can do neither meets it in the open, no card.
        TEST(expedition_test, a_man_who_can_hide_chooses_how_to_meet_a_beast)
        {
            MockLogger logger;

            ElementMap sly = hunter();
            sly["stealth"] = ElementType{6};

            Walker w(sly);

            ASSERT_TRUE(w.ask("enter", WalkAct{"", "hollow", 0}));
            ASSERT_TRUE(w.act("seek", "wolf", 1));
            answerZoneEvents(w);

            ASSERT_EQ(w.screen(), "event");
            EXPECT_EQ(textOf(w.last.card, "id"), "approach");
            ASSERT_GE(w.last.options.size(), 3u);
            EXPECT_EQ(textOf(w.last.options[0], "id"), "open");

            const ElementMap* ambush = byId(w.last.options, "ambush");

            ASSERT_NE(ambush, nullptr);
            EXPECT_NE(textOf(*ambush, "line").find("%"), std::string::npos);
            EXPECT_NE(byId(w.last.options, "slip"), nullptr);

            Walker plain(hunter());

            ASSERT_TRUE(plain.ask("enter", WalkAct{"", "hollow", 0}));
            ASSERT_TRUE(plain.act("seek", "wolf", 1));
            answerZoneEvents(plain);

            EXPECT_EQ(plain.screen(), "fight");
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // A trap set on the walk springs when he leads the beast into it: held, and hurt.
        TEST(expedition_test, a_trap_set_holds_the_beast_at_the_start)
        {
            MockLogger logger;

            ElementMap trapper = hunter();
            trapper["snare_set"] = ElementType{1};

            Walker w(trapper);

            ASSERT_TRUE(w.ask("enter", WalkAct{"", "hollow", 0}));

            const ElementMap* trap = byId(w.last.works, "trap");

            ASSERT_NE(trap, nullptr);
            EXPECT_EQ(textOf(*trap, "costText"), "1 AP");

            const int ap = intOf(w.state.run, "ap");

            ASSERT_TRUE(w.act("trap"));
            answerZoneEvents(w);
            EXPECT_EQ(intOf(w.state.run, "ap"), ap - 1);
            EXPECT_EQ(intOf(w.state.run, "traps"), 1);

            ASSERT_TRUE(w.act("seek", "wolf", 1));
            answerZoneEvents(w);
            ASSERT_EQ(textOf(w.last.card, "id"), "approach");
            ASSERT_NE(byId(w.last.options, "trap"), nullptr);

            ASSERT_TRUE(w.act("option", "trap"));
            ASSERT_EQ(w.screen(), "fight");
            EXPECT_EQ(intOf(w.state.foes[0], "held"), 1);
            EXPECT_EQ(intOf(w.state.foes[0], "hp"), intOf(w.state.foes[0], "hpMax") - 6);
            EXPECT_EQ(intOf(w.state.run, "traps"), 0);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // He may leave it be: the days of the search are spent, and there is no fight.
        TEST(expedition_test, slipping_past_spends_the_days_and_fights_nothing)
        {
            MockLogger logger;

            ElementMap sly = hunter();
            sly["stealth"] = ElementType{3};

            Walker w(sly);

            ASSERT_TRUE(w.ask("enter", WalkAct{"", "hollow", 0}));
            ASSERT_TRUE(w.act("seek", "wolf", 1));
            answerZoneEvents(w);
            ASSERT_EQ(textOf(w.last.card, "id"), "approach");

            ASSERT_TRUE(w.act("option", "slip"));
            EXPECT_EQ(w.screen(), "site");
            EXPECT_TRUE(w.state.foes.empty());
            ASSERT_FALSE(w.state.record.empty());
            EXPECT_NE(textOf(w.state.record.back(), "text").find("slipped past"), std::string::npos);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // A fire flask from the market: thrown, it does the same to any beast, past its hide and
        // whatever it resists, and leaves it burning.
        TEST(expedition_test, a_fire_flask_burns_whatever_it_is_thrown_at)
        {
            MockLogger logger;

            ElementMap thrower = hunter();
            thrower["flask"] = ElementType{1};

            auto w = boarFight(thrower);

            const ElementMap* row = menuRowOf(*w, "item.flask");

            ASSERT_NE(row, nullptr);
            EXPECT_EQ(textOf(*row, "group"), "Items");

            ASSERT_TRUE(w->act("move", "item.flask"));

            bool thrown = false;

            for (const auto& a : w->state.account)
            {
                if (textOf(a, "text").find("Fire flask") != std::string::npos and textOf(a, "figure") == "10")
                    thrown = true;
            }

            EXPECT_TRUE(thrown);
            EXPECT_EQ(intOf(w->character, "flask"), 0);
            EXPECT_EQ(menuRowOf(*w, "item.flask"), nullptr);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // Blessed oil on his blade: every blow of his is holy for the rest of the fight.
        TEST(expedition_test, blessed_oil_makes_his_blows_holy)
        {
            MockLogger logger;

            ElementMap pious = hunter();
            pious["oil"] = ElementType{1};

            auto w = boarFight(pious);

            ASSERT_TRUE(w->act("move", "item.oil"));
            EXPECT_EQ(intOf(w->state.fight, "oil"), 1);

            ASSERT_TRUE(w->act("pick", "strike"));
            EXPECT_NE(textOf(w->last.preview, "text").find("holy"), std::string::npos);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // A kill trains no part of him: the first kill of a kind of beast in a life is a lesson
        // about it instead, and the beast is known 3 of 4 from then on.
        TEST(expedition_test, a_kill_teaches_a_lesson_and_no_part)
        {
            MockLogger logger;

            ElementMap boy = knowing(gatherer(), "hedgerows", 10);
            boy["str"] = ElementType{20};
            boy["xp.dex"] = ElementType{49};   // (a kill of a rat used to train Dexterity: one more point was a point)

            Walker w(boy);
            w.age = 10.0f;

            ASSERT_TRUE(w.ask("enter", WalkAct{"", "millpond", 0}));
            ASSERT_TRUE(w.act("seek", "rat", 1));
            w.answer();
            ASSERT_EQ(w.screen(), "fight");

            const int dex = intOf(w.character, "dex");
            bool lesson = false;

            for (int i = 0; i < 20 and textOf(w.last.duel, "over").empty(); ++i)
            {
                ASSERT_TRUE(w.act("move", "strike"));

                for (const auto& e : w.last.entries)
                {
                    if (textOf(e, "glyph") == "study" and textOf(e, "kind") == "gain")
                        lesson = true;
                }
            }

            ASSERT_EQ(textOf(w.last.duel, "over"), "won");
            EXPECT_TRUE(lesson);
            EXPECT_EQ(intOf(w.character, "lesson.rat"), 1);
            EXPECT_EQ(intOf(w.character, "dex"), dex);

            ASSERT_TRUE(w.act("back"));
            w.answer();

            ASSERT_NE(byId(w.last.seeks, "rat"), nullptr);
            EXPECT_NE(textOf(*byId(w.last.seeks, "rat"), "rank").find("KNOWN 3 OF 4"), std::string::npos);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // A technique used often rises a rank: a little stronger, and a point of the part it trains.
        TEST(expedition_test, a_technique_used_often_rises_a_rank)
        {
            MockLogger logger;

            ElementMap edge = hunter();
            edge["slash_learned"] = ElementType{1};
            edge["uses.slash"] = ElementType{9};

            auto w = boarFight(edge);

            const int str = intOf(w->character, "str");
            const std::string before = textOf(*menuRowOf(*w, "tech.slash"), "deals");

            ASSERT_TRUE(w->act("move", "tech.slash"));

            EXPECT_EQ(intOf(w->character, "uses.slash"), 10);
            EXPECT_EQ(intOf(w->character, "rank.slash"), 1);
            EXPECT_EQ(intOf(w->character, "str"), str + 1);
            EXPECT_NE(textOf(*menuRowOf(*w, "tech.slash"), "deals"), before);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // A record kept before kills stopped training him is walked again: its loot, no part.
        TEST(expedition_test, an_old_record_gives_its_loot_and_no_part)
        {
            MockLogger logger;

            ElementMap boy = knowing(gatherer(), "hedgerows", 10);
            boy["xp.dex"] = ElementType{49};

            Walker w(boy);
            w.age = 10.0f;

            WalkRecipe old;
            old.id = "r1";
            old.fields = {{"zone", ElementType{std::string("millpond")}}, {"name", ElementType{std::string("Mill Pond")}}, {"kind", ElementType{std::string("forage")}},
                          {"steps", ElementType{1}}, {"ap", ElementType{1}}, {"hp", ElementType{0}}, {"months", ElementType{2}}, {"walkedAt", ElementType{120}},
                          {"kills", ElementType{std::string("2 water rats")}}, {"downed", ElementType{0}}};
            old.steps = {{{"n", ElementType{1}}, {"kind", ElementType{std::string("fight")}}, {"target", ElementType{std::string("rat")}}, {"count", ElementType{2}},
                          {"ap", ElementType{1}}, {"hp", ElementType{0}}, {"once", ElementType{0}}}};
            old.marks = {{{"step", ElementType{1}}, {"kind", ElementType{std::string("kill")}}, {"id", ElementType{std::string("rat")}}, {"n", ElementType{2}}}};

            w.recipes.push_back(old);

            const int dex = intOf(w.character, "dex");

            ASSERT_TRUE(w.ask("term", WalkAct{}, "r1"));
            EXPECT_EQ(intOf(w.character, "dex"), dex);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // Every beast leaves a page of the chronicle (the curse: the book writes itself on what he
        // kills), but the Collegium's dummies, and a demon, if one comes. The outskirts' slime is a
        // story's beast that leaves pages all the same: four, the market's lot, the one beast that
        // leaves more than one
        TEST(expedition_test, every_beast_drops_a_page)
        {
            std::ifstream file(std::string(CHRONICLE_SOURCE_DIR) + "/examples/Chronicle/rules/expeditiontable.pg");
            ASSERT_TRUE(file.good());

            std::stringstream all;
            all << file.rdbuf();
            const std::string text = all.str();

            const size_t from = text.find("const walkMobs = [");
            const size_t to = text.find("// What a walk may meet besides one beast");
            ASSERT_NE(from, std::string::npos);
            ASSERT_NE(to, std::string::npos);

            const std::string mobs = text.substr(from, to - from);

            int rows = 0;
            int paged = 0;
            size_t at = mobs.find("{id: \"");

            while (at != std::string::npos)
            {
                const size_t next = mobs.find("\n    {id: \"", at + 1);
                const std::string row = mobs.substr(at, next == std::string::npos ? std::string::npos : next - at);
                ++rows;

                const bool slime = row.find("{id: \"slime\"") != std::string::npos;
                const bool exempt = (row.find("story: true") != std::string::npos and not slime) or row.find("demon: true") != std::string::npos;
                const bool has = row.find(slime ? "{id: \"page\", chance: 100, least: 4, most: 4}" : "{id: \"page\", chance: 100, least: 1, most: 1}") != std::string::npos;

                if (exempt)
                    EXPECT_FALSE(has) << row.substr(0, 40);
                else
                {
                    EXPECT_TRUE(has) << row.substr(0, 40);
                    ++paged;
                }

                at = next == std::string::npos ? next : next + 1;
            }

            EXPECT_GE(rows, 15);
            EXPECT_GE(paged, 13);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // A page is sold at the market, four for a coin (two paid the walks above the rule)
        TEST(expedition_test, a_page_sells_four_for_a_coin)
        {
            MockLogger logger;
            EntitySystem ecs;
            Rules rules;
            ASSERT_TRUE(rules.load(&ecs, compiledRules()));

            ElementMap seller;
            seller["page"] = ElementType{4};

            bool found = false;

            std::vector<RuleActivity> rows;
            ASSERT_TRUE(rules.activities(20.0f, seller, rows));

            for (const auto& a : rows)
            {
                if (textOf(a.fields, "id") != "sell.page")
                    continue;

                found = true;
                ASSERT_EQ(a.gains.size(), 1u);
                EXPECT_EQ(textOf(a.gains[0], "stat"), "coin");
                EXPECT_EQ(intOf(a.gains[0], "amount"), 1);
                ASSERT_EQ(a.costs.size(), 1u);
                EXPECT_EQ(textOf(a.costs[0], "stat"), "page");
                EXPECT_EQ(intOf(a.costs[0], "amount"), 4);
            }

            EXPECT_TRUE(found);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // Short of a lot, the sell row is listed (he holds some) and refused, and its tooltip says
        // so in one voice: the pages to sell are 1 of 4, unmet, as the cost of 4 says
        TEST(expedition_test, a_sell_row_short_of_its_lot_says_so)
        {
            MockLogger logger;
            EntitySystem ecs;
            Rules rules;
            ASSERT_TRUE(rules.load(&ecs, compiledRules()));

            for (const int held : {1, 3, 4})
            {
                ElementMap seller;
                seller["page"] = ElementType{held};
                seller["town_known"] = ElementType{1};

                std::vector<RuleActivity> rows;
                ASSERT_TRUE(rules.activities(8.0f, seller, rows));

                const RuleActivity* sell = nullptr;

                for (const auto& a : rows)
                {
                    if (textOf(a.fields, "id") == "sell.page")
                        sell = &a;
                }

                ASSERT_NE(sell, nullptr) << held;
                EXPECT_EQ(intOf(sell->fields, "listed"), 1) << held;
                EXPECT_EQ(intOf(sell->fields, "locked"), held < 4 ? 1 : 0) << held;

                const ElementMap* asked = nullptr;

                for (const auto& r : sell->requires)
                {
                    if (textOf(r, "stat") == "page" and intOf(r, "cost") == 0)
                        asked = &r;
                }

                ASSERT_NE(asked, nullptr) << held;
                EXPECT_EQ(intOf(*asked, "current"), held);
                EXPECT_EQ(intOf(*asked, "needed"), 4);
            }

            // None held: no row
            std::vector<RuleActivity> none;
            ASSERT_TRUE(rules.activities(8.0f, {{"town_known", ElementType{1}}}, none));

            for (const auto& a : none)
            {
                if (textOf(a.fields, "id") == "sell.page")
                    EXPECT_EQ(intOf(a.fields, "listed"), 0);
            }
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // Held, the pages say what they are in the stores: a word of the curse, and the market's
        TEST(expedition_test, a_page_says_what_it_is)
        {
            MockLogger logger;
            EntitySystem ecs;
            Rules rules;
            ASSERT_TRUE(rules.load(&ecs, compiledRules()));

            RuleMonth month;
            ASSERT_TRUE(rules.month(8.0f, {{"vit", ElementType{30}}, {"rations", ElementType{6}}, {"page", ElementType{3}}}, month));

            bool found = false;

            for (const auto& gloss : month.glosses)
            {
                if (textOf(gloss.fields, "id") != "page")
                    continue;

                found = true;

                const std::string text = textOf(gloss.fields, "text");

                EXPECT_NE(text.find("curse"), std::string::npos) << text;
                EXPECT_NE(text.find("The market buys them four for a coin."), std::string::npos) << text;
            }

            EXPECT_TRUE(found);

            // The other things sold say how many make their coin, and a thing sold one at a time
            // that it is bought
            ASSERT_TRUE(rules.month(8.0f, {{"vit", ElementType{30}}, {"rations", ElementType{6}}, {"fangs", ElementType{1}}, {"antler", ElementType{1}}}, month));

            int said = 0;

            for (const auto& gloss : month.glosses)
            {
                if (textOf(gloss.fields, "id") == "fangs")
                {
                    EXPECT_EQ(textOf(gloss.fields, "text"), "Brought home from a walk. The market buys them two for a coin.");
                    ++said;
                }

                if (textOf(gloss.fields, "id") == "antler")
                {
                    EXPECT_EQ(textOf(gloss.fields, "text"), "Brought home from a walk. The market buys it.");
                    ++said;
                }
            }

            EXPECT_EQ(said, 2);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // A kill on a walk brings a page home
        TEST(expedition_test, a_kill_on_a_walk_brings_a_page)
        {
            MockLogger logger;

            ElementMap boy = knowing(gatherer(), "hedgerows", 10);
            boy["str"] = ElementType{20};

            Walker w(boy);
            w.age = 10.0f;

            ASSERT_TRUE(w.ask("enter", WalkAct{"", "millpond", 0}));
            ASSERT_TRUE(w.act("seek", "rat", 1));
            w.answer();
            ASSERT_EQ(w.screen(), "fight");

            for (int i = 0; i < 20 and textOf(w.last.duel, "over").empty(); ++i)
                ASSERT_TRUE(w.act("move", "strike"));

            ASSERT_EQ(textOf(w.last.duel, "over"), "won");
            EXPECT_EQ(marked(w.state.marks, "loot", "page"), 1);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // The Old Grey leads: while he stands his wolves are quicker. Hurt below half, he howls
        // and the wood sends one more, once.
        TEST(expedition_test, the_old_grey_leads_and_calls_the_wood)
        {
            MockLogger logger;

            ElementMap strong = knowing(hunter(), "hollow", 10);
            strong["str"] = ElementType{30};
            strong["arms"] = ElementType{8};
            strong["dex"] = ElementType{30};
            strong["vit"] = ElementType{300};
            strong["vitmax"] = ElementType{300};

            Walker w(strong);

            ASSERT_TRUE(w.ask("enter", WalkAct{"", "hollow", 0}));
            ASSERT_TRUE(w.act("seek", "enc.grey", 0));
            w.answer();
            ASSERT_EQ(w.screen(), "fight");
            ASSERT_EQ(w.state.foes.size(), 3u);

            // A wolf of his is as quick as 9, and 3 more while he leads
            ASSERT_TRUE(w.act("target", "", 1));
            EXPECT_NE(textOf(w.last.duel, "quick").find("AGAINST 12"), std::string::npos) << textOf(w.last.duel, "quick");

            ASSERT_TRUE(w.act("target", "", 0));
            ASSERT_TRUE(pressUntil(w, "strike", 4, [&w]() { return accountSays(w, "howls"); }));
            EXPECT_EQ(w.state.foes.size(), 4u);

            for (int i = 0; i < 3 and textOf(w.last.duel, "over").empty(); ++i)
                ASSERT_TRUE(w.act("move", "strike"));

            EXPECT_LE(w.state.foes.size(), 4u);

            int howls = 0;

            for (const auto& a : w.state.account)
            {
                if (textOf(a, "text").find("howls") != std::string::npos)
                    ++howls;
            }

            EXPECT_EQ(howls, 1);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // The White Stag does not stay to be killed: hurt, it bolts at its next turn, with what it
        // would have left.
        TEST(expedition_test, the_white_stag_bolts_once_hurt)
        {
            MockLogger logger;

            ElementMap hunterOfIt = knowing(hunter(), "hollow", 10);
            hunterOfIt["str"] = ElementType{17};
            hunterOfIt["dex"] = ElementType{30};

            Walker w(hunterOfIt);

            ASSERT_TRUE(w.ask("enter", WalkAct{"", "hollow", 0}));
            ASSERT_TRUE(w.act("seek", "stag", 1));
            w.answer();
            ASSERT_EQ(w.screen(), "fight");

            for (int i = 0; i < 6 and textOf(w.last.duel, "over").empty() and not accountSays(w, "bolts"); ++i)
                ASSERT_TRUE(w.act("move", "strike"));

            ASSERT_TRUE(accountSays(w, "bolts"));
            EXPECT_FALSE(textOf(w.last.duel, "over").empty());
            EXPECT_EQ(intOf(w.state.foes[0], "fled"), 1);
            EXPECT_EQ(marked(w.state.marks, "kill", "stag"), 0);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // The Old Shell turns a working on its stone back: below half the shell cracks, and the
        // ward is gone.
        TEST(expedition_test, the_old_shell_cracks_at_half)
        {
            MockLogger logger;

            ElementMap mage = knowing(hunter(), "quarry", 10);
            mage["collegium"] = ElementType{1};
            mage["fire_course"] = ElementType{1};
            mage["int"] = ElementType{16};
            mage["arcana"] = ElementType{10};
            mage["vit"] = ElementType{300};
            mage["vitmax"] = ElementType{300};

            Walker w(mage);

            ASSERT_TRUE(w.ask("enter", WalkAct{"", "quarry", 0}));
            ASSERT_TRUE(w.act("seek", "enc.shell", 0));
            w.answer();
            ASSERT_EQ(w.screen(), "fight");

            // Int 16, Arcana 10: 23-26; fire it is weak to, a quarter more; less its ward of 4
            ASSERT_NE(menuRowOf(w, "tech.firebolt"), nullptr);
            EXPECT_EQ(textOf(*menuRowOf(w, "tech.firebolt"), "deals"), "24\xE2\x80\x93" "28");

            for (int i = 0; i < 8 and not accountSays(w, "cracks") and textOf(w.last.duel, "over").empty(); ++i)
            {
                const ElementMap* bolt = menuRowOf(w, "tech.firebolt");

                ASSERT_TRUE(w.act("move", bolt and intOf(*bolt, "locked") == 0 ? "tech.firebolt" : "guard"));
            }

            ASSERT_TRUE(accountSays(w, "cracks"));
            ASSERT_TRUE(textOf(w.last.duel, "over").empty());

            // (his hits on it since its last turn would make it more: count none)
            if (intOf(w.state.foes[0], "hits") == 0)
                EXPECT_EQ(textOf(*menuRowOf(w, "tech.firebolt"), "deals"), "28\xE2\x80\x93" "32");
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // A boss is met by choice, its tile saying what is at stake; beaten, it gives what it has
        // and is beaten for this life. Its fight is written once: a record walked again passes it by.
        TEST(expedition_test, a_boss_is_met_by_choice_and_beaten_once)
        {
            MockLogger logger;

            ElementMap champion = knowing(hunter(), "hollow", 10);
            champion["str"] = ElementType{20};
            champion["arms"] = ElementType{8};
            champion["dex"] = ElementType{30};
            champion["vit"] = ElementType{300};
            champion["vitmax"] = ElementType{300};

            Walker w(champion);

            ASSERT_TRUE(w.ask("enter", WalkAct{"", "hollow", 0}));

            const ElementMap* tile = byId(w.last.seeks, "enc.grey");

            ASSERT_NE(tile, nullptr);
            EXPECT_EQ(intOf(*tile, "boss"), 1);
            EXPECT_NE(textOf(*tile, "note").find("you die"), std::string::npos);

            ASSERT_TRUE(w.act("seek", "enc.grey", 0));
            w.answer();
            ASSERT_EQ(w.screen(), "fight");

            ASSERT_TRUE(pressUntil(w, "strike", 30, [&w]() { return not textOf(w.last.duel, "over").empty(); }));
            ASSERT_EQ(textOf(w.last.duel, "over"), "won");

            EXPECT_EQ(intOf(w.character, "beaten.grey"), 1);
            EXPECT_EQ(intOf(w.character, "grey_pelt"), 1);

            for (const auto& step : w.state.record)
            {
                if (textOf(step, "target") == "enc.grey")
                    EXPECT_EQ(intOf(step, "once"), 1) << textOf(step, "kind");
            }

            // Another walk in the same life: it is not there to be fought again
            w.state = WalkState{};

            ASSERT_TRUE(w.ask("enter", WalkAct{"", "hollow", 0}));
            tile = byId(w.last.seeks, "enc.grey");

            ASSERT_NE(tile, nullptr);
            EXPECT_EQ(intOf(*tile, "locked"), 1);
            EXPECT_EQ(textOf(*tile, "asks"), "BEATEN IN THIS LIFE");
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // Brought down by a boss, he does not come home.
        TEST(expedition_test, falling_to_a_boss_is_death)
        {
            MockLogger logger;

            ElementMap doomed = knowing(hunter(), "hollow", 10);
            doomed["vit"] = ElementType{1};

            Walker w(doomed);

            ASSERT_TRUE(w.ask("enter", WalkAct{"", "hollow", 0}));
            ASSERT_TRUE(w.act("seek", "enc.grey", 0));
            w.answer();

            if (w.screen() == "fight")
            {
                ASSERT_TRUE(pressUntil(w, "guard", 30, [&w]() { return not textOf(w.last.duel, "over").empty(); }));
                ASSERT_TRUE(w.act("back"));
            }

            EXPECT_EQ(w.screen(), "done");
            EXPECT_EQ(intOf(w.character, "vit"), 0);
            EXPECT_EQ(intOf(w.character, "fell"), 1);
            EXPECT_EQ(intOf(w.last.recipe, "died"), 1);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // Brought down by an ordinary beast, he is carried home without what the walk had gathered,
        // and with nothing worse.
        TEST(expedition_test, brought_down_he_loses_what_the_walk_gathered)
        {
            MockLogger logger;

            ElementMap boy = knowing(gatherer(), "hedgerows", 10);
            boy["vit"] = ElementType{1};

            Walker w(boy);
            w.age = 10.0f;

            ASSERT_TRUE(w.ask("enter", WalkAct{"", "millpond", 0}));

            const int reeds = intOf(w.character, "reeds");
            const int most = intOf(w.character, "vitmax");

            ASSERT_TRUE(w.act("gather", "reeds"));
            w.answer();
            ASSERT_GT(intOf(w.character, "reeds"), reeds);

            ASSERT_TRUE(w.act("seek", "rat", 1));
            w.answer();
            ASSERT_EQ(w.screen(), "fight");
            ASSERT_TRUE(pressUntil(w, "guard", 30, [&w]() { return not textOf(w.last.duel, "over").empty(); }));
            ASSERT_EQ(textOf(w.last.duel, "over"), "lost");
            ASSERT_TRUE(w.act("back"));

            EXPECT_EQ(w.screen(), "done");
            EXPECT_EQ(intOf(w.character, "reeds"), reeds);
            EXPECT_EQ(intOf(w.character, "vitmax"), most);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // On a walk the blow that brings him down is a blow: counted, and told as the beast's,
        // whatever little was left of him to take (only a story's dummy spares him).
        TEST(expedition_test, the_blow_that_brings_him_down_is_a_blow)
        {
            MockLogger logger;

            ElementMap boy = knowing(gatherer(), "hedgerows", 10);
            boy["vit"] = ElementType{1};

            Walker w(boy);
            w.age = 10.0f;

            ASSERT_TRUE(w.ask("enter", WalkAct{"", "millpond", 0}));
            ASSERT_TRUE(w.act("seek", "rat", 1));
            w.answer();
            ASSERT_EQ(w.screen(), "fight");
            ASSERT_TRUE(pressUntil(w, "guard", 30, [&w]() { return not textOf(w.last.duel, "over").empty(); }));
            ASSERT_EQ(textOf(w.last.duel, "over"), "lost");

            EXPECT_GE(intOf(w.state.fight, "bitten"), 1);

            bool bitten = false;

            for (const auto& a : w.state.account)
            {
                EXPECT_EQ(textOf(a, "text").find("barely touches him"), std::string::npos) << textOf(a, "text");

                if (textOf(a, "text") == "The water rat bites")
                    bitten = true;
            }

            EXPECT_TRUE(bitten);
        }

        namespace
        {
            // The last thing he did, as the account tells it
            std::string lastHeroLine(const Walker& w)
            {
                std::string line;

                for (const auto& a : w.state.account)
                {
                    if (textOf(a, "by") == "hero")
                        line = textOf(a, "text");
                }

                return line;
            }
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // Handed over, his stance plays his rhythm: a full momentum is spent on a finisher.
        TEST(expedition_test, the_stance_spends_a_full_momentum)
        {
            MockLogger logger;

            ElementMap levy = hunter();
            levy["slash_learned"] = ElementType{1};
            levy["levy_served"] = ElementType{1};
            levy["dex"] = ElementType{30};

            auto w = boarFight(levy);

            ASSERT_TRUE(pressUntil(*w, "tech.slash", 10, [&w]() { return intOf(w->state.fight, "momentum") >= 3; }));
            ASSERT_TRUE(w->act("step"));
            EXPECT_EQ(lastHeroLine(*w).rfind("Heavy cut", 0), 0u) << lastHeroLine(*w);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // Handed over, a beast burning and a working of frost known: his stance sets it off.
        TEST(expedition_test, the_stance_sets_off_a_reaction)
        {
            MockLogger logger;

            ElementMap scholar = hunter();
            scholar["forbidden_read"] = ElementType{1};
            scholar["mara_student"] = ElementType{1};
            scholar["int"] = ElementType{14};

            auto w = boarFight(scholar);

            ASSERT_TRUE(pressUntil(*w, "tech.spark", 3, [&w]() { return textOf(w->state.foes[0], "mark") == "fire"; }));

            const size_t before = w->state.account.size();

            ASSERT_TRUE(w->act("step"));

            // (Rime, and a Shatter after it when it lands)
            bool rime = false;

            for (size_t i = before; i < w->state.account.size(); ++i)
            {
                if (textOf(w->state.account[i], "text").rfind("Rime", 0) == 0)
                    rime = true;
            }

            EXPECT_TRUE(rime);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // Handed over against the Old Grey and his wolves, his stance goes for the leader first.
        TEST(expedition_test, the_stance_aims_at_the_leader)
        {
            MockLogger logger;

            ElementMap strong = knowing(hunter(), "hollow", 10);
            strong["vit"] = ElementType{300};
            strong["vitmax"] = ElementType{300};

            Walker w(strong);

            ASSERT_TRUE(w.ask("enter", WalkAct{"", "hollow", 0}));
            ASSERT_TRUE(w.act("seek", "enc.grey", 0));
            w.answer();
            ASSERT_EQ(w.screen(), "fight");

            ASSERT_TRUE(w.act("target", "", 1));
            ASSERT_TRUE(w.act("step"));
            EXPECT_EQ(intOf(w.state.fight, "target"), 0);
        }

        namespace
        {
            // A fight against a boss of a place known 4 of 4, for one who will not fall soon
            std::unique_ptr<Walker> bossFight(ElementMap who, const std::string& zone, const std::string& enc)
            {
                who = knowing(std::move(who), zone, 10);

                auto w = std::make_unique<Walker>(who);

                EXPECT_TRUE(w->ask("enter", WalkAct{"", zone, 0}));
                EXPECT_TRUE(w->act("seek", enc, 0));
                w->answer();
                EXPECT_EQ(w->screen(), "fight");

                return w;
            }
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // The bosses teach the kinds that need deep places: the Old Shell the mole, the Old Grey
        // the weaver.
        TEST(expedition_test, the_old_shell_teaches_the_mole)
        {
            MockLogger logger;

            ElementMap champion = hunter();
            champion["str"] = ElementType{20};
            champion["arms"] = ElementType{8};
            champion["dex"] = ElementType{30};
            champion["vit"] = ElementType{300};
            champion["vitmax"] = ElementType{300};

            auto w = bossFight(champion, "quarry", "enc.shell");

            ASSERT_TRUE(pressUntil(*w, "strike", 30, [&w]() { return not textOf(w->last.duel, "over").empty(); }));
            ASSERT_EQ(textOf(w->last.duel, "over"), "won");

            EXPECT_EQ(intOf(w->character, "know.fam.mole"), 1);
        }

        TEST(expedition_test, the_old_grey_teaches_the_weaver)
        {
            MockLogger logger;

            ElementMap champion = hunter();
            champion["str"] = ElementType{20};
            champion["arms"] = ElementType{8};
            champion["dex"] = ElementType{30};
            champion["vit"] = ElementType{300};
            champion["vitmax"] = ElementType{300};

            auto w = bossFight(champion, "hollow", "enc.grey");

            ASSERT_TRUE(pressUntil(*w, "strike", 30, [&w]() { return not textOf(w->last.duel, "over").empty(); }));
            ASSERT_EQ(textOf(w->last.duel, "over"), "won");

            EXPECT_EQ(intOf(w->character, "know.fam.weaver"), 1);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // Millbrook Marsh is a man's walk: listed from sixteen, his from eighteen once the Quarry
        // Road is known 4 of 4, three months long. Its lights come as a swarm from the first visit,
        // its bog hounds one at a time; pitch is gathered there from the first, bog iron once the
        // marsh is known 3 of 4, and the leech mass and the Lantern Mother once it is known 4 of 4.
        TEST(expedition_test, the_marsh_is_walked_from_eighteen)
        {
            MockLogger logger;

            Walker w(knowing(hunter(), "quarry", 10));
            std::vector<RuleActivity> activities;

            auto marsh = [&activities](Walker& who, float age) -> const RuleActivity* {
                activities.clear();
                EXPECT_TRUE(who.rules.activities(age, who.character, activities));

                for (const auto& a : activities)
                {
                    if (textOf(a.fields, "id") == "walk.marsh")
                        return &a;
                }

                return nullptr;
            };

            ASSERT_NE(marsh(w, 15.0f), nullptr);
            EXPECT_EQ(intOf(marsh(w, 15.0f)->fields, "listed"), 0);
            ASSERT_NE(marsh(w, 17.0f), nullptr);
            EXPECT_EQ(intOf(marsh(w, 17.0f)->fields, "listed"), 1);
            EXPECT_EQ(intOf(marsh(w, 17.0f)->fields, "locked"), 1);

            const RuleActivity* grown = marsh(w, 18.0f);

            ASSERT_NE(grown, nullptr);
            EXPECT_EQ(intOf(grown->fields, "locked"), 0);
            EXPECT_EQ(textOf(grown->fields, "group"), "Adventure");
            EXPECT_EQ(textOf(grown->fields, "zone"), "marsh");
            EXPECT_EQ(intOf(grown->fields, "months"), 3);

            // The Quarry Road not known through and through: shut at eighteen
            Walker green(hunter());
            ASSERT_NE(marsh(green, 18.0f), nullptr);
            EXPECT_EQ(intOf(marsh(green, 18.0f)->fields, "locked"), 1);

            // A first visit: the lights, as a swarm of four or one alone, and a lone bog hound;
            // pitch to gather
            w.age = 18.0f;
            ASSERT_TRUE(w.ask("enter", WalkAct{"", "marsh", 0}));
            EXPECT_NE(byId(w.last.seeks, "light*pack"), nullptr);
            EXPECT_NE(byId(w.last.seeks, "light"), nullptr);
            EXPECT_NE(byId(w.last.seeks, "hound"), nullptr);
            EXPECT_EQ(byId(w.last.seeks, "hound*pack"), nullptr);
            EXPECT_EQ(byId(w.last.seeks, "leech"), nullptr);
            EXPECT_EQ(byId(w.last.seeks, "enc.lantern"), nullptr);
            EXPECT_NE(byId(w.last.works, "gather.pitch"), nullptr);
            EXPECT_EQ(byId(w.last.works, "gather.bog_iron"), nullptr);

            ASSERT_TRUE(w.act("seek", "light", 4));
            w.answer();
            ASSERT_EQ(w.screen(), "fight");
            EXPECT_EQ(w.state.foes.size(), 4u);

            // Known 4 of 4: the pack of hounds, the leech mass, bog iron, and the Lantern Mother,
            // a boss met by choice
            Walker deep(knowing(knowing(hunter(), "quarry", 10), "marsh", 10));
            deep.age = 18.0f;

            ASSERT_TRUE(deep.ask("enter", WalkAct{"", "marsh", 0}));
            EXPECT_NE(byId(deep.last.seeks, "hound*pack"), nullptr);
            EXPECT_NE(byId(deep.last.seeks, "leech"), nullptr);
            EXPECT_NE(byId(deep.last.works, "gather.bog_iron"), nullptr);

            const ElementMap* lantern = byId(deep.last.seeks, "enc.lantern");

            ASSERT_NE(lantern, nullptr);
            EXPECT_EQ(intOf(*lantern, "boss"), 1);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // The Lantern Mother comes with two of her lights and keeps calling more as she is hurt.
        // Beaten, she gives a bead of wisp-glass and teaches the wisp: from then on it is his to bind.
        TEST(expedition_test, the_lantern_mother_teaches_the_wisp)
        {
            MockLogger logger;

            ElementMap champion = hunter();
            champion["str"] = ElementType{40};
            champion["arms"] = ElementType{10};
            champion["dex"] = ElementType{30};
            champion["vit"] = ElementType{400};
            champion["vitmax"] = ElementType{400};

            auto bindWisp = [](Walker& who) -> bool {
                std::vector<RuleActivity> activities;
                EXPECT_TRUE(who.rules.activities(18.0f, who.character, activities));

                for (const auto& a : activities)
                {
                    if (textOf(a.fields, "id") == "bind.wisp")
                        return intOf(a.fields, "listed") == 1;
                }

                return false;
            };

            auto w = bossFight(champion, "marsh", "enc.lantern");
            w->age = 18.0f;

            ASSERT_EQ(w->state.foes.size(), 3u);
            EXPECT_FALSE(bindWisp(*w));

            ASSERT_TRUE(pressUntil(*w, "strike", 40, [&w]() { return not textOf(w->last.duel, "over").empty(); }));
            ASSERT_EQ(textOf(w->last.duel, "over"), "won");

            // Hurt, she called more of her lights, never more than four standing
            EXPECT_TRUE(accountSays(*w, "lamp"));
            EXPECT_GT(w->state.foes.size(), 3u);
            EXPECT_LE(w->state.foes.size(), 4u);

            EXPECT_EQ(intOf(w->character, "know.fam.wisp"), 1);
            EXPECT_GE(intOf(w->character, "wisp_glass"), 1);
            EXPECT_EQ(intOf(w->character, "beaten.lantern"), 1);
            EXPECT_TRUE(bindWisp(*w));
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // Holds that cost nothing, thrown again and again by a quick man at a boss, weaken as
        // delays do: the boss still acts.
        TEST(expedition_test, a_boss_is_never_held_for_good)
        {
            MockLogger logger;

            ElementMap binder = hunter();
            binder["smuggler_runner"] = ElementType{1};
            binder["spellwright"] = ElementType{1};
            binder["dex"] = ElementType{30};
            binder["vit"] = ElementType{500};
            binder["vitmax"] = ElementType{500};

            auto w = bossFight(binder, "quarry", "enc.shell");

            for (int i = 0; i < 16 and textOf(w->last.duel, "over").empty(); ++i)
            {
                std::string move = "guard";

                for (const char* hold : {"tech.sand", "tech.frostbind"})
                {
                    const ElementMap* row = menuRowOf(*w, hold);

                    if (move == "guard" and row and intOf(*row, "locked") == 0)
                        move = hold;
                }

                ASSERT_TRUE(w->act("move", move));
            }

            int acted = 0;

            for (const auto& a : w->state.account)
            {
                if (textOf(a, "by") == "foe" or textOf(a, "text").find("circles") != std::string::npos)
                    ++acted;
            }

            EXPECT_GE(acted, 3);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // A beast held does not bolt: its turn is lost, running too.
        TEST(expedition_test, a_held_beast_does_not_bolt)
        {
            MockLogger logger;

            ElementMap trapper = knowing(hunter(), "hollow", 10);
            trapper["snare_set"] = ElementType{1};
            trapper["str"] = ElementType{17};
            trapper["dex"] = ElementType{30};

            Walker w(trapper);

            ASSERT_TRUE(w.ask("enter", WalkAct{"", "hollow", 0}));
            ASSERT_TRUE(w.act("trap"));
            answerZoneEvents(w);
            ASSERT_TRUE(w.act("seek", "stag", 1));
            answerZoneEvents(w);
            ASSERT_EQ(textOf(w.last.card, "id"), "approach");
            ASSERT_TRUE(w.act("option", "trap"));
            ASSERT_EQ(w.screen(), "fight");
            ASSERT_EQ(intOf(w.state.foes[0], "held"), 1);

            ASSERT_TRUE(w.act("move", "strike"));

            EXPECT_FALSE(accountSays(w, "bolts"));
            EXPECT_TRUE(accountSays(w, "strains"));
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // A way deals nothing and rolls nothing: what it would do says neither a hit nor a blow.
        TEST(expedition_test, a_way_says_no_hit_and_no_blow)
        {
            MockLogger logger;

            ElementMap watchman = hunter();
            watchman["watch_known"] = ElementType{1};

            auto w = boarFight(watchman);

            ASSERT_TRUE(w->act("pick", "tech.brace"));
            EXPECT_EQ(textOf(w->last.preview, "hits"), "");
            EXPECT_EQ(textOf(w->last.preview, "deals"), "");
            EXPECT_EQ(textOf(w->last.preview, "odds").find("A miss"), std::string::npos);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // Fleeing a boss ends the walk and loses what it gathered: the button says so.
        TEST(expedition_test, fleeing_a_boss_says_the_walk_is_lost)
        {
            MockLogger logger;

            ElementMap tough = hunter();
            tough["vit"] = ElementType{300};
            tough["vitmax"] = ElementType{300};

            auto w = bossFight(tough, "hollow", "enc.grey");

            ASSERT_NE(menuRowOf(*w, "retreat"), nullptr);
            EXPECT_NE(textOf(*menuRowOf(*w, "retreat"), "note").find("LOST"), std::string::npos);

            ASSERT_TRUE(w->act("pick", "retreat"));
            EXPECT_NE(textOf(w->last.preview, "text").find("lost"), std::string::npos);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // Handed over in a boss fight he would not survive, his stance runs: the haul is lost,
        // not the life.
        TEST(expedition_test, the_stance_runs_from_a_boss_it_cannot_survive)
        {
            MockLogger logger;

            ElementMap frail = hunter();
            frail["vit"] = ElementType{5};
            frail["remedy"] = ElementType{0};

            auto w = bossFight(frail, "hollow", "enc.grey");

            ASSERT_TRUE(w->act("step"));
            EXPECT_EQ(textOf(w->last.duel, "over"), "fled");
            EXPECT_GT(intOf(w->character, "vit"), 0);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // A record walked again passes a boss fight by, and what that fight used is not asked.
        TEST(expedition_test, a_record_does_not_charge_for_a_boss_fight)
        {
            MockLogger logger;

            Walker w(knowing(hunter(), "hollow", 10));

            WalkRecipe kept;
            kept.id = "r1";
            kept.fields = {{"zone", ElementType{std::string("hollow")}}, {"name", ElementType{std::string("The Hollow Woods")}}, {"kind", ElementType{std::string("hunt")}},
                           {"steps", ElementType{1}}, {"ap", ElementType{3}}, {"hp", ElementType{0}}, {"months", ElementType{3}}, {"walkedAt", ElementType{180}},
                           {"kills", ElementType{std::string("")}}, {"downed", ElementType{0}}};
            kept.steps = {{{"n", ElementType{1}}, {"kind", ElementType{std::string("fight")}}, {"target", ElementType{std::string("enc.grey")}}, {"count", ElementType{3}},
                           {"ap", ElementType{3}}, {"hp", ElementType{0}}, {"once", ElementType{1}}}};
            kept.marks = {{{"step", ElementType{1}}, {"kind", ElementType{std::string("use")}}, {"id", ElementType{std::string("flask")}}, {"n", ElementType{2}}}};

            w.recipes.push_back(kept);

            ASSERT_TRUE(w.ask("begin", WalkAct{}, "r1"));
            EXPECT_EQ(w.last.blocked, "");
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // The Hidden Hand's run is on the beast's panel: his hits on it since its last turn. And a
        // fire flask is in the bag he carries.
        TEST(expedition_test, the_run_of_hits_and_the_flasks_are_shown)
        {
            MockLogger logger;

            ElementMap quick = hunter();
            quick["thrust_learned"] = ElementType{1};
            quick["dex"] = ElementType{20};
            quick["flask"] = ElementType{1};

            auto w = boarFight(quick);

            bool flasks = false;

            for (const auto& b : w->last.bag)
            {
                if (textOf(b, "id") == "flask")
                    flasks = true;
            }

            EXPECT_TRUE(flasks);

            ASSERT_TRUE(pressUntil(*w, "tech.thrust", 3, [&w]() { return intOf(w->state.foes[0], "hits") >= 1; }));
            EXPECT_NE(textOf(w->last.targets[0], "life").find("RUN 1"), std::string::npos) << textOf(w->last.targets[0], "life");
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // The builds to try a fight with (--build): a boy of each early path, a man of each class
        // and a plain boy, each with his age, what he holds and every place known through and through.
        TEST(expedition_test, the_builds_to_try_fights_with)
        {
            MockLogger logger;
            Walker w(hunter());

            ASSERT_TRUE(w.ask("builds"));
            ASSERT_EQ(w.last.builds.size(), 11u);

            const ElementMap* keep = byId(w.last.builds, "keep");

            ASSERT_NE(keep, nullptr);
            EXPECT_EQ(intOf(*keep, "age"), 25);
            EXPECT_FALSE(textOf(*keep, "name").empty());
            EXPECT_FALSE(textOf(*keep, "about").empty());
            EXPECT_EQ(intOf(*keep, "stat.keep_oath"), 1);
            EXPECT_EQ(intOf(*keep, "stat.vit"), intOf(*keep, "stat.vitmax"));
            EXPECT_EQ(intOf(*keep, "know.study.hollow"), 10);
            EXPECT_EQ(intOf(*keep, "know.visit.quarry"), 1);

            for (const char* id : {"plain", "blade", "scholar", "street", "hedge", "collegium", "collegium.dropout", "spellblade", "hand", "greenwood"})
                EXPECT_NE(byId(w.last.builds, id), nullptr) << id;

            // The Collegium's Firebolt is its fire course's: the scholar, the drop-out and the
            // spell-blade of the Keep who left it all have it; the drop-out has not the class
            for (const char* id : {"collegium", "collegium.dropout", "spellblade"})
            {
                ASSERT_NE(byId(w.last.builds, id), nullptr) << id;
                EXPECT_EQ(intOf(*byId(w.last.builds, id), "stat.fire_course"), 1) << id;
            }

            EXPECT_EQ(intOf(*byId(w.last.builds, "collegium.dropout"), "stat.collegium"), 0);
            EXPECT_EQ(intOf(*byId(w.last.builds, "collegium.dropout"), "stat.dropout.collegium"), 1);
            EXPECT_EQ(intOf(*byId(w.last.builds, "spellblade"), "stat.keep_oath"), 1);
            EXPECT_EQ(intOf(*byId(w.last.builds, "spellblade"), "stat.dropout.collegium"), 1);

            // Asked anything else, the walk says nothing of them
            ASSERT_TRUE(w.ask("enter", WalkAct{"", "hollow", 0}));
            EXPECT_TRUE(w.last.builds.empty());
        }

        namespace
        {
            // The marks of the timeline for one move ("" as it stands): {kind, n, at, text, tone}
            std::vector<ElementMap> timelineOf(const Walker& w, const std::string& move)
            {
                std::vector<ElementMap> marks;

                for (const auto& t : w.last.timeline)
                {
                    if (textOf(t, "move") == move)
                        marks.push_back(t);
                }

                return marks;
            }

            // Where his next turn falls on the gauge, 0 to 1000; -1 for none
            int hisNextAt(const std::vector<ElementMap>& marks)
            {
                for (const auto& m : marks)
                {
                    if (textOf(m, "kind") == "next")
                        return intOf(m, "at");
                }

                return -1;
            }

            bool hasKind(const std::vector<ElementMap>& marks, const std::string& kind, const std::string& tone = "")
            {
                for (const auto& m : marks)
                {
                    if (textOf(m, "kind") == kind and (tone.empty() or textOf(m, "tone") == tone))
                        return true;
                }

                return false;
            }
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // The timeline is a gauge from now to his next turn: how long each move he may make takes
        // (a quick one less, a heavy one more), the beasts that act before it, the next one after it
        // faint, a hold putting the beast's turn off, and the end of the day on the turn that ends it.
        TEST(expedition_test, the_timeline_shows_where_each_move_puts_him)
        {
            MockLogger logger;

            ElementMap sly = hunter();
            sly["thrust_learned"] = ElementType{1};
            sly["smuggler_runner"] = ElementType{1};
            sly["levy_served"] = ElementType{1};

            auto w = boarFight(sly);

            const auto now = timelineOf(*w, "");

            ASSERT_FALSE(now.empty());
            ASSERT_GT(hisNextAt(now), 0);
            EXPECT_LT(hisNextAt(now), 1000);
            EXPECT_TRUE(hasKind(now, "foe"));
            EXPECT_TRUE(hasKind(now, "warn"));
            EXPECT_FALSE(hasKind(now, "day"));

            for (const auto& m : now)
                EXPECT_LE(intOf(m, "at"), 1000);

            EXPECT_FALSE(textOf(w->last.duel, "dayText").empty());

            // A quick move takes less time, a heavy one more
            EXPECT_LT(hisNextAt(timelineOf(*w, "tech.thrust")), hisNextAt(now));

            // A hold: the beast's next turn is lost, and said so
            EXPECT_TRUE(hasKind(timelineOf(*w, "tech.sand"), "foe", "held"));

            // The turn that ends his day has the day's line where his next turn falls
            for (int i = 0; i < 6 and not hasKind(timelineOf(*w, ""), "day") and textOf(w->last.duel, "over").empty(); ++i)
                ASSERT_TRUE(w->act("move", "guard"));

            const auto last = timelineOf(*w, "");

            ASSERT_TRUE(hasKind(last, "day"));

            for (const auto& m : last)
            {
                if (textOf(m, "kind") == "day")
                    EXPECT_EQ(intOf(m, "at"), hisNextAt(last));
            }
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        namespace
        {
            // The ends of a range as the fight page writes it, "11–14" or "3"
            std::pair<int, int> rangeOf(const std::string& text)
            {
                const std::string dash = "\xE2\x80\x93";
                const size_t at = text.find(dash);

                if (at == std::string::npos)
                    return {std::stoi(text), std::stoi(text)};

                return {std::stoi(text.substr(0, at)), std::stoi(text.substr(at + dash.size()))};
            }

            // A hunter in a fight with one wolf whose bite he knows, his strike looked at
            std::unique_ptr<Walker> wolfFight(ElementMap who)
            {
                who["know.move.wolf.bite"] = ElementType{1};

                auto w = std::make_unique<Walker>(who);

                EXPECT_TRUE(w->ask("enter", WalkAct{"", "hollow", 0}));
                EXPECT_TRUE(w->act("seek", "wolf", 1));
                w->answer();
                EXPECT_EQ(w->screen(), "fight");
                EXPECT_TRUE(w->act("pick", "strike"));

                return w;
            }

            std::string heroValue(const Walker& w, const std::string& id)
            {
                const ElementMap* row = byId(w.last.hero, id);

                return row ? textOf(*row, "value") : std::string();
            }
        }

        // What the smithy forges goes with him into a fight: the iron blade is four of Might, two
        // more on each end of a blow (Might counts half in a blow, as Arms counts twice in
        // Might), and the jerkin's armour is two off each bite.
        TEST(expedition_test, gear_adds_to_a_blow)
        {
            MockLogger logger;

            ElementMap armed = hunter();
            armed["gear.iron_blade"] = ElementType{1};
            armed["gear.jerkin"] = ElementType{1};

            auto plain = wolfFight(hunter());
            auto geared = wolfFight(armed);

            EXPECT_EQ(heroValue(*plain, "might"), "15");
            EXPECT_EQ(heroValue(*geared, "might"), "19");

            const auto bare = rangeOf(textOf(plain->last.preview, "deals"));
            const auto edged = rangeOf(textOf(geared->last.preview, "deals"));

            EXPECT_EQ(edged.first, bare.first + 2);
            EXPECT_EQ(edged.second, bare.second + 2);

            // The wolf's bite, said on its panel: the jerkin takes two off each end, never below 1
            ASSERT_FALSE(plain->last.targets.empty());
            ASSERT_FALSE(geared->last.targets.empty());
            ASSERT_EQ(textOf(plain->last.targets[0], "intent"), "BITE");
            ASSERT_EQ(textOf(geared->last.targets[0], "intent"), "BITE");

            EXPECT_EQ(textOf(plain->last.targets[0], "moveEffect"), "\xE2\x88\x92" "2\xE2\x80\x93" "4 vitality if it lands");
            EXPECT_EQ(textOf(geared->last.targets[0], "moveEffect"), "\xE2\x88\x92" "1\xE2\x80\x93" "2 vitality if it lands");
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // A record keeps the walk as it was made: walked again with the gear or without it, it
        // brings the same and costs the same. Gear is not asked of a record, and a record made
        // without it does not grow stronger once he has it.
        TEST(expedition_test, a_record_replays_the_same_with_or_without_gear)
        {
            MockLogger logger;

            ElementMap armed = knowing(hunter(), "hollow", 3);
            armed["gear.iron_blade"] = ElementType{1};
            armed["gear.jerkin"] = ElementType{1};

            const ElementMap bare = knowing(hunter(), "hollow", 3);

            // A record made with gear, and one made without it
            auto record = [](const ElementMap& who) {
                Walker w(who);

                EXPECT_TRUE(w.ask("enter", WalkAct{"", "hollow", 0}));
                EXPECT_TRUE(w.act("seek", "wolf", 1));
                w.fight();
                EXPECT_TRUE(w.act("gather", "ashwood"));
                w.answer();
                EXPECT_TRUE(w.act("leave"));
                EXPECT_TRUE(w.act("leave"));
                EXPECT_EQ(w.screen(), "done");

                w.keep("r1");

                return w.recipes;
            };

            // Walked again by the record, on the same dice: what he comes home with, and his Vitality
            auto replayed = [](const ElementMap& who, const std::vector<WalkRecipe>& recipes) {
                Walker w(who);
                w.recipes = recipes;
                w.seed = 700;

                EXPECT_TRUE(w.ask("begin", {}, "r1"));
                EXPECT_TRUE(w.last.blocked.empty()) << w.last.blocked;
                EXPECT_TRUE(w.ask("term", {}, "r1"));

                std::string brought;

                for (const char* id : {"pelts", "fangs", "ashwood", "rations", "remedy", "vit", "walked.hollow", "know.fights.wolf"})
                    brought += std::string(id) + "=" + std::to_string(intOf(w.character, id)) + ";";

                return brought;
            };

            for (const auto& made : {armed, bare})
            {
                const auto recipes = record(made);

                ASSERT_EQ(recipes.size(), 1u);

                const std::string with = replayed(armed, recipes);
                const std::string without = replayed(bare, recipes);

                EXPECT_EQ(with, without);
                EXPECT_NE(with.find("walked.hollow=1"), std::string::npos) << with;
            }
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // A marsh light's sear is a working: the warded charm takes two off each end of it. A
        // bite of the body is none of the charm's business: a wolf's is the same with it or without it.
        TEST(expedition_test, the_charm_wards_a_working_and_not_a_bite)
        {
            MockLogger logger;

            auto lightFight = [](ElementMap who) {
                who["know.move.light.bite"] = ElementType{1};

                auto w = std::make_unique<Walker>(who);
                w->age = 18.0f;

                EXPECT_TRUE(w->ask("enter", WalkAct{"", "marsh", 0}));
                EXPECT_TRUE(w->act("seek", "light", 1));
                w->answer();
                EXPECT_EQ(w->screen(), "fight");

                return w;
            };

            ElementMap warded = hunter();
            warded["gear.charm"] = ElementType{1};

            auto plain = lightFight(hunter());
            auto charmed = lightFight(warded);

            ASSERT_FALSE(plain->last.targets.empty());
            ASSERT_FALSE(charmed->last.targets.empty());

            // Its sear, 4-6 less the one his guard takes: 3-5, and with the charm 1-3
            EXPECT_EQ(textOf(plain->last.targets[0], "moveEffect"), "\xE2\x88\x92" "3\xE2\x80\x93" "5 vitality if it lands");
            EXPECT_EQ(textOf(charmed->last.targets[0], "moveEffect"), "\xE2\x88\x92" "1\xE2\x80\x93" "3 vitality if it lands");

            // A wolf's bite is untouched by it
            auto bare = wolfFight(hunter());
            auto wolfWarded = wolfFight(warded);

            ASSERT_FALSE(bare->last.targets.empty());
            ASSERT_FALSE(wolfWarded->last.targets.empty());
            EXPECT_EQ(textOf(wolfWarded->last.targets[0], "moveEffect"), textOf(bare->last.targets[0], "moveEffect"));
            EXPECT_EQ(textOf(wolfWarded->last.targets[0], "moveEffect"), "\xE2\x88\x92" "2\xE2\x80\x93" "4 vitality if it lands");
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // The warded charm waits for the world to know what a marsh light's cold fire does: its
        // forge row is gated on the literal key "know.move.light.bite". A real fight with a light
        // sets that key once the light has used its cold fire on him, landed or dodged, and the
        // charm comes to the smithy's list with it.
        TEST(expedition_test, a_marsh_light_fight_opens_the_charm)
        {
            MockLogger logger;

            Walker w(hunter());
            w.age = 18.0f;

            auto charmListed = [&w]() {
                ElementMap who = w.character;
                who["town_known"] = ElementType{1};
                who["town.smithy"] = ElementType{2};
                who["gems"] = ElementType{3};
                who["water"] = ElementType{10};

                std::vector<RuleActivity> activities;
                EXPECT_TRUE(w.rules.activities(w.age, who, activities)) << (w.rules.errors.empty() ? "" : w.rules.errors.front());

                for (const auto& a : activities)
                {
                    if (textOf(a.fields, "id") == "forge.charm")
                        return intOf(a.fields, "listed") == 1;
                }

                return false;
            };

            EXPECT_FALSE(charmListed());

            ASSERT_TRUE(w.ask("enter", WalkAct{"", "marsh", 0}));
            ASSERT_TRUE(w.act("seek", "light", 1));
            w.answer();
            ASSERT_EQ(w.screen(), "fight");
            EXPECT_EQ(intOf(w.character, "know.move.light.bite"), 0);

            // He guards until the light has used its cold fire on him
            bool seen = false;

            for (int turn = 0; turn < 12 and intOf(w.character, "know.move.light.bite") == 0 and textOf(w.last.duel, "over").empty(); ++turn)
            {
                ASSERT_FALSE(w.last.targets.empty());

                if (textOf(w.last.targets[0], "moveName") == "Cold fire")
                    seen = true;

                ASSERT_TRUE(w.act("move", "guard"));
            }

            EXPECT_TRUE(seen);
            ASSERT_EQ(intOf(w.character, "know.move.light.bite"), 1);

            // The world knows it now, and the smithy forges the charm
            EXPECT_TRUE(charmListed());
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // The outskirts teach a boy three first moves, one for each path (outskirts.children,
        // outskirts.nature, outskirts.animals): he has the one his path gave and none of the others.
        TEST(expedition_test, the_outskirts_teach_three_moves)
        {
            MockLogger logger;

            auto menuOf = [](const std::string& flag) {
                ElementMap boy = knowing(gatherer(), "hedgerows", 10);

                if (not flag.empty())
                    boy[flag] = ElementType{1};

                auto w = std::make_unique<Walker>(boy);
                w->age = 10.0f;

                EXPECT_TRUE(w->ask("enter", WalkAct{"", "millpond", 0}));
                EXPECT_TRUE(w->act("seek", "rat", 1));
                w->answer();
                EXPECT_EQ(w->screen(), "fight");

                return std::vector<bool>{byId(w->last.menu, "tech.step") != nullptr, byId(w->last.menu, "tech.read") != nullptr,
                                         byId(w->last.menu, "tech.stick") != nullptr};
            };

            EXPECT_EQ(menuOf(""), (std::vector<bool>{false, false, false}));
            EXPECT_EQ(menuOf("outskirts.children"), (std::vector<bool>{true, false, false}));
            EXPECT_EQ(menuOf("outskirts.nature"), (std::vector<bool>{false, true, false}));
            EXPECT_EQ(menuOf("outskirts.animals"), (std::vector<bool>{false, false, true}));
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // What a move of his says of itself, and the note under it, say two things: the note is
        // not the line said again (Read the beast said "known 3 of 4 for this fight" twice).
        TEST(expedition_test, a_move_says_its_note_once)
        {
            MockLogger logger;

            ElementMap boy = knowing(gatherer(), "hedgerows", 10);

            for (const char* flag : {"outskirts.children", "outskirts.nature", "outskirts.animals"})
                boy[flag] = ElementType{1};

            auto w = std::make_unique<Walker>(boy);
            w->age = 10.0f;

            ASSERT_TRUE(w->ask("enter", WalkAct{"", "millpond", 0}));
            ASSERT_TRUE(w->act("seek", "rat", 1));
            w->answer();
            ASSERT_EQ(w->screen(), "fight");

            auto lower = [](std::string s) {
                std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
                return s;
            };

            int seen = 0;

            for (const auto& row : w->last.menu)
            {
                const std::string text = lower(textOf(row, "text"));
                const std::string note = lower(textOf(row, "note"));

                if (note.empty())
                    continue;

                ++seen;
                EXPECT_EQ(text.find(note), std::string::npos) << textOf(row, "id") << ": " << textOf(row, "text");
            }

            EXPECT_GE(seen, 3);
        }
    }
}
