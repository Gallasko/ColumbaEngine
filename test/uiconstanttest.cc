#include "stdafx.h"

#include "gtest/gtest.h"

#include "UI/uiconstant.h"

#include <iostream>

namespace pg
{
    namespace test
    {
        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(ui_value_test, initialization)
        {
            UiSize value;

            EXPECT_FLOAT_EQ(value, 0.0f);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(ui_value_test, assignment)
        {
            UiSize value;

            EXPECT_FLOAT_EQ(value, 0.0f);

            value = 1;

            EXPECT_FLOAT_EQ(value, 1.0f);

            value = 23.04f;

            EXPECT_FLOAT_EQ(value, 23.04f);

            UiSize value2 = -15.02f;

            value = value2;

            EXPECT_FLOAT_EQ(value, -15.02f);

            value2 = 10.37f;

            EXPECT_FLOAT_EQ(value, -15.02f);
            EXPECT_FLOAT_EQ(value2, 10.37f);

            UiSize value3 = 2.2f;

            value = &value3;

            EXPECT_FLOAT_EQ(value, 2.2f);

            value3 = 12;

            EXPECT_FLOAT_EQ(value, 12.0f);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(ui_value_test, basic_operation)
        {
            UiSize value = 1.0f;

            EXPECT_FLOAT_EQ(value + 10.0f, 11.0f);
            EXPECT_FLOAT_EQ(value - 10.0f, -9.0f);
            EXPECT_FLOAT_EQ(value * 5.0f,  5.0f);
            EXPECT_FLOAT_EQ(value / 2.0f,  0.5f);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(ui_value_test, assignment_operations)
        {
            UiSize value1 = 1.2f;
            UiSize value2 = 2.5f;

            UiSize res = value1 + value2;

            EXPECT_FLOAT_EQ(res, 3.7f);

            value1 = 1.8f;
            value2 = 3.1f;

            EXPECT_FLOAT_EQ(res, 4.9f);

            res = value1 - value2;

            EXPECT_FLOAT_EQ(res, -1.3f);

            value1 = 5.0f;
            value2 = 2.3f;

            EXPECT_FLOAT_EQ(res, 2.7f);

            res = value1 * value2;

            EXPECT_FLOAT_EQ(res, 11.5f);

            value1 = 2.0f;
            value2 = 3.0f;

            EXPECT_FLOAT_EQ(res, 6.0f);

            res = value1 / value2;

            EXPECT_FLOAT_EQ(res, 2.0f / 3.0f);

            value1 = 8.0f;
            value2 = 2.0f;

            EXPECT_FLOAT_EQ(res, 4.0f);

            EXPECT_FLOAT_EQ(-value1, -8.0f);

            value1 = 5.25f;
            value2 = 0.0f;

            EXPECT_FLOAT_EQ(res, 0.0f);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(ui_value_test, operation_with_temp_variable)
        {
            UiSize value1 = 1.2f;

            UiSize res = value1 + 3.8f;

            EXPECT_FLOAT_EQ(res, 5.0f);

            UiSize res2 = 5.1f;

            UiSize resTemp;

            {
                UiSize temp = -1.1f;

                res2 += temp;

                resTemp = temp + 0.5f;

                EXPECT_FLOAT_EQ(resTemp, -0.6f);
            }

            EXPECT_FLOAT_EQ(resTemp, -0.6f);

            UiSize resTemp2 = 1.1f;

            {
                UiSize temp = -1.1f;

                UiSize temp2 = 5.4f;

                resTemp2 += temp * temp2;

                EXPECT_FLOAT_EQ(resTemp2, -4.84f);
            }

            EXPECT_FLOAT_EQ(resTemp2, -4.84f);

            EXPECT_FLOAT_EQ(res2, 4.0f);

            UiSize res3 = res + 1.2f + res2;

            EXPECT_FLOAT_EQ(res3, 10.2f);

            value1 = -3.2f;

            EXPECT_FLOAT_EQ(res, 0.6f);
            EXPECT_FLOAT_EQ(res3, 5.8f);
        }


        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(ui_pos_test, initialization)
        {
            UiPosition pos;

            EXPECT_FLOAT_EQ(pos.x, 0.0f);
            EXPECT_FLOAT_EQ(pos.y, 0.0f);
            EXPECT_FLOAT_EQ(pos.z, 0.0f);

            UiPosition pos1 = {1.5f, -2.5f, 3.0f};

            EXPECT_FLOAT_EQ(pos1.x,  1.5f);
            EXPECT_FLOAT_EQ(pos1.y, -2.5f);
            EXPECT_FLOAT_EQ(pos1.z,  3.0f);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(ui_pos_test, assignment)
        {
            UiPosition pos;

            pos = {1.1f, 1.2f, 2.0f};

            EXPECT_FLOAT_EQ(pos.x, 1.1f);
            EXPECT_FLOAT_EQ(pos.y, 1.2f);
            EXPECT_FLOAT_EQ(pos.z, 2.0f);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(ui_pos_test, reference)
        {
            UiPosition pos;

            UiSize size = 1.5f;

            pos.x = &size;

            EXPECT_FLOAT_EQ(pos.x, 1.5f);

            size = 2.34f;

            EXPECT_FLOAT_EQ(pos.x, 2.34f);

            UiPosition ref = {0.3f, 2.2f, 4.0f};

            pos = &ref;

            EXPECT_FLOAT_EQ(pos.x, 0.3f);
            EXPECT_FLOAT_EQ(pos.y, 2.2f);
            EXPECT_FLOAT_EQ(pos.z, 4.0f);

            ref = {1.5f, 2.6f, 3.3f};

            EXPECT_FLOAT_EQ(pos.x, 1.5f);
            EXPECT_FLOAT_EQ(pos.y, 2.6f);
            EXPECT_FLOAT_EQ(pos.z, 3.3f);

            ref.x = -0.2f;

            EXPECT_FLOAT_EQ(pos.x, -0.2f);
            EXPECT_FLOAT_EQ(pos.y,  2.6f);
            EXPECT_FLOAT_EQ(pos.z,  3.3f);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(ui_pos_test, addition)
        {
            UiPosition pos1 = { 1.4f, 2.34f, 4.0f};
            UiPosition pos2 = {-2.3f, 1.2f,  2.0f};

            UiPosition res = pos1 + pos2;

            EXPECT_FLOAT_EQ(res.x, -0.9f);
            EXPECT_FLOAT_EQ(res.y,  3.54f);
            EXPECT_FLOAT_EQ(res.z,  6.0f);
        }

    } // namespace test

} // namespace pg