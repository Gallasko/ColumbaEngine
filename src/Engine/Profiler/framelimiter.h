#pragma once

// Frame pacing utility. Always compiled (not PROFILE-gated): zero cost
// when no target FPS is set.

#include <algorithm>
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

    /**
     * @brief Phase-locks a worker loop to N iterations per render frame.
     *
     * The render thread calls frameStarted() at the top of every frame; the
     * worker thread calls pace() before each iteration. The N iterations are
     * scheduled evenly across the measured frame period, so the budget is
     * never burned in a burst at the start of the frame and the worker is
     * never left stalled across a render boundary. If the renderer stalls
     * (no new frame within two periods), the worker falls back to the same
     * average rate on a synthetic schedule, so the simulation never blocks
     * on a stuck renderer.
     *
     * frameStarted() is single-writer (render thread); pace() is
     * single-caller (worker thread); setPassesPerFrame() is thread-safe.
     */
    template <typename Clock = std::chrono::steady_clock>
    class BasicFramePassPacer
    {
    public:
        using TimePoint = typename Clock::time_point;

        /** n <= 0 disables the pacer entirely */
        void setPassesPerFrame(int n) { passesPerFrame.store(n, std::memory_order_relaxed); }

        int getPassesPerFrame() const { return passesPerFrame.load(std::memory_order_relaxed); }

        bool enabled() const { return passesPerFrame.load(std::memory_order_relaxed) > 0; }

        /** Render thread: call once at the top of every frame. */
        void frameStarted()
        {
            const int64_t now = nowUs();

            if (lastRenderStartUs > 0)
            {
                int64_t p = now - lastRenderStartUs;

                // Clamp so a debugger pause or window drag doesn't poison the schedule
                p = std::max<int64_t>(1000, std::min<int64_t>(p, 100000));
                framePeriodUs.store(p, std::memory_order_relaxed);
            }

            lastRenderStartUs = now;
            frameStartUs.store(now, std::memory_order_release);
        }

        /** Worker thread: call before each iteration. */
        void pace()
        {
            const int n = passesPerFrame.load(std::memory_order_relaxed);
            const int64_t period = framePeriodUs.load(std::memory_order_relaxed);

            // Disabled, or no frame signal measured yet: run free
            if (n <= 0 or period <= 0)
                return;

            int64_t fs = frameStartUs.load(std::memory_order_acquire);

            if (fs != seenFrameStartUs)
            {
                seenFrameStartUs = fs;
                baseUs = fs;
                passesThisFrame = 0;
            }

            if (passesThisFrame >= n)
            {
                // Budget for this frame spent: wait for the next frame start,
                // but never longer than one extra period past the expected one
                const int64_t giveUpUs = baseUs + 2 * period;

                while (frameStartUs.load(std::memory_order_acquire) == fs and nowUs() < giveUpUs)
                    std::this_thread::sleep_for(std::chrono::microseconds(200));

                fs = frameStartUs.load(std::memory_order_acquire);

                if (fs != seenFrameStartUs)
                {
                    seenFrameStartUs = fs;
                    baseUs = fs;
                }
                else
                {
                    // Renderer stalled: advance a synthetic frame so the
                    // average rate stays at n passes per period
                    baseUs += period;
                }

                passesThisFrame = 0;
            }

            // Spread the n passes evenly across the frame
            const int64_t deadlineUs = baseUs + static_cast<int64_t>(passesThisFrame) * period / n;
            const int64_t now = nowUs();

            if (deadlineUs > now)
                std::this_thread::sleep_for(std::chrono::microseconds(deadlineUs - now));

            passesThisFrame++;
        }

        // Introspection (tests / overlay readouts). Worker-thread values;
        // only meaningful when read from the pacing thread.
        int64_t measuredFramePeriodUs() const { return framePeriodUs.load(std::memory_order_relaxed); }
        int passesScheduledThisFrame() const { return passesThisFrame; }
        int64_t scheduleBaseUs() const { return baseUs; }

    private:
        static int64_t nowUs()
        {
            return std::chrono::duration_cast<std::chrono::microseconds>(Clock::now().time_since_epoch()).count();
        }

        std::atomic<int> passesPerFrame{0};
        std::atomic<int64_t> frameStartUs{0};
        std::atomic<int64_t> framePeriodUs{0};

        // Render-thread state
        int64_t lastRenderStartUs = 0;

        // Worker-thread state
        int64_t seenFrameStartUs = -1;
        int64_t baseUs = 0;
        int passesThisFrame = 0;
    };

    using FramePassPacer = BasicFramePassPacer<>;
}
