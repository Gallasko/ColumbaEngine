#include "stdafx.h"

#include "gtest/gtest.h"

#include "UI/uisystem.h"

namespace pg
{
    namespace test
    {
        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(ui_component_test, initialization)
        {
            UiComponent component;

            EXPECT_FLOAT_EQ(component.pos.x,  0.0f);
            EXPECT_FLOAT_EQ(component.pos.y,  0.0f);
            EXPECT_FLOAT_EQ(component.pos.z,  0.0f);

            EXPECT_FLOAT_EQ(component.width,  0.0f);
            EXPECT_FLOAT_EQ(component.height, 0.0f);

            EXPECT_FLOAT_EQ(component.top,    0.0f);
            EXPECT_FLOAT_EQ(component.left,   0.0f);
            EXPECT_FLOAT_EQ(component.right,  0.0f);
            EXPECT_FLOAT_EQ(component.bottom, 0.0f);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(ui_component_test, change_position)
        {
            UiComponent component;

            component.setX(1.0f);
            component.setY(3.0f);
            component.setZ(2.0f);

            EXPECT_FLOAT_EQ(component.pos.x,  1.0f);
            EXPECT_FLOAT_EQ(component.pos.y,  3.0f);
            EXPECT_FLOAT_EQ(component.pos.z,  2.0f);

            component.pos.x =  1.5f;
            component.pos.y =  2.2f;
            component.pos.z = -1.8f;

            EXPECT_FLOAT_EQ(component.pos.x,  1.5f);
            EXPECT_FLOAT_EQ(component.pos.y,  2.2f);
            EXPECT_FLOAT_EQ(component.pos.z, -1.8f);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(ui_component_test, change_size)
        {
            UiComponent component;

            component.setWidth(2.0f);
            component.setHeight(4.0f);

            EXPECT_FLOAT_EQ(component.width,   2.0f);
            EXPECT_FLOAT_EQ(component.height,  4.0f);

            component.width = 1.4f;
            component.height = -2.5f;

            EXPECT_FLOAT_EQ(component.width,   1.4f);
            EXPECT_FLOAT_EQ(component.height, -2.5f);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(ui_component_test, check_anchor)
        {
            UiComponent component;

            component.pos.x  = 2.0f;
            component.pos.y  = 1.5f;

            component.width  = 5.4f;
            component.height = 2.2f;

            EXPECT_FLOAT_EQ(component.pos.x, 2.0f);
            EXPECT_FLOAT_EQ(component.pos.y, 1.5f);
            EXPECT_FLOAT_EQ(component.width,  5.4f);
            EXPECT_FLOAT_EQ(component.height, 2.2f);

            //                x,   y,   w,   h
            // Component = (2.0, 1.5, 5.4, 2.2)
            EXPECT_FLOAT_EQ(component.top,    1.5f);
            EXPECT_FLOAT_EQ(component.left,   2.0f);
            EXPECT_FLOAT_EQ(component.right,  7.4f);
            EXPECT_FLOAT_EQ(component.bottom, 3.7f);

            component.setWidth(3.6f);
            component.setHeight(-1.4f);

            //                x,   y,   w,    h
            // Component = (2.0, 1.5, 3.6, -1.4)
            EXPECT_FLOAT_EQ(component.top,    1.5f);
            EXPECT_FLOAT_EQ(component.left,   2.0f);
            EXPECT_FLOAT_EQ(component.right,  5.6f);
            EXPECT_FLOAT_EQ(component.bottom, 0.1f);

            component.setX(2.1f);
            component.setY(-0.3f);

            //                x,    y,   w,    h
            // Component = (2.1, -0.3, 3.6, -1.4)
            EXPECT_FLOAT_EQ(component.top,    -0.3f);
            EXPECT_FLOAT_EQ(component.left,    2.1f);
            EXPECT_FLOAT_EQ(component.right,   5.7f);
            EXPECT_FLOAT_EQ(component.bottom, -1.7f);
        }

    } // namespace test

} // namespace pg