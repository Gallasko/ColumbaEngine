#include "stdafx.h"

#include <gtest/gtest.h>

#include "Systems/achievement.h"
#include "Systems/gamefacts.h"

#include "ECS/entitysystem.h"

#include "mocklogger.h"
#include "factfeed.h"

namespace pg
{
    namespace test
    {
        namespace
        {
            // Records the name carried by every unlock event
            struct UnlockRecorder : public System<Listener<StandardEvent>, InitSys, StoragePolicy>
            {
                virtual std::string getSystemName() const override { return "Unlock Recorder"; }

                virtual void init() override
                {
                    addListenerToStandardEvent(AchievementUnlockEventName);
                }

                virtual void onEvent(const StandardEvent& event) override
                {
                    if (event.name != AchievementUnlockEventName)
                        return;

                    unlocked.push_back(event.values.at("name").toString());
                }

                std::vector<std::string> unlocked;
            };

            // Like the facts, the system would load from and save to save/systems.sz
            AchievementSys* createTestAchievements(EntitySystem *ecs)
            {
                auto achievements = ecs->createSystem<AchievementSys>();

                ecs->getComponentRegistry()->unregisterSystemSave(achievements->getSystemName());

                achievements->clear();

                return achievements;
            }

            Achievement makeAchievement(const std::string& name, const std::vector<FactChecker>& asks)
            {
                Achievement achievement;

                achievement.name = name;
                achievement.prerequisiteFacts = asks;

                return achievement;
            }

            void frames(EntitySystem& ecs, int nbFrames)
            {
                for (int i = 0; i < nbFrames; ++i)
                    ecs.executeOnce();
            }
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(achievement_test, unlock_on_fact_change)
        {
            MockLogger logger;
            EntitySystem ecs;

            auto facts = createTestFacts(&ecs);
            auto achievements = createTestAchievements(&ecs);
            auto recorder = ecs.createSystem<UnlockRecorder>();

            achievements->addNewAchivement(makeAchievement("rich", {FactChecker("gold", 10, FactCheckEquality::GreaterEqual)}));

            frames(ecs, 3);

            EXPECT_EQ(achievements->achievementLocked.size(), 1u);
            EXPECT_TRUE(achievements->achievementUnlocked.empty());

            ecs.sendEvent(AddFact{"gold", ElementType{4}});
            frames(ecs, 3);

            EXPECT_TRUE(achievements->achievementUnlocked.empty());

            ecs.sendEvent(IncreaseFact{"gold", 6});
            frames(ecs, 3);

            ASSERT_EQ(achievements->achievementUnlocked.size(), 1u);
            EXPECT_EQ(achievements->achievementUnlocked[0].name, "rich");
            EXPECT_TRUE(achievements->achievementUnlocked[0].unlocked);
            EXPECT_TRUE(achievements->achievementLocked.empty());

            EXPECT_TRUE(facts->getFact<bool>("rich_unlocked", false));

            ASSERT_EQ(recorder->unlocked.size(), 1u);
            EXPECT_EQ(recorder->unlocked[0], "rich");

            // Unlocked once: a later change does not send it again
            ecs.sendEvent(IncreaseFact{"gold", 6});
            frames(ecs, 3);

            EXPECT_EQ(recorder->unlocked.size(), 1u);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(achievement_test, every_prerequisite_is_needed)
        {
            MockLogger logger;
            EntitySystem ecs;

            createTestFacts(&ecs);
            auto achievements = createTestAchievements(&ecs);

            achievements->addNewAchivement(makeAchievement("knight", {
                FactChecker("strength", 18, FactCheckEquality::GreaterEqual),
                FactChecker("sword", 4, FactCheckEquality::GreaterEqual)}));

            frames(ecs, 3);

            ecs.sendEvent(AddFact{"strength", ElementType{18}});
            frames(ecs, 3);

            EXPECT_TRUE(achievements->achievementUnlocked.empty());

            ecs.sendEvent(AddFact{"sword", ElementType{4}});
            frames(ecs, 3);

            ASSERT_EQ(achievements->achievementUnlocked.size(), 1u);
            EXPECT_EQ(achievements->achievementUnlocked[0].name, "knight");
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(achievement_test, already_true_when_added)
        {
            MockLogger logger;
            EntitySystem ecs;

            auto facts = createTestFacts(&ecs);
            auto achievements = createTestAchievements(&ecs);
            auto recorder = ecs.createSystem<UnlockRecorder>();

            ecs.sendEvent(AddFact{"gold", ElementType{50}});
            frames(ecs, 3);

            achievements->addNewAchivement(makeAchievement("rich", {FactChecker("gold", 10, FactCheckEquality::GreaterEqual)}));
            frames(ecs, 3);

            ASSERT_EQ(achievements->achievementUnlocked.size(), 1u);
            EXPECT_TRUE(achievements->achievementUnlocked[0].unlocked);
            EXPECT_TRUE(achievements->achievementLocked.empty());

            EXPECT_TRUE(facts->getFact<bool>("rich_unlocked", false));
            EXPECT_EQ(recorder->unlocked.size(), 1u);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(achievement_test, rewards_are_sent)
        {
            MockLogger logger;
            EntitySystem ecs;

            auto facts = createTestFacts(&ecs);
            auto achievements = createTestAchievements(&ecs);

            ecs.sendEvent(AddFact{"gems", ElementType{1}});
            ecs.sendEvent(AddFact{"curse", ElementType{true}});

            auto achievement = makeAchievement("rich", {FactChecker("gold", 10, FactCheckEquality::GreaterEqual)});

            achievement.rewardFacts.push_back(AchievementReward(AddFact{"title", ElementType{std::string("Merchant")}}));
            achievement.rewardFacts.push_back(AchievementReward(IncreaseFact{"gems", 2}));
            achievement.rewardFacts.push_back(AchievementReward(RemoveFact{"curse"}));

            achievements->addNewAchivement(achievement);
            frames(ecs, 3);

            ecs.sendEvent(AddFact{"gold", ElementType{10}});
            frames(ecs, 3);

            EXPECT_EQ(facts->getFact<std::string>("title", ""), "Merchant");
            EXPECT_EQ(facts->getFact<int>("gems"), 3);
            EXPECT_FALSE(facts->hasFact("curse"));
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(achievement_test, clear_forgets_everything)
        {
            MockLogger logger;
            EntitySystem ecs;

            createTestFacts(&ecs);
            auto achievements = createTestAchievements(&ecs);
            auto recorder = ecs.createSystem<UnlockRecorder>();

            achievements->addNewAchivement(makeAchievement("rich", {FactChecker("gold", 10, FactCheckEquality::GreaterEqual)}));
            achievements->addNewAchivement(makeAchievement("first", {FactChecker("gold", 1, FactCheckEquality::GreaterEqual)}));
            frames(ecs, 3);

            ecs.sendEvent(AddFact{"gold", ElementType{1}});
            frames(ecs, 3);

            EXPECT_EQ(achievements->achievementUnlocked.size(), 1u);
            EXPECT_EQ(achievements->achievementLocked.size(), 1u);

            achievements->clear();

            EXPECT_TRUE(achievements->achievementUnlocked.empty());
            EXPECT_TRUE(achievements->achievementLocked.empty());
            EXPECT_TRUE(achievements->achievementToResolve.empty());

            // Nothing is waited on any more
            ecs.sendEvent(AddFact{"gold", ElementType{100}});
            frames(ecs, 3);

            EXPECT_EQ(recorder->unlocked.size(), 1u);
            EXPECT_TRUE(achievements->achievementUnlocked.empty());
        }
    }
}
