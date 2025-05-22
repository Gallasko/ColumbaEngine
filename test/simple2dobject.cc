#include "stdafx.h"

#include "gtest/gtest.h"

#include "mock2dsimpleshape.h"

namespace pg
{
    namespace test
    {
        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(system_2d, initialization)
        {
            MasterRenderer renderer;

            Mock2DSimpleShape sys(&renderer);

            EXPECT_EQ(sys.getCurrentSize(),     1);
            EXPECT_EQ(sys.getElementIndex(),    0);
            EXPECT_EQ(sys.getVisibleElements(), 0);
            EXPECT_EQ(sys.getNbAttributes(),    8);
        }

        TEST(system_2d, creation)
        {
            EntitySystem ecs;

            MasterRenderer renderer;

            ecs.createSystem<UiComponentSystem>();

            auto sys = ecs.createMockSystem<Simple2DObjectSystem, Mock2DSimpleShape>(&renderer);

            EXPECT_EQ(sys->getCurrentSize(),     1);
            EXPECT_EQ(sys->getElementIndex(),    0);
            EXPECT_EQ(sys->getVisibleElements(), 0);

            auto ent = ecs.createEntity();

            EXPECT_EQ(sys->getCurrentSize(),     1);
            EXPECT_EQ(sys->getElementIndex(),    0);
            EXPECT_EQ(sys->getVisibleElements(), 0);

            auto ui = ecs.attach<UiComponent>(ent);

            ui->setX(15);
            ui->setY(16);

            ui->setWidth(30);
            ui->setHeight(40);

            ecs.attach<Simple2DObject>(ent, Shape2D::Square);

            EXPECT_EQ(sys->getCurrentSize(),     2);
            EXPECT_EQ(sys->getElementIndex(),    1);
            EXPECT_EQ(sys->getVisibleElements(), 1);

            EXPECT_EQ(sys->getBuffer()[0], 15.0f);
            EXPECT_EQ(sys->getBuffer()[1], 16.0f);
            EXPECT_EQ(sys->getBuffer()[2], 0.0f);

            EXPECT_EQ(sys->getBuffer()[3], 30.0f);
            EXPECT_EQ(sys->getBuffer()[4], 40.0f);

            EXPECT_EQ(sys->getBuffer()[5], 255.0f);
            EXPECT_EQ(sys->getBuffer()[6], 255.0f);
            EXPECT_EQ(sys->getBuffer()[7], 255.0f);
        }

        TEST(system_2d, multiple_creation)
        {
            EntitySystem ecs;

            MasterRenderer renderer;

            ecs.createSystem<UiComponentSystem>();

            auto sys = ecs.createMockSystem<Simple2DObjectSystem, Mock2DSimpleShape>(&renderer);

            EXPECT_EQ(sys->getCurrentSize(),     1);
            EXPECT_EQ(sys->getElementIndex(),    0);
            EXPECT_EQ(sys->getVisibleElements(), 0);

            auto ent0 = ecs.createEntity();
            auto ent1 = ecs.createEntity();
            auto ent2 = ecs.createEntity();
            auto ent3 = ecs.createEntity();
            auto ent4 = ecs.createEntity();

            ecs.attach<UiComponent>(ent0);
            ecs.attach<Simple2DObject>(ent0, Shape2D::Square);

            EXPECT_EQ(sys->getCurrentSize(),     2);
            EXPECT_EQ(sys->getElementIndex(),    1);
            EXPECT_EQ(sys->getVisibleElements(), 1);

            EXPECT_EQ(sys->getIdToIndexMap()[ent0.id], 0);

            ecs.attach<UiComponent>(ent1);
            ecs.attach<Simple2DObject>(ent1, Shape2D::Square);

            EXPECT_EQ(sys->getCurrentSize(),     4);
            EXPECT_EQ(sys->getElementIndex(),    2);
            EXPECT_EQ(sys->getVisibleElements(), 2);

            EXPECT_EQ(sys->getIdToIndexMap()[ent0.id], 0);
            EXPECT_EQ(sys->getIdToIndexMap()[ent1.id], 1);

            ecs.attach<UiComponent>(ent2);
            ecs.attach<Simple2DObject>(ent2, Shape2D::Square);

            EXPECT_EQ(sys->getCurrentSize(),     4);
            EXPECT_EQ(sys->getElementIndex(),    3);
            EXPECT_EQ(sys->getVisibleElements(), 3);

            EXPECT_EQ(sys->getIdToIndexMap()[ent0.id], 0);
            EXPECT_EQ(sys->getIdToIndexMap()[ent1.id], 1);
            EXPECT_EQ(sys->getIdToIndexMap()[ent2.id], 2);

            auto ui = ecs.attach<UiComponent>(ent3);
            ui->setVisibility(false);
            ecs.attach<Simple2DObject>(ent3, Shape2D::Square);

            EXPECT_EQ(sys->getCurrentSize(),     8);
            EXPECT_EQ(sys->getElementIndex(),    4);
            EXPECT_EQ(sys->getVisibleElements(), 3);

            EXPECT_EQ(sys->getIdToIndexMap()[ent0.id], 0);
            EXPECT_EQ(sys->getIdToIndexMap()[ent1.id], 1);
            EXPECT_EQ(sys->getIdToIndexMap()[ent2.id], 2);
            EXPECT_EQ(sys->getIdToIndexMap()[ent3.id], 3);

            ecs.attach<UiComponent>(ent4);
            ecs.attach<Simple2DObject>(ent4, Shape2D::Square);

            EXPECT_EQ(sys->getCurrentSize(),     8);
            EXPECT_EQ(sys->getElementIndex(),    5);
            EXPECT_EQ(sys->getVisibleElements(), 4);

            EXPECT_EQ(sys->getIdToIndexMap()[ent0.id], 0);
            EXPECT_EQ(sys->getIdToIndexMap()[ent1.id], 1);
            EXPECT_EQ(sys->getIdToIndexMap()[ent2.id], 2);
            EXPECT_EQ(sys->getIdToIndexMap()[ent3.id], 4);
            EXPECT_EQ(sys->getIdToIndexMap()[ent4.id], 3);
        }

        TEST(system_2d, make_2D_helper)
        {
            EntitySystem ecs;

            MasterRenderer renderer;

            ecs.createSystem<UiComponentSystem>();

            auto sys = ecs.createMockSystem<Simple2DObjectSystem, Mock2DSimpleShape>(&renderer);

            EXPECT_EQ(sys->getCurrentSize(),     1);
            EXPECT_EQ(sys->getElementIndex(),    0);
            EXPECT_EQ(sys->getVisibleElements(), 0);

            makeSimple2DShape(&ecs, Shape2D::Square, 10, 20, {255.0f, 255.0f, 255.0f});

            EXPECT_EQ(sys->getCurrentSize(),     2);
            EXPECT_EQ(sys->getElementIndex(),    1);
            EXPECT_EQ(sys->getVisibleElements(), 1);

            makeSimple2DShape(&ecs, Shape2D::Square, 10, 20, {255.0f, 255.0f, 255.0f});

            EXPECT_EQ(sys->getCurrentSize(),     4);
            EXPECT_EQ(sys->getElementIndex(),    2);
            EXPECT_EQ(sys->getVisibleElements(), 2);
        }

        TEST(system_2d, visibility_change)
        {
            EntitySystem ecs;

            MasterRenderer renderer;

            ecs.createSystem<UiComponentSystem>();

            auto sys = ecs.createMockSystem<Simple2DObjectSystem, Mock2DSimpleShape>(&renderer);

            EXPECT_EQ(sys->getCurrentSize(),     1);
            EXPECT_EQ(sys->getElementIndex(),    0);
            EXPECT_EQ(sys->getVisibleElements(), 0);

            auto ent0 = ecs.createEntity();
            auto ent1 = ecs.createEntity();
            auto ent2 = ecs.createEntity();
            auto ent3 = ecs.createEntity();
            auto ent4 = ecs.createEntity();

            ecs.attach<UiComponent>(ent0);
            ecs.attach<Simple2DObject>(ent0, Shape2D::Square);

            EXPECT_EQ(sys->getCurrentSize(),     2);
            EXPECT_EQ(sys->getElementIndex(),    1);
            EXPECT_EQ(sys->getVisibleElements(), 1);

            EXPECT_EQ(sys->getIdToIndexMap()[ent0.id], 0);

            ecs.attach<UiComponent>(ent1);
            ecs.attach<Simple2DObject>(ent1, Shape2D::Square);

            EXPECT_EQ(sys->getCurrentSize(),     4);
            EXPECT_EQ(sys->getElementIndex(),    2);
            EXPECT_EQ(sys->getVisibleElements(), 2);

            EXPECT_EQ(sys->getIdToIndexMap()[ent0.id], 0);
            EXPECT_EQ(sys->getIdToIndexMap()[ent1.id], 1);

            ecs.attach<UiComponent>(ent2);
            ecs.attach<Simple2DObject>(ent2, Shape2D::Square);

            EXPECT_EQ(sys->getCurrentSize(),     4);
            EXPECT_EQ(sys->getElementIndex(),    3);
            EXPECT_EQ(sys->getVisibleElements(), 3);

            EXPECT_EQ(sys->getIdToIndexMap()[ent0.id], 0);
            EXPECT_EQ(sys->getIdToIndexMap()[ent1.id], 1);
            EXPECT_EQ(sys->getIdToIndexMap()[ent2.id], 2);

            auto ui = ecs.attach<UiComponent>(ent3);
            ui->setVisibility(false);
            ecs.attach<Simple2DObject>(ent3, Shape2D::Square);

            EXPECT_EQ(sys->getCurrentSize(),     8);
            EXPECT_EQ(sys->getElementIndex(),    4);
            EXPECT_EQ(sys->getVisibleElements(), 3);

            EXPECT_EQ(sys->getIdToIndexMap()[ent0.id], 0);
            EXPECT_EQ(sys->getIdToIndexMap()[ent1.id], 1);
            EXPECT_EQ(sys->getIdToIndexMap()[ent2.id], 2);
            EXPECT_EQ(sys->getIdToIndexMap()[ent3.id], 3);

            ecs.attach<UiComponent>(ent4);
            ecs.attach<Simple2DObject>(ent4, Shape2D::Square);

            EXPECT_EQ(sys->getCurrentSize(),     8);
            EXPECT_EQ(sys->getElementIndex(),    5);
            EXPECT_EQ(sys->getVisibleElements(), 4);

            EXPECT_EQ(sys->getIdToIndexMap()[ent0.id], 0);
            EXPECT_EQ(sys->getIdToIndexMap()[ent1.id], 1);
            EXPECT_EQ(sys->getIdToIndexMap()[ent2.id], 2);
            EXPECT_EQ(sys->getIdToIndexMap()[ent3.id], 4);
            EXPECT_EQ(sys->getIdToIndexMap()[ent4.id], 3);

            ui->setVisibility(true);

            EXPECT_EQ(sys->getCurrentSize(),     8);
            EXPECT_EQ(sys->getElementIndex(),    5);
            EXPECT_EQ(sys->getVisibleElements(), 5);

            EXPECT_EQ(sys->getIdToIndexMap()[ent0.id], 0);
            EXPECT_EQ(sys->getIdToIndexMap()[ent1.id], 1);
            EXPECT_EQ(sys->getIdToIndexMap()[ent2.id], 2);

            // Should stay "swapped" because the last index is the last visible element !
            EXPECT_EQ(sys->getIdToIndexMap()[ent3.id], 4);
            EXPECT_EQ(sys->getIdToIndexMap()[ent4.id], 3);
        }

        TEST(system_2d, multiple_visibility_change)
        {
            EntitySystem ecs;

            MasterRenderer renderer;

            ecs.createSystem<UiComponentSystem>();

            auto sys = ecs.createMockSystem<Simple2DObjectSystem, Mock2DSimpleShape>(&renderer);

            EXPECT_EQ(sys->getCurrentSize(),     1);
            EXPECT_EQ(sys->getElementIndex(),    0);
            EXPECT_EQ(sys->getVisibleElements(), 0);

            auto ent0 = ecs.createEntity();
            auto ent1 = ecs.createEntity();
            auto ent2 = ecs.createEntity();
            auto ent3 = ecs.createEntity();
            auto ent4 = ecs.createEntity();

            auto ui0 = ecs.attach<UiComponent>(ent0);
            ecs.attach<Simple2DObject>(ent0, Shape2D::Square);

            EXPECT_EQ(sys->getCurrentSize(),     2);
            EXPECT_EQ(sys->getElementIndex(),    1);
            EXPECT_EQ(sys->getVisibleElements(), 1);

            EXPECT_EQ(sys->getIdToIndexMap()[ent0.id], 0);

            ecs.attach<UiComponent>(ent1);
            ecs.attach<Simple2DObject>(ent1, Shape2D::Square);

            EXPECT_EQ(sys->getCurrentSize(),     4);
            EXPECT_EQ(sys->getElementIndex(),    2);
            EXPECT_EQ(sys->getVisibleElements(), 2);

            EXPECT_EQ(sys->getIdToIndexMap()[ent0.id], 0);
            EXPECT_EQ(sys->getIdToIndexMap()[ent1.id], 1);

            ecs.attach<UiComponent>(ent2);
            ecs.attach<Simple2DObject>(ent2, Shape2D::Square);

            EXPECT_EQ(sys->getCurrentSize(),     4);
            EXPECT_EQ(sys->getElementIndex(),    3);
            EXPECT_EQ(sys->getVisibleElements(), 3);

            EXPECT_EQ(sys->getIdToIndexMap()[ent0.id], 0);
            EXPECT_EQ(sys->getIdToIndexMap()[ent1.id], 1);
            EXPECT_EQ(sys->getIdToIndexMap()[ent2.id], 2);

            auto ui = ecs.attach<UiComponent>(ent3);
            ui->setVisibility(false);
            ecs.attach<Simple2DObject>(ent3, Shape2D::Square);

            EXPECT_EQ(sys->getCurrentSize(),     8);
            EXPECT_EQ(sys->getElementIndex(),    4);
            EXPECT_EQ(sys->getVisibleElements(), 3);

            EXPECT_EQ(sys->getIdToIndexMap()[ent0.id], 0);
            EXPECT_EQ(sys->getIdToIndexMap()[ent1.id], 1);
            EXPECT_EQ(sys->getIdToIndexMap()[ent2.id], 2);
            EXPECT_EQ(sys->getIdToIndexMap()[ent3.id], 3);

            ecs.attach<UiComponent>(ent4);
            ecs.attach<Simple2DObject>(ent4, Shape2D::Square);

            EXPECT_EQ(sys->getCurrentSize(),     8);
            EXPECT_EQ(sys->getElementIndex(),    5);
            EXPECT_EQ(sys->getVisibleElements(), 4);

            EXPECT_EQ(sys->getIdToIndexMap()[ent0.id], 0);
            EXPECT_EQ(sys->getIdToIndexMap()[ent1.id], 1);
            EXPECT_EQ(sys->getIdToIndexMap()[ent2.id], 2);
            EXPECT_EQ(sys->getIdToIndexMap()[ent3.id], 4);
            EXPECT_EQ(sys->getIdToIndexMap()[ent4.id], 3);

            ui0->setVisibility(false);

            EXPECT_EQ(sys->getCurrentSize(),     8);
            EXPECT_EQ(sys->getElementIndex(),    5);
            EXPECT_EQ(sys->getVisibleElements(), 3);

            EXPECT_EQ(sys->getIdToIndexMap()[ent0.id], 3);
            EXPECT_EQ(sys->getIdToIndexMap()[ent1.id], 1);
            EXPECT_EQ(sys->getIdToIndexMap()[ent2.id], 2);
            EXPECT_EQ(sys->getIdToIndexMap()[ent3.id], 4);
            EXPECT_EQ(sys->getIdToIndexMap()[ent4.id], 0);

            ui->setVisibility(true);

            EXPECT_EQ(sys->getCurrentSize(),     8);
            EXPECT_EQ(sys->getElementIndex(),    5);
            EXPECT_EQ(sys->getVisibleElements(), 4);

            EXPECT_EQ(sys->getIdToIndexMap()[ent0.id], 4);
            EXPECT_EQ(sys->getIdToIndexMap()[ent1.id], 1);
            EXPECT_EQ(sys->getIdToIndexMap()[ent2.id], 2);
            EXPECT_EQ(sys->getIdToIndexMap()[ent3.id], 3);
            EXPECT_EQ(sys->getIdToIndexMap()[ent4.id], 0);

            ui0->setVisibility(true);

            EXPECT_EQ(sys->getCurrentSize(),     8);
            EXPECT_EQ(sys->getElementIndex(),    5);
            EXPECT_EQ(sys->getVisibleElements(), 5);

            EXPECT_EQ(sys->getIdToIndexMap()[ent0.id], 4);
            EXPECT_EQ(sys->getIdToIndexMap()[ent1.id], 1);
            EXPECT_EQ(sys->getIdToIndexMap()[ent2.id], 2);
            EXPECT_EQ(sys->getIdToIndexMap()[ent3.id], 3);
            EXPECT_EQ(sys->getIdToIndexMap()[ent4.id], 0);
        }

        TEST(system_2d, buffer_values)
        {
            EntitySystem ecs;

            MasterRenderer renderer;

            ecs.createSystem<UiComponentSystem>();

            auto sys = ecs.createMockSystem<Simple2DObjectSystem, Mock2DSimpleShape>(&renderer);

            EXPECT_EQ(sys->getCurrentSize(),     1);
            EXPECT_EQ(sys->getElementIndex(),    0);
            EXPECT_EQ(sys->getVisibleElements(), 0);

            makeSimple2DShape(&ecs, Shape2D::Square, 10, 20, {255.0f, 255.0f, 255.0f});

            EXPECT_EQ(sys->getCurrentSize(),     2);
            EXPECT_EQ(sys->getElementIndex(),    1);
            EXPECT_EQ(sys->getVisibleElements(), 1);

            EXPECT_EQ(sys->getBuffer()[0], 0.0f);
            EXPECT_EQ(sys->getBuffer()[1], 0.0f);
            EXPECT_EQ(sys->getBuffer()[2], 0.0f);

            EXPECT_EQ(sys->getBuffer()[3], 10.0f);
            EXPECT_EQ(sys->getBuffer()[4], 20.0f);

            EXPECT_EQ(sys->getBuffer()[5], 255.0f);
            EXPECT_EQ(sys->getBuffer()[6], 255.0f);
            EXPECT_EQ(sys->getBuffer()[7], 255.0f);

            makeSimple2DShape(&ecs, Shape2D::Square, 40, 50, {255.0f, 255.0f, 255.0f});

            EXPECT_EQ(sys->getCurrentSize(),     4);
            EXPECT_EQ(sys->getElementIndex(),    2);
            EXPECT_EQ(sys->getVisibleElements(), 2);

            EXPECT_EQ(sys->getBuffer()[0], 0.0f);
            EXPECT_EQ(sys->getBuffer()[1], 0.0f);
            EXPECT_EQ(sys->getBuffer()[2], 0.0f);

            EXPECT_EQ(sys->getBuffer()[3], 10.0f);
            EXPECT_EQ(sys->getBuffer()[4], 20.0f);

            EXPECT_EQ(sys->getBuffer()[5], 255.0f);
            EXPECT_EQ(sys->getBuffer()[6], 255.0f);
            EXPECT_EQ(sys->getBuffer()[7], 255.0f);

            EXPECT_EQ(sys->getBuffer()[8], 0.0f);
            EXPECT_EQ(sys->getBuffer()[9], 0.0f);
            EXPECT_EQ(sys->getBuffer()[10], 0.0f);

            EXPECT_EQ(sys->getBuffer()[11], 40.0f);
            EXPECT_EQ(sys->getBuffer()[12], 50.0f);

            EXPECT_EQ(sys->getBuffer()[13], 255.0f);
            EXPECT_EQ(sys->getBuffer()[14], 255.0f);
            EXPECT_EQ(sys->getBuffer()[15], 255.0f);

            auto list2 = makeSimple2DShape(&ecs, Shape2D::Square, 60, 70, {255.0f, 255.0f, 255.0f});
            list2.get<UiComponent>()->setVisibility(false);

            EXPECT_EQ(sys->getCurrentSize(),     4);
            EXPECT_EQ(sys->getElementIndex(),    3);
            EXPECT_EQ(sys->getVisibleElements(), 2);

            EXPECT_EQ(sys->getBuffer()[0], 0.0f);
            EXPECT_EQ(sys->getBuffer()[1], 0.0f);
            EXPECT_EQ(sys->getBuffer()[2], 0.0f);

            EXPECT_EQ(sys->getBuffer()[3], 10.0f);
            EXPECT_EQ(sys->getBuffer()[4], 20.0f);

            EXPECT_EQ(sys->getBuffer()[5], 255.0f);
            EXPECT_EQ(sys->getBuffer()[6], 255.0f);
            EXPECT_EQ(sys->getBuffer()[7], 255.0f);

            EXPECT_EQ(sys->getBuffer()[8], 0.0f);
            EXPECT_EQ(sys->getBuffer()[9], 0.0f);
            EXPECT_EQ(sys->getBuffer()[10], 0.0f);

            EXPECT_EQ(sys->getBuffer()[11], 40.0f);
            EXPECT_EQ(sys->getBuffer()[12], 50.0f);

            EXPECT_EQ(sys->getBuffer()[13], 255.0f);
            EXPECT_EQ(sys->getBuffer()[14], 255.0f);
            EXPECT_EQ(sys->getBuffer()[15], 255.0f);

            EXPECT_EQ(sys->getBuffer()[16], 0.0f);
            EXPECT_EQ(sys->getBuffer()[17], 0.0f);
            EXPECT_EQ(sys->getBuffer()[18], 0.0f);

            EXPECT_EQ(sys->getBuffer()[19], 60.0f);
            EXPECT_EQ(sys->getBuffer()[20], 70.0f);

            EXPECT_EQ(sys->getBuffer()[21], 255.0f);
            EXPECT_EQ(sys->getBuffer()[22], 255.0f);
            EXPECT_EQ(sys->getBuffer()[23], 255.0f);

            makeSimple2DShape(&ecs, Shape2D::Square, 80, 90, {255.0f, 255.0f, 255.0f});

            EXPECT_EQ(sys->getCurrentSize(),     8);
            EXPECT_EQ(sys->getElementIndex(),    4);
            EXPECT_EQ(sys->getVisibleElements(), 3);

            EXPECT_EQ(sys->getBuffer()[0], 0.0f);
            EXPECT_EQ(sys->getBuffer()[1], 0.0f);
            EXPECT_EQ(sys->getBuffer()[2], 0.0f);

            EXPECT_EQ(sys->getBuffer()[3], 10.0f);
            EXPECT_EQ(sys->getBuffer()[4], 20.0f);

            EXPECT_EQ(sys->getBuffer()[5], 255.0f);
            EXPECT_EQ(sys->getBuffer()[6], 255.0f);
            EXPECT_EQ(sys->getBuffer()[7], 255.0f);

            EXPECT_EQ(sys->getBuffer()[8], 0.0f);
            EXPECT_EQ(sys->getBuffer()[9], 0.0f);
            EXPECT_EQ(sys->getBuffer()[10], 0.0f);

            EXPECT_EQ(sys->getBuffer()[11], 40.0f);
            EXPECT_EQ(sys->getBuffer()[12], 50.0f);

            EXPECT_EQ(sys->getBuffer()[13], 255.0f);
            EXPECT_EQ(sys->getBuffer()[14], 255.0f);
            EXPECT_EQ(sys->getBuffer()[15], 255.0f);

            EXPECT_EQ(sys->getBuffer()[16], 0.0f);
            EXPECT_EQ(sys->getBuffer()[17], 0.0f);
            EXPECT_EQ(sys->getBuffer()[18], 0.0f);

            EXPECT_EQ(sys->getBuffer()[19], 80.0f);
            EXPECT_EQ(sys->getBuffer()[20], 90.0f);

            EXPECT_EQ(sys->getBuffer()[21], 255.0f);
            EXPECT_EQ(sys->getBuffer()[22], 255.0f);
            EXPECT_EQ(sys->getBuffer()[23], 255.0f);

            EXPECT_EQ(sys->getBuffer()[24], 0.0f);
            EXPECT_EQ(sys->getBuffer()[25], 0.0f);
            EXPECT_EQ(sys->getBuffer()[26], 0.0f);

            EXPECT_EQ(sys->getBuffer()[27], 60.0f);
            EXPECT_EQ(sys->getBuffer()[28], 70.0f);

            EXPECT_EQ(sys->getBuffer()[29], 255.0f);
            EXPECT_EQ(sys->getBuffer()[30], 255.0f);
            EXPECT_EQ(sys->getBuffer()[31], 255.0f);
        }

        TEST(system_2d, buffer_values_with_detach)
        {
            EntitySystem ecs;

            MasterRenderer renderer;

            ecs.createSystem<UiComponentSystem>();

            auto sys = ecs.createMockSystem<Simple2DObjectSystem, Mock2DSimpleShape>(&renderer);

            EXPECT_EQ(sys->getCurrentSize(),     1);
            EXPECT_EQ(sys->getElementIndex(),    0);
            EXPECT_EQ(sys->getVisibleElements(), 0);

            auto list0 = makeSimple2DShape(&ecs, Shape2D::Square, 10, 20, {255.0f, 255.0f, 255.0f});

            EXPECT_EQ(sys->getCurrentSize(),     2);
            EXPECT_EQ(sys->getElementIndex(),    1);
            EXPECT_EQ(sys->getVisibleElements(), 1);

            EXPECT_EQ(sys->getBuffer()[0], 0.0f);
            EXPECT_EQ(sys->getBuffer()[1], 0.0f);
            EXPECT_EQ(sys->getBuffer()[2], 0.0f);

            EXPECT_EQ(sys->getBuffer()[3], 10.0f);
            EXPECT_EQ(sys->getBuffer()[4], 20.0f);

            EXPECT_EQ(sys->getBuffer()[5], 255.0f);
            EXPECT_EQ(sys->getBuffer()[6], 255.0f);
            EXPECT_EQ(sys->getBuffer()[7], 255.0f);

            makeSimple2DShape(&ecs, Shape2D::Square, 40, 50, {255.0f, 255.0f, 255.0f});

            EXPECT_EQ(sys->getCurrentSize(),     4);
            EXPECT_EQ(sys->getElementIndex(),    2);
            EXPECT_EQ(sys->getVisibleElements(), 2);

            EXPECT_EQ(sys->getBuffer()[0], 0.0f);
            EXPECT_EQ(sys->getBuffer()[1], 0.0f);
            EXPECT_EQ(sys->getBuffer()[2], 0.0f);

            EXPECT_EQ(sys->getBuffer()[3], 10.0f);
            EXPECT_EQ(sys->getBuffer()[4], 20.0f);

            EXPECT_EQ(sys->getBuffer()[5], 255.0f);
            EXPECT_EQ(sys->getBuffer()[6], 255.0f);
            EXPECT_EQ(sys->getBuffer()[7], 255.0f);

            EXPECT_EQ(sys->getBuffer()[8], 0.0f);
            EXPECT_EQ(sys->getBuffer()[9], 0.0f);
            EXPECT_EQ(sys->getBuffer()[10], 0.0f);

            EXPECT_EQ(sys->getBuffer()[11], 40.0f);
            EXPECT_EQ(sys->getBuffer()[12], 50.0f);

            EXPECT_EQ(sys->getBuffer()[13], 255.0f);
            EXPECT_EQ(sys->getBuffer()[14], 255.0f);
            EXPECT_EQ(sys->getBuffer()[15], 255.0f);

            auto list2 = makeSimple2DShape(&ecs, Shape2D::Square, 60, 70, {255.0f, 255.0f, 255.0f});
            list2.get<UiComponent>()->setVisibility(false);

            EXPECT_EQ(sys->getCurrentSize(),     4);
            EXPECT_EQ(sys->getElementIndex(),    3);
            EXPECT_EQ(sys->getVisibleElements(), 2);

            EXPECT_EQ(sys->getBuffer()[0], 0.0f);
            EXPECT_EQ(sys->getBuffer()[1], 0.0f);
            EXPECT_EQ(sys->getBuffer()[2], 0.0f);

            EXPECT_EQ(sys->getBuffer()[3], 10.0f);
            EXPECT_EQ(sys->getBuffer()[4], 20.0f);

            EXPECT_EQ(sys->getBuffer()[5], 255.0f);
            EXPECT_EQ(sys->getBuffer()[6], 255.0f);
            EXPECT_EQ(sys->getBuffer()[7], 255.0f);

            EXPECT_EQ(sys->getBuffer()[8], 0.0f);
            EXPECT_EQ(sys->getBuffer()[9], 0.0f);
            EXPECT_EQ(sys->getBuffer()[10], 0.0f);

            EXPECT_EQ(sys->getBuffer()[11], 40.0f);
            EXPECT_EQ(sys->getBuffer()[12], 50.0f);

            EXPECT_EQ(sys->getBuffer()[13], 255.0f);
            EXPECT_EQ(sys->getBuffer()[14], 255.0f);
            EXPECT_EQ(sys->getBuffer()[15], 255.0f);

            EXPECT_EQ(sys->getBuffer()[16], 0.0f);
            EXPECT_EQ(sys->getBuffer()[17], 0.0f);
            EXPECT_EQ(sys->getBuffer()[18], 0.0f);

            EXPECT_EQ(sys->getBuffer()[19], 60.0f);
            EXPECT_EQ(sys->getBuffer()[20], 70.0f);

            EXPECT_EQ(sys->getBuffer()[21], 255.0f);
            EXPECT_EQ(sys->getBuffer()[22], 255.0f);
            EXPECT_EQ(sys->getBuffer()[23], 255.0f);

            makeSimple2DShape(&ecs, Shape2D::Square, 80, 90, {255.0f, 255.0f, 255.0f});

            EXPECT_EQ(sys->getCurrentSize(),     8);
            EXPECT_EQ(sys->getElementIndex(),    4);
            EXPECT_EQ(sys->getVisibleElements(), 3);

            EXPECT_EQ(sys->getBuffer()[0], 0.0f);
            EXPECT_EQ(sys->getBuffer()[1], 0.0f);
            EXPECT_EQ(sys->getBuffer()[2], 0.0f);

            EXPECT_EQ(sys->getBuffer()[3], 10.0f);
            EXPECT_EQ(sys->getBuffer()[4], 20.0f);

            EXPECT_EQ(sys->getBuffer()[5], 255.0f);
            EXPECT_EQ(sys->getBuffer()[6], 255.0f);
            EXPECT_EQ(sys->getBuffer()[7], 255.0f);

            EXPECT_EQ(sys->getBuffer()[8], 0.0f);
            EXPECT_EQ(sys->getBuffer()[9], 0.0f);
            EXPECT_EQ(sys->getBuffer()[10], 0.0f);

            EXPECT_EQ(sys->getBuffer()[11], 40.0f);
            EXPECT_EQ(sys->getBuffer()[12], 50.0f);

            EXPECT_EQ(sys->getBuffer()[13], 255.0f);
            EXPECT_EQ(sys->getBuffer()[14], 255.0f);
            EXPECT_EQ(sys->getBuffer()[15], 255.0f);

            EXPECT_EQ(sys->getBuffer()[16], 0.0f);
            EXPECT_EQ(sys->getBuffer()[17], 0.0f);
            EXPECT_EQ(sys->getBuffer()[18], 0.0f);

            EXPECT_EQ(sys->getBuffer()[19], 80.0f);
            EXPECT_EQ(sys->getBuffer()[20], 90.0f);

            EXPECT_EQ(sys->getBuffer()[21], 255.0f);
            EXPECT_EQ(sys->getBuffer()[22], 255.0f);
            EXPECT_EQ(sys->getBuffer()[23], 255.0f);

            EXPECT_EQ(sys->getBuffer()[24], 0.0f);
            EXPECT_EQ(sys->getBuffer()[25], 0.0f);
            EXPECT_EQ(sys->getBuffer()[26], 0.0f);

            EXPECT_EQ(sys->getBuffer()[27], 60.0f);
            EXPECT_EQ(sys->getBuffer()[28], 70.0f);

            EXPECT_EQ(sys->getBuffer()[29], 255.0f);
            EXPECT_EQ(sys->getBuffer()[30], 255.0f);
            EXPECT_EQ(sys->getBuffer()[31], 255.0f);

            ecs.detach<UiComponent>(list0.entity);

            EXPECT_EQ(sys->getCurrentSize(),     8);
            EXPECT_EQ(sys->getElementIndex(),    3);
            EXPECT_EQ(sys->getVisibleElements(), 2);

            EXPECT_EQ(sys->getBuffer()[0], 0.0f);
            EXPECT_EQ(sys->getBuffer()[1], 0.0f);
            EXPECT_EQ(sys->getBuffer()[2], 0.0f);

            EXPECT_EQ(sys->getBuffer()[3], 80.0f);
            EXPECT_EQ(sys->getBuffer()[4], 90.0f);

            EXPECT_EQ(sys->getBuffer()[5], 255.0f);
            EXPECT_EQ(sys->getBuffer()[6], 255.0f);
            EXPECT_EQ(sys->getBuffer()[7], 255.0f);

            EXPECT_EQ(sys->getBuffer()[8], 0.0f);
            EXPECT_EQ(sys->getBuffer()[9], 0.0f);
            EXPECT_EQ(sys->getBuffer()[10], 0.0f);

            EXPECT_EQ(sys->getBuffer()[11], 40.0f);
            EXPECT_EQ(sys->getBuffer()[12], 50.0f);

            EXPECT_EQ(sys->getBuffer()[13], 255.0f);
            EXPECT_EQ(sys->getBuffer()[14], 255.0f);
            EXPECT_EQ(sys->getBuffer()[15], 255.0f);

            EXPECT_EQ(sys->getBuffer()[16], 0.0f);
            EXPECT_EQ(sys->getBuffer()[17], 0.0f);
            EXPECT_EQ(sys->getBuffer()[18], 0.0f);

            EXPECT_EQ(sys->getBuffer()[19], 60.0f);
            EXPECT_EQ(sys->getBuffer()[20], 70.0f);

            EXPECT_EQ(sys->getBuffer()[21], 255.0f);
            EXPECT_EQ(sys->getBuffer()[22], 255.0f);
            EXPECT_EQ(sys->getBuffer()[23], 255.0f);
        }

    }
}