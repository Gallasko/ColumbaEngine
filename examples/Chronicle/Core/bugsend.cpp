#include "bugsend.h"

#include <chrono>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <map>
#include <mutex>
#include <sstream>

#include "analytics.h"
#include "logger.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

namespace chronicle
{
    namespace
    {
        static constexpr const char * const DOM = "Chronicle Bug Send";

        std::string& bugUrlOverride()
        {
            static std::string url;
            return url;
        }

        using Done = std::function<void(SendResult, int, std::string)>;

        struct Answer { SendResult result; int wait; std::string line; };

        // The sends in flight, and the answers that came back and wait for the main loop
        std::mutex tableMutex;
        std::map<int, Done> flying;
        std::map<int, Answer> arrived;
        [[maybe_unused]] int nextId = 1;

        [[maybe_unused]] void arrive(int id, SendResult result, int wait, const std::string& line)
        {
            std::lock_guard<std::mutex> lock(tableMutex);

            if (flying.count(id))
                arrived[id] = Answer{ result, wait, line };
        }
    }

    std::string decodeBase64(const std::string& text)
    {
        size_t from = 0;
        const auto marker = text.find("base64,");
        if (text.rfind("data:", 0) == 0 and marker != std::string::npos)
            from = marker + 7;

        std::string out;
        unsigned buffer = 0;
        int bits = 0;

        for (size_t i = from; i < text.size(); ++i)
        {
            const char c = text[i];
            int v;
            if (c >= 'A' and c <= 'Z') v = c - 'A';
            else if (c >= 'a' and c <= 'z') v = c - 'a' + 26;
            else if (c >= '0' and c <= '9') v = c - '0' + 52;
            else if (c == '+' or c == '-') v = 62;
            else if (c == '/' or c == '_') v = 63;
            else continue;

            buffer = (buffer << 6) | static_cast<unsigned>(v);
            bits += 6;

            if (bits >= 8)
            {
                bits -= 8;
                out.push_back(static_cast<char>((buffer >> bits) & 0xFF));
            }
        }

        return out;
    }

    std::string bugUrl()
    {
        if (not bugUrlOverride().empty())
            return bugUrlOverride();

        return Analytics::BugUrl;
    }

    void setBugUrl(const std::string& url) { bugUrlOverride() = url; }

#ifdef __EMSCRIPTEN__
    // The answer comes back from the promise into this C function, which only files it in the table;
    // the `done` runs from pollReports, on the scene's own thread. A cancelled id is ignored.
    extern "C" EMSCRIPTEN_KEEPALIVE void chronicle_bug_answer(int id, int status, int wait)
    {
        if (status == 200)
            arrive(id, SendResult::Sent, 0, "");
        else if (status == 429)
            arrive(id, SendResult::Limited, wait, "");
        else if (status == 413)
            arrive(id, SendResult::TooLarge, 0, "The report is too large to send");
        else
            arrive(id, SendResult::Failed, 0, status > 0 ? "The report could not be sent (" + std::to_string(status) + ")" : "The report could not be sent");
    }

    // A page served from this machine sends nothing, unless a --bug-url was given (`forced`)
    EM_JS(void, chronicle_bug_post, (int id, const char* url, const char* body, int forced), {
        var local = location.hostname === "localhost" || location.hostname === "127.0.0.1";

        function give(status, wait) {
            Module._chronicle_bug_answer(id, status, wait);
        }

        if (local && !forced) {
            console.log("[bug] not sent from a local page");
            give(-1, 0);
            return;
        }

        fetch(UTF8ToString(url), {
            method: "POST",
            headers: { "Content-Type": "application/json" },
            body: UTF8ToString(body)
        }).then(function (response) {
            if (response.status === 429) {
                return response.json().catch(function () { return {}; }).then(function (j) {
                    give(429, Math.max(0, Math.ceil(Number(j.wait) || 0)));
                });
            }
            give(response.status, 0);
        }).catch(function (error) {
            console.warn("[bug] not sent: " + error);
            give(0, 0);
        });
    });
#endif

    void sendReport(const BugReport& report, std::function<void(SendResult, int, std::string)> done, const std::string& dir)
    {
        const std::string body = bugReportJson(report);

        if (not fits(body))
        {
            done(SendResult::TooLarge, 0, "The report is too large to send");
            return;
        }

#ifdef __EMSCRIPTEN__
        (void)dir;

        const bool local = bugUrlOverride().empty();
        int id;
        {
            std::lock_guard<std::mutex> lock(tableMutex);
            id = nextId++;
            flying[id] = std::move(done);
        }

        // A local page without a --bug-url says so at once
        const bool isLocal = MAIN_THREAD_EM_ASM_INT({
            return (location.hostname === "localhost" || location.hostname === "127.0.0.1") ? 1 : 0;
        });

        if (isLocal and local)
        {
            arrive(id, SendResult::Failed, 0, "Reports cannot be sent from a local page");
            return;
        }

        chronicle_bug_post(id, bugUrl().c_str(), body.c_str(), local ? 0 : 1);
#else
        namespace fs = std::filesystem;

        const std::time_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        std::tm parts{};
        localtime_r(&now, &parts);

        char stamp[32];
        std::strftime(stamp, sizeof(stamp), "%Y-%m-%d-%H%M%S", &parts);

        std::error_code ec;
        fs::path folder = fs::path(dir) / "reports" / stamp;

        for (int n = 2; fs::exists(folder, ec); ++n)
            folder = fs::path(dir) / "reports" / (std::string(stamp) + "-" + std::to_string(n));

        fs::create_directories(folder, ec);

        auto write = [&](const char* name, const std::string& content) {
            std::ofstream file(folder / name, std::ios::binary);
            file << content;
            return static_cast<bool>(file);
        };

        bool ok = not ec;
        ok = ok and write("message.txt", report.message);
        ok = ok and write("screen.jpg", decodeBase64(report.screen));
        ok = ok and write("life.sz", report.save);
        ok = ok and write("log.txt", report.logs);
        ok = ok and write("place.txt", report.place);

        if (not ok)
        {
            LOG_ERROR(DOM, "Could not write the report to " << folder.string());
            done(SendResult::Failed, 0, "The report could not be written to " + folder.string());
            return;
        }

        done(SendResult::Written, 0, "Written to " + folder.string());
#endif
    }

    void pollReports()
    {
        std::vector<std::pair<Done, Answer>> ready;
        {
            std::lock_guard<std::mutex> lock(tableMutex);

            for (auto& [id, answer] : arrived)
            {
                auto it = flying.find(id);
                if (it != flying.end())
                {
                    ready.emplace_back(std::move(it->second), answer);
                    flying.erase(it);
                }
            }

            arrived.clear();
        }

        for (auto& [done, answer] : ready)
            if (done)
                done(answer.result, answer.wait, answer.line);
    }

    void cancelReports()
    {
        std::lock_guard<std::mutex> lock(tableMutex);
        flying.clear();
        arrived.clear();
    }

    bool reportPending()
    {
        std::lock_guard<std::mutex> lock(tableMutex);
        return not flying.empty();
    }
}
