#include "stdafx.h"

#include <algorithm>
#include <functional>
#include <map>
#include <cctype>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "Core/rulevm.h"
#include "Core/walk.h"

#include "ECS/entitysystem.h"

#include "mocklogger.h"

#include "compiledrules.h"

using namespace chronicle;

// A story: a scene played day by day, its tiles offered by the day and by what he chose before,
// as the rules play it (rules/expedition.pg and storylib.pg, asked through Rules::walk the way
// the Life screen asks a walk). The tests' story is `trial` (storytable.pg): four days for four
// months, a lecture for a coin, the library for nothing, an advanced course from the third day
// for who was taught twice, an early tile of the first two days, and a tile taken once.
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

            // A youth of sixteen with a little coin put by
            ElementMap student()
            {
                return {{"coin", ElementType{5}}, {"vit", ElementType{40}}, {"vitmax", ElementType{40}}, {"int", ElementType{12}}};
            }

            // One who plays a story: the rules, his character and the scene, each answer taken as the
            // Life screen takes it (the stats become his, the scene becomes the walk as it stands).
            // The Walker of expedition.cc.
            struct Walker
            {
                EntitySystem ecs;
                Rules rules;
                ElementMap character;
                float age = 16.0f;
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

                // A tile taken, as the page sends it: its id is the act's kind
                bool take(const std::string& tile)
                {
                    return act("tile." + tile);
                }

                std::string screen() const { return textOf(last.site, "screen"); }

                // The card up, "story.<id>" for a story's own, "" when none
                std::string cardId() const { return screen() == "event" ? textOf(last.card, "id") : std::string(); }

                // The card up answered with its first answer that can be given
                bool answerFirst()
                {
                    for (const auto& o : last.options)
                    {
                        if (intOf(o, "locked") == 0)
                            return act("option", textOf(o, "id"));
                    }

                    ADD_FAILURE() << "no answer can be given to " << cardId();
                    return false;
                }

                // The story's chance meeting, when one came at the start of the day, answered
                void settle()
                {
                    for (int guard = 0; guard < 4 and cardId() == "story.rumour"; ++guard)
                        answerFirst();
                }

                // Every card that waits answered with its first answer, till the tiles are back, the
                // scene is over, or the card `until` is up
                void answerAll(const std::string& until = "")
                {
                    for (int guard = 0; guard < 8 and screen() == "event" and (until.empty() or cardId() != until); ++guard)
                        answerFirst();
                }

                // A tile taken, then the chance meeting answered if one came
                bool spend(const std::string& tile)
                {
                    const bool ok = take(tile);

                    settle();

                    return ok;
                }

                int tilesTaken() const
                {
                    int n = 0;

                    for (const auto& step : state.record)
                    {
                        if (textOf(step, "kind") == "tile")
                            ++n;
                    }

                    return n;
                }

                const ElementMap* option(const std::string& id) const { return byId(last.options, id); }

                const ElementMap* work(const std::string& tile) const { return byId(last.works, "tile." + tile); }

                int day() const { return intOf(state.run, "day"); }
                int mark(const std::string& key) const { return intOf(state.run, "m." + key); }

                // A fight of the story: how it ended ("" while it goes on), the beasts still up
                std::string over() const { return textOf(state.fight, "over"); }

                int standing() const
                {
                    int n = 0;

                    for (const auto& foe : state.foes)
                    {
                        if (intOf(foe, "alive") > 0)
                            ++n;
                    }

                    return n;
                }

                // The same move made turn after turn till the fight is over (or till `until` beasts stand);
                // "step" hands the turn to his stance
                void fightWith(const std::string& move, int until = -1)
                {
                    for (int guard = 0; guard < 40 and screen() == "fight" and over().empty() and standing() != until; ++guard)
                    {
                        if (move == "step")
                            act("step");
                        else
                            act("move", move);
                    }
                }

                const ElementMap* step(const std::string& kind) const
                {
                    for (const auto& s : state.record)
                    {
                        if (textOf(s, "kind") == kind)
                            return &s;
                    }

                    return nullptr;
                }
            };

            // A mage at the level of a pass: his wits and a little of the art
            ElementMap mage()
            {
                ElementMap who = student();

                who["arcana"] = ElementType{1};

                return who;
            }
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // He arrives on the first day: the tiles of that day, and not what asks more of him.
        TEST(story_test, a_story_begins_on_its_first_day)
        {
            MockLogger logger;
            Walker w(student());

            ASSERT_TRUE(w.ask("enter", WalkAct{"story", "trial", 0}));

            EXPECT_EQ(textOf(w.state.run, "story"), "trial");
            EXPECT_EQ(textOf(w.state.run, "zone"), "");
            EXPECT_EQ(w.day(), 1);
            EXPECT_EQ(intOf(w.state.run, "ap"), 4);
            EXPECT_EQ(w.screen(), "site");

            EXPECT_NE(w.work("lecture"), nullptr);
            EXPECT_NE(w.work("library"), nullptr);
            EXPECT_NE(w.work("early"), nullptr);
            EXPECT_NE(w.work("once"), nullptr);
            EXPECT_EQ(w.work("advanced"), nullptr);

            // A story walks no zone: no beast to seek
            EXPECT_TRUE(w.last.seeks.empty());

            // No story under an unknown name
            Walker lost(student());
            MockLogger quiet;

            EXPECT_FALSE(lost.rules.walk(16.0f, lost.character, "Aldren", "enter", 1, WalkState{}, WalkAct{"story", "nowhere", 0}, {}, {}, WalkQueue{}, "", lost.last));
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // A tile is a day: taken, it gives what it gives and the next day comes.
        TEST(story_test, a_tile_taken_ends_the_day)
        {
            MockLogger logger;
            Walker w(student());

            ASSERT_TRUE(w.ask("enter", WalkAct{"story", "trial", 0}));
            ASSERT_TRUE(w.act("tile", "library"));

            EXPECT_EQ(textOf(w.last.site, "refused"), "");
            EXPECT_EQ(w.day(), 2);
            EXPECT_EQ(intOf(w.state.run, "ap"), 3);
            EXPECT_EQ(w.mark("revised"), 1);

            // (the first day is its stage's first: its line comes before the day's)
            ASSERT_EQ(w.state.record.size(), 2u);
            EXPECT_EQ(textOf(w.state.record.front(), "kind"), "stage");
            EXPECT_EQ(textOf(w.state.record.back(), "text"), "You read in the library");

            // The page's own form of it, `tile.<id>` as the act's kind, is the same (a chance meeting
            // at the start of the second day answered first, and written as a step of its own)
            w.settle();
            ASSERT_TRUE(w.take("library"));

            EXPECT_EQ(w.day(), 3);
            EXPECT_EQ(w.mark("revised"), 2);
            EXPECT_EQ(w.tilesTaken(), 2);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // What is offered moves with the days and with what he chose: the early tile goes after its
        // second day, the advanced course is shown from the third, shut until he was taught twice.
        TEST(story_test, the_offer_changes_with_the_day)
        {
            MockLogger logger;

            // Two days in the library: not taught, the course is shown and shut
            {
                Walker w(student());

                ASSERT_TRUE(w.ask("enter", WalkAct{"story", "trial", 0}));
                ASSERT_TRUE(w.spend("library"));

                EXPECT_NE(w.work("early"), nullptr);
                EXPECT_EQ(w.work("advanced"), nullptr);

                ASSERT_TRUE(w.take("library"));

                // The third day's cards, answered first
                w.answerAll();

                EXPECT_EQ(w.day(), 3);
                EXPECT_EQ(w.screen(), "site");
                EXPECT_EQ(w.work("early"), nullptr);

                ASSERT_NE(w.work("advanced"), nullptr);
                EXPECT_EQ(intOf(*w.work("advanced"), "locked"), 1);
                EXPECT_FALSE(textOf(*w.work("advanced"), "asks").empty());

                // Shut, it is refused and the day does not pass
                ASSERT_TRUE(w.take("advanced"));
                EXPECT_FALSE(textOf(w.last.site, "refused").empty());
                EXPECT_EQ(w.day(), 3);
                EXPECT_EQ(w.mark("deep"), 0);
            }

            // Two lectures: taught twice, the course is open on the third day
            {
                Walker w(student());

                ASSERT_TRUE(w.ask("enter", WalkAct{"story", "trial", 0}));
                ASSERT_TRUE(w.spend("lecture"));
                ASSERT_TRUE(w.take("lecture"));
                w.answerAll();

                EXPECT_EQ(w.mark("taught"), 2);

                ASSERT_NE(w.work("advanced"), nullptr);
                EXPECT_EQ(intOf(*w.work("advanced"), "locked"), 0);
                EXPECT_EQ(textOf(*w.work("advanced"), "asks"), "");

                ASSERT_TRUE(w.take("advanced"));
                EXPECT_EQ(textOf(w.last.site, "refused"), "");
                EXPECT_EQ(w.mark("deep"), 1);
                EXPECT_EQ(intOf(w.character, "coin"), 2);
            }
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // A course is paid the day it is followed; without the coin it is shut, saying what it costs.
        TEST(story_test, a_tile_charges_its_fee_when_taken)
        {
            MockLogger logger;

            {
                Walker w(student());

                ASSERT_TRUE(w.ask("enter", WalkAct{"story", "trial", 0}));

                EXPECT_EQ(intOf(*w.work("lecture"), "locked"), 0);

                ASSERT_TRUE(w.take("lecture"));

                EXPECT_EQ(intOf(w.last.after, "coin"), 4);
                EXPECT_EQ(w.mark("taught"), 1);
            }

            {
                ElementMap poor = student();
                poor["coin"] = ElementType{0};

                Walker w(poor);

                ASSERT_TRUE(w.ask("enter", WalkAct{"story", "trial", 0}));

                ASSERT_NE(w.work("lecture"), nullptr);
                EXPECT_EQ(intOf(*w.work("lecture"), "locked"), 1);
                EXPECT_NE(textOf(*w.work("lecture"), "asks").find("COIN"), std::string::npos) << textOf(*w.work("lecture"), "asks");

                ASSERT_TRUE(w.act("tile", "lecture"));

                EXPECT_FALSE(textOf(w.state.run, "refused").empty());
                EXPECT_EQ(w.day(), 1);
                EXPECT_EQ(intOf(w.character, "coin"), 0);
                EXPECT_EQ(w.mark("taught"), 0);
                EXPECT_EQ(w.tilesTaken(), 0);
            }
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // A tile that may be taken once is gone once taken.
        TEST(story_test, a_tile_taken_most_times_leaves)
        {
            MockLogger logger;
            Walker w(student());

            ASSERT_TRUE(w.ask("enter", WalkAct{"story", "trial", 0}));
            ASSERT_NE(w.work("once"), nullptr);

            ASSERT_TRUE(w.spend("once"));

            EXPECT_EQ(w.day(), 2);
            EXPECT_EQ(w.work("once"), nullptr);

            // Asked again, it is refused
            ASSERT_TRUE(w.take("once"));
            EXPECT_FALSE(textOf(w.last.site, "refused").empty());
            EXPECT_EQ(w.day(), 2);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // Without a coin there is still a day's work for him, every day the tiles are offered (the
        // fourth is the examination's: having heard no lecture, he is sent away before it).
        TEST(story_test, a_day_always_offers_a_free_tile)
        {
            MockLogger logger;

            ElementMap poor = student();
            poor["coin"] = ElementType{0};

            Walker w(poor);

            ASSERT_TRUE(w.ask("enter", WalkAct{"story", "trial", 0}));

            for (int day = 1; day <= 3; ++day)
            {
                w.answerAll();

                ASSERT_EQ(w.day(), day);
                ASSERT_EQ(w.screen(), "site");

                std::string open;

                for (const auto& row : w.last.works)
                {
                    if (open.empty() and intOf(row, "locked") == 0)
                        open = textOf(row, "id");
                }

                ASSERT_FALSE(open.empty()) << "day " << day;

                ASSERT_TRUE(w.act(open));
            }

            EXPECT_EQ(w.cardId(), "story.sent_away");

            w.answerAll();

            EXPECT_EQ(textOf(w.state.run, "mode"), "done");
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // The days are worth the months: four days for four months, a month falls due at the end of
        // each, the examination's day as well. The rules count both, from the day; a look counts
        // nothing, nor does an answer to a card.
        TEST(story_test, months_fall_due_as_days_pass)
        {
            MockLogger logger;
            Walker w(student());

            ASSERT_TRUE(w.ask("enter", WalkAct{"story", "trial", 0}));

            EXPECT_EQ(intOf(w.state.run, "months"), 4);
            EXPECT_EQ(intOf(w.state.run, "days"), 4);
            EXPECT_EQ(intOf(w.state.run, "monthsDue"), 0);
            EXPECT_EQ(intOf(w.state.run, "monthsRun"), 0);

            const char* tiles[] = {"lecture", "library", "library"};

            for (int day = 1; day <= 4; ++day)
            {
                // The fourth day is the examination
                if (day < 4)
                {
                    ASSERT_TRUE(w.take(tiles[day - 1]));
                }
                else
                {
                    w.answerAll("story.exam");
                    ASSERT_EQ(w.cardId(), "story.exam");
                    ASSERT_TRUE(w.act("option", "plain"));
                }

                EXPECT_EQ(intOf(w.state.run, "monthsDue"), 1) << "day " << day;
                EXPECT_EQ(intOf(w.state.run, "monthsRun"), day) << "day " << day;

                // A look runs no month, and moves neither count
                ASSERT_TRUE(w.ask("look"));

                EXPECT_EQ(intOf(w.last.state.run, "monthsDue"), 1);
                EXPECT_EQ(intOf(w.last.state.run, "monthsRun"), day);

                // A card answered ends no day
                if (w.screen() == "event" and w.cardId() != "story.exam")
                {
                    ASSERT_TRUE(w.answerFirst());

                    EXPECT_EQ(intOf(w.state.run, "monthsDue"), 0) << "day " << day;
                    EXPECT_EQ(intOf(w.state.run, "monthsRun"), day) << "day " << day;
                }
            }

            EXPECT_EQ(textOf(w.state.run, "mode"), "done");

            // An act that ends no day has no month due
            Walker idle(student());

            ASSERT_TRUE(idle.ask("enter", WalkAct{"story", "trial", 0}));
            ASSERT_TRUE(idle.take("library"));
            EXPECT_EQ(intOf(idle.state.run, "monthsDue"), 1);

            idle.settle();
            ASSERT_TRUE(idle.act("leave"));
            EXPECT_EQ(intOf(idle.state.run, "monthsDue"), 0);
            EXPECT_EQ(intOf(idle.state.run, "monthsRun"), 1);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // The last day ends the scene (its examination, here), and nothing of it is kept as a record.
        TEST(story_test, the_last_day_ends_the_scene)
        {
            MockLogger logger;
            Walker w(student());

            ASSERT_TRUE(w.ask("enter", WalkAct{"story", "trial", 0}));

            const char* tiles[] = {"lecture", "library", "library"};

            for (int day = 1; day <= 3; ++day)
            {
                w.answerAll();
                ASSERT_TRUE(w.take(tiles[day - 1]));
                EXPECT_EQ(textOf(w.state.run, "mode"), "site");
            }

            w.answerAll("story.exam");
            ASSERT_EQ(w.cardId(), "story.exam");
            ASSERT_TRUE(w.act("option", "plain"));

            EXPECT_EQ(textOf(w.state.run, "mode"), "done");
            EXPECT_EQ(w.screen(), "done");
            EXPECT_EQ(textOf(w.last.recipe, "zone"), "");
            EXPECT_EQ(textOf(w.state.run, "result"), "pass");
            EXPECT_EQ(w.mark("revised"), 2);

            // No walk of a zone was written to him
            for (const auto& [key, value] : w.character)
                EXPECT_NE(key.rfind("walked.", 0), 0u) << key;

            // Done, nothing more is taken
            ASSERT_TRUE(w.take("library"));
            EXPECT_FALSE(textOf(w.state.run, "refused").empty());
            EXPECT_EQ(w.mark("revised"), 2);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // Leaving is asked twice, and what the scene gave and took stays as it is.
        TEST(story_test, leaving_is_asked_twice_and_keeps_what_was_given)
        {
            MockLogger logger;
            Walker w(student());

            ASSERT_TRUE(w.ask("enter", WalkAct{"story", "trial", 0}));
            ASSERT_TRUE(w.spend("lecture"));
            ASSERT_EQ(w.day(), 2);

            ASSERT_TRUE(w.act("leave"));

            EXPECT_EQ(textOf(w.state.run, "mode"), "site");
            EXPECT_EQ(intOf(w.state.run, "leaving"), 1);
            EXPECT_EQ(w.day(), 2);

            ASSERT_TRUE(w.act("leave"));

            EXPECT_EQ(textOf(w.state.run, "mode"), "done");
            EXPECT_EQ(w.screen(), "done");
            EXPECT_EQ(textOf(w.state.run, "result"), "left");

            // The coin paid stays paid, the lecture stays heard
            EXPECT_EQ(intOf(w.last.after, "coin"), 4);
            EXPECT_EQ(intOf(w.character, "coin"), 4);
            EXPECT_EQ(w.mark("taught"), 1);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // Leaving a story he can drop out of makes him a drop-out of it, keeping what it gave.
        TEST(story_test, leaving_a_class_story_is_a_dropout)
        {
            MockLogger logger;
            Walker w(student());

            ASSERT_TRUE(w.ask("enter", WalkAct{"story", "trial", 0}));
            ASSERT_TRUE(w.spend("lecture"));

            EXPECT_EQ(intOf(w.character, "dropout.trial"), 0);

            ASSERT_TRUE(w.act("leave"));
            EXPECT_EQ(intOf(w.character, "dropout.trial"), 0);

            ASSERT_TRUE(w.act("leave"));

            EXPECT_EQ(textOf(w.state.run, "result"), "left");
            EXPECT_EQ(intOf(w.last.after, "dropout.trial"), 1);
            EXPECT_EQ(intOf(w.character, "coin"), 4);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // The third day's card comes on the third day, with its four answers; an answer is not a
        // day: the card shuts and the day's tiles are there again.
        TEST(story_test, a_card_opens_on_its_day)
        {
            MockLogger logger;
            Walker w(student());

            ASSERT_TRUE(w.ask("enter", WalkAct{"story", "trial", 0}));
            ASSERT_TRUE(w.spend("lecture"));

            EXPECT_EQ(w.screen(), "site");

            ASSERT_TRUE(w.take("lecture"));

            EXPECT_EQ(w.day(), 3);
            ASSERT_EQ(w.screen(), "event");
            EXPECT_EQ(w.cardId(), "story.crossroads");
            EXPECT_EQ(textOf(w.state.run, "event"), "story.crossroads");
            EXPECT_FALSE(textOf(w.last.card, "title").empty());
            ASSERT_EQ(w.last.options.size(), 4u);

            for (const char* id : {"a", "b", "c", "d"})
            {
                ASSERT_NE(w.option(id), nullptr) << id;
                EXPECT_EQ(intOf(*w.option(id), "locked"), 0) << id;
                EXPECT_FALSE(textOf(*w.option(id), "label").empty()) << id;
            }

            // A tile is refused while the card waits
            ASSERT_TRUE(w.take("library"));
            EXPECT_FALSE(textOf(w.last.site, "refused").empty());
            EXPECT_EQ(w.mark("revised"), 0);

            ASSERT_TRUE(w.act("option", "b"));

            EXPECT_EQ(textOf(w.last.site, "refused"), "");
            EXPECT_EQ(w.mark("b"), 1);
            EXPECT_EQ(w.mark("a"), 0);
            EXPECT_EQ(w.screen(), "site");
            EXPECT_EQ(textOf(w.state.run, "event"), "");
            EXPECT_EQ(w.day(), 3);
            EXPECT_NE(w.work("library"), nullptr);
            EXPECT_EQ(intOf(*w.work("library"), "locked"), 0);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // A card that waits on a mark opens at the start of the first day it is met, before that
        // day's own card; each opens once.
        TEST(story_test, a_card_opens_when_its_mark_is_met)
        {
            MockLogger logger;
            Walker w(student());

            ASSERT_TRUE(w.ask("enter", WalkAct{"story", "trial", 0}));
            ASSERT_TRUE(w.spend("library"));

            EXPECT_EQ(w.screen(), "site");

            ASSERT_TRUE(w.take("library"));

            EXPECT_EQ(w.day(), 3);
            EXPECT_EQ(w.mark("revised"), 2);
            ASSERT_EQ(w.cardId(), "story.noticed");

            ASSERT_TRUE(w.answerFirst());
            ASSERT_EQ(w.cardId(), "story.crossroads");

            ASSERT_TRUE(w.answerFirst());
            EXPECT_EQ(w.screen(), "site");
            EXPECT_EQ(w.day(), 3);

            // The next day neither comes again
            ASSERT_TRUE(w.take("library"));

            EXPECT_EQ(w.day(), 4);
            EXPECT_NE(w.cardId(), "story.noticed");
            EXPECT_NE(w.cardId(), "story.crossroads");
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // A story walks no zone: the chance meetings of its days are its own, never a place's.
        TEST(story_test, story_events_come_from_the_story)
        {
            MockLogger logger;

            const std::vector<std::string> own = {"story.rumour", "story.crossroads", "story.noticed", "story.sent_away", "story.exam"};
            int rumours = 0;

            for (int s = 1; s <= 50; ++s)
            {
                Walker w(student());

                w.seed = s;

                ASSERT_TRUE(w.ask("enter", WalkAct{"story", "trial", 0}));

                for (int guard = 0; guard < 16 and textOf(w.state.run, "mode") != "done"; ++guard)
                {
                    if (w.screen() == "event")
                    {
                        const std::string id = w.cardId();

                        EXPECT_NE(std::find(own.begin(), own.end(), id), own.end()) << "seed " << s << ": " << id;

                        if (id == "story.rumour")
                            ++rumours;

                        ASSERT_TRUE(w.answerFirst());
                    }
                    else
                    {
                        ASSERT_TRUE(w.take(w.mark("taught") == 0 ? "lecture" : "library"));
                    }
                }

                EXPECT_EQ(textOf(w.state.run, "mode"), "done") << "seed " << s;
            }

            EXPECT_GT(rumours, 0);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // The examination counts his wits once and his days in the library twice: 12 and 2 make 16,
        // a pass, and the pass gives what it gives.
        TEST(story_test, a_test_scores_his_stats_and_marks)
        {
            MockLogger logger;
            Walker w(student());

            ASSERT_TRUE(w.ask("enter", WalkAct{"story", "trial", 0}));
            ASSERT_TRUE(w.spend("lecture"));
            ASSERT_TRUE(w.spend("library"));
            w.answerAll();
            ASSERT_TRUE(w.take("library"));

            EXPECT_EQ(w.mark("revised"), 2);

            w.answerAll("story.exam");
            ASSERT_EQ(w.cardId(), "story.exam");
            EXPECT_EQ(w.day(), 4);
            EXPECT_EQ(w.last.options.size(), 1u);

            ASSERT_TRUE(w.act("option", "plain"));

            EXPECT_EQ(intOf(w.state.run, "score.exam"), 16);
            EXPECT_EQ(textOf(w.state.run, "result"), "pass");
            EXPECT_EQ(textOf(w.state.run, "mode"), "done");
            EXPECT_EQ(intOf(w.character, "trialled"), 1);
            EXPECT_EQ(intOf(w.character, "dropout.trial"), 0);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // Answering from the old tome is there only for who holds it, and counts what it taught in
        // place of his wits.
        TEST(story_test, an_answer_appears_for_who_holds_its_flag)
        {
            MockLogger logger;

            auto toTheExam = [](Walker& w)
            {
                ASSERT_TRUE(w.ask("enter", WalkAct{"story", "trial", 0}));
                ASSERT_TRUE(w.spend("lecture"));
                ASSERT_TRUE(w.spend("library"));
                w.answerAll();
                ASSERT_TRUE(w.take("library"));
                w.answerAll("story.exam");
                ASSERT_EQ(w.cardId(), "story.exam");
            };

            {
                Walker w(student());

                toTheExam(w);

                EXPECT_NE(w.option("plain"), nullptr);
                EXPECT_EQ(w.option("tome"), nullptr);

                // Not listed, it cannot be given either
                ASSERT_TRUE(w.act("option", "tome"));
                EXPECT_FALSE(textOf(w.last.site, "refused").empty());
                EXPECT_EQ(w.cardId(), "story.exam");
            }

            {
                ElementMap reader = student();
                reader["tome"] = ElementType{1};
                reader["lore"] = ElementType{8};

                Walker w(reader);

                toTheExam(w);

                EXPECT_NE(w.option("plain"), nullptr);
                ASSERT_NE(w.option("tome"), nullptr);
                EXPECT_EQ(intOf(*w.option("tome"), "locked"), 0);

                ASSERT_TRUE(w.act("option", "tome"));

                // lore 8 in place of int 12, and the library's 2 twice
                EXPECT_EQ(intOf(w.state.run, "score.exam"), 12);
            }
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // A score below the pass is a fail: he drops out, and keeps what the days gave him.
        TEST(story_test, a_poor_score_is_a_dropout)
        {
            MockLogger logger;

            ElementMap dull = student();
            dull["int"] = ElementType{6};

            Walker w(dull);

            ASSERT_TRUE(w.ask("enter", WalkAct{"story", "trial", 0}));
            ASSERT_TRUE(w.spend("once"));
            ASSERT_TRUE(w.spend("lecture"));
            w.answerAll();
            ASSERT_TRUE(w.take("library"));
            w.answerAll("story.exam");
            ASSERT_EQ(w.cardId(), "story.exam");

            ASSERT_TRUE(w.act("option", "plain"));

            EXPECT_EQ(intOf(w.state.run, "score.exam"), 8);
            EXPECT_EQ(textOf(w.state.run, "result"), "fail");
            EXPECT_EQ(textOf(w.state.run, "mode"), "done");
            EXPECT_EQ(intOf(w.last.after, "dropout.trial"), 1);
            EXPECT_EQ(intOf(w.last.after, "trialled"), 0);

            // The dean's renown and the lecture's coin are as the days left them
            EXPECT_EQ(intOf(w.last.after, "renown"), 1);
            EXPECT_EQ(intOf(w.last.after, "coin"), 4);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // Who has heard no lecture is sent away on the examination's day: a drop-out, and the
        // examination never comes.
        TEST(story_test, too_few_courses_and_he_is_sent_away)
        {
            MockLogger logger;
            Walker w(student());

            ASSERT_TRUE(w.ask("enter", WalkAct{"story", "trial", 0}));

            std::vector<std::string> seen;

            for (int day = 1; day <= 3; ++day)
            {
                for (int guard = 0; guard < 4 and w.screen() == "event"; ++guard)
                {
                    seen.push_back(w.cardId());
                    ASSERT_TRUE(w.answerFirst());
                }

                ASSERT_TRUE(w.take("library"));
            }

            EXPECT_EQ(w.day(), 4);
            ASSERT_EQ(w.cardId(), "story.sent_away");

            ASSERT_TRUE(w.act("option", "go"));

            EXPECT_EQ(textOf(w.state.run, "mode"), "done");
            EXPECT_EQ(w.screen(), "done");
            EXPECT_EQ(textOf(w.state.run, "result"), "dropout");
            EXPECT_EQ(intOf(w.last.after, "dropout.trial"), 1);
            EXPECT_EQ(intOf(w.state.run, "sat.exam"), 0);
            EXPECT_EQ(std::find(seen.begin(), seen.end(), "story.exam"), seen.end());

            // Over, nothing more is answered
            ASSERT_TRUE(w.act("option", "plain"));
            EXPECT_FALSE(textOf(w.last.site, "refused").empty());
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // A card's day passed inside a tile of two days still comes: the third day's question is put
        // to him on the fourth, when the copying is done. And so does the line of the stage that
        // began on the third day, after the copying's own
        TEST(story_test, a_card_is_not_skipped_by_a_long_tile)
        {
            MockLogger logger;
            Walker w(student());

            ASSERT_TRUE(w.ask("enter", WalkAct{"story", "trial", 0}));
            ASSERT_TRUE(w.spend("lecture"));

            ASSERT_EQ(w.day(), 2);
            ASSERT_TRUE(w.take("copy"));
            EXPECT_EQ(textOf(w.last.site, "refused"), "");

            EXPECT_EQ(w.day(), 4);
            EXPECT_EQ(w.cardId(), "story.crossroads");

            std::vector<std::string> stages;

            for (const auto& s : w.state.record)
            {
                if (textOf(s, "kind") == "stage")
                    stages.push_back(textOf(s, "target"));
            }

            EXPECT_EQ(stages, (std::vector<std::string>{"The first days", "The last days"}));

            // Answered, the examination follows on the same day
            ASSERT_TRUE(w.act("option", "a"));
            w.settle();
            EXPECT_EQ(w.cardId(), "story.exam");
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // The practice dummy is fought on its own: no fight screen, a line of the record tells it,
        // and a win is a day on the dummies.
        TEST(story_test, a_practice_tile_is_fought_on_its_own)
        {
            MockLogger logger;

            ElementMap who = student();
            who["forbidden_read"] = ElementType{1};

            Walker w(who);

            ASSERT_TRUE(w.ask("enter", WalkAct{"story", "trial", 0}));

            ASSERT_NE(w.work("practice"), nullptr);
            EXPECT_EQ(intOf(*w.work("practice"), "locked"), 0);

            ASSERT_TRUE(w.take("practice"));
            EXPECT_EQ(textOf(w.last.site, "refused"), "");

            EXPECT_NE(w.screen(), "fight");
            EXPECT_EQ(textOf(w.state.fight, "mob"), "");
            EXPECT_EQ(textOf(w.state.run, "found"), "");
            EXPECT_EQ(w.mark("practised"), 1);
            EXPECT_EQ(w.day(), 2);
            EXPECT_EQ(intOf(w.state.run, "ap"), 3);
            EXPECT_EQ(intOf(w.state.run, "monthsDue"), 1);

            const ElementMap* told = w.step("fight");

            ASSERT_NE(told, nullptr);
            EXPECT_EQ(textOf(*told, "text").rfind("You beat the practice dummy", 0), 0u) << textOf(*told, "text");

            // Without a working, the practice is shut
            Walker plain(student());

            ASSERT_TRUE(plain.ask("enter", WalkAct{"story", "trial", 0}));
            ASSERT_NE(plain.work("practice"), nullptr);
            EXPECT_EQ(intOf(*plain.work("practice"), "locked"), 1);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // A working taught on the first day is his against the dummies on the second.
        TEST(story_test, a_spell_given_in_the_story_is_in_the_fight_menu)
        {
            MockLogger logger;

            auto hasFirebolt = [](const Walker& w)
            {
                return byId(w.last.menu, "tech.firebolt") != nullptr;
            };

            Walker w(mage());

            ASSERT_TRUE(w.ask("enter", WalkAct{"story", "drill", 0}));
            ASSERT_TRUE(w.take("fire"));

            EXPECT_EQ(intOf(w.character, "fire_course"), 1);
            EXPECT_EQ(w.day(), 2);
            ASSERT_EQ(w.cardId(), "story.practical");

            ASSERT_TRUE(w.act("option", "face"));
            ASSERT_EQ(w.screen(), "fight");
            EXPECT_EQ(textOf(w.state.fight, "mob"), "exam_dummy");
            EXPECT_EQ(w.state.foes.size(), 2u);
            EXPECT_TRUE(hasFirebolt(w));

            // Who idled the first day has no Firebolt
            Walker idle(mage());

            ASSERT_TRUE(idle.ask("enter", WalkAct{"story", "drill", 0}));
            ASSERT_TRUE(idle.take("idle"));
            ASSERT_TRUE(idle.act("option", "face"));
            ASSERT_EQ(idle.screen(), "fight");
            EXPECT_FALSE(hasFirebolt(idle));
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // The dummies' blows are weak and never bring him down: with three Vitality he stands the
        // whole examination behind his guard, and time is called with neither side fallen.
        TEST(story_test, a_dummy_cannot_bring_him_down)
        {
            MockLogger logger;

            ElementMap who = mage();
            who["vit"] = ElementType{3};

            Walker w(who);

            ASSERT_TRUE(w.ask("enter", WalkAct{"story", "drill", 0}));
            ASSERT_TRUE(w.take("idle"));
            ASSERT_TRUE(w.act("option", "face"));
            ASSERT_EQ(w.screen(), "fight");

            w.fightWith("guard");

            EXPECT_EQ(w.over(), "held");
            EXPECT_EQ(intOf(w.state.fight, "turns"), 10);
            EXPECT_EQ(intOf(w.state.run, "downed"), 0);
            EXPECT_GE(intOf(w.state.run, "hp"), 1);
            EXPECT_GE(intOf(w.character, "vit"), 1);
            EXPECT_EQ(w.standing(), 2);

            // Back to the scene: the day is over, and with its only test sat the drill is over too
            // (no grade: seen to its end), not lost
            ASSERT_TRUE(w.act("back"));
            EXPECT_EQ(w.screen(), "done");
            EXPECT_EQ(textOf(w.state.run, "result"), "over");
            EXPECT_EQ(w.day(), 3);
            EXPECT_EQ(intOf(w.state.run, "sat.practical"), 1);
            EXPECT_EQ(intOf(w.state.run, "score.practical"), 0);
            EXPECT_EQ(intOf(w.state.run, "monthsDue"), 1);
            EXPECT_EQ(intOf(w.state.run, "downed"), 0);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // The practical scores ten for each dummy beaten in its time: both, twenty; one beaten and
        // the other standing when time is called, ten.
        TEST(story_test, a_fight_test_is_scored)
        {
            MockLogger logger;

            {
                Walker w(mage());

                ASSERT_TRUE(w.ask("enter", WalkAct{"story", "drill", 0}));
                ASSERT_TRUE(w.take("fire"));
                ASSERT_TRUE(w.act("option", "face"));

                w.fightWith("step");

                EXPECT_EQ(w.over(), "won");
                EXPECT_LE(intOf(w.state.fight, "turns"), 10);

                ASSERT_TRUE(w.act("back"));
                EXPECT_EQ(intOf(w.state.run, "score.practical"), 20);
                EXPECT_EQ(w.day(), 3);
            }

            {
                Walker w(mage());

                ASSERT_TRUE(w.ask("enter", WalkAct{"story", "drill", 0}));
                ASSERT_TRUE(w.take("fire"));
                ASSERT_TRUE(w.act("option", "face"));

                w.fightWith("step", 1);
                ASSERT_EQ(w.standing(), 1);

                w.fightWith("guard");
                EXPECT_EQ(w.over(), "held");

                ASSERT_TRUE(w.act("back"));
                EXPECT_EQ(intOf(w.state.run, "score.practical"), 10);
            }
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // A card answered with a fight: the fight is the day's work, and the day ends when he is
        // back from it.
        TEST(story_test, a_fight_from_a_card_ends_the_day)
        {
            MockLogger logger;

            ElementMap who = mage();
            who["forbidden_read"] = ElementType{1};

            Walker w(who);

            ASSERT_TRUE(w.ask("enter", WalkAct{"story", "drill", 0}));
            ASSERT_TRUE(w.take("boast"));

            ASSERT_EQ(w.day(), 2);
            ASSERT_EQ(w.cardId(), "story.challenge");

            const int ap = intOf(w.state.run, "ap");

            ASSERT_TRUE(w.act("option", "accept"));
            ASSERT_EQ(w.screen(), "fight");
            EXPECT_EQ(textOf(w.state.fight, "mob"), "dummy");
            EXPECT_EQ(w.day(), 2);

            w.fightWith("step");
            ASSERT_FALSE(w.over().empty());

            ASSERT_TRUE(w.act("back"));
            EXPECT_EQ(w.day(), 3);
            EXPECT_EQ(intOf(w.state.run, "ap"), ap - 1);
            EXPECT_EQ(intOf(w.state.run, "monthsDue"), 1);
            EXPECT_EQ(intOf(w.state.run, "monthsRun"), 2);

            // The practical, whose day went by in the fight, comes next
            EXPECT_EQ(w.cardId(), "story.practical");
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // A fight of the story lost or fled from takes nothing the days gave: the marks and the stats
        // stay his, he is not carried home, and the scene goes on.
        TEST(story_test, a_story_fight_never_loses_the_gains)
        {
            MockLogger logger;

            // Fled from the practical, the fire course taught the day before is kept
            {
                Walker w(mage());

                ASSERT_TRUE(w.ask("enter", WalkAct{"story", "drill", 0}));
                ASSERT_TRUE(w.take("fire"));
                ASSERT_TRUE(w.act("option", "face"));
                ASSERT_TRUE(w.act("move", "retreat"));

                EXPECT_EQ(w.over(), "fled");

                ASSERT_TRUE(w.act("back"));
                EXPECT_EQ(intOf(w.character, "fire_course"), 1);
                EXPECT_EQ(intOf(w.state.run, "routed"), 0);
                EXPECT_EQ(intOf(w.state.run, "sat.practical"), 1);
                EXPECT_EQ(textOf(w.state.run, "result"), "over");
                EXPECT_EQ(w.day(), 3);
            }

            // Brought down by the wolf of the bout, what the bout gave before the fight is kept
            {
                ElementMap who = mage();
                who["vit"] = ElementType{3};

                Walker w(who);

                ASSERT_TRUE(w.ask("enter", WalkAct{"story", "drill", 0}));
                ASSERT_TRUE(w.take("bout"));
                ASSERT_EQ(w.screen(), "fight");

                w.fightWith("strike");

                EXPECT_EQ(w.over(), "lost");
                EXPECT_EQ(intOf(w.state.run, "downed"), 0);
                EXPECT_EQ(w.day(), 1);

                ASSERT_TRUE(w.act("back"));
                EXPECT_EQ(w.mark("braved"), 1);
                EXPECT_EQ(intOf(w.character, "renown"), 1);
                EXPECT_GE(intOf(w.character, "vit"), 1);
                EXPECT_EQ(intOf(w.state.run, "downed"), 0);
                EXPECT_EQ(textOf(w.state.run, "mode"), "site");
                EXPECT_EQ(w.day(), 2);
                EXPECT_EQ(intOf(w.state.run, "monthsDue"), 1);

                // No line of a walk lost
                for (const auto& entry : w.last.entries)
                    EXPECT_EQ(textOf(entry, "text").find("with nothing"), std::string::npos);
            }
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // The page's rows: what is new is marked on its first day only, and a look changes nothing.
        TEST(story_test, a_new_tile_is_marked_fresh_on_its_first_day)
        {
            MockLogger logger;
            Walker w(student());

            ASSERT_TRUE(w.ask("enter", WalkAct{"story", "trial", 0}));
            EXPECT_EQ(w.work("advanced"), nullptr);
            ASSERT_NE(w.work("lecture"), nullptr);
            EXPECT_EQ(intOf(*w.work("lecture"), "fresh"), 1);

            ASSERT_TRUE(w.spend("lecture"));
            ASSERT_TRUE(w.spend("lecture"));
            w.answerAll();

            EXPECT_EQ(w.day(), 3);
            ASSERT_NE(w.work("advanced"), nullptr);
            EXPECT_EQ(intOf(*w.work("advanced"), "fresh"), 1);
            EXPECT_EQ(intOf(*w.work("lecture"), "fresh"), 0);

            // A look asks again: nothing moves
            ASSERT_TRUE(w.ask("look"));
            ASSERT_NE(w.work("advanced"), nullptr);
            EXPECT_EQ(intOf(*w.work("advanced"), "fresh"), 1);

            ASSERT_TRUE(w.spend("library"));
            w.settle();

            EXPECT_EQ(w.day(), 4);
            ASSERT_NE(w.work("advanced"), nullptr);
            EXPECT_EQ(intOf(*w.work("advanced"), "fresh"), 0);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // A tile that left is said on the day it left, and not again.
        TEST(story_test, a_tile_that_left_is_said_once)
        {
            MockLogger logger;
            Walker w(student());

            ASSERT_TRUE(w.ask("enter", WalkAct{"story", "trial", 0}));
            EXPECT_TRUE(w.last.gone.empty());

            ASSERT_TRUE(w.spend("library"));
            EXPECT_TRUE(w.last.gone.empty());

            ASSERT_TRUE(w.spend("library"));
            w.answerAll();

            EXPECT_EQ(w.day(), 3);
            ASSERT_EQ(w.last.gone.size(), 1u);
            EXPECT_EQ(textOf(w.last.gone.front(), "id"), "tile.early");
            EXPECT_EQ(textOf(w.last.gone.front(), "name"), "Walk the halls");

            // A look says it still, that day
            ASSERT_TRUE(w.ask("look"));
            EXPECT_EQ(w.last.gone.size(), 1u);

            ASSERT_TRUE(w.spend("library"));
            w.answerAll();

            EXPECT_EQ(w.day(), 4);
            EXPECT_EQ(byId(w.last.gone, "tile.early"), nullptr);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // A stage says its text on its first day only.
        TEST(story_test, a_stage_says_its_text_on_its_first_day)
        {
            MockLogger logger;
            Walker w(student());

            ASSERT_TRUE(w.ask("enter", WalkAct{"story", "trial", 0}));
            EXPECT_EQ(textOf(w.last.site, "stage"), "The first days");
            EXPECT_NE(textOf(w.last.site, "stageText"), "");
            EXPECT_EQ(intOf(w.last.site, "day"), 1);
            EXPECT_EQ(intOf(w.last.site, "days"), 4);
            EXPECT_EQ(textOf(w.last.site, "unit"), "day");
            EXPECT_EQ(intOf(w.last.site, "monthsTotal"), 4);

            ASSERT_TRUE(w.spend("library"));
            EXPECT_EQ(textOf(w.last.site, "stageText"), "");
            EXPECT_EQ(intOf(w.last.site, "months"), 1);

            ASSERT_TRUE(w.spend("library"));
            w.answerAll();

            EXPECT_EQ(textOf(w.last.site, "stage"), "The last days");
            EXPECT_NE(textOf(w.last.site, "stageText"), "");

            ASSERT_TRUE(w.spend("library"));
            w.answerAll();

            EXPECT_EQ(w.day(), 4);
            EXPECT_EQ(textOf(w.last.site, "stageText"), "");
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // The figure at the head's corner and the word under it agree: the last day is one day.
        TEST(story_test, the_last_day_is_a_day_left)
        {
            MockLogger logger;
            Walker w(student());

            ASSERT_TRUE(w.ask("enter", WalkAct{"story", "trial", 0}));
            EXPECT_EQ(textOf(w.last.site, "daysLeft"), "4");
            EXPECT_EQ(textOf(w.last.site, "daysNote"), "DAYS LEFT");

            // A lecture first, so the gate of the fourth day lets him through
            ASSERT_TRUE(w.spend("lecture"));
            w.answerAll();

            ASSERT_TRUE(w.spend("library"));
            w.answerAll();

            // The fourth day, its examination waiting for him
            ASSERT_TRUE(w.spend("library"));

            EXPECT_EQ(w.day(), 4);
            EXPECT_EQ(textOf(w.state.run, "result"), "");
            EXPECT_EQ(textOf(w.last.site, "daysLeft"), "1");
            EXPECT_EQ(textOf(w.last.site, "daysNote"), "DAY LEFT");
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // His standing is said in words, and rises with what he has read.
        TEST(story_test, the_standing_is_words_only)
        {
            MockLogger logger;
            Walker w(student());

            ASSERT_TRUE(w.ask("enter", WalkAct{"story", "trial", 0}));

            const std::string low = textOf(w.last.site, "standing");

            EXPECT_NE(low, "");
            EXPECT_EQ(std::count_if(low.begin(), low.end(), [](unsigned char c) { return std::isdigit(c); }), 0);

            ASSERT_TRUE(w.spend("library"));

            const std::string one = textOf(w.last.site, "standing");

            ASSERT_TRUE(w.spend("library"));
            w.answerAll();

            const std::string two = textOf(w.last.site, "standing");

            EXPECT_NE(one, low);
            EXPECT_NE(two, one);

            for (const auto& line : {one, two})
                EXPECT_EQ(std::count_if(line.begin(), line.end(), [](unsigned char c) { return std::isdigit(c); }), 0);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // The Collegium's term (storytable.pg's `collegium`): sixteen days for the row's 24 months.
        // Its courses and their fees, the course card of the seventh day, the curriculum checked for
        // the fifteenth, the theory then, the practical on the sixteenth.
        namespace
        {
            // A youth fit to enter: his wits, his letters, a little lore, and the coin of the fees
            ElementMap collegian(int coin = 40)
            {
                return {{"coin", ElementType{coin}}, {"vit", ElementType{50}}, {"vitmax", ElementType{50}}, {"int", ElementType{12}},
                        {"letters", ElementType{3}}, {"lore", ElementType{2}}};
            }

            // What the Collegium's own scenes are: anything else that comes up is a chance meeting
            bool collegiumScene(const std::string& card)
            {
                return card == "story.course" or card == "story.sent_away" or card == "story.theory" or card == "story.practical";
            }

            // A tile of the term taken, then the chance meetings that came answered by their first
            // answer (which never costs a day)
            bool study(Walker& w, const std::string& tile)
            {
                const bool ok = w.take(tile);

                for (int guard = 0; guard < 4 and w.screen() == "event" and not collegiumScene(w.cardId()); ++guard)
                    w.answerFirst();

                return ok;
            }

            // A story's plan (storytable.pg's `plan`, asked "plan"): its tiles in the order he takes
            // them, each a day, and what he answers to its cards and its tests
            struct StoryPlan
            {
                std::vector<std::string> tiles;
                std::map<std::string, std::string> cards;
                std::map<std::string, std::string> tests;
            };

            StoryPlan planOf(Walker& w, const std::string& story)
            {
                StoryPlan plan;

                EXPECT_TRUE(w.ask("plan", WalkAct{"", story, 0}));

                const int n = intOf(w.last.site, "tiles");

                for (int i = 0; i < n; ++i)
                    plan.tiles.push_back(textOf(w.last.site, "tile." + std::to_string(i)));

                for (const auto& [key, value] : w.last.site)
                {
                    if (key.rfind("card.", 0) == 0)
                        plan.cards[key.substr(5)] = value.toString();
                    else if (key.rfind("test.", 0) == 0)
                        plan.tests[key.substr(5)] = value.toString();
                }

                return plan;
            }

            // The plan followed, as the solver follows it (Task 8's `storyBy`): each day the first of
            // its tiles not yet spent that is offered and open that day, else the first open tile;
            // its cards and tests answered as it says, a chance meeting by its first answer; a
            // fight handed to his stance, and back from it. Till the scene is over, or the day
            // `until` begins
            void followPlan(Walker& w, const StoryPlan& plan, int until = 1000, const std::function<void(Walker&)>& look = {})
            {
                std::vector<bool> spent(plan.tiles.size(), false);

                for (int guard = 0; guard < 400; ++guard)
                {
                    const std::string screen = w.screen();

                    if (screen == "done" or textOf(w.state.run, "mode") == "done" or w.day() >= until)
                        return;

                    // (what the page says, each time it is back to the day or a card)
                    if (look and screen != "fight")
                        look(w);

                    if (screen == "fight")
                    {
                        w.act(w.over().empty() ? "step" : "back");
                        continue;
                    }

                    if (screen == "event")
                    {
                        const std::string id = w.cardId().substr(6);

                        if (plan.cards.count(id) > 0)
                            w.act("option", plan.cards.at(id));
                        else if (plan.tests.count(id) > 0)
                            w.act("option", plan.tests.at(id));
                        else
                            w.answerFirst();

                        continue;
                    }

                    bool took = false;

                    for (size_t i = 0; i < plan.tiles.size() and not took; ++i)
                    {
                        const ElementMap* tile = spent[i] ? nullptr : w.work(plan.tiles[i]);

                        if (tile and intOf(*tile, "locked") == 0)
                        {
                            spent[i] = true;
                            took = w.take(plan.tiles[i]);
                        }
                    }

                    for (size_t i = 0; i < w.last.works.size() and not took; ++i)
                    {
                        if (intOf(w.last.works[i], "locked") == 0)
                            took = w.act(textOf(w.last.works[i], "id"));
                    }

                    if (not took)
                    {
                        ADD_FAILURE() << "nothing he can take on day " << w.day();
                        return;
                    }
                }
            }

            // What the fees took of his coin, in all
            int feesPaid(const Walker& w)
            {
                int paid = 0;

                for (const auto& mark : w.state.marks)
                {
                    if (textOf(mark, "kind") == "fee" and textOf(mark, "id") == "coin")
                        paid += intOf(mark, "n");
                }

                return paid;
            }
        }

        // Every row that opens a story opens one that is there, written for that row and as long as
        // it: the story's months are the row's (the term comes with the story's last month)
        TEST(story_test, collegium_rows_and_story_agree)
        {
            MockLogger logger;
            Walker w(collegian());

            std::vector<RuleActivity> rows;
            ASSERT_TRUE(w.rules.activities(16.0f, w.character, rows));

            std::vector<std::string> told;

            for (const auto& row : rows)
            {
                const std::string story = textOf(row.fields, "story");
                const std::string id = textOf(row.fields, "id");

                if (story.empty())
                    continue;

                told.push_back(id);

                ASSERT_TRUE(w.ask("plan", WalkAct{"", story, 0})) << id;
                EXPECT_EQ(textOf(w.last.site, "id"), story) << id;
                EXPECT_EQ(textOf(w.last.site, "row"), id) << id;
                EXPECT_EQ(intOf(w.last.site, "months"), intOf(row.fields, "months")) << id;
            }

            // The Collegium and the tests' trial at least (the drill has no row)
            EXPECT_NE(std::find(told.begin(), told.end(), "collegium"), told.end());
            EXPECT_NE(std::find(told.begin(), told.end(), "story.trial"), told.end());

            // An unknown story has no plan
            ASSERT_TRUE(w.ask("plan", WalkAct{"", "nowhere", 0}));
            EXPECT_EQ(textOf(w.last.site, "id"), "");
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // A plan says what its tiles ask in fees, all of them taken (the solver begins a story with
        // them in hand): the Collegium's twelve fee'd tiles of 3 coin, a term's 36. Of the two
        // binding halls only the one he would take is counted: the imp's, or the brownie's for who
        // knows the imp. A plan of no tiles asks none
        TEST(story_test, a_plan_says_what_it_asks_and_gives)
        {
            MockLogger logger;
            Walker w(collegian());

            ASSERT_TRUE(w.ask("plan", WalkAct{"", "collegium", 0}));
            EXPECT_EQ(intOf(w.last.site, "fee.coin"), 36);

            // And what following it gives: the binding hall and the fire course on the way, what a
            // pass gives at its end (the search learns from it what the story makes)
            EXPECT_EQ(intOf(w.last.site, "gives.know.fam.imp"), 1);
            EXPECT_EQ(intOf(w.last.site, "gives.know.fam.brownie"), 0);
            EXPECT_EQ(intOf(w.last.site, "gives.fire_course"), 1);
            EXPECT_EQ(intOf(w.last.site, "gives.collegium"), 1);
            EXPECT_EQ(intOf(w.last.site, "gives.arcana"), 1);
            EXPECT_EQ(intOf(w.last.site, "gives.top_floor"), 0);

            // Who knows the imp is shown the brownie's hall in its place
            ElementMap knows = collegian();
            knows["know.fam.imp"] = ElementType{1};
            Walker k(knows);

            ASSERT_TRUE(k.ask("plan", WalkAct{"", "collegium", 0}));
            EXPECT_EQ(intOf(k.last.site, "fee.coin"), 36);
            EXPECT_EQ(intOf(k.last.site, "gives.know.fam.imp"), 0);
            EXPECT_EQ(intOf(k.last.site, "gives.know.fam.brownie"), 1);

            ASSERT_TRUE(w.ask("plan", WalkAct{"", "trial", 0}));
            EXPECT_EQ(textOf(w.last.site, "id"), "trial");
            EXPECT_EQ(intOf(w.last.site, "fee.coin"), 0);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // Two days of the elemental lectures and the seventh day is the fork: fire, frost, storm or
        // the basics. One day only, and there is no fork: he stays on the basics.
        TEST(story_test, the_collegium_course_card_opens_on_day_seven)
        {
            MockLogger logger;

            {
                Walker w(collegian());

                ASSERT_TRUE(w.ask("enter", WalkAct{"story", "collegium", 0}));
                EXPECT_EQ(intOf(w.state.run, "days"), 16);
                EXPECT_EQ(intOf(w.state.run, "months"), 24);

                for (const char* tile : {"lectures", "lectures", "library", "library", "library", "library"})
                    ASSERT_TRUE(study(w, tile)) << tile;

                EXPECT_EQ(w.day(), 7);
                ASSERT_EQ(w.cardId(), "story.course");
                EXPECT_EQ(w.last.options.size(), 4u);

                for (const char* answer : {"fire", "frost", "storm", "basic"})
                    EXPECT_NE(w.option(answer), nullptr) << answer;

                // The basics close for who takes an element; his course opens
                ASSERT_TRUE(w.act("option", "fire"));
                EXPECT_EQ(w.screen(), "site");
                EXPECT_NE(w.work("fire"), nullptr);
                EXPECT_EQ(w.work("frost"), nullptr);
                EXPECT_EQ(w.work("basics"), nullptr);
                EXPECT_EQ(w.work("lectures"), nullptr);
            }

            {
                Walker w(collegian());

                ASSERT_TRUE(w.ask("enter", WalkAct{"story", "collegium", 0}));

                for (const char* tile : {"lectures", "library", "library", "library", "library", "library"})
                    ASSERT_TRUE(study(w, tile)) << tile;

                EXPECT_EQ(w.day(), 7);
                EXPECT_EQ(w.screen(), "site");
                EXPECT_EQ(w.cardId(), "");
                ASSERT_NE(w.work("basics"), nullptr);
                EXPECT_EQ(intOf(*w.work("basics"), "locked"), 0);
                EXPECT_EQ(w.work("fire"), nullptr);

                // And the fork does not come later, once the lectures are caught up
                ASSERT_TRUE(study(w, "basics"));
                EXPECT_EQ(w.day(), 8);
                EXPECT_NE(w.cardId(), "story.course");
            }
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // The fire course gives Firebolt on its second day, there and then: he has it before the
        // examinations, and would keep it if he left.
        TEST(story_test, a_fire_course_gives_firebolt)
        {
            MockLogger logger;
            Walker w(collegian());

            ASSERT_TRUE(w.ask("enter", WalkAct{"story", "collegium", 0}));

            for (const char* tile : {"lectures", "lectures", "library", "library", "library", "library"})
                ASSERT_TRUE(study(w, tile)) << tile;

            ASSERT_EQ(w.cardId(), "story.course");
            ASSERT_TRUE(w.act("option", "fire"));

            ASSERT_TRUE(study(w, "fire"));
            EXPECT_EQ(intOf(w.character, "fire_course"), 0);

            ASSERT_TRUE(study(w, "fire"));
            EXPECT_EQ(intOf(w.character, "fire_course"), 1);
            EXPECT_EQ(w.day(), 9);
            EXPECT_EQ(intOf(w.state.run, "sat.theory"), 0);

            // A third day gives it no second time
            ASSERT_TRUE(study(w, "fire"));
            EXPECT_EQ(intOf(w.character, "fire_course"), 1);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // Who stays on the basics is taught the three cantrips once he has followed four days of the
        // elemental lectures in all.
        TEST(story_test, the_basics_teach_the_cantrips)
        {
            MockLogger logger;
            Walker w(collegian());

            ASSERT_TRUE(w.ask("enter", WalkAct{"story", "collegium", 0}));

            for (const char* tile : {"lectures", "library", "library", "library", "library", "library", "basics", "basics"})
                ASSERT_TRUE(study(w, tile)) << tile;

            EXPECT_EQ(intOf(w.character, "spark_taught"), 0);

            ASSERT_TRUE(study(w, "basics"));

            for (const char* flag : {"spark_taught", "rime_taught", "static_taught"})
                EXPECT_EQ(intOf(w.character, flag), 1) << flag;

            // A spell to practise: the dummies are open to him
            ASSERT_NE(w.work("practice"), nullptr);
            EXPECT_EQ(intOf(*w.work("practice"), "locked"), 0);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // The coin the term asked up front is its courses' fees: about 35 for a full curriculum, the
        // plan's (the solver's) path.
        TEST(story_test, a_full_curriculum_costs_about_thirty_five)
        {
            MockLogger logger;
            Walker w(collegian(60));

            const StoryPlan plan = planOf(w, "collegium");
            ASSERT_FALSE(plan.tiles.empty());
            ASSERT_EQ(plan.cards.count("course"), 1u);
            EXPECT_EQ(plan.cards.at("course"), "fire");

            ASSERT_TRUE(w.ask("enter", WalkAct{"story", "collegium", 0}));
            followPlan(w, plan);

            EXPECT_EQ(textOf(w.state.run, "mode"), "done");
            EXPECT_GE(feesPaid(w), 33);
            EXPECT_LE(feesPaid(w), 39);
            EXPECT_EQ(intOf(w.character, "coin"), 60 - feesPaid(w));
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // A good student, who followed the plan's fire course and has what his childhood gave him,
        // passes: the class is his, with what it teaches.
        TEST(story_test, a_good_student_passes)
        {
            MockLogger logger;

            ElementMap who = collegian();
            who["int"] = ElementType{13};
            who["mara_student"] = ElementType{1};

            Walker w(who);

            const StoryPlan plan = planOf(w, "collegium");

            ASSERT_TRUE(w.ask("enter", WalkAct{"story", "collegium", 0}));
            followPlan(w, plan);

            const std::string result = textOf(w.state.run, "result");

            // (the theory 30, the practical 20 for both dummies, 5 for a reaction, 3 for his day on
            // the dummies: 58, a merit and no more)
            EXPECT_EQ(result, "merit") << result << " theory " << intOf(w.state.run, "score.theory") << " practical " << intOf(w.state.run, "score.practical");
            EXPECT_EQ(intOf(w.character, "collegium"), 1);
            EXPECT_GE(intOf(w.character, "arcana"), 1);
            EXPECT_GE(intOf(w.character, "lore"), 3);
            EXPECT_EQ(intOf(w.character, "fire_course"), 1);
            EXPECT_EQ(intOf(w.character, "know.fam.imp"), 1);
            EXPECT_EQ(intOf(w.character, "dropout.collegium"), 0);
            EXPECT_EQ(w.day(), 17);
            EXPECT_EQ(intOf(w.state.run, "monthsRun"), 24);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // Who only copied for the scriptorium has not followed the curriculum: the day after his last
        // day of study the masters send him away. A drop-out, without the class.
        TEST(story_test, a_truant_is_sent_away)
        {
            MockLogger logger;
            Walker w(collegian(0));

            ASSERT_TRUE(w.ask("enter", WalkAct{"story", "collegium", 0}));

            for (int day = 1; day <= 13; ++day)
                ASSERT_TRUE(study(w, "scriptorium")) << day;

            EXPECT_EQ(intOf(w.character, "coin"), 39);

            // The last day of study, the examinations' first: the library, the dummies or rest
            EXPECT_EQ(w.day(), 14);
            EXPECT_EQ(w.work("scriptorium"), nullptr);
            ASSERT_TRUE(study(w, "rest"));

            EXPECT_EQ(w.day(), 15);
            ASSERT_EQ(w.cardId(), "story.sent_away");
            ASSERT_TRUE(w.answerFirst());

            EXPECT_EQ(textOf(w.state.run, "result"), "dropout");
            EXPECT_EQ(intOf(w.character, "dropout.collegium"), 1);
            EXPECT_EQ(w.character.count("collegium"), 0u);
            EXPECT_EQ(intOf(w.state.run, "sat.theory"), 0);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // Who read the forbidden shelf as a boy may answer from it: it counts for much, and he is
        // marked for it.
        TEST(story_test, the_forbidden_shelf_answer_marks_him)
        {
            MockLogger logger;

            ElementMap who = collegian();
            who["forbidden_read"] = ElementType{1};

            Walker w(who);

            StoryPlan plan = planOf(w, "collegium");
            plan.tests["theory"] = "shelf";

            ASSERT_TRUE(w.ask("enter", WalkAct{"story", "collegium", 0}));
            followPlan(w, plan, 15);

            ASSERT_EQ(w.day(), 15);
            ASSERT_EQ(w.cardId(), "story.theory");
            ASSERT_NE(w.option("shelf"), nullptr);
            ASSERT_NE(w.option("taught"), nullptr);
            EXPECT_EQ(w.option("fathers"), nullptr);

            ASSERT_TRUE(w.act("option", "shelf"));
            EXPECT_EQ(intOf(w.character, "suspected"), 1);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // The masters' word never promises a pass that one examination's floor refuses: a student
        // of the basics who practised twice but read once (theory 22, under its 24) is told he is
        // behind in his books, and he fails.
        TEST(story_test, the_standing_never_promises_what_a_floor_refuses)
        {
            MockLogger logger;
            Walker w(collegian());

            ASSERT_TRUE(w.ask("enter", WalkAct{"story", "collegium", 0}));

            for (const char* tile : {"lectures", "lectures", "lectures", "library", "scriptorium", "scriptorium"})
                ASSERT_TRUE(study(w, tile)) << tile;

            ASSERT_EQ(w.cardId(), "story.course");
            ASSERT_TRUE(w.act("option", "basic"));

            for (const char* tile : {"basics", "basics", "basics", "practice", "practice", "scriptorium", "scriptorium", "rest"})
                ASSERT_TRUE(study(w, tile)) << tile;

            ASSERT_EQ(w.day(), 15);
            ASSERT_EQ(w.cardId(), "story.theory");
            ASSERT_EQ(w.mark("courses"), 6);
            ASSERT_EQ(w.mark("revised"), 1);
            ASSERT_EQ(w.mark("practised"), 2);

            const std::string said = textOf(w.last.site, "standing");

            EXPECT_EQ(said, "The masters think him behind in his books.");
            EXPECT_NE(said, "The masters expect him to pass.");

            ASSERT_TRUE(w.act("option", "taught"));
            EXPECT_EQ(intOf(w.state.run, "score.theory"), 22);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // Who comes to the examinations with no working he can throw (all his days on the familiar
        // studies) cannot break a dummy: his books are enough, and the masters say he is behind
        // with the workings.
        TEST(story_test, the_standing_says_who_has_no_working_is_behind)
        {
            MockLogger logger;
            Walker w(collegian());

            ASSERT_TRUE(w.ask("enter", WalkAct{"story", "collegium", 0}));

            for (int day = 1; day <= 13; ++day)
                ASSERT_TRUE(study(w, "familiar")) << day;

            ASSERT_NE(w.work("practice"), nullptr);
            EXPECT_EQ(intOf(*w.work("practice"), "locked"), 1);

            ASSERT_TRUE(study(w, "library"));

            ASSERT_EQ(w.day(), 15);
            ASSERT_EQ(w.cardId(), "story.theory");
            EXPECT_EQ(textOf(w.last.site, "standing"), "The masters think him behind with the workings.");
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // The grades and the masters' word, measured on the playtest's scholar (life (a) of the
        // ninth task): his wits 15, Mara's pupil, the Old Chronicle and the forbidden shelf read as
        // a boy, his letters; in the term the lectures, the familiar studies, the fire course, the
        // binding hall and two days on the dummies, and the notes copied by candlelight on the
        // third day. Answering what was taught, a merit; from the forbidden shelf, or with a third
        // day on the dummies, a distinction
        namespace
        {
            const char* const ExpectPass = "The masters expect him to pass.";
            const char* const ThinkWell = "The masters think well of him.";
            const char* const TopFloor = "The masters speak of him on the top floor.";
            const char* const SeldomSeen = "The masters seldom see him in their lecture halls.";

            bool promises(const std::string& said)
            {
                return said == ExpectPass or said == ThinkWell or said == TopFloor;
            }

            ElementMap scholar()
            {
                ElementMap who = collegian(60);

                who["int"] = ElementType{15};
                who["lore"] = ElementType{4};
                who["mara_student"] = ElementType{1};
                who["forbidden_read"] = ElementType{1};

                return who;
            }

            // A chance meeting of the term put up at the start of the day, as the dice would, and
            // answered
            void meet(Walker& w, const std::string& event, const std::string& answer)
            {
                w.state.run["event"] = ElementType{"story." + event};

                ASSERT_TRUE(w.ask("look"));
                ASSERT_EQ(w.cardId(), "story." + event);
                ASSERT_TRUE(w.act("option", answer));
            }

            // His term, by its plan: the lectures, the familiar studies, the fire course, then
            // `last`; the theory answered by `theory`; the notes copied on the third day
            void scholarTerm(Walker& w, const std::vector<std::string>& last, const std::string& theory)
            {
                w.rules.done["chronicle"] = ElementType{1};

                StoryPlan plan;
                plan.tiles = {"lectures", "lectures", "lectures", "familiar", "familiar", "familiar", "fire", "fire", "fire", "fire", "fire"};
                plan.tiles.insert(plan.tiles.end(), last.begin(), last.end());
                plan.cards["course"] = "fire";
                plan.tests["theory"] = theory;
                plan.tests["practical"] = "face";

                ASSERT_TRUE(w.ask("enter", WalkAct{"story", "collegium", 0}));

                bool copied = false;

                followPlan(w, plan, 1000, [&copied](Walker& at) {
                    if (at.day() == 3 and at.screen() == "site" and not copied)
                    {
                        copied = true;
                        meet(at, "notes", "copy");
                    }
                });

                EXPECT_TRUE(copied);
            }

            std::string scores(const Walker& w)
            {
                return "theory " + std::to_string(intOf(w.state.run, "score.theory")) + ", practical " + std::to_string(intOf(w.state.run, "score.practical"));
            }
        }

        // Answering what was taught: 36 and 31 (both dummies, a reaction, two days on them), a merit
        TEST(story_test, the_scholar_who_answers_what_was_taught_has_merit)
        {
            MockLogger logger;
            Walker w(scholar());

            scholarTerm(w, {"binding", "practice", "practice"}, "taught");

            ASSERT_EQ(textOf(w.state.run, "mode"), "done");
            EXPECT_EQ(w.mark("revised"), 1);
            EXPECT_EQ(w.mark("courses"), 11);
            EXPECT_EQ(w.mark("practised"), 2);
            EXPECT_EQ(intOf(w.state.run, "score.theory"), 36) << scores(w);
            EXPECT_EQ(intOf(w.state.run, "score.practical"), 31) << scores(w);
            EXPECT_EQ(textOf(w.state.run, "result"), "merit") << scores(w);
            EXPECT_EQ(intOf(w.character, "top_floor"), 0);
        }

        // From the forbidden shelf, a distinction (and he is suspected of it)
        TEST(story_test, the_scholar_who_answers_from_the_shelf_has_distinction)
        {
            MockLogger logger;
            Walker w(scholar());

            scholarTerm(w, {"binding", "practice", "practice"}, "shelf");

            ASSERT_EQ(textOf(w.state.run, "mode"), "done");
            EXPECT_EQ(intOf(w.state.run, "score.theory"), 42) << scores(w);
            EXPECT_EQ(textOf(w.state.run, "result"), "distinction") << scores(w);
            EXPECT_EQ(intOf(w.character, "top_floor"), 1);
            EXPECT_EQ(intOf(w.character, "suspected"), 1);

            // Taken so far says it in one line, its word going on from its name, and no figure
            const ElementMap* suspected = byId(w.last.taken, "stat.suspected");
            ASSERT_NE(suspected, nullptr);
            EXPECT_EQ(textOf(*suspected, "name"), "Suspected of the forbidden shelf by the masters");
            EXPECT_EQ(textOf(*suspected, "value"), "");
        }

        // Answering what was taught, with a third day on the dummies in the binding hall's place: a
        // distinction, by his practice (36 and 34)
        TEST(story_test, the_scholar_who_practised_three_days_has_distinction)
        {
            MockLogger logger;
            Walker w(scholar());

            scholarTerm(w, {"practice", "practice", "practice"}, "taught");

            ASSERT_EQ(textOf(w.state.run, "mode"), "done");
            EXPECT_EQ(w.mark("practised"), 3);
            EXPECT_EQ(intOf(w.state.run, "score.practical"), 34) << scores(w);
            EXPECT_EQ(textOf(w.state.run, "result"), "distinction") << scores(w);
        }

        // The masters promise him nothing on his first day: what he has done counts, not the answer
        // the forbidden shelf might give him; and their word moves up as he studies
        TEST(story_test, the_standing_counts_what_he_has_done)
        {
            MockLogger logger;
            Walker w(scholar());

            w.rules.done["chronicle"] = ElementType{1};

            ASSERT_TRUE(w.ask("enter", WalkAct{"story", "collegium", 0}));

            const std::string first = textOf(w.last.site, "standing");

            EXPECT_FALSE(promises(first)) << first;

            // His term looked at day by day, to the theory
            std::vector<std::string> said;

            StoryPlan plan;
            plan.tiles = {"lectures", "lectures", "lectures", "familiar", "familiar", "familiar", "fire", "fire", "fire", "fire", "fire", "binding", "practice", "practice"};
            plan.cards["course"] = "fire";
            plan.tests["theory"] = "taught";
            plan.tests["practical"] = "face";

            followPlan(w, plan, 15, [&said](Walker& at) {
                const std::string line = textOf(at.last.site, "standing");

                if (said.empty() or said.back() != line)
                    said.push_back(line);
            });

            // Behind on the first day, expected to pass once he follows the lectures, and well
            // thought of by the last days; never the top floor, which asks more than he has done
            ASSERT_GE(said.size(), 3u);
            EXPECT_FALSE(promises(said.front())) << said.front();
            EXPECT_NE(std::find(said.begin(), said.end(), ExpectPass), said.end());
            EXPECT_EQ(said.back(), ThinkWell);
            EXPECT_EQ(std::find(said.begin(), said.end(), TopFloor), said.end());
        }

        // From the old tome his lore counts three times in place of his wits: for who has five or
        // more, it is the better answer
        TEST(story_test, the_tome_answer_wins_for_a_lore_rich_student)
        {
            MockLogger logger;

            ElementMap who = collegian(60);
            who["int"] = ElementType{13};
            who["lore"] = ElementType{6};
            who["tome"] = ElementType{1};

            int scored[2] = {0, 0};
            const char* answers[2] = {"taught", "tome"};

            for (int i = 0; i < 2; ++i)
            {
                Walker w(who);

                StoryPlan plan = planOf(w, "collegium");

                ASSERT_TRUE(w.ask("enter", WalkAct{"story", "collegium", 0}));
                followPlan(w, plan, 15);

                ASSERT_EQ(w.cardId(), "story.theory");
                ASSERT_NE(w.option(answers[i]), nullptr) << answers[i];
                ASSERT_TRUE(w.act("option", answers[i]));

                scored[i] = intOf(w.state.run, "score.theory");
            }

            // 13 for his wits, 18 for his lore
            EXPECT_EQ(scored[1], scored[0] + 5);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // Who reads and copies and seldom sits in a lecture hall is told so in time, from the
        // second day: never a promise of a pass, and on the fifteenth the masters send him away
        TEST(story_test, the_masters_warn_of_the_curriculum_in_time)
        {
            MockLogger logger;

            ElementMap who = collegian();
            who["mara_student"] = ElementType{1};

            Walker w(who);

            ASSERT_TRUE(w.ask("enter", WalkAct{"story", "collegium", 0}));

            int heard = 0;

            for (int day = 1; day <= 14; ++day)
            {
                ASSERT_EQ(w.day(), day);
                ASSERT_EQ(w.screen(), "site") << day;

                const std::string said = textOf(w.last.site, "standing");

                if (said == SeldomSeen and heard == 0)
                    heard = day;

                if (heard > 0)
                    EXPECT_FALSE(promises(said)) << "day " << day << ": " << said;

                ASSERT_TRUE(study(w, day % 2 == 1 or day == 14 ? "library" : "scriptorium")) << day;
            }

            EXPECT_GT(heard, 0);
            EXPECT_LE(heard, 5);

            ASSERT_EQ(w.day(), 15);
            ASSERT_EQ(w.cardId(), "story.sent_away");
            ASSERT_TRUE(w.answerFirst());
            EXPECT_EQ(textOf(w.state.run, "result"), "dropout");
        }

        // Who follows the curriculum is never told he is seldom seen
        TEST(story_test, the_plan_student_is_never_told_he_is_seldom_seen)
        {
            MockLogger logger;

            ElementMap who = collegian();
            who["int"] = ElementType{13};
            who["mara_student"] = ElementType{1};

            Walker w(who);

            const StoryPlan plan = planOf(w, "collegium");
            int looked = 0;

            ASSERT_TRUE(w.ask("enter", WalkAct{"story", "collegium", 0}));

            followPlan(w, plan, 1000, [&looked](Walker& at) {
                ++looked;
                EXPECT_NE(textOf(at.last.site, "standing"), SeldomSeen) << "day " << at.day();
            });

            EXPECT_GE(looked, 14);
            EXPECT_EQ(textOf(w.state.run, "mode"), "done");
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // A chance answer that takes his last Vitality does not end the term: he is left with one,
        // and the day goes on (a story ends by its days, a test, the gate or his leaving)
        TEST(story_test, brought_down_outside_a_fight_he_goes_on)
        {
            MockLogger logger;

            ElementMap who = collegian();
            who["vit"] = ElementType{2};

            Walker w(who);

            ASSERT_TRUE(w.ask("enter", WalkAct{"story", "collegium", 0}));
            ASSERT_TRUE(study(w, "lectures"));
            ASSERT_TRUE(study(w, "lectures"));
            ASSERT_EQ(w.day(), 3);
            ASSERT_EQ(w.screen(), "site");

            // The fever, and he goes down to the halls all the same
            meet(w, "fever", "go");

            EXPECT_EQ(intOf(w.state.run, "hp"), 1);
            EXPECT_EQ(intOf(w.character, "vit"), 1);
            EXPECT_EQ(textOf(w.state.run, "result"), "");
            EXPECT_EQ(textOf(w.state.run, "mode"), "site");
            EXPECT_EQ(w.screen(), "site");
            EXPECT_EQ(intOf(w.character, "dropout.collegium"), 0);
            EXPECT_EQ(w.day(), 3);

            // And the day's work is his to choose
            ASSERT_NE(w.work("lectures"), nullptr);
            EXPECT_EQ(intOf(*w.work("lectures"), "locked"), 0);
            ASSERT_TRUE(w.take("lectures"));
            EXPECT_EQ(w.day(), 4);
            EXPECT_EQ(textOf(w.state.run, "result"), "");
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // The fire course the seventh day's card opens is new that day, and not the next; what the
        // card shut (the basics) is said gone that day too
        TEST(story_test, a_tile_a_card_opens_is_new_that_day)
        {
            MockLogger logger;
            Walker w(collegian());

            ASSERT_TRUE(w.ask("enter", WalkAct{"story", "collegium", 0}));

            for (const char* tile : {"lectures", "lectures", "library", "library", "library", "library"})
                ASSERT_TRUE(study(w, tile)) << tile;

            ASSERT_EQ(w.cardId(), "story.course");
            ASSERT_TRUE(w.act("option", "fire"));

            ASSERT_EQ(w.day(), 7);
            ASSERT_NE(w.work("fire"), nullptr);
            EXPECT_EQ(intOf(*w.work("fire"), "fresh"), 1);
            EXPECT_NE(byId(w.last.gone, "tile.basics"), nullptr);
            EXPECT_NE(byId(w.last.gone, "tile.lectures"), nullptr);

            // A look says the same
            ASSERT_TRUE(w.ask("look"));
            ASSERT_NE(w.work("fire"), nullptr);
            EXPECT_EQ(intOf(*w.work("fire"), "fresh"), 1);
            EXPECT_NE(byId(w.last.gone, "tile.basics"), nullptr);

            ASSERT_TRUE(study(w, "fire"));
            ASSERT_EQ(w.day(), 8);
            ASSERT_NE(w.work("fire"), nullptr);
            EXPECT_EQ(intOf(*w.work("fire"), "fresh"), 0);
            EXPECT_EQ(byId(w.last.gone, "tile.basics"), nullptr);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // What a flag he is given is said by its name ("Firebolt learned"), in the record and in
        // what the scene has given; a counted stat by its figure ("+3 Coin")
        TEST(story_test, a_flag_given_reads_by_name)
        {
            MockLogger logger;
            Walker w(collegian());

            ASSERT_TRUE(w.ask("enter", WalkAct{"story", "collegium", 0}));

            for (const char* tile : {"scriptorium", "lectures", "lectures", "library", "library", "library"})
                ASSERT_TRUE(study(w, tile)) << tile;

            ASSERT_EQ(w.cardId(), "story.course");
            ASSERT_TRUE(w.act("option", "fire"));

            // Before: what the course teaches, in time
            ASSERT_NE(w.work("fire"), nullptr);
            EXPECT_NE(textOf(*w.work("fire"), "gives").find("Firebolt in time"), std::string::npos) << textOf(*w.work("fire"), "gives");

            ASSERT_TRUE(study(w, "fire"));
            ASSERT_TRUE(study(w, "fire"));
            ASSERT_EQ(intOf(w.character, "fire_course"), 1);

            const ElementMap* copied = nullptr;
            const ElementMap* taught = nullptr;

            for (const auto& s : w.state.record)
            {
                if (textOf(s, "target") == "scriptorium")
                    copied = &s;

                if (textOf(s, "target") == "fire" and textOf(s, "figure").find("FIREBOLT") != std::string::npos)
                    taught = &s;
            }

            ASSERT_NE(copied, nullptr);
            ASSERT_NE(taught, nullptr);

            EXPECT_NE(textOf(*copied, "figure").find("+3 COIN"), std::string::npos) << textOf(*copied, "figure");
            EXPECT_NE(textOf(*taught, "figure").find("FIREBOLT LEARNED"), std::string::npos) << textOf(*taught, "figure");
            EXPECT_EQ(textOf(*taught, "figure").find("+1"), std::string::npos) << textOf(*taught, "figure");

            ASSERT_NE(byId(w.last.taken, "stat.fire_course"), nullptr);
            EXPECT_EQ(textOf(*byId(w.last.taken, "stat.fire_course"), "name"), "Firebolt");
            EXPECT_EQ(textOf(*byId(w.last.taken, "stat.fire_course"), "value"), "learned");
            ASSERT_NE(byId(w.last.taken, "stat.coin"), nullptr);
            EXPECT_EQ(textOf(*byId(w.last.taken, "stat.coin"), "value"), "+3");
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // A stage's title and text are written to the record on its first day, once (the head of a
        // phone has the room for its title only): the first, the seventh and the fourteenth days of
        // the term. A look writes nothing
        TEST(story_test, a_stage_is_written_to_the_record_on_its_first_day)
        {
            MockLogger logger;

            ElementMap who = collegian();
            who["int"] = ElementType{13};
            who["mara_student"] = ElementType{1};

            Walker w(who);

            const StoryPlan plan = planOf(w, "collegium");

            ASSERT_TRUE(w.ask("enter", WalkAct{"story", "collegium", 0}));
            ASSERT_EQ(w.state.record.size(), 1u);
            EXPECT_EQ(textOf(w.state.record.front(), "kind"), "stage");
            EXPECT_EQ(textOf(w.state.record.front(), "text"), "The first weeks. Every lecture hall is open to him, and every master asks his fee at the door.");
            EXPECT_EQ(textOf(w.state.record.front(), "figure"), "");
            EXPECT_EQ(intOf(w.state.record.front(), "ap"), 0);

            ASSERT_TRUE(w.ask("look"));
            EXPECT_EQ(w.state.record.size(), 1u);

            followPlan(w, plan);
            ASSERT_EQ(textOf(w.state.run, "mode"), "done");

            std::vector<std::string> stages;

            for (const auto& s : w.state.record)
            {
                if (textOf(s, "kind") == "stage")
                    stages.push_back(textOf(s, "target"));
            }

            EXPECT_EQ(stages, (std::vector<std::string>{"The first weeks", "The chosen course", "The examinations"}));

            // Its days are the scene's still: sixteen of them, none for the lines of the stages
            EXPECT_EQ(intOf(w.state.run, "spent"), 16);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // The outskirts (storytable.pg's `outskirts`): the last month before he is seven, four days,
        // his lunch, one of three paths, that path's fork, and the slime in the ditch.
        namespace
        {
            // A boy of six years and eleven months, the harvest in: nothing left to eat
            ElementMap boyOfEleven()
            {
                return {{"str", ElementType{6}}, {"dex", ElementType{6}}, {"int", ElementType{7}}, {"vit", ElementType{38}}, {"vitmax", ElementType{38}},
                        {"coin", ElementType{3}}, {"rations", ElementType{0}}};
            }

            std::vector<std::string> workIds(const Walker& w)
            {
                std::vector<std::string> ids;

                for (const auto& row : w.last.works)
                    ids.push_back(textOf(row, "id"));

                return ids;
            }

            // Into the outskirts with `seed`'s dice, lunch, the path and its fork, then the ditch's card
            // answered: he is face to face with the slime
            bool toTheDitch(Walker& w, const std::string& path, const std::string& fork, int seed = 100)
            {
                w.seed = seed;
                w.age = 6.0f + 11.0f / 12.0f;

                return w.ask("enter", WalkAct{"story", "outskirts", 0}) and w.take("lunch") and w.take(path) and w.take(fork)
                       and w.cardId() == "story.ditch" and w.act("option", "go") and w.screen() == "fight";
            }
        }

        // The first day offers his lunch alone, and it gives the six rations the month at the mill
        // once did: he has none after the harvest
        TEST(story_test, the_outskirts_open_with_lunch)
        {
            MockLogger logger;
            Walker w(boyOfEleven());
            w.age = 6.0f + 11.0f / 12.0f;

            ASSERT_TRUE(w.ask("enter", WalkAct{"story", "outskirts", 0}));
            EXPECT_EQ(intOf(w.state.run, "days"), 4);
            EXPECT_EQ(intOf(w.state.run, "months"), 1);
            EXPECT_EQ(w.screen(), "site");
            EXPECT_EQ(workIds(w), (std::vector<std::string>{"tile.lunch"}));

            ASSERT_TRUE(w.take("lunch"));
            EXPECT_EQ(intOf(w.character, "rations"), 6);
            EXPECT_EQ(w.day(), 2);

            // No month falls due before the last day is over: the lunch is still his
            EXPECT_EQ(intOf(w.state.run, "monthsRun"), 0);
        }

        // The second day's three paths: one taken, the next day offers that path's two forks and
        // nothing of the other paths
        TEST(story_test, a_path_closes_the_others)
        {
            MockLogger logger;
            Walker w(boyOfEleven());
            w.age = 6.0f + 11.0f / 12.0f;

            ASSERT_TRUE(w.ask("enter", WalkAct{"story", "outskirts", 0}));
            ASSERT_TRUE(w.take("lunch"));

            EXPECT_EQ(workIds(w), (std::vector<std::string>{"tile.children", "tile.nature", "tile.animals"}));

            ASSERT_TRUE(w.take("nature"));
            EXPECT_EQ(w.day(), 3);
            EXPECT_EQ(intOf(w.character, "int"), 8);
            EXPECT_EQ(intOf(w.character, "outskirts.nature"), 1);
            EXPECT_EQ(intOf(w.character, "outskirts.children"), 0);
            EXPECT_EQ(intOf(w.character, "outskirts.animals"), 0);

            EXPECT_EQ(workIds(w), (std::vector<std::string>{"tile.beetles", "tile.wortleaf"}));

            // The wortleaf: two remedies, for the fight
            ASSERT_TRUE(w.take("wortleaf"));
            EXPECT_EQ(intOf(w.character, "remedy"), 2);
            EXPECT_EQ(w.day(), 4);
            EXPECT_EQ(w.cardId(), "story.ditch");
        }

        // Each path teaches its move: in the slime's fight it is in his menu, and the other two are not
        TEST(story_test, each_path_teaches_its_move)
        {
            MockLogger logger;

            const std::vector<std::vector<std::string>> paths = {{"children", "chase", "tech.step"}, {"nature", "beetles", "tech.read"}, {"animals", "tracks", "tech.stick"}};

            for (const auto& path : paths)
            {
                Walker w(boyOfEleven());

                ASSERT_TRUE(toTheDitch(w, path[0], path[1])) << path[0];
                EXPECT_EQ(textOf(w.state.fight, "mob"), "slime") << path[0];
                EXPECT_EQ(intOf(w.character, "outskirts." + path[0]), 1) << path[0];

                for (const auto& other : paths)
                    EXPECT_EQ(byId(w.last.menu, other[2]) != nullptr, other[2] == path[2]) << path[0] << " " << other[2];
            }

            // What each fork gave: a point more of the path's part, Lore, or Dexterity
            Walker chase(boyOfEleven());
            ASSERT_TRUE(toTheDitch(chase, "children", "chase"));
            EXPECT_EQ(intOf(chase.character, "dex"), 8);

            Walker tales(boyOfEleven());
            ASSERT_TRUE(toTheDitch(tales, "children", "tales"));
            EXPECT_EQ(intOf(tales.character, "dex"), 7);
            EXPECT_EQ(intOf(tales.character, "lore"), 1);

            Walker tracks(boyOfEleven());
            ASSERT_TRUE(toTheDitch(tracks, "animals", "tracks"));
            EXPECT_EQ(intOf(tracks.character, "str"), 8);

            Walker crows(boyOfEleven());
            ASSERT_TRUE(toTheDitch(crows, "animals", "crows"));
            EXPECT_EQ(intOf(crows.character, "str"), 7);
            EXPECT_EQ(intOf(crows.character, "dex"), 7);
        }

        // Beaten, the slime leaves his first chronicle pages, four of them (the market's lot: they
        // sell for a coin), the fight's end says so, and the story is seen to its end
        TEST(story_test, the_slime_leaves_a_page)
        {
            MockLogger logger;
            Walker w(boyOfEleven());

            ASSERT_TRUE(toTheDitch(w, "nature", "beetles"));

            for (int guard = 0; guard < 12 and w.screen() == "fight" and w.over().empty(); ++guard)
                ASSERT_TRUE(w.act("move", "strike"));

            ASSERT_EQ(w.over(), "won");
            EXPECT_EQ(intOf(w.last.after, "page"), 4);
            EXPECT_EQ(intOf(w.character, "page"), 4);

            // What it left, then the story's word
            const std::string spoils = textOf(w.last.duel, "spoils");
            EXPECT_NE(spoils.find("4 chronicle pages"), std::string::npos) << spoils;
            EXPECT_NE(spoils.find("Written to the day"), std::string::npos) << spoils;
            EXPECT_LT(spoils.find("4 chronicle pages"), spoils.find("Written to the day")) << spoils;

            ASSERT_TRUE(w.act("back"));
            EXPECT_EQ(w.screen(), "done");
            EXPECT_EQ(textOf(w.state.run, "result"), "over");
            EXPECT_EQ(intOf(w.state.run, "monthsRun"), 1);
            EXPECT_EQ(intOf(w.character, "page"), 4);

            // Seen to their end: the Outskirts are a plural
            bool said = false;

            for (const auto& e : w.last.entries)
            {
                if (textOf(e, "text") == "You saw the Outskirts to their end")
                    said = true;

                EXPECT_EQ(textOf(e, "text").find("to its end"), std::string::npos) << textOf(e, "text");
            }

            EXPECT_TRUE(said);
        }

        // What the slime shows is what it does: its Ooze, shown, draws no blood, and only its Slap
        // hurts. Over twenty seeds of a boy striking it: no Vitality is lost to a turn its panel said
        // OOZE, and some is lost to one it said SLAP
        TEST(story_test, the_slime_shows_its_blow_before_it)
        {
            MockLogger logger;

            int oozes = 0;
            int slapsLanded = 0;

            for (int seed = 1; seed <= 20; ++seed)
            {
                Walker w(boyOfEleven());

                ASSERT_TRUE(toTheDitch(w, "nature", "beetles", seed * 37));

                for (int guard = 0; guard < 12 and w.screen() == "fight" and w.over().empty(); ++guard)
                {
                    ASSERT_EQ(w.last.targets.size(), 1u);

                    const std::string intent = textOf(w.last.targets[0], "intent");
                    const int hp = intOf(w.state.run, "hp");

                    ASSERT_TRUE(intent == "OOZE" or intent == "SLAP") << intent;
                    ASSERT_TRUE(w.act("move", "strike"));

                    if (intent == "OOZE")
                    {
                        ++oozes;
                        EXPECT_EQ(intOf(w.state.run, "hp"), hp) << "seed " << seed;
                    }
                    else if (intOf(w.state.run, "hp") < hp)
                        ++slapsLanded;
                }

                // Its Ooze is told in its own word, not as a slap
                bool oozed = false;

                for (const auto& a : w.state.account)
                {
                    if (textOf(a, "text").find("oozes toward you") != std::string::npos)
                        oozed = true;
                }

                EXPECT_TRUE(oozed) << "seed " << seed;
            }

            EXPECT_GT(oozes, 0);
            EXPECT_GT(slapsLanded, 0);
        }

        // As quick as the slime, he acts as often as it does, and the panel says so in those words
        TEST(story_test, as_quick_as_the_slime_he_acts_as_often)
        {
            MockLogger logger;

            ElementMap boy = boyOfEleven();
            boy["dex"] = ElementType{3};

            Walker w(boy);

            ASSERT_TRUE(toTheDitch(w, "nature", "beetles"));

            const std::string quick = textOf(w.last.duel, "quick");
            EXPECT_NE(quick.find("YOU ACT AS OFTEN AS IT DOES"), std::string::npos) << quick;
            EXPECT_EQ(quick.find(" TO ITS "), std::string::npos) << quick;
        }

        // The slime cannot kill him: with one Vitality and his guard up for all its turns he is never
        // downed, and the story goes on to its end
        TEST(story_test, the_slime_spares_him)
        {
            MockLogger logger;

            ElementMap boy = boyOfEleven();
            boy["vit"] = ElementType{1};

            Walker w(boy);

            ASSERT_TRUE(toTheDitch(w, "nature", "beetles"));
            EXPECT_EQ(textOf(w.state.fight, "mob"), "slime");

            w.fightWith("guard");

            EXPECT_EQ(w.over(), "held");
            EXPECT_EQ(intOf(w.state.run, "downed"), 0);
            EXPECT_GE(intOf(w.state.run, "hp"), 1);

            ASSERT_TRUE(w.act("back"));
            EXPECT_EQ(w.screen(), "done");
            EXPECT_EQ(intOf(w.character, "page"), 0);
            EXPECT_GE(intOf(w.character, "vit"), 1);
        }

        // A boy with Strength 6 and Dexterity 6 beats the slime with a plain strike in six turns at
        // most, whatever the seed
        TEST(story_test, a_boy_beats_the_slime_with_a_strike)
        {
            MockLogger logger;

            for (int seed = 1; seed <= 20; ++seed)
            {
                Walker w(boyOfEleven());

                ASSERT_TRUE(toTheDitch(w, "nature", "beetles", seed * 37));

                int turns = 0;

                for (; turns < 8 and w.screen() == "fight" and w.over().empty(); ++turns)
                    ASSERT_TRUE(w.act("move", "strike"));

                EXPECT_EQ(w.over(), "won") << "seed " << seed;
                EXPECT_LE(turns, 6) << "seed " << seed;
                EXPECT_GE(turns, 2) << "seed " << seed;
                EXPECT_EQ(w.standing(), 0) << "seed " << seed;
                EXPECT_EQ(intOf(w.character, "page"), 4) << "seed " << seed;
            }
        }

        // The row that opens it: the last month before seven, its story's month, and no board (the
        // lunch feeds him); its plan the warrior's path, as the solver plays it
        TEST(story_test, the_outskirts_are_begun_once_a_life)
        {
            MockLogger logger;
            Walker w(boyOfEleven());
            w.age = 6.0f + 11.0f / 12.0f;

            auto listed = [&]() {
                std::vector<RuleActivity> rows;
                EXPECT_TRUE(w.rules.activities(w.age, w.character, rows));

                for (const auto& row : rows)
                {
                    if (textOf(row.fields, "id") == "outskirts")
                        return intOf(row.fields, "listed") > 0;
                }

                return false;
            };

            ASSERT_TRUE(listed());

            // Entering marks him, and the row is not offered again
            ASSERT_TRUE(w.ask("enter", WalkAct{"story", "outskirts", 0}));
            EXPECT_EQ(intOf(w.character, "outskirts.begun"), 1);
            EXPECT_FALSE(listed());

            // Turned back on the second day: he keeps his lunch, and the mark
            ASSERT_TRUE(w.take("lunch"));
            ASSERT_TRUE(w.act("leave"));
            ASSERT_TRUE(w.act("leave"));

            EXPECT_EQ(w.screen(), "done");
            EXPECT_EQ(textOf(w.state.run, "result"), "left");
            EXPECT_EQ(intOf(w.character, "rations"), 6);
            EXPECT_EQ(intOf(w.character, "outskirts.begun"), 1);
            EXPECT_FALSE(listed());
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(story_test, the_outskirts_row_and_plan)
        {
            MockLogger logger;
            Walker w(boyOfEleven());

            ASSERT_TRUE(w.ask("plan", WalkAct{"", "outskirts", 0}));
            EXPECT_EQ(textOf(w.last.site, "id"), "outskirts");
            EXPECT_EQ(textOf(w.last.site, "row"), "outskirts");
            EXPECT_EQ(intOf(w.last.site, "months"), 1);
            EXPECT_EQ(intOf(w.last.site, "gives.rations"), 6);
            EXPECT_EQ(intOf(w.last.site, "gives.outskirts.animals"), 1);
            // (what a plan gives is said once a stat, the most one tile gives: storylib.pg's planGives)
            EXPECT_EQ(intOf(w.last.site, "gives.str"), 1);
            EXPECT_EQ(intOf(w.last.site, "gives.outskirts.nature"), 0);
            EXPECT_EQ(intOf(w.last.site, "fee.coin"), 0);
        }
    }
}
