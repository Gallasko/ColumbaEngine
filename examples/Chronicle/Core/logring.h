#pragma once

#include <deque>
#include <mutex>
#include <string>

#include "logger.h"

namespace chronicle
{
    // The last messages the game wrote, kept for a bug report

    constexpr size_t LogRingLines = 300;

    // A sink that keeps the last LogRingLines lines, "[LEVEL] scope: message". The Logger does not
    // lock around its sinks and systems log from worker threads, so the ring locks itself.
    class LogRing : public pg::Logger::LogSink
    {
    public:
        LogRing();
        ~LogRing() override;

        // The ring registered last and still alive, or nullptr
        static LogRing* instance();

        // The lines, oldest first, one per line
        std::string text() const;

    private:
        void processLog(const pg::Logger::Info& log) override;

        mutable std::mutex lock;
        std::deque<std::string> lines;
    };

    // The recent messages: the browser console's ring on the web, the LogRing natively ("" with none)
    std::string recentLogs();
}
