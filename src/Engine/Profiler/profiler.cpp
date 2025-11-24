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
        , maxEvents(100000)
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

        std::lock_guard<std::mutex> lock(eventMutex);
        events.emplace_back("Frame", currentFrame, frameStartTime, true, "Frame", getCurrentThreadId());
    }

    void Profiler::endFrame()
    {
        if (!enabled) return;

        std::lock_guard<std::mutex> lock(eventMutex);
        events.emplace_back("Frame", currentFrame, getCurrentTimeMs(), false, "Frame", getCurrentThreadId());

        // Limit buffer size (keep only recent events)
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
        events.emplace_back(name, currentFrame, getCurrentTimeMs(), true, category, getCurrentThreadId());
    }

    void Profiler::recordEnd(const std::string& name, const std::string& category)
    {
        if (!enabled) return;

        std::lock_guard<std::mutex> lock(eventMutex);
        events.emplace_back(name, currentFrame, getCurrentTimeMs(), false, category, getCurrentThreadId());
    }

    std::vector<ProfileInterval> Profiler::computeIntervals(uint64_t startFrame, uint64_t endFrame) const
    {
        std::lock_guard<std::mutex> lock(eventMutex);

        std::vector<ProfileInterval> intervals;

        // Stack to handle nested scopes per thread
        std::map<uint32_t, std::stack<const ProfileEvent*>> threadStacks;

        for (const auto& event : events)
        {
            if (event.frameNumber < startFrame || event.frameNumber > endFrame)
                continue;

            if (event.isBegin)
            {
                // Push begin event
                threadStacks[event.threadId].push(&event);
            }
            else
            {
                // Pop and match with begin event
                auto& stack = threadStacks[event.threadId];
                if (!stack.empty())
                {
                    const ProfileEvent* beginEvent = stack.top();

                    // Match by name (simple approach)
                    if (beginEvent->name == event.name)
                    {
                        double duration = event.timestampMs - beginEvent->timestampMs;
                        intervals.emplace_back(
                            event.name,
                            event.frameNumber,
                            beginEvent->timestampMs,
                            duration,
                            event.category,
                            event.threadId
                        );
                        stack.pop();
                    }
                }
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
        std::cout << "Frames: " << startFrame << " to " << currentFrame << std::endl;
        std::cout << "Total intervals: " << intervals.size() << std::endl;
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
