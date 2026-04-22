#include "analyticssystem.h"
#include "analyticsbridge.h"
#include "Files/filemanager.h"

#include <cstdio>
#include <cstdlib>
#include <sstream>

AnalyticsSystem::AnalyticsSystem()
{
}

void AnalyticsSystem::onEvent(const TickEvent& event)
{
    sessionElapsedMs += static_cast<size_t>(event.tick);
    heartbeatAccMs   += static_cast<size_t>(event.tick);
}

void AnalyticsSystem::execute()
{
    // Lazy init: register exit callback and send session_start on first execute.
    // We do this here (not in the constructor) because ecsRef/world() isn't set
    // until the system is fully registered.
    if (sessionId.empty())
    {
        char* id = js_analytics_get_session_id();
        if (id)
        {
            sessionId = id;
            free(id);
        }
        else
        {
            sessionId = "desktop";
        }

        sessionStartMs = js_analytics_now_ms();

        // Register exit callback so we fire after forceSaveNow() writes to disk.
        world()->registerOnExitCallback([this]() { sendExitAnalytics(); });

        sendToNeon("session_start", "");
        printf("[Analytics] session %s started\n", sessionId.c_str());
    }

    // Periodic heartbeat
    if (heartbeatAccMs >= HEARTBEAT_INTERVAL_MS)
    {
        heartbeatAccMs -= HEARTBEAT_INTERVAL_MS;
        sendToNeon("heartbeat", "");
    }
}

// --- SaveSys: persist cumulative play time across sessions ---

void AnalyticsSystem::save(pg::Archive& archive)
{
    size_t total = totalPlayTimeMs + sessionElapsedMs;
    serialize(archive, "totalPlayTimeMs", total);
}

void AnalyticsSystem::load(const pg::UnserializedObject& serializedString)
{
    defaultDeserialize(serializedString, "totalPlayTimeMs", totalPlayTimeMs);
    printf("[Analytics] loaded totalPlayTimeMs = %zu\n", totalPlayTimeMs);
}

// --- Exit callback: read save files and send to Neon ---

void AnalyticsSystem::sendExitAnalytics()
{
    if (exitSent)
        return;
    exitSent = true;

    // At this point forceSaveNow() has already written the save files.
    // Read the systems save file which contains all game state.
    std::string snapshot;

    auto systemsFile = pg::UniversalFileAccessor::openTextFile("save/systems.sz");
    if (!systemsFile.data.empty())
        snapshot = systemsFile.data;

    sendToNeon("session_end", snapshot);
    printf("[Analytics] session_end sent (%zu bytes save snapshot)\n", snapshot.size());
}

// --- Build and send the Neon SQL query ---

void AnalyticsSystem::sendToNeon(const std::string& eventType, const std::string& saveSnapshot)
{
#ifdef __EMSCRIPTEN__
    size_t total = totalPlayTimeMs + sessionElapsedMs;
    double nowMs = js_analytics_now_ms();

    // Build JSON params array: [$1 sessionId, $2 eventType, $3 duration, $4 totalTime, $5 timestamp, $6 saveSnapshot]
    std::ostringstream params;
    params << "[\"" << jsonEscape(sessionId) << "\","
           << "\"" << jsonEscape(eventType) << "\","
           << sessionElapsedMs << ","
           << total << ","
           << static_cast<long long>(nowMs) << ",";

    if (saveSnapshot.empty())
        params << "null";
    else
        params << "\"" << jsonEscape(saveSnapshot) << "\"";

    params << "]";

    const char* query =
        "INSERT INTO analytics_events "
        "(session_id, event_type, session_duration_ms, total_play_time_ms, timestamp_ms, save_snapshot) "
        "VALUES ($1, $2, $3, $4, $5, $6)";

    std::string paramsStr = params.str();

    js_analytics_send(NEON_HOST, NEON_CONN, query, paramsStr.c_str());
#else
    (void)eventType;
    (void)saveSnapshot;
#endif
}

// --- JSON string escaping ---

std::string AnalyticsSystem::jsonEscape(const std::string& s)
{
    std::string out;
    out.reserve(s.size() + 16);

    for (char c : s)
    {
        switch (c)
        {
        case '"':  out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\n': out += "\\n";  break;
        case '\r': out += "\\r";  break;
        case '\t': out += "\\t";  break;
        default:
            if (static_cast<unsigned char>(c) < 0x20)
            {
                // Control character — encode as \u00xx
                char buf[8];
                snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned char>(c));
                out += buf;
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
