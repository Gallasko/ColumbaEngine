#include "stdafx.h"

#include "gtest/gtest.h"

#include "UI/sizer.h"
#include "ECS/entitysystem.h"

namespace pg
{
    namespace test
    {
        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(layout_test, add_entities_to_layout)
        {
            EntitySystem ecs;

            ecs.createSystem<PositionComponentSystem>();
            auto layoutSystem = ecs.createSystem<LayoutSystem>();
            ecs.succeed<PositionComponentSystem, LayoutSystem>();

            // Create a horizontal layout
            auto layoutEntity = ecs.createEntity();
            auto layout = ecs.attach<HorizontalLayout>(layoutEntity);
            auto layoutPos = ecs.attach<PositionComponent>(layoutEntity);
            ecs.attach<UiAnchor>(layoutEntity);

            layoutPos->setX(0.0f);
            layoutPos->setY(0.0f);
            layoutPos->setWidth(200.0f);
            layoutPos->setHeight(50.0f);

            // Create an entity to add to the layout
            auto childEntity = ecs.createEntity();
            auto childPos = ecs.attach<PositionComponent>(childEntity);
            ecs.attach<UiAnchor>(childEntity);

            childPos->setWidth(50.0f);
            childPos->setHeight(50.0f);

            EXPECT_EQ(layout->entities.size(), 0);

            // Add the child entity to the layout
            layout->addEntity(childEntity);

            layoutSystem->_execute();

            EXPECT_EQ(layout->entities.size(), 1);
            EXPECT_EQ(layout->entities[0].id, childEntity.id);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(layout_test, add_entities_to_layout_with_helper)
        {
            EntitySystem ecs;

            ecs.createSystem<PositionComponentSystem>();
            auto layoutSystem = ecs.createSystem<LayoutSystem>();
            ecs.succeed<PositionComponentSystem, LayoutSystem>();

            // Create a horizontal layout using makeLayout
            auto layoutEntity = makeHorizontalLayout(&ecs, 0.0f, 0.0f, 200.0f, 50.0f);
            auto layout = layoutEntity.get<HorizontalLayout>();

            // Create an entity to add to the layout
            auto childEntity = ecs.createEntity();
            auto childPos = ecs.attach<PositionComponent>(childEntity);
            ecs.attach<UiAnchor>(childEntity);

            childPos->setWidth(50.0f);
            childPos->setHeight(50.0f);

            EXPECT_EQ(layout->entities.size(), 0);

            // Add the child entity to the layout
            layout->addEntity(childEntity);

            layoutSystem->_execute();

            EXPECT_EQ(layout->entities.size(), 1);
            EXPECT_EQ(layout->entities[0].id, childEntity.id);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(layout_test, remove_entities_from_layout)
        {
            EntitySystem ecs;

            ecs.createSystem<PositionComponentSystem>();
            auto layoutSystem = ecs.createSystem<LayoutSystem>();

            // Create a vertical layout
            auto layoutEntity = ecs.createEntity();
            auto layout = ecs.attach<VerticalLayout>(layoutEntity);
            auto layoutPos = ecs.attach<PositionComponent>(layoutEntity);
            ecs.attach<UiAnchor>(layoutEntity);

            layoutPos->setX(0.0f);
            layoutPos->setY(0.0f);
            layoutPos->setWidth(100.0f);
            layoutPos->setHeight(300.0f);

            // Create entities to add to the layout
            auto childEntity1 = ecs.createEntity();
            ecs.attach<PositionComponent>(childEntity1);
            ecs.attach<UiAnchor>(childEntity1);

            auto childEntity2 = ecs.createEntity();
            ecs.attach<PositionComponent>(childEntity2);
            ecs.attach<UiAnchor>(childEntity2);

            EXPECT_EQ(layout->entities.size(), 0);

            layout->addEntity(childEntity1);
            layout->addEntity(childEntity2);

            layoutSystem->_execute();

            EXPECT_EQ(layout->entities.size(), 2);

            // Remove the first child entity
            layout->removeEntity(childEntity1);

            layoutSystem->_execute();

            EXPECT_EQ(layout->entities.size(), 1);
            EXPECT_EQ(layout->entities[0].id, childEntity2.id);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(layout_test, update_visibility)
        {
            EntitySystem ecs;

            ecs.createSystem<PositionComponentSystem>();
            ecs.createSystem<LayoutSystem>();

            // Create a horizontal layout
            auto layoutEntity = ecs.createEntity();
            auto layout = ecs.attach<HorizontalLayout>(layoutEntity);
            auto layoutPos = ecs.attach<PositionComponent>(layoutEntity);
            ecs.attach<UiAnchor>(layoutEntity);

            layoutPos->setX(0.0f);
            layoutPos->setY(0.0f);
            layoutPos->setWidth(300.0f);
            layoutPos->setHeight(100.0f);

            // Create an entity to add to the layout
            auto childEntity = ecs.createEntity();
            auto childPos = ecs.attach<PositionComponent>(childEntity);
            ecs.attach<UiAnchor>(childEntity);

            childPos->setWidth(50.0f);
            childPos->setHeight(50.0f);

            layout->addEntity(childEntity);

            ecs.executeOnce();

            EXPECT_TRUE(childPos->visible);
            EXPECT_TRUE(childPos->observable);

            // Update visibility
            layoutPos->setVisibility(false);

            ecs.executeOnce();

            EXPECT_TRUE(childPos->visible);
            EXPECT_FALSE(childPos->observable);

            layoutPos->setVisibility(true);

            ecs.executeOnce();

            EXPECT_TRUE(childPos->visible);
            EXPECT_TRUE(childPos->observable);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(layout_test, recalculate_layout)
        {
            EntitySystem ecs;

            ecs.createSystem<PositionComponentSystem>();
            ecs.createSystem<LayoutSystem>();
            ecs.succeed<PositionComponentSystem, LayoutSystem>();

            // Create a vertical layout
            auto layoutEntity = ecs.createEntity();
            auto layout = ecs.attach<VerticalLayout>(layoutEntity);
            auto layoutPos = ecs.attach<PositionComponent>(layoutEntity);
            auto layoutAnchor = ecs.attach<UiAnchor>(layoutEntity);

            layoutPos->setX(0.0f);
            layoutPos->setY(0.0f);
            layoutPos->setWidth(100.0f);
            layoutPos->setHeight(300.0f);

            ecs.executeOnce();

            EXPECT_FLOAT_EQ(layoutPos->x, 0.0f);
            EXPECT_FLOAT_EQ(layoutPos->y, 0.0f);
            EXPECT_FLOAT_EQ(layoutPos->width, 100.0f);
            EXPECT_FLOAT_EQ(layoutPos->height, 300.0f);
            EXPECT_FLOAT_EQ(layoutAnchor->left.value, 0.0f);
            EXPECT_FLOAT_EQ(layoutAnchor->top.value, 0.0f);
            EXPECT_FLOAT_EQ(layoutAnchor->right.value, 100.0f);
            EXPECT_FLOAT_EQ(layoutAnchor->bottom.value, 300.0f);

            // Create entities to add to the layout
            auto childEntity1 = ecs.createEntity();
            auto childPos1 = ecs.attach<PositionComponent>(childEntity1);
            ecs.attach<UiAnchor>(childEntity1);

            auto childEntity2 = ecs.createEntity();
            auto childPos2 = ecs.attach<PositionComponent>(childEntity2);
            ecs.attach<UiAnchor>(childEntity2);

            childPos1->setWidth(50.0f);
            childPos1->setHeight(50.0f);

            childPos2->setWidth(50.0f);
            childPos2->setHeight(50.0f);

            layout->addEntity(childEntity1);
            layout->addEntity(childEntity2);

            ecs.executeOnce();

            EXPECT_FLOAT_EQ(layoutPos->x, 0.0f);
            EXPECT_FLOAT_EQ(layoutPos->y, 0.0f);
            EXPECT_FLOAT_EQ(layoutPos->width, 100.0f);
            EXPECT_FLOAT_EQ(layoutPos->height, 300.0f);
            EXPECT_FLOAT_EQ(layoutAnchor->left.value, 0.0f);
            EXPECT_FLOAT_EQ(layoutAnchor->top.value, 0.0f);
            EXPECT_FLOAT_EQ(layoutAnchor->right.value, 100.0f);
            EXPECT_FLOAT_EQ(layoutAnchor->bottom.value, 300.0f);

            // Check positions after recalculation
            EXPECT_FLOAT_EQ(childPos1->x, 0.0f);
            EXPECT_FLOAT_EQ(childPos1->y, 0.0f);

            EXPECT_FLOAT_EQ(childPos2->x, 0.0f);
            EXPECT_FLOAT_EQ(childPos2->y, 50.0f); // Positioned below the first entity
        }

        TEST(layout_test, recalculate_layout_sized_to_children)
        {
            EntitySystem ecs;

            ecs.createSystem<PositionComponentSystem>();
            ecs.createSystem<LayoutSystem>();
            ecs.succeed<PositionComponentSystem, LayoutSystem>();

            // Create a vertical layout
            auto layoutEntity = ecs.createEntity();
            auto layout = ecs.attach<VerticalLayout>(layoutEntity);
            auto layoutPos = ecs.attach<PositionComponent>(layoutEntity);
            auto layoutAnchor = ecs.attach<UiAnchor>(layoutEntity);

            layoutPos->setX(0.0f);
            layoutPos->setY(0.0f);
            layoutPos->setWidth(100.0f);
            layoutPos->setHeight(300.0f);

            layout->scrollable = false;

            ecs.executeOnce();

            EXPECT_FLOAT_EQ(layoutPos->x, 0.0f);
            EXPECT_FLOAT_EQ(layoutPos->y, 0.0f);
            EXPECT_FLOAT_EQ(layoutPos->width, 100.0f);
            EXPECT_FLOAT_EQ(layoutPos->height, 300.0f);
            EXPECT_FLOAT_EQ(layoutAnchor->left.value, 0.0f);
            EXPECT_FLOAT_EQ(layoutAnchor->top.value, 0.0f);
            EXPECT_FLOAT_EQ(layoutAnchor->right.value, 100.0f);
            EXPECT_FLOAT_EQ(layoutAnchor->bottom.value, 300.0f);

            // Create entities to add to the layout
            auto childEntity1 = ecs.createEntity();
            auto childPos1 = ecs.attach<PositionComponent>(childEntity1);
            ecs.attach<UiAnchor>(childEntity1);

            auto childEntity2 = ecs.createEntity();
            auto childPos2 = ecs.attach<PositionComponent>(childEntity2);
            ecs.attach<UiAnchor>(childEntity2);

            childPos1->setWidth(50.0f);
            childPos1->setHeight(50.0f);

            childPos2->setWidth(50.0f);
            childPos2->setHeight(50.0f);

            layout->addEntity(childEntity1);
            layout->addEntity(childEntity2);

            ecs.executeOnce();

            EXPECT_FLOAT_EQ(layoutPos->x, 0.0f);
            EXPECT_FLOAT_EQ(layoutPos->y, 0.0f);
            EXPECT_FLOAT_EQ(layoutPos->width, 50.0f);
            EXPECT_FLOAT_EQ(layoutPos->height, 100.0f);
            EXPECT_FLOAT_EQ(layoutAnchor->left.value, 0.0f);
            EXPECT_FLOAT_EQ(layoutAnchor->top.value, 0.0f);
            EXPECT_FLOAT_EQ(layoutAnchor->right.value, 50.0f);
            EXPECT_FLOAT_EQ(layoutAnchor->bottom.value, 100.0f);

            // Check positions after recalculation
            EXPECT_FLOAT_EQ(childPos1->x, 0.0f);
            EXPECT_FLOAT_EQ(childPos1->y, 0.0f);

            EXPECT_FLOAT_EQ(childPos2->x, 0.0f);
            EXPECT_FLOAT_EQ(childPos2->y, 50.0f); // Positioned below the first entity
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(layout_test, recalculate_layout_spaced)
        {
            EntitySystem ecs;

            ecs.createSystem<PositionComponentSystem>();
            ecs.createSystem<LayoutSystem>();
            ecs.succeed<PositionComponentSystem, LayoutSystem>();
            // ecs.succeed<LayoutSystem, PositionComponentSystem>();

            // Create a vertical layout
            auto layoutEntity = ecs.createEntity();
            auto layout = ecs.attach<VerticalLayout>(layoutEntity);
            auto layoutPos = ecs.attach<PositionComponent>(layoutEntity);
            auto layoutAnchor = ecs.attach<UiAnchor>(layoutEntity);

            layoutPos->setX(0.0f);
            layoutPos->setY(0.0f);
            layoutPos->setWidth(100.0f);
            layoutPos->setHeight(300.0f);

            layout->spaced = true;

            ecs.executeOnce();

            EXPECT_FLOAT_EQ(layoutPos->x, 0.0f);
            EXPECT_FLOAT_EQ(layoutPos->y, 0.0f);
            EXPECT_FLOAT_EQ(layoutPos->width, 100.0f);
            EXPECT_FLOAT_EQ(layoutPos->height, 300.0f);
            EXPECT_FLOAT_EQ(layoutAnchor->left.value, 0.0f);
            EXPECT_FLOAT_EQ(layoutAnchor->top.value, 0.0f);
            EXPECT_FLOAT_EQ(layoutAnchor->right.value, 100.0f);
            EXPECT_FLOAT_EQ(layoutAnchor->bottom.value, 300.0f);

            // Create entities to add to the layout
            auto childEntity1 = ecs.createEntity();
            auto childPos1 = ecs.attach<PositionComponent>(childEntity1);
            ecs.attach<UiAnchor>(childEntity1);

            auto childEntity2 = ecs.createEntity();
            auto childPos2 = ecs.attach<PositionComponent>(childEntity2);
            ecs.attach<UiAnchor>(childEntity2);

            childPos1->setWidth(50.0f);
            childPos1->setHeight(50.0f);

            childPos2->setWidth(50.0f);
            childPos2->setHeight(50.0f);

            layout->addEntity(childEntity1);
            layout->addEntity(childEntity2);

            ecs.executeOnce();

            EXPECT_FLOAT_EQ(layoutPos->x, 0.0f);
            EXPECT_FLOAT_EQ(layoutPos->y, 0.0f);
            EXPECT_FLOAT_EQ(layoutPos->width, 50.0f);
            EXPECT_FLOAT_EQ(layoutPos->height, 300.0f);
            EXPECT_FLOAT_EQ(layoutAnchor->left.value, 0.0f);
            EXPECT_FLOAT_EQ(layoutAnchor->top.value, 0.0f);
            EXPECT_FLOAT_EQ(layoutAnchor->right.value, 50.0f);
            EXPECT_FLOAT_EQ(layoutAnchor->bottom.value, 300.0f);

            // Check positions after recalculation
            EXPECT_FLOAT_EQ(childPos1->x, 0.0f);
            EXPECT_FLOAT_EQ(childPos1->y, (1.0f/3.0f) * 200.0f);

            EXPECT_FLOAT_EQ(childPos2->x, 0.0f);
            EXPECT_FLOAT_EQ(childPos2->y, (2.0f/3.0f) * 200.0f + 50); // Positioned below the first entity
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(layout_test, recalculate_layout_fixed)
        {
            EntitySystem ecs;

            ecs.createSystem<PositionComponentSystem>();
            ecs.createSystem<LayoutSystem>();
            ecs.succeed<PositionComponentSystem, LayoutSystem>();
            // ecs.succeed<LayoutSystem, PositionComponentSystem>();

            // Create a vertical layout
            auto layoutEntity = ecs.createEntity();
            auto layout = ecs.attach<VerticalLayout>(layoutEntity);
            auto layoutPos = ecs.attach<PositionComponent>(layoutEntity);
            auto layoutAnchor = ecs.attach<UiAnchor>(layoutEntity);

            layoutPos->setX(0.0f);
            layoutPos->setY(0.0f);
            layoutPos->setWidth(100.0f);
            layoutPos->setHeight(300.0f);

            layout->fitToAxis = true;

            ecs.executeOnce();

            EXPECT_FLOAT_EQ(layoutPos->x, 0.0f);
            EXPECT_FLOAT_EQ(layoutPos->y, 0.0f);
            EXPECT_FLOAT_EQ(layoutPos->width, 100.0f);
            EXPECT_FLOAT_EQ(layoutPos->height, 300.0f);
            EXPECT_FLOAT_EQ(layoutAnchor->left.value, 0.0f);
            EXPECT_FLOAT_EQ(layoutAnchor->top.value, 0.0f);
            EXPECT_FLOAT_EQ(layoutAnchor->right.value, 100.0f);
            EXPECT_FLOAT_EQ(layoutAnchor->bottom.value, 300.0f);

            // Create entities to add to the layout
            auto childEntity1 = ecs.createEntity();
            auto childPos1 = ecs.attach<PositionComponent>(childEntity1);
            ecs.attach<UiAnchor>(childEntity1);

            auto childEntity2 = ecs.createEntity();
            auto childPos2 = ecs.attach<PositionComponent>(childEntity2);
            ecs.attach<UiAnchor>(childEntity2);

            childPos1->setWidth(50.0f);
            childPos1->setHeight(50.0f);

            childPos2->setWidth(50.0f);
            childPos2->setHeight(50.0f);

            layout->addEntity(childEntity1);
            layout->addEntity(childEntity2);

            ecs.executeOnce();

            EXPECT_FLOAT_EQ(layoutPos->x, 0.0f);
            EXPECT_FLOAT_EQ(layoutPos->y, 0.0f);
            EXPECT_FLOAT_EQ(layoutPos->width, 50.0f);
            EXPECT_FLOAT_EQ(layoutPos->height, 300.0f);
            EXPECT_FLOAT_EQ(layoutAnchor->left.value, 0.0f);
            EXPECT_FLOAT_EQ(layoutAnchor->top.value, 0.0f);
            EXPECT_FLOAT_EQ(layoutAnchor->right.value, 50.0f);
            EXPECT_FLOAT_EQ(layoutAnchor->bottom.value, 300.0f);

            // Check positions after recalculation
            EXPECT_FLOAT_EQ(childPos1->x, 0.0f);
            EXPECT_FLOAT_EQ(childPos1->y, 0.0f);

            EXPECT_FLOAT_EQ(childPos2->x, 0.0f);
            EXPECT_FLOAT_EQ(childPos2->y, 50.0f); // Positioned below the first entity
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(layout_test, layout_spacing)
        {
            EntitySystem ecs;

            ecs.createSystem<PositionComponentSystem>();
            ecs.createSystem<LayoutSystem>();
            ecs.succeed<PositionComponentSystem, LayoutSystem>();

            // Create a horizontal layout
            auto layoutEntity = ecs.createEntity();
            auto layout = ecs.attach<HorizontalLayout>(layoutEntity);
            auto layoutPos = ecs.attach<PositionComponent>(layoutEntity);
            ecs.attach<UiAnchor>(layoutEntity);

            layoutPos->setX(0.0f);
            layoutPos->setY(0.0f);
            layoutPos->setWidth(300.0f);
            layoutPos->setHeight(100.0f);

            layout->spacing = 10;

            // Create entities to add to the layout
            auto childEntity1 = ecs.createEntity();
            auto childPos1 = ecs.attach<PositionComponent>(childEntity1);
            ecs.attach<UiAnchor>(childEntity1);

            auto childEntity2 = ecs.createEntity();
            auto childPos2 = ecs.attach<PositionComponent>(childEntity2);
            ecs.attach<UiAnchor>(childEntity2);

            childPos1->setWidth(50.0f);
            childPos1->setHeight(50.0f);

            childPos2->setWidth(50.0f);
            childPos2->setHeight(50.0f);

            layout->addEntity(childEntity1);
            layout->addEntity(childEntity2);

            ecs.executeOnce();

            // Check positions after recalculation
            EXPECT_FLOAT_EQ(childPos1->x, 0.0f);
            EXPECT_FLOAT_EQ(childPos1->y, 0.0f);

            EXPECT_FLOAT_EQ(childPos2->x, 60.0f); // Positioned with spacing of 10
            EXPECT_FLOAT_EQ(childPos2->y, 0.0f);
        }

        TEST(layout_test, layout_stick_to_end_horizontal)
        {
            EntitySystem ecs;

            ecs.createSystem<PositionComponentSystem>();
            ecs.createSystem<LayoutSystem>();
            ecs.succeed<PositionComponentSystem, LayoutSystem>();

            auto layoutEntity = ecs.createEntity();
            auto layout = ecs.attach<HorizontalLayout>(layoutEntity);
            auto layoutPos = ecs.attach<PositionComponent>(layoutEntity);

            layoutPos->setX(0.0f);
            layoutPos->setY(0.0f);
            layoutPos->setWidth(300.0f);
            layoutPos->setHeight(100.0f);

            layout->stickToEnd = true; // Enable stick to end

            // Add entities to the layout
            for (int i = 0; i < 5; ++i)
            {
                auto childEntity = ecs.createEntity();
                auto childPos = ecs.attach<PositionComponent>(childEntity);

                childPos->setWidth(100.0f);
                childPos->setHeight(100.0f);

                layout->addEntity(childEntity);
            }

            ecs.executeOnce();

            EXPECT_FLOAT_EQ(layout->xOffset, 400.0f);

            ecs.executeOnce();

            // Verify that the layout scrolled to the end
            EXPECT_FLOAT_EQ(layout->xOffset, 200.0f); // Total content width (500) - visible width (300)
        }
    }
}