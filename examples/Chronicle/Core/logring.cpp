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
    // window.chronicleConsole is filled by the HTML shell before the module starts
    EM_JS(char*, chronicle_console_text, (), {
        var lines = (typeof window !== 'undefined' && window.chronicleConsole) ? window.chronicleConsole : [];
        return stringToNewUTF8(lines.join('\n') + (lines.length ? '\n' : ''));
    });

    std::string recentLogs()
    {
        char* raw = chronicle_console_text();
        std::string out = raw ? raw : "";
        free(raw);
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
