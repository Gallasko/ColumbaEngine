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
        TEST(prefab_test, helpers_die_with_their_prefab)
        {
            MockLogger logger;
            EntitySystem ecs;

            ecs.createSystem<PositionComponentSystem>();
            auto* sys = ecs.createSystem<PrefabSystem>();
            ecs.succeed<PositionComponentSystem, PrefabSystem>();

            auto prefabEnt = makeAnchoredPrefab(&ecs);
            auto prefab = prefabEnt.get<Prefab>();
            const _unique_id id = prefabEnt.entity.id;

            prefab->addHelper("ping", [](Prefab*) -> int { return 7; });

            EXPECT_TRUE(prefab->hasHelper("ping"));
            EXPECT_FALSE(prefab->hasHelper("pong"));
            EXPECT_EQ(prefab->callHelper<int>("ping"), 7);
            EXPECT_EQ(sys->helperRegistry.count(id), 1u);

            ecs.removeEntity(id);

            EXPECT_EQ(sys->helperRegistry.count(id), 0u);
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

            auto prefabId = prefabEnt.id;

            auto childId = childEnt.id;

            EXPECT_TRUE(ecs.getEntity(childId));

            ecs.removeEntity(prefabEnt);

            // First the prefab is deleted
            ecs.executeOnce();

            EXPECT_FALSE(ecs.getEntity(prefabId));

            // Then at next exec all the child in the prefab are cleaned up
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
        // A prefab hidden a while ago sends no position change: what is added to it then has only
        // the add itself to be told by
        TEST(prefab_test, child_added_to_a_hidden_prefab_is_hidden)
        {
            EntitySystem ecs;

            ecs.createSystem<PositionComponentSystem>();
            ecs.createSystem<PrefabSystem>();
            ecs.succeed<PositionComponentSystem, PrefabSystem>();

            auto prefabEnt = ecs.createEntity();
            auto prefabPos = ecs.attach<PositionComponent>(prefabEnt);
            auto prefab = ecs.attach<Prefab>(prefabEnt);

            prefabPos->setWidth(100.0f);
            prefabPos->setHeight(100.0f);
            prefabPos->setVisibility(false);

            ecs.executeOnce();

            auto childEnt = ecs.createEntity();
            auto childPos = ecs.attach<PositionComponent>(childEnt);
            prefab->addToPrefab(childEnt);

            ecs.executeOnce();

            EXPECT_FALSE(childPos->observable);

            // A prefab of its own, with what it holds, added after: hidden to the last part
            auto innerEnt = ecs.createEntity();
            auto innerPos = ecs.attach<PositionComponent>(innerEnt);
            auto inner = ecs.attach<Prefab>(innerEnt);

            auto partEnt = ecs.createEntity();
            auto partPos = ecs.attach<PositionComponent>(partEnt);
            inner->addToPrefab(partEnt);

            ecs.executeOnce();

            EXPECT_TRUE(partPos->observable);

            prefab->addToPrefab(innerEnt);

            for (size_t i = 0; i < 3; ++i)
                ecs.executeOnce();

            EXPECT_FALSE(innerPos->observable);
            EXPECT_FALSE(partPos->observable);

            // Shown again, all of it comes back
            prefabPos->setVisibility(true);

            for (size_t i = 0; i < 3; ++i)
                ecs.executeOnce();

            EXPECT_TRUE(childPos->observable);
            EXPECT_TRUE(innerPos->observable);
            EXPECT_TRUE(partPos->observable);
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

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // Regression test: setMainEntity used fillIn() which created a circular anchor dependency
        // (prefab.width = main.width, main.right = prefab.right -> main.width = prefab.width).
        // The anchor system resolved the cycle to 0, making everything invisible.
        // Fix: only anchor main entity's top+left to the prefab, preserving its own size.
        TEST(prefab_test, set_main_entity_size_not_collapsed)
        {
            EntitySystem ecs;

            ecs.createSystem<PositionComponentSystem>();
            ecs.createSystem<PrefabSystem>();
            ecs.succeed<PositionComponentSystem, PrefabSystem>();

            auto prefabComp = makeAnchoredPrefab(&ecs, 0.0f, 0.0f, 0.0f);
            auto prefab = prefabComp.get<Prefab>();

            auto mainEnt = ecs.createEntity();
            auto mainPos = ecs.attach<PositionComponent>(mainEnt);
            ecs.attach<UiAnchor>(mainEnt);

            mainPos->setWidth(32.0f);
            mainPos->setHeight(32.0f);

            prefab->setMainEntity(mainEnt);

            ecs.executeOnce();

            // Main entity must keep its own size - not collapse to 0 due to circular anchor
            EXPECT_FLOAT_EQ(mainPos->width, 32.0f);
            EXPECT_FLOAT_EQ(mainPos->height, 32.0f);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(prefab_test, set_main_entity_position_follows_prefab)
        {
            EntitySystem ecs;

            ecs.createSystem<PositionComponentSystem>();
            ecs.createSystem<PrefabSystem>();
            ecs.succeed<PositionComponentSystem, PrefabSystem>();

            auto prefabComp = makeAnchoredPrefab(&ecs, 100.0f, 200.0f, 0.0f);
            auto prefab = prefabComp.get<Prefab>();

            auto mainEnt = ecs.createEntity();
            auto mainPos = ecs.attach<PositionComponent>(mainEnt);
            ecs.attach<UiAnchor>(mainEnt);

            mainPos->setWidth(32.0f);
            mainPos->setHeight(16.0f);

            prefab->setMainEntity(mainEnt);

            ecs.executeOnce();

            // Main entity is anchored to the prefab's top-left, so it follows its position
            EXPECT_FLOAT_EQ(mainPos->x, 100.0f);
            EXPECT_FLOAT_EQ(mainPos->y, 200.0f);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(prefab_test, set_main_entity_prefab_adopts_main_entity_size)
        {
            EntitySystem ecs;

            ecs.createSystem<PositionComponentSystem>();
            ecs.createSystem<PrefabSystem>();
            ecs.succeed<PositionComponentSystem, PrefabSystem>();

            auto prefabComp = makeAnchoredPrefab(&ecs, 0.0f, 0.0f, 0.0f);
            auto prefab = prefabComp.get<Prefab>();
            auto prefabPos = prefabComp.get<PositionComponent>();

            auto mainEnt = ecs.createEntity();
            auto mainPos = ecs.attach<PositionComponent>(mainEnt);
            ecs.attach<UiAnchor>(mainEnt);

            mainPos->setWidth(64.0f);
            mainPos->setHeight(48.0f);

            prefab->setMainEntity(mainEnt);

            ecs.executeOnce();

            // Prefab's size is driven by the main entity's size via width/height constrains
            EXPECT_FLOAT_EQ(prefabPos->width, 64.0f);
            EXPECT_FLOAT_EQ(prefabPos->height, 48.0f);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // The prefab builder registers the children of a node in the node's container, and puts
        // them in the layout the node exposes: two owners. The layout is the one that decides
        TEST(prefab_test, leaves_a_layout_child_to_its_layout)
        {
            MockLogger logger;
            EntitySystem ecs;

            ecs.createSystem<PositionComponentSystem>();
            ecs.createSystem<LayoutSystem>();
            ecs.createSystem<PrefabSystem>();
            ecs.succeed<PositionComponentSystem, LayoutSystem>();
            ecs.succeed<PositionComponentSystem, PrefabSystem>();

            // 50 tall and scrolling: it clips its children and hides the ones out of view
            auto layout = makeVerticalLayout(&ecs, 0.0f, 0.0f, 100.0f, 50.0f, true);

            // Not clipped itself
            auto prefabComp = makeAnchoredPrefab(&ecs, 0.0f, 0.0f, 0.0f);
            auto prefab = prefabComp.get<Prefab>();

            std::vector<EntityRef> children;

            for (size_t i = 0; i < 3; ++i)
            {
                auto child = ecs.createEntity();
                auto childPos = ecs.attach<PositionComponent>(child);
                ecs.attach<UiAnchor>(child);
                childPos->setWidth(100.0f);
                childPos->setHeight(30.0f);

                prefab->addToPrefab(child);
                layout.get<VerticalLayout>()->addEntity(child);

                children.push_back(child);
            }

            for (size_t i = 0; i < 5; ++i)
                ecs.executeOnce();

            for (auto& child : children)
            {
                ASSERT_TRUE(child->has<ClippedTo>());
                EXPECT_EQ(child->get<ClippedTo>()->clipperId, layout.entity.id);
            }

            // The third one starts at 60, under the layout's 50
            EXPECT_TRUE(children[0]->get<PositionComponent>()->observable);
            EXPECT_FALSE(children[2]->get<PositionComponent>()->observable);

            // The prefab updates its children when it changes
            prefabComp.get<PositionComponent>()->setX(5.0f);

            for (size_t i = 0; i < 5; ++i)
                ecs.executeOnce();

            for (auto& child : children)
                EXPECT_TRUE(child->has<ClippedTo>());

            EXPECT_TRUE(children[0]->get<PositionComponent>()->observable);
            EXPECT_FALSE(children[2]->get<PositionComponent>()->observable);
        }
    }
}
