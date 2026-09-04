#pragma once

// Frame pacing utility. Always compiled (not PROFILE-gated): zero cost
// when no target FPS is set.

#include <atomic>
#include <chrono>
#include <cstdint>
#include <thread>

namespace pg
{
    /**
     * @brief Paces a loop to a target FPS.
     *
     * pace() must be called from a single thread (once per loop iteration);
     * setTargetFPS() can be called from any thread at any time.
     *
     * Templated on the clock so tests can drive nextDelay() with a fake one.
     */
    template <typename Clock = std::chrono::steady_clock>
    class BasicFrameLimiter
    {
    public:
        using TimePoint = typename Clock::time_point;
        using Duration = std::chrono::microseconds;

        /** fps <= 0 disables pacing */
        void setTargetFPS(int fps)
        {
            targetUs.store(fps > 0 ? 1000000 / fps : 0, std::memory_order_relaxed);
        }

        int getTargetFPS() const
        {
            const auto t = targetUs.load(std::memory_order_relaxed);
            return t > 0 ? static_cast<int>(1000000 / t) : 0;
        }

        /**
         * @brief Pure pacing computation, exposed for tests.
         *
         * Given the current time, advances the internal deadline and returns
         * how long the pacing thread should sleep before starting the next
         * iteration (zero when uncapped, on the first call, or after a stall
         * longer than one period — the deadline then resyncs to now).
         *
         * Deadlines advance by exactly one period per call, so the average
         * rate is drift-free as long as the loop keeps up.
         */
        Duration nextDelay(TimePoint now)
        {
            const auto t = targetUs.load(std::memory_order_relaxed);

            if (t == 0)
            {
                armed = false;
                return Duration::zero();
            }

            const Duration period{t};

            if (not armed or now > next + period)
            {
                next = now + period;
                armed = true;
                return Duration::zero();
            }

            auto delay = std::chrono::duration_cast<Duration>(next - now);

            if (delay < Duration::zero())
                delay = Duration::zero();

            next += period;

            return delay;
        }

        /** Sleep until the next frame deadline (no-op when uncapped). */
        void pace()
        {
            const auto delay = nextDelay(Clock::now());

            if (delay > Duration::zero())
                std::this_thread::sleep_for(delay);
        }

    private:
        std::atomic<int64_t> targetUs{0};

        // Pacing-thread-local state (only touched by nextDelay/pace)
        TimePoint next{};
        bool armed = false;
    };

    using FrameLimiter = BasicFrameLimiter<>;
}
