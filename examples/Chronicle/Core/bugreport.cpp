#include "stdafx.h"

#include "bugreport.h"

#include "analytics.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <random>
#include <sstream>

// The game's version, as its changelog heads it (CMakeLists.txt reads it from CHANGELOG.md)
#ifndef CHRONICLE_VERSION
#define CHRONICLE_VERSION "dev"
#endif

namespace chronicle
{
    std::string bugReportJson(const BugReport& r)
    {
        auto field = [](const char* key, const std::string& value, bool last = false)
        {
            return std::string("\"") + key + "\":\"" + Analytics::jsonEscape(value) + "\"" + (last ? "" : ",");
        };

        std::string out = "{";
        out += field("player_id", r.playerId);
        out += field("session_id", r.sessionId);
        out += field("version", r.version);
        out += field("message", r.message);
        out += field("screen", r.screen);
        out += field("save", r.save);
        out += field("logs", r.logs);
        out += field("place", r.place);
        out += field("client", r.client, true);
        out += "}";
        return out;
    }

    bool fits(const std::string& body)
    {
        return body.size() <= MaxBody;
    }

    std::string clipMessage(const std::string& message)
    {
        if (message.size() <= MaxMessage)
            return message;

        size_t end = MaxMessage;
        // Step back off a continuation byte (10xxxxxx) so the cut falls before a whole character
        while (end > 0 && (static_cast<unsigned char>(message[end]) & 0xC0) == 0x80)
            --end;

        return message.substr(0, end);
    }

    std::string reportMessage(const std::string& words)
    {
        if (words.find_first_not_of(" \t\r\n") == std::string::npos)
            return "(no words)";

        return clipMessage(words);
    }

    std::string gameVersion()
    {
        return CHRONICLE_VERSION;
    }

    bool ReportLimit::allowed(int64_t nowS) const
    {
        return waitSeconds(nowS) == 0;
    }

    int ReportLimit::waitSeconds(int64_t nowS) const
    {
        // The reports still inside the window
        std::vector<int64_t> live;
        for (auto t : times)
            if (nowS - t < ReportWindowS)
                live.push_back(t);

        if (static_cast<int>(live.size()) < ReportsAllowed)
            return 0;

        // The oldest of the last ReportsAllowed leaves the window first
        auto oldest = live[live.size() - ReportsAllowed];
        return static_cast<int>(oldest + ReportWindowS - nowS);
    }

    void ReportLimit::sent(int64_t nowS)
    {
        times.push_back(nowS);
        std::sort(times.begin(), times.end());

        // Keep only the last few: older ones can never count again
        if (times.size() > static_cast<size_t>(ReportsAllowed))
            times.erase(times.begin(), times.end() - ReportsAllowed);
    }

    std::string ReportLimit::text() const
    {
        std::string out;
        for (auto t : times)
            out += std::to_string(t) + "\n";
        return out;
    }

    ReportLimit ReportLimit::fromText(const std::string& text)
    {
        ReportLimit limit;
        std::istringstream in(text);
        std::string line;
        while (std::getline(in, line))
        {
            if (line.empty())
                continue;

            try
            {
                size_t used = 0;
                auto t = std::stoll(line, &used);
                if (used == line.size())
                    limit.times.push_back(t);
            }
            catch (...)
            {
            }
        }

        std::sort(limit.times.begin(), limit.times.end());
        if (limit.times.size() > static_cast<size_t>(ReportsAllowed))
            limit.times.erase(limit.times.begin(), limit.times.end() - ReportsAllowed);

        return limit;
    }

    std::string waitLine(int seconds)
    {
        int minutes = std::max(1, (seconds + 59) / 60);
        return "You may send another in " + std::to_string(minutes) + (minutes == 1 ? " minute" : " minutes");
    }

    std::string playerIdAt(const std::string& dir)
    {
        auto path = std::filesystem::path(dir) / "player.id";

        {
            std::ifstream in(path);
            std::string id;
            if (in && std::getline(in, id) && id.size() == 32
                && std::all_of(id.begin(), id.end(), [](char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'); }))
                return id;
        }

        std::random_device device;
        std::mt19937_64 gen(device());
        static const char* hex = "0123456789abcdef";
        std::string id;
        for (int i = 0; i < 32; ++i)
            id += hex[gen() & 15];

        std::error_code ec;
        std::filesystem::create_directories(path.parent_path(), ec);
        std::ofstream out(path, std::ios::trunc);
        out << id;

        return id;
    }
}
