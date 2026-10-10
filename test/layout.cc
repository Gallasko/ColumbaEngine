#include "stdafx.h"

#include "gtest/gtest.h"

#ifdef __linux__
#include <SDL2/SDL.h>
#elif _WIN32
#include <SDL.h>
#endif

#include "UI/sizer.h"
#include "UI/prefab.h"
#include "Input/input.h"
#include "Input/inputcomponent.h"
#include "ECS/callable.h"
#include "ECS/entitysystem.h"

#include "mocklogger.h"

namespace pg
{
    namespace test
    {
        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(layout_test, layout_creation)
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

            EXPECT_EQ(layout->entities.size(), 0);
        }

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

            EXPECT_EQ(layout->entities.size(), 0);

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
            if (layout->entities.size() > 0)
            {
                // Check if the child entity was added correctly
                EXPECT_EQ(layout->entities[0].id, childEntity.id);
            }
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
            MockLogger<TerminalSink> logger;

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

            EXPECT_TRUE(layoutPos->visible);
            EXPECT_TRUE(layoutPos->observable);
            EXPECT_FLOAT_EQ(layoutPos->x, 0.0f);
            EXPECT_FLOAT_EQ(layoutPos->y, 0.0f);
            EXPECT_FLOAT_EQ(layoutPos->width, 0.0f);
            EXPECT_FLOAT_EQ(layoutPos->height, 0.0f);
            EXPECT_FLOAT_EQ(layoutAnchor->left.value, 0.0f);
            EXPECT_FLOAT_EQ(layoutAnchor->top.value, 0.0f);
            EXPECT_FLOAT_EQ(layoutAnchor->right.value, 0.0f);
            EXPECT_FLOAT_EQ(layoutAnchor->bottom.value, 0.0f);

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
            EXPECT_FLOAT_EQ(layoutPos->width, 0.0f);
            EXPECT_FLOAT_EQ(layoutPos->height, 300.0f);
            EXPECT_FLOAT_EQ(layoutAnchor->left.value, 0.0f);
            EXPECT_FLOAT_EQ(layoutAnchor->top.value, 0.0f);
            EXPECT_FLOAT_EQ(layoutAnchor->right.value, 0.0f);
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
            auto layout = ecs.attach<HorizontalLayout>(layoutEntity);
            auto layoutPos = ecs.attach<PositionComponent>(layoutEntity);
            auto layoutAnchor = ecs.attach<UiAnchor>(layoutEntity);

            layoutPos->setX(0.0f);
            layoutPos->setY(0.0f);
            layoutPos->setWidth(50.0f);
            layoutPos->setHeight(50.0f);

            layout->fitToAxis = true;

            ecs.executeOnce();

            EXPECT_FLOAT_EQ(layoutPos->x, 0.0f);
            EXPECT_FLOAT_EQ(layoutPos->y, 0.0f);
            EXPECT_FLOAT_EQ(layoutPos->width, 50.0f);
            EXPECT_FLOAT_EQ(layoutPos->height, 0.0f);
            EXPECT_FLOAT_EQ(layoutAnchor->left.value, 0.0f);
            EXPECT_FLOAT_EQ(layoutAnchor->top.value, 0.0f);
            EXPECT_FLOAT_EQ(layoutAnchor->right.value, 50.0f);
            EXPECT_FLOAT_EQ(layoutAnchor->bottom.value, 0.0f);

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

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // Every child is followed by `spacing`, so an element stuck to the end moves the offset by
        // its height and the spacing after it: the view lands exactly on the new end, one element
        // or several added in the same pass.
        TEST(layout_test, layout_stick_to_end_vertical_with_spacing)
        {
            EntitySystem ecs;

            ecs.createSystem<PositionComponentSystem>();
            ecs.createSystem<LayoutSystem>();
            ecs.succeed<PositionComponentSystem, LayoutSystem>();

            auto layoutEntity = ecs.createEntity();
            auto layout = ecs.attach<VerticalLayout>(layoutEntity);
            auto layoutPos = ecs.attach<PositionComponent>(layoutEntity);

            layoutPos->setX(0.0f);
            layoutPos->setY(0.0f);
            layoutPos->setWidth(100.0f);
            layoutPos->setHeight(300.0f);

            layout->spacing = 1;

            auto addChild = [&](float height) {
                auto childEntity = ecs.createEntity();
                auto childPos = ecs.attach<PositionComponent>(childEntity);

                childPos->setWidth(100.0f);
                childPos->setHeight(height);

                layout->addEntity(childEntity);
            };

            for (int i = 0; i < 40; ++i)
                addChild(20.0f);

            ecs.executeOnce();
            ecs.executeOnce();

            EXPECT_FLOAT_EQ(layout->contentHeight, 40.0f * 21.0f);
            EXPECT_FLOAT_EQ(layout->yOffset, 0.0f);

            layout->stickToEnd = true;

            // One element
            addChild(20.0f);

            ecs.executeOnce();
            ecs.executeOnce();

            EXPECT_FLOAT_EQ(layout->contentHeight, 41.0f * 21.0f);
            EXPECT_FLOAT_EQ(layout->yOffset, layout->contentHeight - 300.0f);

            // Two in the same pass, of different heights
            addChild(34.0f);
            addChild(20.0f);

            ecs.executeOnce();
            ecs.executeOnce();

            EXPECT_FLOAT_EQ(layout->contentHeight, 41.0f * 21.0f + 35.0f + 21.0f);
            EXPECT_FLOAT_EQ(layout->yOffset, layout->contentHeight - 300.0f);
        }

        namespace
        {
            // A scrollable vertical layout 100 x 300 at the origin, 40 rows 20 tall, spacing 1:
            // 840 of content, 540 to scroll.
            struct DragFixture
            {
                EntitySystem ecs;
                VerticalLayout* layout = nullptr;
                EntityRef layoutEntity;
                std::vector<EntityRef> rows;

                // With an input, the mouse click system reads it and sends the mouse events
                DragFixture(Input* input = nullptr)
                {
                    ecs.createSystem<PositionComponentSystem>();
                    ecs.createSystem<LayoutSystem>();
                    ecs.succeed<PositionComponentSystem, LayoutSystem>();

                    if (input)
                        ecs.createSystem<MouseClickSystem>(input);

                    auto list = makeVerticalLayout(&ecs, 0.0f, 0.0f, 100.0f, 300.0f, true);
                    layout = list.get<VerticalLayout>();
                    layoutEntity = list.entity;
                    layout->spacing = 1;
                    layout->dragToScroll = true;

                    for (int i = 0; i < 40; ++i)
                    {
                        auto row = ecs.createEntity();
                        auto pos = ecs.attach<PositionComponent>(row);
                        ecs.attach<UiAnchor>(row);
                        pos->setWidth(100.0f);
                        pos->setHeight(20.0f);

                        layout->addEntity(row);
                        rows.push_back(row);
                    }

                    settle();
                }

                void settle() { ecs.executeOnce(); ecs.executeOnce(); ecs.executeOnce(); }

                float rowY(size_t i) { return rows[i]->get<PositionComponent>()->y; }
            };

            struct RowClicked {};

            struct ClickSpy : public System<Listener<RowClicked>, Listener<OnMouseRelease>, StoragePolicy>
            {
                virtual std::string getSystemName() const override { return "Click Spy"; }

                virtual void onEvent(const RowClicked&) override { ++clicks; }

                virtual void onEvent(const OnMouseRelease& event) override { releases.push_back(event.cancelled); }

                int clicks = 0;
                std::vector<bool> releases;   // cancelled, per release
            };
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // A press in a dragToScroll layout stays a click until the mouse has moved dragThreshold;
        // then the content follows the mouse, clamped to its ends, until the release.
        TEST(layout_test, layout_drag_to_scroll)
        {
            DragFixture f;

            ASSERT_FLOAT_EQ(f.layout->contentHeight, 840.0f);
            ASSERT_FLOAT_EQ(f.rowY(0), 0.0f);

            f.ecs.sendEvent(OnMouseClick{Point2D{50.0f, 150.0f}, SDL_BUTTON_LEFT});

            // Under the threshold: nothing moves
            f.ecs.sendEvent(OnMouseMove{Point2D{50.0f, 147.0f}, nullptr});
            EXPECT_FLOAT_EQ(f.layout->yOffset, 0.0f);

            // Up by 100: the content follows
            f.ecs.sendEvent(OnMouseMove{Point2D{50.0f, 50.0f}, nullptr});
            EXPECT_FLOAT_EQ(f.layout->yOffset, 100.0f);

            f.settle();
            EXPECT_FLOAT_EQ(f.rowY(0), -100.0f);

            // Never past either end
            f.ecs.sendEvent(OnMouseMove{Point2D{50.0f, 400.0f}, nullptr});
            EXPECT_FLOAT_EQ(f.layout->yOffset, 0.0f);

            f.ecs.sendEvent(OnMouseMove{Point2D{50.0f, -2000.0f}, nullptr});
            EXPECT_FLOAT_EQ(f.layout->yOffset, 540.0f);

            f.settle();
            EXPECT_FLOAT_EQ(f.rowY(39), 300.0f - 20.0f - 1.0f);

            // Released: the mouse moves freely again
            f.ecs.sendEvent(OnMouseRelease{Point2D{50.0f, -2000.0f}, SDL_BUTTON_LEFT});
            f.ecs.sendEvent(OnMouseMove{Point2D{50.0f, 150.0f}, nullptr});
            EXPECT_FLOAT_EQ(f.layout->yOffset, 540.0f);

            // A press outside the view does not drag it
            f.ecs.sendEvent(OnMouseClick{Point2D{250.0f, 150.0f}, SDL_BUTTON_LEFT});
            f.ecs.sendEvent(OnMouseMove{Point2D{250.0f, 300.0f}, nullptr});
            EXPECT_FLOAT_EQ(f.layout->yOffset, 540.0f);
            f.ecs.sendEvent(OnMouseRelease{Point2D{250.0f, 300.0f}, SDL_BUTTON_LEFT});

            // Nor does one in a layout that did not opt in
            f.layout->dragToScroll = false;
            f.ecs.sendEvent(OnMouseClick{Point2D{50.0f, 150.0f}, SDL_BUTTON_LEFT});
            f.ecs.sendEvent(OnMouseMove{Point2D{50.0f, 300.0f}, nullptr});
            EXPECT_FLOAT_EQ(f.layout->yOffset, 540.0f);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // Through the mouse click system: a drag cancels the click (its release calls no release
        // area and reaches the listeners cancelled); a press that does not move is still a click.
        TEST(layout_test, layout_drag_cancels_the_click)
        {
            Input input;
            DragFixture f(&input);
            auto spy = f.ecs.createSystem<ClickSpy>();

            // The row under the press answers a click on release
            f.ecs.attach<MouseLeftClickComponent>(f.rows[7], makeCallable<RowClicked>(), MouseStateTrigger::OnRelease);
            f.settle();

            // What a system sends while it executes reaches the listeners on a later pass
            auto frame = [&]() { f.settle(); };

            // Press on row 7, drag up 60, release: a scroll, not a click
            input.registerMouseMove(Point2D{50.0f, 150.0f}, Point2D{0.0f, 0.0f});
            input.registerMouseInput(SDL_BUTTON_LEFT, Input::InputState::MOUSEPRESS);
            frame();

            input.registerMouseMove(Point2D{50.0f, 90.0f}, Point2D{0.0f, -60.0f});
            frame();

            EXPECT_FLOAT_EQ(f.layout->yOffset, 60.0f);

            input.registerMouseInput(SDL_BUTTON_LEFT, Input::InputState::MOUSERELEASE);
            frame();

            EXPECT_EQ(spy->clicks, 0);
            ASSERT_EQ(spy->releases.size(), 1u);
            EXPECT_TRUE(spy->releases[0]);

            input.updateInput(0.0);

            // Press and release in place on row 7 (now 60 higher): a click
            input.registerMouseMove(Point2D{50.0f, 100.0f}, Point2D{0.0f, 0.0f});
            input.registerMouseInput(SDL_BUTTON_LEFT, Input::InputState::MOUSEPRESS);
            frame();
            input.registerMouseInput(SDL_BUTTON_LEFT, Input::InputState::MOUSERELEASE);
            frame();

            EXPECT_EQ(spy->clicks, 1);
            ASSERT_EQ(spy->releases.size(), 2u);
            EXPECT_FALSE(spy->releases[1]);
            EXPECT_FLOAT_EQ(f.layout->yOffset, 60.0f);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // A layout removed while an update of it is queued (a scene left, a list cleared, in the
        // frame something moved in it) is forgotten, not recalculated.
        TEST(layout_test, layout_removed_with_an_update_pending)
        {
            MockLogger logger;
            EntitySystem ecs;

            ecs.createSystem<PositionComponentSystem>();
            ecs.createSystem<LayoutSystem>();
            ecs.succeed<PositionComponentSystem, LayoutSystem>();

            std::vector<EntityRef> layouts;

            for (int n = 0; n < 3; ++n)
            {
                auto list = makeVerticalLayout(&ecs, 0.0f, 0.0f, 100.0f, 300.0f, true);

                for (int i = 0; i < 5; ++i)
                {
                    auto row = ecs.createEntity();
                    ecs.attach<PositionComponent>(row)->setHeight(20.0f);
                    ecs.attach<UiAnchor>(row);
                    list.get<VerticalLayout>()->addEntity(row);
                }

                layouts.push_back(list.entity);
            }

            ecs.executeOnce();
            ecs.executeOnce();

            // Something moves in each list, then the lists go, all in one frame
            for (auto& layout : layouts)
            {
                layout->get<PositionComponent>()->setY(10.0f);
                ecs.removeEntity(layout);
            }

            ecs.executeOnce();
            ecs.executeOnce();
            ecs.executeOnce();

            for (auto& layout : layouts)
                EXPECT_EQ(ecs.getEntity(layout.id), nullptr);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // A prefab container follows the height of its main entity through a constraint, so the
        // resize reaches the layout as a settled position, not as a setter call
        TEST(layout_test, restacks_when_a_constraint_resizes_a_child)
        {
            MockLogger logger;
            EntitySystem ecs;

            ecs.createSystem<PositionComponentSystem>();
            ecs.createSystem<LayoutSystem>();
            ecs.createSystem<PrefabSystem>();
            ecs.succeed<PositionComponentSystem, LayoutSystem>();
            ecs.succeed<PositionComponentSystem, PrefabSystem>();

            auto layout = makeVerticalLayout(&ecs, 0.0f, 0.0f, 100.0f, 0.0f);

            auto container = makeAnchoredPrefab(&ecs, 0.0f, 0.0f, 0.0f);

            auto mainEnt = ecs.createEntity();
            auto mainPos = ecs.attach<PositionComponent>(mainEnt);
            ecs.attach<UiAnchor>(mainEnt);
            mainPos->setWidth(100.0f);
            mainPos->setHeight(40.0f);

            container.get<Prefab>()->setMainEntity(mainEnt);

            auto second = ecs.createEntity();
            auto secondPos = ecs.attach<PositionComponent>(second);
            ecs.attach<UiAnchor>(second);
            secondPos->setWidth(100.0f);
            secondPos->setHeight(20.0f);

            layout.get<VerticalLayout>()->addEntity(container.entity);
            layout.get<VerticalLayout>()->addEntity(second);

            for (size_t i = 0; i < 5; ++i)
                ecs.executeOnce();

            EXPECT_FLOAT_EQ(container.get<PositionComponent>()->height, 40.0f);
            EXPECT_FLOAT_EQ(secondPos->y, 40.0f);
            EXPECT_FLOAT_EQ(layout.get<PositionComponent>()->height, 60.0f);

            mainPos->setHeight(70.0f);

            for (size_t i = 0; i < 5; ++i)
                ecs.executeOnce();

            EXPECT_FLOAT_EQ(container.get<PositionComponent>()->height, 70.0f);
            EXPECT_FLOAT_EQ(secondPos->y, 70.0f);
            EXPECT_FLOAT_EQ(layout.get<PositionComponent>()->height, 90.0f);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(layout_test, moved_by_an_anchor_places_its_children_again)
        {
            MockLogger logger;
            EntitySystem ecs;

            ecs.createSystem<PositionComponentSystem>();
            ecs.createSystem<LayoutSystem>();
            ecs.succeed<PositionComponentSystem, LayoutSystem>();

            auto holder = ecs.createEntity();
            auto holderPos = ecs.attach<PositionComponent>(holder);
            ecs.attach<UiAnchor>(holder);
            holderPos->setX(10.0f);
            holderPos->setY(10.0f);

            auto layout = makeVerticalLayout(&ecs, 0.0f, 0.0f, 100.0f, 0.0f);
            layout.get<UiAnchor>()->setTopAnchor(PosAnchor{holder.id, AnchorType::Top});
            layout.get<UiAnchor>()->setLeftAnchor(PosAnchor{holder.id, AnchorType::Left});

            auto first = ecs.createEntity();
            auto firstPos = ecs.attach<PositionComponent>(first);
            ecs.attach<UiAnchor>(first);
            firstPos->setWidth(100.0f);
            firstPos->setHeight(20.0f);

            auto second = ecs.createEntity();
            auto secondPos = ecs.attach<PositionComponent>(second);
            ecs.attach<UiAnchor>(second);
            secondPos->setWidth(100.0f);
            secondPos->setHeight(30.0f);

            layout.get<VerticalLayout>()->addEntity(first);
            layout.get<VerticalLayout>()->addEntity(second);

            for (size_t i = 0; i < 5; ++i)
                ecs.executeOnce();

            EXPECT_FLOAT_EQ(layout.get<PositionComponent>()->y, 10.0f);
            EXPECT_FLOAT_EQ(firstPos->y, 10.0f);
            EXPECT_FLOAT_EQ(secondPos->y, 30.0f);

            // The layout follows its holder by anchor: no setter is called on it
            holderPos->setY(50.0f);

            for (size_t i = 0; i < 5; ++i)
                ecs.executeOnce();

            EXPECT_FLOAT_EQ(layout.get<PositionComponent>()->y, 50.0f);
            EXPECT_FLOAT_EQ(firstPos->y, 50.0f);
            EXPECT_FLOAT_EQ(secondPos->y, 70.0f);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(layout_test, restacks_when_a_nested_layout_is_hidden_or_grows)
        {
            MockLogger logger;
            EntitySystem ecs;

            ecs.createSystem<PositionComponentSystem>();
            ecs.createSystem<LayoutSystem>();
            ecs.succeed<PositionComponentSystem, LayoutSystem>();

            auto makeBox = [&ecs](float height) {
                auto box = ecs.createEntity();
                auto boxPos = ecs.attach<PositionComponent>(box);
                ecs.attach<UiAnchor>(box);
                boxPos->setWidth(100.0f);
                boxPos->setHeight(height);

                return box;
            };

            // A layout in a layout, over a box: the box stands under what the inner one holds
            auto outer = makeVerticalLayout(&ecs, 0.0f, 0.0f, 100.0f, 0.0f);
            auto inner = makeVerticalLayout(&ecs, 0.0f, 0.0f, 100.0f, 0.0f);

            auto held = makeBox(40.0f);
            auto under = makeBox(20.0f);

            inner.get<VerticalLayout>()->addEntity(held);
            outer.get<VerticalLayout>()->addEntity(inner.entity);
            outer.get<VerticalLayout>()->addEntity(under);

            for (size_t i = 0; i < 5; ++i)
                ecs.executeOnce();

            auto innerPos = inner.get<PositionComponent>();
            auto outerPos = outer.get<PositionComponent>();
            auto underPos = under->get<PositionComponent>();

            EXPECT_FLOAT_EQ(innerPos->height, 40.0f);
            EXPECT_FLOAT_EQ(underPos->y, 40.0f);
            EXPECT_FLOAT_EQ(outerPos->height, 60.0f);

            // Hidden, the inner layout leaves the stack: the box takes its place
            innerPos->setVisibility(false);

            for (size_t i = 0; i < 5; ++i)
                ecs.executeOnce();

            EXPECT_FLOAT_EQ(underPos->y, 0.0f);
            EXPECT_FLOAT_EQ(outerPos->height, 20.0f);

            innerPos->setVisibility(true);

            for (size_t i = 0; i < 5; ++i)
                ecs.executeOnce();

            EXPECT_FLOAT_EQ(underPos->y, 40.0f);
            EXPECT_FLOAT_EQ(outerPos->height, 60.0f);

            // Grown by what it holds, it pushes the box down
            inner.get<VerticalLayout>()->addEntity(makeBox(30.0f));

            for (size_t i = 0; i < 5; ++i)
                ecs.executeOnce();

            EXPECT_FLOAT_EQ(innerPos->height, 70.0f);
            EXPECT_FLOAT_EQ(underPos->y, 70.0f);
            EXPECT_FLOAT_EQ(outerPos->height, 90.0f);
        }
    }
}
