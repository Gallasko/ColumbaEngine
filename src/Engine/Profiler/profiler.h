#pragma once

#include <string>
#include <vector>
#include <chrono>
#include <mutex>
#include <cstdint>
#include <fstream>
#include <algorithm>
#include <map>

namespace pg
{
    // Completed profiling interval (already computed)
    struct ProfileEvent
    {
        std::string name;
        uint64_t frameNumber;
        double startMs;
        double durationMs;
        std::string category;
        uint32_t threadId;

        ProfileEvent(const std::string& n, uint64_t frame, double start, double dur, const std::string& cat, uint32_t tid)
            : name(n), frameNumber(frame), startMs(start), durationMs(dur), category(cat), threadId(tid)
        {}
    };

    // Computed interval from begin/end pairs
    struct ProfileInterval
    {
        std::string name;
        uint64_t frameNumber;
        double startMs;
        double durationMs;
        std::string category;
        uint32_t threadId;

        ProfileInterval(const std::string& n, uint64_t frame, double start, double dur, const std::string& cat, uint32_t tid)
            : name(n), frameNumber(frame), startMs(start), durationMs(dur), category(cat), threadId(tid)
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
        };

        // Completed intervals (already filtered for zero-duration)
        std::vector<ProfileEvent> events;
        std::map<uint32_t, std::vector<PendingEvent>> pendingEvents;
        mutable std::mutex eventMutex;

        uint64_t currentFrame;
        double frameStartTime;

        std::chrono::steady_clock::time_point sessionStart;

        size_t maxEvents;
        bool enabled;

        Profiler();

    public:
        static Profiler& instance();

        // Frame control
        void beginFrame();
        void endFrame();

        // Event recording
        void recordBegin(const std::string& name, const std::string& category);
        void recordEnd(const std::string& name, const std::string& category);

        // Data access
        std::vector<ProfileInterval> computeIntervals(uint64_t startFrame, uint64_t endFrame) const;
        std::vector<ProfileInterval> computeLastFrames(size_t numFrames) const;

        // Export
        void exportToCSV(const std::string& filename, size_t numFrames = 300);
        void exportAllToCSV(const std::string& filename);  // Export all captured frames

        // Control
        void setEnabled(bool enable) { enabled = enable; }
        bool isEnabled() const { return enabled; }
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