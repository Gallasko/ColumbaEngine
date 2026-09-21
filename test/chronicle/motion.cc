#include "stdafx.h"

#include <gtest/gtest.h>

#include "Core/motion.h"

using namespace chronicle;

namespace pg
{
    namespace test
    {
        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(motion_test, reduced_flag_round_trips)
        {
            Motion::setReduced(false);
            EXPECT_FALSE(Motion::reduced());

            Motion::setReduced(true);
            EXPECT_TRUE(Motion::reduced());

            Motion::setReduced(false);
            EXPECT_FALSE(Motion::reduced());

            EXPECT_FLOAT_EQ(Motion::kMsPerPercent, 6.0f);
        }
    }
}
