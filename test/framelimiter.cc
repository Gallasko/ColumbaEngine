#include "stdafx.h"

#include "gtest/gtest.h"

#include "Profiler/framelimiter.h"

namespace pg
{
    namespace test
    {
        namespace
        {
            using Limiter = BasicFrameLimiter<std::chrono::steady_clock>;
            using TimePoint = Limiter::TimePoint;
            using Us = std::chrono::microseconds;

            TimePoint at(int64_t us) { return TimePoint{Us{us}}; }
        }

        TEST(framelimiter_test, uncapped_is_noop)
        {
            Limiter limiter;

            EXPECT_EQ(limiter.getTargetFPS(), 0);

            EXPECT_EQ(limiter.nextDelay(at(0)).count(), 0);
            EXPECT_EQ(limiter.nextDelay(at(123456)).count(), 0);
        }

        TEST(framelimiter_test, paces_to_target_period)
        {
            Limiter limiter;
            limiter.setTargetFPS(100); // 10000 us period

            // First call arms the limiter without sleeping
            EXPECT_EQ(limiter.nextDelay(at(0)).count(), 0);

            // Frame body took 2ms: sleep the remaining 8ms
            EXPECT_EQ(limiter.nextDelay(at(2000)).count(), 8000);

            // Next deadline advanced by exactly one period (drift-free)
            EXPECT_EQ(limiter.nextDelay(at(12000)).count(), 8000);
        }

        TEST(framelimiter_test, slow_frame_gets_no_sleep)
        {
            Limiter limiter;
            limiter.setTargetFPS(100);

            EXPECT_EQ(limiter.nextDelay(at(0)).count(), 0);       // arms, next = 10000

            // Frame slightly over budget (12ms > 10ms, but < 2 periods): no sleep,
            // deadline still advances so the average rate recovers
            EXPECT_EQ(limiter.nextDelay(at(12000)).count(), 0);   // next = 20000
            EXPECT_EQ(limiter.nextDelay(at(14000)).count(), 6000);
        }

        TEST(framelimiter_test, resyncs_after_stall)
        {
            Limiter limiter;
            limiter.setTargetFPS(100);

            EXPECT_EQ(limiter.nextDelay(at(0)).count(), 0);       // next = 10000

            // Stalled way past the deadline (> one full period late): resync,
            // no sleep, and pacing resumes from the stall point
            EXPECT_EQ(limiter.nextDelay(at(50000)).count(), 0);   // next = 60000
            EXPECT_EQ(limiter.nextDelay(at(52000)).count(), 8000);
        }

        TEST(framelimiter_test, runtime_retarget)
        {
            Limiter limiter;

            limiter.setTargetFPS(60);
            EXPECT_EQ(limiter.getTargetFPS(), 60);

            limiter.setTargetFPS(30);
            EXPECT_EQ(limiter.getTargetFPS(), 30);

            EXPECT_EQ(limiter.nextDelay(at(0)).count(), 0);        // arms, period 33333
            EXPECT_EQ(limiter.nextDelay(at(3333)).count(), 30000);

            // Disabling mid-run stops all pacing
            limiter.setTargetFPS(0);
            EXPECT_EQ(limiter.nextDelay(at(40000)).count(), 0);
            EXPECT_EQ(limiter.getTargetFPS(), 0);

            // Re-enabling re-arms from scratch
            limiter.setTargetFPS(100);
            EXPECT_EQ(limiter.nextDelay(at(100000)).count(), 0);
            EXPECT_EQ(limiter.nextDelay(at(102000)).count(), 8000);
        }
    }
}
