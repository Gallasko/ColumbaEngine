#include "gtest/gtest.h"

#include "2D/position.h"
#include "ECS/entitysystem.h"

namespace pg
{
    struct PositionTestSystem : public System<Listener<PositionComponentChangedEvent>, StoragePolicy>
    {
        virtual void onEvent(const PositionComponentChangedEvent& event) override
        {
            nbEventReceived++;
        }

        size_t nbEventReceived = 0;
    };

    namespace test
    {
        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(position_component_test, initialization)
        {
            PositionComponent component;
            
            EXPECT_FLOAT_EQ(component.x,  0.0f);
            EXPECT_FLOAT_EQ(component.y,  0.0f);
            EXPECT_FLOAT_EQ(component.z,  0.0f);

            EXPECT_FLOAT_EQ(component.width,  0.0f);
            EXPECT_FLOAT_EQ(component.height, 0.0f);

            EXPECT_EQ(component.visible, true);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(position_component_test, set_basic_value)
        {
            PositionComponent component;
            
            EXPECT_FLOAT_EQ(component.x,  0.0f);
            EXPECT_FLOAT_EQ(component.y,  0.0f);
            EXPECT_FLOAT_EQ(component.z,  0.0f);

            component.setX(1.5f);
            component.setY(2.2f);
            component.setZ(-1.8f);

            EXPECT_FLOAT_EQ(component.x,  1.5f);
            EXPECT_FLOAT_EQ(component.y,  2.2f);
            EXPECT_FLOAT_EQ(component.z, -1.8f);

            EXPECT_FLOAT_EQ(component.width,  0.0f);
            EXPECT_FLOAT_EQ(component.height, 0.0f);

            component.setWidth(2.34f);
            component.setHeight(4.56f);

            EXPECT_FLOAT_EQ(component.width,  2.34f);
            EXPECT_FLOAT_EQ(component.height, 4.56f);

            EXPECT_EQ(component.visible, true);

            component.setVisibility(false);

            EXPECT_EQ(component.visible, false);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(position_component_test, set_basic_value_in_stopped_ecs)
        {
            EntitySystem ecs;

            EXPECT_FALSE(ecs.isRunning());

            ecs.createSystem<PositionComponentSystem>();
            auto sys = ecs.createSystem<PositionTestSystem>();

            EXPECT_EQ(sys->nbEventReceived, 0);

            auto entity = ecs.createEntity();

            auto pos = ecs.attach<PositionComponent>(entity);

            pos->setX(1.5f);
            pos->setY(2.2f);
            pos->setZ(-1.8f);

            EXPECT_FLOAT_EQ(pos->x,  1.5f);
            EXPECT_FLOAT_EQ(pos->y,  2.2f);
            EXPECT_FLOAT_EQ(pos->z, -1.8f);

            EXPECT_EQ(sys->nbEventReceived, 3);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(position_component_test, set_basic_value_in_started_ecs)
        {
            EntitySystem ecs;

            EXPECT_FALSE(ecs.isRunning());

            ecs.createSystem<PositionComponentSystem>();
            auto sys = ecs.createSystem<PositionTestSystem>();

            ecs.fakeStart();
            EXPECT_TRUE(ecs.isRunning());

            EXPECT_EQ(sys->nbEventReceived, 0);

            auto entity = ecs.createEntity();

            auto pos = ecs.attach<PositionComponent>(entity);

            pos->setX(1.5f);
            pos->setY(2.2f);
            pos->setZ(-1.8f);

            EXPECT_FLOAT_EQ(pos->x,  1.5f);
            EXPECT_FLOAT_EQ(pos->y,  2.2f);
            EXPECT_FLOAT_EQ(pos->z, -1.8f);

            EXPECT_EQ(sys->nbEventReceived, 0);

            auto ent0 = ecs.getEntity(entity.id);

            if (ent0)
            {
                EXPECT_FALSE("Entity should not exist in the system now");
                return;
            }

            ecs.executeOnce();

            auto ent = ecs.getEntity(entity.id);

            if (not ent)
            {
                EXPECT_FALSE("Entity should exist in the system now");
                return;
            }

            auto pos2 = ent->get<PositionComponent>();
            EXPECT_FLOAT_EQ(pos2->x,  1.5f);
            EXPECT_FLOAT_EQ(pos2->y,  2.2f);
            EXPECT_FLOAT_EQ(pos2->z, -1.8f);
            
            EXPECT_EQ(sys->nbEventReceived, 3);
        }

    } // namespace test
    
} // namespace pg