#include "stdafx.h"

#include "gtest/gtest.h"

#include "UI/prefab.h"
#include "UI/sizer.h"
#include "ECS/entitysystem.h"

#include "mocklogger.h"

namespace pg
{
    namespace test
    {
        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(prefab_test, prefab_creation)
        {
            EntitySystem ecs;

            ecs.createSystem<PositionComponentSystem>();
            ecs.createSystem<PrefabSystem>();
            ecs.succeed<PositionComponentSystem, PrefabSystem>();

            auto prefabEnt = ecs.createEntity();
            ecs.attach<PositionComponent>(prefabEnt);
            auto prefab = ecs.attach<Prefab>(prefabEnt);

            EXPECT_EQ(prefab->childrenIds.size(), 0);
            EXPECT_EQ(prefab->namedChildrenIds.size(), 0);
            EXPECT_TRUE(prefab->deleteEntityUponRelease);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(prefab_test, prefab_creation_with_factory)
        {
            EntitySystem ecs;

            ecs.createSystem<PositionComponentSystem>();
            ecs.createSystem<PrefabSystem>();
            ecs.succeed<PositionComponentSystem, PrefabSystem>();

            auto prefabEnt = makePrefab(&ecs, 10.0f, 20.0f);

            auto prefab = prefabEnt.get<Prefab>();
            auto pos = prefabEnt.get<PositionComponent>();

            EXPECT_FLOAT_EQ(pos->x, 10.0f);
            EXPECT_FLOAT_EQ(pos->y, 20.0f);
            EXPECT_EQ(prefab->childrenIds.size(), 0);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(prefab_test, prefab_anchored_creation_with_factory)
        {
            EntitySystem ecs;

            ecs.createSystem<PositionComponentSystem>();
            ecs.createSystem<PrefabSystem>();
            ecs.succeed<PositionComponentSystem, PrefabSystem>();

            auto prefabEnt = makeAnchoredPrefab(&ecs, 5.0f, 15.0f, 1.0f);

            auto prefab = prefabEnt.get<Prefab>();
            auto pos = prefabEnt.get<PositionComponent>();

            EXPECT_FLOAT_EQ(pos->x, 5.0f);
            EXPECT_FLOAT_EQ(pos->y, 15.0f);
            EXPECT_FLOAT_EQ(pos->z, 1.0f);
            EXPECT_EQ(prefab->childrenIds.size(), 0);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(prefab_test, add_entity_to_prefab)
        {
            EntitySystem ecs;

            ecs.createSystem<PositionComponentSystem>();
            ecs.createSystem<PrefabSystem>();
            ecs.succeed<PositionComponentSystem, PrefabSystem>();

            auto prefabEnt = ecs.createEntity();
            ecs.attach<PositionComponent>(prefabEnt);
            auto prefab = ecs.attach<Prefab>(prefabEnt);

            EXPECT_EQ(prefab->childrenIds.size(), 0);

            auto childEnt = ecs.createEntity();
            ecs.attach<PositionComponent>(childEnt);

            prefab->addToPrefab(childEnt);

            EXPECT_EQ(prefab->childrenIds.size(), 1);
            EXPECT_TRUE(prefab->childrenIds.count(childEnt.id) > 0);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(prefab_test, add_named_entity_to_prefab)
        {
            EntitySystem ecs;

            ecs.createSystem<PositionComponentSystem>();
            ecs.createSystem<PrefabSystem>();
            ecs.succeed<PositionComponentSystem, PrefabSystem>();

            auto prefabEnt = ecs.createEntity();
            ecs.attach<PositionComponent>(prefabEnt);
            auto prefab = ecs.attach<Prefab>(prefabEnt);

            auto childEnt = ecs.createEntity();
            ecs.attach<PositionComponent>(childEnt);

            prefab->addToPrefab(childEnt, "Header");

            EXPECT_EQ(prefab->childrenIds.size(), 1);
            EXPECT_EQ(prefab->namedChildrenIds.size(), 1);
            EXPECT_TRUE(prefab->namedChildrenIds.count("Header") > 0);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(prefab_test, get_entity_by_name)
        {
            EntitySystem ecs;

            ecs.createSystem<PositionComponentSystem>();
            ecs.createSystem<PrefabSystem>();
            ecs.succeed<PositionComponentSystem, PrefabSystem>();

            auto prefabEnt = ecs.createEntity();
            ecs.attach<PositionComponent>(prefabEnt);
            auto prefab = ecs.attach<Prefab>(prefabEnt);

            auto childEnt = ecs.createEntity();
            ecs.attach<PositionComponent>(childEnt);

            prefab->addToPrefab(childEnt, "Button");

            auto retrieved = prefab->getEntity("Button");

            EXPECT_EQ(retrieved.id, childEnt.id);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(prefab_test, get_unknown_entity_returns_null)
        {
            MockLogger<TerminalSink> logger;

            EntitySystem ecs;

            ecs.createSystem<PositionComponentSystem>();
            ecs.createSystem<PrefabSystem>();
            ecs.succeed<PositionComponentSystem, PrefabSystem>();

            auto prefabEnt = ecs.createEntity();
            ecs.attach<PositionComponent>(prefabEnt);
            auto prefab = ecs.attach<Prefab>(prefabEnt);

            auto result = prefab->getEntity("NonExistent");

            EXPECT_TRUE(result.empty());
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(prefab_test, add_multiple_children)
        {
            EntitySystem ecs;

            ecs.createSystem<PositionComponentSystem>();
            ecs.createSystem<PrefabSystem>();
            ecs.succeed<PositionComponentSystem, PrefabSystem>();

            auto prefabEnt = ecs.createEntity();
            ecs.attach<PositionComponent>(prefabEnt);
            auto prefab = ecs.attach<Prefab>(prefabEnt);

            for (int i = 0; i < 3; ++i)
            {
                auto childEnt = ecs.createEntity();
                ecs.attach<PositionComponent>(childEnt);
                prefab->addToPrefab(childEnt);
            }

            EXPECT_EQ(prefab->childrenIds.size(), 3);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(prefab_test, clear_prefab_on_deletion)
        {
            EntitySystem ecs;

            ecs.createSystem<PositionComponentSystem>();
            ecs.createSystem<PrefabSystem>();
            ecs.succeed<PositionComponentSystem, PrefabSystem>();

            auto prefabEnt = ecs.createEntity();
            ecs.attach<PositionComponent>(prefabEnt);
            auto prefab = ecs.attach<Prefab>(prefabEnt);

            auto childEnt = ecs.createEntity();
            ecs.attach<PositionComponent>(childEnt);
            prefab->addToPrefab(childEnt);

            auto childId = childEnt.id;

            EXPECT_TRUE(ecs.getEntity(childId));

            ecs.removeEntity(prefabEnt);

            ecs.executeOnce();

            EXPECT_FALSE(ecs.getEntity(childId));
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(prefab_test, no_clear_on_deletion_when_disabled)
        {
            EntitySystem ecs;

            ecs.createSystem<PositionComponentSystem>();
            ecs.createSystem<PrefabSystem>();
            ecs.succeed<PositionComponentSystem, PrefabSystem>();

            auto prefabEnt = ecs.createEntity();
            ecs.attach<PositionComponent>(prefabEnt);
            auto prefab = ecs.attach<Prefab>(prefabEnt);

            prefab->deleteEntityUponRelease = false;

            auto childEnt = ecs.createEntity();
            ecs.attach<PositionComponent>(childEnt);
            prefab->addToPrefab(childEnt);

            auto childId = childEnt.id;

            ecs.removeEntity(prefabEnt);

            ecs.executeOnce();

            // Child should still exist because deleteEntityUponRelease was disabled
            EXPECT_TRUE(ecs.getEntity(childId));
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(prefab_test, update_child_observable_on_prefab_visibility)
        {
            EntitySystem ecs;

            ecs.createSystem<PositionComponentSystem>();
            ecs.createSystem<PrefabSystem>();
            ecs.succeed<PositionComponentSystem, PrefabSystem>();

            auto prefabEnt = ecs.createEntity();
            auto prefabPos = ecs.attach<PositionComponent>(prefabEnt);
            auto prefab = ecs.attach<Prefab>(prefabEnt);

            prefabPos->setX(0.0f);
            prefabPos->setY(0.0f);
            prefabPos->setWidth(100.0f);
            prefabPos->setHeight(100.0f);

            auto childEnt = ecs.createEntity();
            auto childPos = ecs.attach<PositionComponent>(childEnt);
            prefab->addToPrefab(childEnt);

            ecs.executeOnce();

            EXPECT_TRUE(childPos->observable);

            // Hiding the prefab should propagate to child
            prefabPos->setVisibility(false);

            ecs.executeOnce();

            EXPECT_FALSE(childPos->observable);

            // Showing it again should restore the child
            prefabPos->setVisibility(true);

            ecs.executeOnce();

            EXPECT_TRUE(childPos->observable);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(prefab_test, set_main_entity)
        {
            EntitySystem ecs;

            ecs.createSystem<PositionComponentSystem>();
            ecs.createSystem<PrefabSystem>();
            ecs.succeed<PositionComponentSystem, PrefabSystem>();

            // makeAnchoredPrefab gives the prefab a UiAnchor, required by SetMainEntityEvent handler
            auto prefabComp = makeAnchoredPrefab(&ecs, 0.0f, 0.0f, 0.0f);
            auto prefab = prefabComp.get<Prefab>();

            auto childEnt = ecs.createEntity();
            auto childPos = ecs.attach<PositionComponent>(childEnt);
            ecs.attach<UiAnchor>(childEnt);

            childPos->setWidth(100.0f);
            childPos->setHeight(50.0f);

            prefab->setMainEntity(childEnt);

            ecs.executeOnce();

            // After event processing, child is registered under "MainEntity"
            EXPECT_EQ(prefab->childrenIds.size(), 1);

            auto mainEnt = prefab->getEntity("MainEntity");
            EXPECT_EQ(mainEnt.id, childEnt.id);
        }
    }
}