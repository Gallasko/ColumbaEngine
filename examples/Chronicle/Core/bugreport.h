#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace chronicle
{
    // The in-game bug report: its fields, its JSON body, its caps, and the client-side limit

    constexpr size_t MaxMessage = 2000;
    constexpr size_t MaxBody = 2 * 1024 * 1024;
    constexpr int ReportsAllowed = 3;
    constexpr int ReportWindowS = 900;

    struct BugReport
    {
        std::string playerId, sessionId, version, message, screen, save, logs, place, client;
    };

    // One JSON object: player_id, session_id, version, message, screen (a data:image/jpeg;base64, string),
    // save, logs, place, client; every string escaped
    std::string bugReportJson(const BugReport& report);

    // True when the body is within MaxBody
    bool fits(const std::string& body);

    // The message cut to MaxMessage bytes at most, never inside a UTF-8 character
    std::string clipMessage(const std::string& message);

    // The game's version (CHRONICLE_VERSION, "dev" when the build did not give one)
    std::string gameVersion();

    // The times (seconds) of the last reports, kept as text
    class ReportLimit
    {
    public:
        bool allowed(int64_t nowS) const;
        int waitSeconds(int64_t nowS) const;
        void sent(int64_t nowS);

        std::string text() const;
        static ReportLimit fromText(const std::string& text);

    private:
        // Only the times still inside the window of the newest are of use; kept oldest first
        std::vector<int64_t> times;
    };

    // "You may send another in N minutes" (rounded up; "1 minute")
    std::string waitLine(int seconds);

    // Reads dir/player.id, or makes a random 32-hex id, writes it and returns it
    std::string playerIdAt(const std::string& dir);
}
