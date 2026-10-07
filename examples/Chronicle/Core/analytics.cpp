#include "analytics.h"

#include <cstdint>
#include <cstdio>
#include <random>
#include <sstream>

#include "ECS/entitysystem.h"
#include "logger.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>

// The time the page is in view: counted by the browser's clock between the moments it is shown and
// hidden, whatever the game's own ticks did meanwhile. Both run on the main thread, the only one
// with a document, whichever thread asks. Safe to call again.
static void chronicle_analytics_begin()
{
    MAIN_THREAD_EM_ASM({
        if (Module.chronicleAnalytics)
            return;

        var state = ({ activeMs: 0, since: document.hidden ? 0 : Date.now() });

        Module.chronicleAnalytics = state;

        document.addEventListener('visibilitychange', function () {
            if (document.hidden) {
                if (state.since)
                    state.activeMs += Date.now() - state.since;

                state.since = 0;
            } else if (!state.since) {
                state.since = Date.now();
            }
        });
    });
}

static double chronicle_analytics_active_ms()
{
    return MAIN_THREAD_EM_ASM_DOUBLE({
        var state = Module.chronicleAnalytics;

        if (!state)
            return 0;

        return state.activeMs + (state.since ? Date.now() - state.since : 0);
    });
}

EM_JS(double, chronicle_analytics_now_ms, (), {
    return Date.now();
});

// One row to the proxy. keepalive lets the request outlive the page that is closing. A page served
// from this machine sends nothing: a test is not a player.
EM_JS(void, chronicle_analytics_post, (const char* proxyUrl, const char* query, const char* paramsJson), {
    var body = JSON.stringify({ query: UTF8ToString(query), params: JSON.parse(UTF8ToString(paramsJson)) });

    if (location.hostname === "localhost" || location.hostname === "127.0.0.1") {
        console.log("[analytics] not sent from a local page: " + body);
        return;
    }

    fetch(UTF8ToString(proxyUrl), {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: body,
        keepalive: true
    }).then(function (response) {
        if (!response.ok)
            console.warn("[analytics] the proxy answered " + response.status);
    }).catch(function (error) {
        console.warn("[analytics] not sent: " + error);
    });
});
#endif

using namespace pg;

namespace chronicle
{
    namespace
    {
        static constexpr const char * const DOM = "Chronicle Analytics";

        // The events of this game, in a table shared with another
        constexpr const char * const EventPrefix = "chronicle.";

#ifdef __EMSCRIPTEN__
        // Hiding a page and closing it call the exit callback one after the other
        constexpr double ExitDebounceMs = 1000.0;

        // The query the proxy runs, its values in the order of Analytics::row
        constexpr const char * const InsertQuery =
            "INSERT INTO analytics_events "
            "(session_id, event_type, session_duration_ms, total_play_time_ms, timestamp_ms, save_snapshot) "
            "VALUES ($1, $2, $3, $4, $5, $6)";
#endif

        // Random, new for every page load: it ties the rows of one session and says nothing of who plays
        std::string makeSessionId()
        {
            std::random_device device;
            std::mt19937_64 engine((static_cast<uint64_t>(device()) << 32) ^ device());

            char buffer[40];
            std::snprintf(buffer, sizeof(buffer), "%016llx%016llx", static_cast<unsigned long long>(engine()), static_cast<unsigned long long>(engine()));

            return buffer;
        }

        size_t activeSessionMs()
        {
#ifdef __EMSCRIPTEN__
            return static_cast<size_t>(chronicle_analytics_active_ms());
#else
            return 0;
#endif
        }
    }

    void Analytics::execute()
    {
        if (not sessionId.empty())
            return;

        sessionId = makeSessionId();

#ifdef __EMSCRIPTEN__
        chronicle_analytics_begin();
#endif

        // After the engine saved on a hidden or closing page: the time played is in the save by then
        ecsRef->registerOnExitCallback([this]() { onExit(); });

        send("session_start");
    }

    void Analytics::save(Archive& archive)
    {
        const size_t total = playedBeforeMs + activeSessionMs();

        serialize(archive, "totalPlayTimeMs", total);
    }

    void Analytics::load(const UnserializedObject& serializedString)
    {
        defaultDeserialize(serializedString, "totalPlayTimeMs", playedBeforeMs);
    }

    void Analytics::note(const std::string& digest)
    {
        std::lock_guard<std::mutex> lock(digestMutex);

        this->digest = digest;
    }

    void Analytics::onExit()
    {
#ifdef __EMSCRIPTEN__
        const double now = chronicle_analytics_now_ms();

        if (now - lastExitMs < ExitDebounceMs)
            return;

        lastExitMs = now;
#endif

        send("session_end");
    }

    void Analytics::send(const std::string& event)
    {
        // Before the first pass there is no session to speak of
        if (sessionId.empty())
            return;

        std::string last;

        {
            std::lock_guard<std::mutex> lock(digestMutex);

            last = digest;
        }

#ifdef __EMSCRIPTEN__
        const size_t session = activeSessionMs();
        const std::string values = row(sessionId, EventPrefix + event, session, playedBeforeMs + session, static_cast<long long>(chronicle_analytics_now_ms()), last);

        chronicle_analytics_post(ProxyUrl, InsertQuery, values.c_str());
#else
        LOG_INFO(DOM, "Not sent from a native build: " << EventPrefix << event << " " << last);
#endif
    }

    std::string Analytics::row(const std::string& session, const std::string& event, size_t sessionMs, size_t totalMs, long long clockMs, const std::string& digest)
    {
        std::ostringstream values;

        values << "[\"" << jsonEscape(session) << "\",\"" << jsonEscape(event) << "\"," << sessionMs << "," << totalMs << "," << clockMs << ",";

        if (digest.empty())
            values << "null";
        else
            values << "\"" << jsonEscape(digest) << "\"";

        values << "]";

        return values.str();
    }

    std::string Analytics::jsonEscape(const std::string& text)
    {
        std::string out;
        out.reserve(text.size() + 16);

        for (char c : text)
        {
            switch (c)
            {
            case '"':
                out += "\\\"";
                break;

            case '\\':
                out += "\\\\";
                break;

            case '\n':
                out += "\\n";
                break;

            case '\r':
                out += "\\r";
                break;

            case '\t':
                out += "\\t";
                break;

            default:
                if (static_cast<unsigned char>(c) < 0x20)
                {
                    char buffer[8];
                    std::snprintf(buffer, sizeof(buffer), "\\u%04x", static_cast<unsigned char>(c));
                    out += buffer;
                }
                else
                {
                    out += c;
                }

                break;
            }
        }

        return out;
    }
}
