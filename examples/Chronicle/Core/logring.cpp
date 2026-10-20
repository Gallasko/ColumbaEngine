#include "stdafx.h"

#include "logring.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

using namespace pg;

namespace chronicle
{
    namespace
    {
        LogRing* current = nullptr;
        std::mutex currentLock;

        const char* levelName(Logger::InfoLevel level)
        {
            switch (level)
            {
                case Logger::InfoLevel::log:      return "LOG";
                case Logger::InfoLevel::test:     return "TEST";
                case Logger::InfoLevel::mile:     return "MILE";
                case Logger::InfoLevel::info:     return "INFO";
                case Logger::InfoLevel::alert:    return "ALERT";
                case Logger::InfoLevel::warning:  return "WARNING";
                case Logger::InfoLevel::error:    return "ERROR";
                case Logger::InfoLevel::critical: return "CRITICAL";
            }
            return "UNKNOWN";
        }
    }

    LogRing::LogRing()
    {
        std::lock_guard<std::mutex> guard(currentLock);
        current = this;
    }

    LogRing::~LogRing()
    {
        std::lock_guard<std::mutex> guard(currentLock);
        if (current == this)
            current = nullptr;
    }

    LogRing* LogRing::instance()
    {
        std::lock_guard<std::mutex> guard(currentLock);
        return current;
    }

    void LogRing::processLog(const Logger::Info& log)
    {
        std::string line = std::string("[") + levelName(log.level) + "] " + log.scope + ": " + log.message;

        std::lock_guard<std::mutex> guard(lock);

        lines.push_back(std::move(line));
        while (lines.size() > LogRingLines)
            lines.pop_front();
    }

    std::string LogRing::text() const
    {
        std::lock_guard<std::mutex> guard(lock);

        std::string out;
        for (const auto& l : lines)
        {
            out += l;
            out += '\n';
        }
        return out;
    }

#ifdef __EMSCRIPTEN__
    // window.chronicleConsole is filled by the HTML shell before the module starts. Read on the main
    // thread, whichever thread asks: a worker of the threads build has no window
    std::string recentLogs()
    {
        const int raw = MAIN_THREAD_EM_ASM_INT({
            var lines = (typeof window !== "undefined" && window.chronicleConsole) ? window.chronicleConsole : [];
            return stringToNewUTF8(lines.join("\n") + (lines.length ? "\n" : ""));
        });

        if (raw == 0)
            return "";

        char* text = reinterpret_cast<char*>(raw);
        std::string out = text;
        free(text);

        return out;
    }
#else
    std::string recentLogs()
    {
        auto* ring = LogRing::instance();
        return ring ? ring->text() : std::string();
    }
#endif
}

#ifdef __EMSCRIPTEN__
// A way to call recentLogs() from the page (the headless check, Module.ccall)
extern "C" EMSCRIPTEN_KEEPALIVE const char* chronicle_recent_logs()
{
    static std::string held;
    held = chronicle::recentLogs();
    return held.c_str();
}
#endif
