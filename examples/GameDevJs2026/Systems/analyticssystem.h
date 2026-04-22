#pragma once

#include "ECS/system.h"
#include "Systems/coresystems.h"

#include <string>

using namespace pg;

// Sends play-session analytics (duration + save snapshot) to a Neon Postgres DB
// via the browser's fetch API.  On desktop builds this is a harmless no-op.
//
// The system registers an exit callback on EntitySystem::forceSaveNow() so that
// when the player closes/hides the tab, we read the freshly-written save files
// and POST them to Neon's SQL-over-HTTP endpoint.
struct AnalyticsSystem : public System<Listener<TickEvent>, SaveSys>
{
    // Neon endpoint and connection string — set these to your project's values.
    // The connection string should use a restricted role (INSERT-only).
    static constexpr const char* NEON_HOST = "https://ep-CHANGEME.us-east-2.aws.neon.tech";
    static constexpr const char* NEON_CONN = "postgresql://analytics_writer:CHANGEME@ep-CHANGEME.us-east-2.aws.neon.tech/neondb";

    AnalyticsSystem();

    virtual std::string getSystemName() const override { return "Analytics"; }

    virtual void onEvent(const TickEvent& event) override;
    virtual void execute() override;

    // SaveSys — persist cumulative play time across sessions
    virtual void save(Archive& archive) override;
    virtual void load(const UnserializedObject& serializedString) override;

    // Called by the exit callback registered on EntitySystem.
    // Reads save files from disk and sends them to Neon.
    void sendExitAnalytics();

private:
    void sendToNeon(const std::string& eventType, const std::string& saveSnapshot);

    // Escape a string for safe inclusion in a JSON string value.
    static std::string jsonEscape(const std::string& s);

    std::string sessionId;
    double sessionStartMs = 0.0;

    size_t sessionElapsedMs = 0;
    size_t totalPlayTimeMs  = 0;

    // Heartbeat: send a lightweight ping every 60s
    size_t heartbeatAccMs   = 0;
    static constexpr size_t HEARTBEAT_INTERVAL_MS = 60000;

    bool exitSent = false;
};
