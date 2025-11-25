#include "profiler.h"
#include <thread>
#include <iostream>
#include <map>
#include <stack>
#include <iomanip>

namespace pg
{
    Profiler::Profiler()
        : currentFrame(0)
        , frameStartTime(0.0)
        , maxEvents(500000)  // Increased to 500k for fast frames
        , enabled(true)
    {
        sessionStart = std::chrono::steady_clock::now();
        events.reserve(maxEvents);
    }

    Profiler& Profiler::instance()
    {
        static Profiler profiler;
        return profiler;
    }

    void Profiler::beginFrame()
    {
        if (!enabled) return;

        currentFrame++;
        frameStartTime = getCurrentTimeMs();
        recordBegin("Frame", "Frame");
    }

    void Profiler::endFrame()
    {
        if (!enabled) return;

        recordEnd("Frame", "Frame");

        // Limit buffer size (keep only recent events)
        std::lock_guard<std::mutex> lock(eventMutex);
        if (events.size() > maxEvents)
        {
            // Remove oldest 20% when we hit the limit
            size_t removeCount = maxEvents / 5;
            events.erase(events.begin(), events.begin() + removeCount);
        }
    }

    void Profiler::recordBegin(const std::string& name, const std::string& category)
    {
        if (!enabled) return;

        std::lock_guard<std::mutex> lock(eventMutex);
        uint32_t tid = getCurrentThreadId();
        pendingEvents[tid].push_back({name, category, getCurrentTimeMs(), currentFrame});
    }

    void Profiler::recordEnd(const std::string& name, const std::string& category)
    {
        if (!enabled) return;

        double endTime = getCurrentTimeMs();
        uint32_t tid = getCurrentThreadId();

        std::lock_guard<std::mutex> lock(eventMutex);

        auto& pending = pendingEvents[tid];
        if (pending.empty()) return;

        // Find matching begin event (search backwards for nested scopes)
        for (auto it = pending.rbegin(); it != pending.rend(); ++it)
        {
            if (it->name == name && it->category == category)
            {
                double duration = endTime - it->startMs;

                // Todo make the cut off parametrable
                // Only store if duration is meaningful (>= 0.002ms = 2us)
                if (duration >= 0.002)
                {
                    events.emplace_back(name, it->frameNumber, it->startMs, duration, category, tid);
                }

                // Remove the pending event
                pending.erase(std::next(it).base());
                break;
            }
        }
    }

    std::vector<ProfileInterval> Profiler::computeIntervals(uint64_t startFrame, uint64_t endFrame) const
    {
        std::lock_guard<std::mutex> lock(eventMutex);

        std::vector<ProfileInterval> intervals;

        // Events are already completed intervals, just filter by frame range
        for (const auto& event : events)
        {
            if (event.frameNumber >= startFrame && event.frameNumber <= endFrame)
            {
                intervals.emplace_back(
                    event.name,
                    event.frameNumber,
                    event.startMs,
                    event.durationMs,
                    event.category,
                    event.threadId
                );
            }
        }

        return intervals;
    }

    std::vector<ProfileInterval> Profiler::computeLastFrames(size_t numFrames) const
    {
        if (currentFrame == 0)
            return {};

        uint64_t startFrame = currentFrame > numFrames ? currentFrame - numFrames : 0;
        return computeIntervals(startFrame, currentFrame);
    }

    void Profiler::exportToCSV(const std::string& filename, size_t numFrames)
    {
        if (!enabled)
        {
            std::cout << "Profiler is disabled, skipping CSV export" << std::endl;
            return;
        }

        std::ofstream file(filename);
        if (!file.is_open())
        {
            std::cerr << "Failed to open file for profiler export: " << filename << std::endl;
            return;
        }

        // Compute intervals for last N frames
        uint64_t startFrame = currentFrame > numFrames ? currentFrame - numFrames : 0;
        auto intervals = computeIntervals(startFrame, currentFrame);

        // Write header
        file << "frame,name,category,start_ms,duration_ms,thread_id\n";

        // Write data
        file << std::fixed << std::setprecision(6);
        for (const auto& interval : intervals)
        {
            file << interval.frameNumber << ","
                 << "\"" << interval.name << "\","
                 << "\"" << interval.category << "\","
                 << interval.startMs << ","
                 << interval.durationMs << ","
                 << interval.threadId << "\n";
        }

        file.close();

        std::cout << "Profile data exported to: " << filename << std::endl;
        std::cout << "Frames: " << startFrame << " to " << currentFrame
                  << " (" << intervals.size() << " intervals)" << std::endl;
    }

    void Profiler::exportAllToCSV(const std::string& filename)
    {
        if (!enabled)
        {
            std::cout << "Profiler is disabled, skipping CSV export" << std::endl;
            return;
        }

        std::ofstream file(filename);
        if (!file.is_open())
        {
            std::cerr << "Failed to open file for profiler export: " << filename << std::endl;
            return;
        }

        // Get all frames
        std::lock_guard<std::mutex> lock(eventMutex);

        uint64_t minFrame = UINT64_MAX;
        uint64_t maxFrame = 0;

        for (const auto& event : events)
        {
            if (event.frameNumber < minFrame) minFrame = event.frameNumber;
            if (event.frameNumber > maxFrame) maxFrame = event.frameNumber;
        }

        // Release lock and compute intervals
        eventMutex.unlock();
        auto intervals = computeIntervals(minFrame, maxFrame);
        eventMutex.lock();

        // Write header
        file << "frame,name,category,start_ms,duration_ms,thread_id\n";

        // Write data
        file << std::fixed << std::setprecision(6);
        for (const auto& interval : intervals)
        {
            file << interval.frameNumber << ","
                 << "\"" << interval.name << "\","
                 << "\"" << interval.category << "\","
                 << interval.startMs << ","
                 << interval.durationMs << ","
                 << interval.threadId << "\n";
        }

        file.close();

        std::cout << "Profile data exported to: " << filename << std::endl;
        std::cout << "All frames: " << minFrame << " to " << maxFrame
                  << " (total " << (maxFrame - minFrame + 1) << " frames, "
                  << intervals.size() << " intervals)" << std::endl;
    }

    void Profiler::clear()
    {
        std::lock_guard<std::mutex> lock(eventMutex);
        events.clear();
        currentFrame = 0;
    }

    double Profiler::getCurrentTimeMs() const
    {
        auto now = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(now - sessionStart);
        return duration.count() / 1000.0; // Convert to milliseconds
    }

    uint32_t Profiler::getCurrentThreadId() const
    {
        return static_cast<uint32_t>(std::hash<std::thread::id>{}(std::this_thread::get_id()));
    }
}
