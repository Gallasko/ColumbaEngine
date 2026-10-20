#pragma once

#include <functional>
#include <string>

#include "bugreport.h"

namespace chronicle
{
    // Sending a bug report: to the author's worker on the web, to a folder beside the save natively

    enum class SendResult { Sent, Limited, Failed, TooLarge, Written };

    // The address reports go to: --bug-url when given, else Analytics::ProxyUrl + "/bug"
    std::string bugUrl();

    // --bug-url <url> (main.cpp): "" goes back to the worker's own address
    void setBugUrl(const std::string& url);

    // Sends the report. `done(result, waitS, line)` is called once: waitS only on Limited (seconds the
    // worker asks to wait), line is a sentence for the player ("Written to <path>", the reason of a failure).
    //
    // Web: POST of bugReportJson to bugUrl(). 200 -> Sent; 429 with {wait} -> Limited; anything else
    // or a network error -> Failed; a body over MaxBody -> TooLarge; a local page (localhost) -> Failed
    // unless a --bug-url was given. `done` is called later, from the main loop (never from the promise):
    // the result waits in a table that sendReport owns, and pollReports() hands it over.
    // Native: writes `dir`/reports/<yyyy-mm-dd-hhmmss>/ (message.txt, screen.jpg, life.sz, log.txt,
    // place.txt) and calls `done` at once with Written.
    void sendReport(const BugReport& report, std::function<void(SendResult, int waitS, std::string line)> done,
                    const std::string& dir = "save/chronicle");

    // Hands the answers that came in to their `done`. Call it every frame while a send may be pending
    // (the scene's execute). Cheap when nothing waits
    void pollReports();

    // Forgets the sends in flight: their `done` is never called. The scene calls it when it is destroyed,
    // so that no answer reaches an object that is gone
    void cancelReports();

    // True while a send waits for its answer
    bool reportPending();

    // The player's words across a send: kept when it did not go, cleared when it did
    struct ReportFlow
    {
        std::string message;

        void result(SendResult r)
        {
            if (r == SendResult::Sent or r == SendResult::Written)
                message.clear();
        }
    };

    // "ab" -> bytes of a standard base64 string (padding or not); "data:...;base64," prefix is skipped
    std::string decodeBase64(const std::string& text);
}
