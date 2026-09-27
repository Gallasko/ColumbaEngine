#include "stdafx.h"

#include <gtest/gtest.h>

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
            // Records the name of every fact carried by a WorldFactsUpdate
            struct FactRecorder : public System<Listener<WorldFactsUpdate>, StoragePolicy>
            {
                virtual std::string getSystemName() const override { return "Fact Recorder"; }

                virtual void onEvent(const WorldFactsUpdate& event) override
                {
                    ++nbUpdates;

                    for (const auto& name : event.changedFacts)
                        changed.push_back(name);
                }

                size_t nbUpdates = 0;

                std::vector<std::string> changed;
            };
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(gamefacts_test, add_fact)
        {
            MockLogger logger;
            EntitySystem ecs;

            auto facts = createTestFacts(&ecs);

            EXPECT_FALSE(facts->hasFact("gold"));
            EXPECT_EQ(facts->getFact<int>("gold", 3), 3);

            ecs.sendEvent(AddFact{"gold", ElementType{10}});

            EXPECT_TRUE(facts->hasFact("gold"));
            EXPECT_EQ(facts->getFact<int>("gold"), 10);

            facts->setFact("name", std::string("Bellmoor"));

            EXPECT_EQ(facts->getFact<std::string>("name"), "Bellmoor");
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(gamefacts_test, increase_fact)
        {
            MockLogger logger;
            EntitySystem ecs;

            auto facts = createTestFacts(&ecs);

            // A missing fact starts at the increase
            ecs.sendEvent(IncreaseFact{"gold", 5});

            EXPECT_EQ(facts->getFact<int>("gold"), 5);

            ecs.sendEvent(IncreaseFact{"gold", 7});
            facts->increaseFact("gold", -2);

            EXPECT_EQ(facts->getFact<int>("gold"), 10);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(gamefacts_test, remove_fact)
        {
            MockLogger logger;
            EntitySystem ecs;

            auto facts = createTestFacts(&ecs);

            facts->setFact("gold", 10);
            ecs.sendEvent(RemoveFact{"gold"});

            EXPECT_FALSE(facts->hasFact("gold"));
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(gamefacts_test, default_fact_keeps_existing_value)
        {
            MockLogger logger;
            EntitySystem ecs;

            auto facts = createTestFacts(&ecs);

            facts->setDefaultFact("gold", 4);
            facts->setDefaultFact("gold", 9);

            EXPECT_EQ(facts->getFact<int>("gold"), 4);

            facts->setFact("wood", 2);
            facts->setFactIfNotExists("wood", 8);

            EXPECT_EQ(facts->getFact<int>("wood"), 2);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(gamefacts_test, update_lists_changed_facts)
        {
            MockLogger logger;
            EntitySystem ecs;

            auto facts = createTestFacts(&ecs);
            auto recorder = ecs.createSystem<FactRecorder>();

            facts->setFact("gold", 10);
            facts->increaseFact("wood", 2);

            // The update is sent on the first pass and delivered on the next one
            ecs.executeOnce();
            ecs.executeOnce();

            ASSERT_EQ(recorder->nbUpdates, 1u);
            ASSERT_EQ(recorder->changed.size(), 2u);
            EXPECT_EQ(recorder->changed[0], "gold");
            EXPECT_EQ(recorder->changed[1], "wood");

            // Nothing changed, nothing is sent
            ecs.executeOnce();
            ecs.executeOnce();

            EXPECT_EQ(recorder->nbUpdates, 1u);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(gamefacts_test, fact_checker)
        {
            MockLogger logger;

            std::unordered_map<std::string, ElementType> map;

            map["strength"] = ElementType{14};

            EXPECT_TRUE(FactChecker("strength", 14, FactCheckEquality::Equal).check(map));
            EXPECT_TRUE(FactChecker("strength", 18, FactCheckEquality::Lesser).check(map));
            EXPECT_TRUE(FactChecker("strength", 14, FactCheckEquality::GreaterEqual).check(map));
            EXPECT_FALSE(FactChecker("strength", 14, FactCheckEquality::NotEqual).check(map));
            EXPECT_FALSE(FactChecker("strength", 14, FactCheckEquality::None).check(map));

            // A missing fact never passes
            EXPECT_FALSE(FactChecker("dexterity", 0, FactCheckEquality::GreaterEqual).check(map));
        }
    }
}
