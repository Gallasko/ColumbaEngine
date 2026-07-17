#pragma once

#include "ECS/system.h"
#include "Input/inputcomponent.h"
#include "Systems/coresystems.h"

#include <string>

// Sends play-session analytics (duration + save snapshot) to a Neon Postgres DB
// via the browser's fetch API.  On desktop builds this is a harmless no-op.
//
// The system registers an exit callback on EntitySystem::forceSaveNow() so that
// when the player closes/hides the tab, we read the freshly-written save files
// and POST them to Neon's SQL-over-HTTP endpoint.
struct AnalyticsSystem : public pg::System<pg::Listener<pg::TickEvent>, pg::Listener<pg::OnSDLScanCode>, pg::SaveSys>
{
    // Analytics proxy worker URL — the worker holds the Neon connection string.
    // Deploy the worker in analytics-worker/ and replace this URL.
    static constexpr const char* PROXY_URL = "https://analytics-proxy.pigeoncodeur.workers.dev";
    // Kept for API compat — unused, the worker holds the real connection string.
    static constexpr const char* NEON_CONN = "";

    AnalyticsSystem();

    virtual std::string getSystemName() const override { return "Analytics"; }

    virtual void onEvent(const pg::TickEvent& event) override;
    virtual void onEvent(const pg::OnSDLScanCode& event) override;
    virtual void execute() override;

    // SaveSys — persist cumulative play time across sessions
    virtual void save(pg::Archive& archive) override;
    virtual void load(const pg::UnserializedObject& serializedString) override;

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

    bool exitSent = false;
};
