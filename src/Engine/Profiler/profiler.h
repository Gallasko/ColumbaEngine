#pragma once

#include <string>
#include <vector>
#include <chrono>
#include <mutex>
#include <cstdint>
#include <fstream>
#include <algorithm>
#include <map>
#include <atomic>

namespace pg
{
    // Completed profiling interval (already computed)
    struct ProfileEvent
    {
        std::string name;
        uint64_t frameNumber;
        uint64_t ecsPass;
        double startMs;
        double durationMs;
        std::string category;
        uint32_t threadId;

        ProfileEvent(const std::string& n, uint64_t frame, uint64_t pass, double start, double dur, const std::string& cat, uint32_t tid)
            : name(n), frameNumber(frame), ecsPass(pass), startMs(start), durationMs(dur), category(cat), threadId(tid)
        {}
    };

    // Computed interval from begin/end pairs
    struct ProfileInterval
    {
        std::string name;
        uint64_t frameNumber;
        uint64_t ecsPass;
        double startMs;
        double durationMs;
        std::string category;
        uint32_t threadId;

        ProfileInterval(const std::string& n, uint64_t frame, uint64_t pass, double start, double dur, const std::string& cat, uint32_t tid)
            : name(n), frameNumber(frame), ecsPass(pass), startMs(start), durationMs(dur), category(cat), threadId(tid)
        {}
    };

    // Main profiler class - singleton
    class Profiler
    {
    private:
        // Pending begin events (per thread)
        struct PendingEvent {
            std::string name;
            std::string category;
            double startMs;
            uint64_t frameNumber;
            uint64_t ecsPass;
        };

        // Completed intervals (already filtered for zero-duration)
        std::vector<ProfileEvent> events;
        std::map<uint32_t, std::vector<PendingEvent>> pendingEvents;
        mutable std::mutex eventMutex;

        uint64_t currentFrame;
        std::atomic<uint64_t> ecsPassCounter{0};
        double frameStartTime;

        std::chrono::steady_clock::time_point sessionStart;

        size_t maxEvents;
        bool enabled;
        bool recordFrameEvents;  // Whether to record Frame timing events

        Profiler();

    public:
        static Profiler& instance();

        // Frame control
        void beginFrame();
        void endFrame();

        // Event recording
        void recordBegin(const std::string& name, const std::string& category);
        void recordEnd(const std::string& name, const std::string& category);

        // Zero-duration marker event (bypasses the minimum-duration filter)
        void recordInstant(const std::string& name, const std::string& category);

        // ECS-pass correlation: bumped once per taskflow graph iteration
        // (from the BasicTask), stamped on every recorded event so the
        // simulation loop and the render loop can be correlated.
        uint64_t beginEcsPass() { return ecsPassCounter.fetch_add(1, std::memory_order_relaxed) + 1; }
        uint64_t getCurrentEcsPass() const { return ecsPassCounter.load(std::memory_order_relaxed); }

        // Data access
        std::vector<ProfileInterval> computeIntervals(uint64_t startFrame, uint64_t endFrame) const;
        std::vector<ProfileInterval> computeLastFrames(size_t numFrames) const;

        // Copy of all completed events starting at or after sinceMs
        // (cheap single pass under the lock; the overlay's main query)
        std::vector<ProfileEvent> snapshotSince(double sinceMs) const;

        // Current time on the profiler's session clock, in milliseconds
        double nowMs() const { return getCurrentTimeMs(); }

        // Export
        void exportToCSV(const std::string& filename, size_t numFrames = 300);
        void exportAllToCSV(const std::string& filename);  // Export all captured frames

        // Control
        void setEnabled(bool enable) { enabled = enable; }
        bool isEnabled() const { return enabled; }
        void setRecordFrameEvents(bool record) { recordFrameEvents = record; }
        void clear();

        uint64_t getCurrentFrame() const { return currentFrame; }

    private:
        double getCurrentTimeMs() const;
        uint32_t getCurrentThreadId() const;
    };

    // RAII scope timer
    class ProfileScope
    {
    private:
        std::string name;
        std::string category;

    public:
        ProfileScope(const std::string& n, const std::string& cat)
            : name(n), category(cat)
        {
            Profiler::instance().recordBegin(name, category);
        }

        ~ProfileScope()
        {
            Profiler::instance().recordEnd(name, category);
        }
    };
}

// Profiling macros
#ifdef PROFILE
    #define PROFILE_FRAME_BEGIN() pg::Profiler::instance().beginFrame()
    #define PROFILE_FRAME_END() pg::Profiler::instance().endFrame()
    #define PROFILE_SCOPE(name, category) pg::ProfileScope _profileScope##__LINE__(name, category)
    #define PROFILE_BEGIN(name, category) pg::Profiler::instance().recordBegin(name, category)
    #define PROFILE_END(name, category) pg::Profiler::instance().recordEnd(name, category)
#else
    #define PROFILE_FRAME_BEGIN()
    #define PROFILE_FRAME_END()
    #define PROFILE_SCOPE(name, category)
    #define PROFILE_BEGIN(name, category)
    #define PROFILE_END(name, category)
#endif