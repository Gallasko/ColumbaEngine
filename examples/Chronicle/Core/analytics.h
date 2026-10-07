#pragma once

#include <mutex>
#include <string>

#include "ECS/system.h"

namespace chronicle
{
    // How long a session is played and where the life stood when it stopped, sent from a browser
    // to the analytics proxy (the worker that holds the database, shared with GameDevJs2026: the
    // same table, the events told apart by their "chronicle." prefix). A native build sends
    // nothing, and neither does a page served from localhost: it says in the console what it
    // would have sent.
    //
    // One row an event: the session's id (random, a new one for every page load, nothing of the
    // player), the time the page was in view in this session, the same over every session of
    // this browser, the clock, and the digest the scene last gave (LifeSave::digest).
    //
    //   chronicle.session_start   the first pass
    //   chronicle.session_end     every time the page is hidden or closed: the last one of a
    //                             session id is where the player stopped
    //   chronicle.life_end        a life is lost
    struct Analytics : public pg::System<pg::SaveSys>
    {
        // The worker holds the connection string, the page only knows its address
        static constexpr const char * const ProxyUrl = "https://analytics-proxy.pigeoncodeur.workers.dev";

        virtual std::string getSystemName() const override { return "Chronicle Analytics"; }

        virtual void execute() override;

        // What is kept between sessions: the time played before this one
        virtual void save(pg::Archive& archive) override;
        virtual void load(const pg::UnserializedObject& serializedString) override;

        // Where the life stands, kept for the next event. Called from the scene's thread
        void note(const std::string& digest);

        // One row for `event` ("life_end"), with the session's time and the last digest
        void send(const std::string& event);

        // The page is hidden or closed: the engine has just saved (its exit callback)
        void onExit();

        // The JSON array of a row's values, in the order the proxy's query names them
        static std::string row(const std::string& session, const std::string& event, size_t sessionMs, size_t totalMs, long long clockMs, const std::string& digest);

        // A string as a JSON string's content
        static std::string jsonEscape(const std::string& text);

        std::string sessionId;

        // The time played in the sessions before this one
        size_t playedBeforeMs = 0;

        // The clock of the last session_end: hiding and closing come together, one row is enough
        double lastExitMs = 0.0;

        // The scene writes the digest from the ecs, the exit callback reads it from the main thread
        std::mutex digestMutex;

        std::string digest;
    };
}
