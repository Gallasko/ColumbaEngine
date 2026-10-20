#include "lifescene.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#include "logger.h"

#include "ECS/entitysystem.h"
#include "2D/position.h"
#include "UI/prefab.h"
#include "UI/sizer.h"
#include "UI/themesystem.h"
#include "UI/ttftext.h"

#include "Core/analytics.h"
#include "Core/bugreport.h"
#include "Core/bugsend.h"
#include "Core/logring.h"
#include "Core/screenshot.h"
#include "UI/button.h"
#include "UI/gloss.h"
#include "UI/label.h"
#include "UI/wordfield.h"

using namespace pg;

// The bug report: the button in the window's corner, the dialog it opens (a page of the browser's
// own on the web, the kit's dialog natively), and the report made from the scene and sent
namespace chronicle
{
    namespace
    {
        constexpr const char * const DOM = "Chronicle.Report";

        constexpr const char * const ReportTag = "life.report";        // The corner's button
        constexpr const char * const ReportSendTag = "report.send";    // The native dialog's buttons
        constexpr const char * const ReportCancelTag = "report.cancel";
        constexpr const char * const ReportGloss = "report";           // Its gloss's key
        constexpr const char * const ReportGlyph = "bug";

        // Over the walks, the fights and the guide's shade and hand (100-129), under the glosses the
        // engine's TooltipSystem puts at 150. Hidden while a dialog is up (130-144), so never over one
        constexpr float ReportZ = 145.0f;
        constexpr float ReportPad = 12.0f;       // A kit button's side padding
        constexpr float ReportGlyphSize = 16.0f; // Its mark, S16
        constexpr float ReportTop = 6.0f;        // As the heads' big figures stand from the window's top
        constexpr float ReportCorner = 12.0f;    // From the window's right where no head stands there
        constexpr float ReportGap = 10.0f;       // Between it and the head's figure at its right
        constexpr float ReportGrabMs = 500.0f;   // The frame may take that long; then the dialog opens without it
        constexpr size_t ReportLogLines = 50;    // The life's own log sent with a report

        std::string fileText(const std::string& path)
        {
            std::ifstream file(path, std::ios::binary);

            if (not file)
                return "";

            std::stringstream text;
            text << file.rdbuf();

            return text.str();
        }

        void windowSize(EntitySystem* ecs, float& width, float& height)
        {
            width = 1320.0f;
            height = 1020.0f;

            if (auto window = ecs->getEntity("__MainWindow"); window and window->has<PositionComponent>())
            {
                auto pos = window->get<PositionComponent>();

                if (pos->width > 0.0f and pos->height > 0.0f)
                {
                    width = pos->width;
                    height = pos->height;
                }
            }
        }

        // "14.4": his age as the log writes it, the year and its month (1-12); 14.3 is in his fourth month
        std::string ageText(float age)
        {
            const int year = static_cast<int>(std::floor(age + 1e-4f));
            const int month = std::min(12, static_cast<int>(std::floor((age - static_cast<float>(year)) * 12.0f + 1e-3f)) + 1);

            return std::to_string(year) + "." + std::to_string(month);
        }

#ifdef __EMSCRIPTEN__
        // The page's dialog (web/index.html.in, window.chronicleReport), from the main thread: the only
        // one with a document, whichever thread asks

        // chronicle_report_open(thumbDataUrl, waitLine): shown with the screen as caught, the words
        // kept from the last time, the line, and Send greyed while the limit holds
        void chronicle_report_open(const std::string& screen, const std::string& words, const std::string& line, bool limited, bool candle)
        {
            MAIN_THREAD_EM_ASM({
                if (window.chronicleReport)
                    window.chronicleReport.open(UTF8ToString($0), UTF8ToString($1), UTF8ToString($2), $3 !== 0, $4 !== 0);
            }, screen.c_str(), words.c_str(), line.c_str(), limited ? 1 : 0, candle ? 1 : 0);
        }

        // chronicle_report_result(kind, line): "sent" closes it; another kind says the line in it, and
        // keeps it open with the words
        void chronicle_report_result(const char* kind, const std::string& line, bool limited)
        {
            MAIN_THREAD_EM_ASM({
                if (window.chronicleReport)
                    window.chronicleReport.result(UTF8ToString($0), UTF8ToString($1), $2 !== 0);
            }, kind, line.c_str(), limited ? 1 : 0);
        }

        void chronicle_report_close()
        {
            MAIN_THREAD_EM_ASM({
                if (window.chronicleReport)
                    window.chronicleReport.close();
            });
        }

        // What the page's dialog was pressed with since the last pass: "send\n<words>",
        // "cancel\n<words>", or "" for nothing
        std::string chronicle_report_take()
        {
            const int raw = MAIN_THREAD_EM_ASM_INT({
                var report = window.chronicleReport;

                if (!report || !report.pressed)
                    return 0;

                var pressed = report.pressed;
                report.pressed = null;

                return stringToNewUTF8(pressed);
            });

            if (raw == 0)
                return "";

            char* text = reinterpret_cast<char*>(raw);
            std::string out = text;
            free(text);

            return out;
        }

        // The browser and what it shows the game on: its user agent, the view, the screen, the pixel ratio
        std::string chronicle_report_client()
        {
            const int raw = MAIN_THREAD_EM_ASM_INT({
                var view = window.innerWidth + 'x' + window.innerHeight;
                var screenSize = (window.screen ? screen.width + 'x' + screen.height : '?');
                var line = navigator.userAgent + ' | view ' + view + ' | screen ' + screenSize + ' | dpr ' + (window.devicePixelRatio || 1)
                           + ' | touch ' + (navigator.maxTouchPoints > 0 ? 1 : 0);

                return stringToNewUTF8(line);
            });

            if (raw == 0)
                return "";

            char* text = reinterpret_cast<char*>(raw);
            std::string out = text;
            free(text);

            return out;
        }
#endif
    }

    void LifeScene::makeReportButton()
    {
        auto window = ecsRef->getEntity("__MainWindow");

        if (not window or not reportButton.root.empty())
            return;

        ButtonSpec spec;
        spec.variant = ButtonVariant::Quiet;
        spec.glyph = ReportGlyph;
        spec.tag = ReportTag;
        spec.z = static_cast<int>(ReportZ);

        reportButton = makeButton(ecsRef, spec);

        // The mark alone, as much room either side of it as a kit button keeps at its ends
        reportButton.root->get<PositionComponent>()->setWidth(2.0f * ReportPad + ReportGlyphSize);

        // It hangs on the window, as the toasts do: no page, no walk's screen is under it for long
        auto anchor = reportButton.root->get<UiAnchor>();
        anchor->setTopAnchor(PosAnchor{window->id, AnchorType::Top});
        anchor->setTopMargin(ReportTop);
        anchor->setRightAnchor(PosAnchor{window->id, AnchorType::Right});
        anchor->setRightMargin(ReportCorner);
        reportRight = ReportCorner;

        // It leaves with the scene; the next one makes its own
        ecsRef->attach<SceneElement>(reportButton.root);

        if (auto registry = ecsRef->getSystem<GlossRegistry>())
        {
            GlossSpec gloss;
            gloss.title = "Report a bug";
            gloss.text = "Tell the author what went wrong. A picture of the screen, your save and the game's recent messages go with your words.";

            registry->set(ReportGloss, gloss);
            attachGloss(ecsRef, reportButton.face, ReportGloss);
        }
    }

    float LifeScene::headFigureLeft() const
    {
        float width, height;
        windowSize(ecsRef, width, height);

        // The big figure of the head's right, on the screen in view: the walk's days or the fight's
        // turn over the page; the world's date, or his age on a page too narrow for three columns
        EntityRef figure;

        if (not walkPage.empty())
        {
            for (const char* name : {"daysLeft", "turn"})
            {
                if (EntityRef e = inWalk(name); not e.empty() and e->has<Label>())
                {
                    figure = e;
                    break;
                }
            }
        }
        else
        {
            for (const char* name : {"date", "age"})
            {
                if (EntityRef e = named(name); not e.empty() and e->has<Label>())
                {
                    figure = e;
                    break;
                }
            }
        }

        if (figure.empty() or not figure->has<PositionComponent>() or not figure->get<PositionComponent>()->isRenderable())
            return width;

        auto pos = figure->get<PositionComponent>();

        // Aligned at its right: the text begins where its width, as drawn, leaves it
        const float drawn = figure->has<TTFText>() ? figure->get<TTFText>()->textWidth : pos->width;

        return pos->x + pos->width - std::min(pos->width, std::max(0.0f, drawn));
    }

    float LifeScene::reportRoom() const
    {
        if (reportButton.root.empty())
            return 0.0f;

        EntityRef root = reportButton.root;

        return root->get<PositionComponent>()->width + ReportGap;
    }

    void LifeScene::placeReportButton()
    {
        if (reportButton.root.empty())
            return;

        // Out of the way of a dialog: the report's own, or the wipe's and the load's questions
        const bool shown = dialog.empty() and not reportOpen;
        auto pos = reportButton.root->get<PositionComponent>();

        if (pos->visible != shown)
            pos->setVisibility(shown);

        // Beside the head's figure, never over it: as far from the window's right as the figure
        // is drawn, and in the very corner where no head stands there
        float width, height;
        windowSize(ecsRef, width, height);

        const float right = std::max(ReportCorner, width - headFigureLeft() + ReportGap);

        if (std::abs(right - reportRight) > 0.5f)
        {
            reportButton.root->get<UiAnchor>()->setRightMargin(right);
            reportRight = right;
        }
    }

    bool LifeScene::onReportButton(const std::string& tag)
    {
        if (tag == ReportTag)
        {
            openReport();
            return true;
        }

        if (tag == ReportCancelTag)
        {
            if (reportOpen)
                closeReport();

            return true;
        }

        if (tag == ReportSendTag)
        {
            if (dialogKind == "report" and not dialog.empty())
            {
                EntityRef field = dialog->get<Prefab>()->findEntity("reportWords");

                if (not field.empty() and field->has<WordField>())
                    sendFromReport(field->get<WordField>()->text(ecsRef));
            }

            return true;
        }

        return false;
    }

    void LifeScene::openReport()
    {
        // Once at a time, never over another question, never while the scene goes
        if (reportOpen or reportAsked or not dialog.empty() or leaving)
            return;

        reportAsked = true;
        reportScreen.clear();
        reportLine.clear();

        // The screen first, as he sees it: the dialog comes with the frame
        if (auto grab = ecsRef->getSystem<FrameGrab>())
        {
            reportGrabLeft = ReportGrabMs;

            grab->request([this](std::string dataUrl) {
                if (not reportAsked or reportOpen)
                    return;

                reportScreen = std::move(dataUrl);
                showReport();
            });

            return;
        }

        showReport();
    }

    void LifeScene::runReport(float ms)
    {
        if (not reportAsked or reportOpen)
            return;

        reportGrabLeft -= ms;

        // No frame in time: the dialog opens without its picture, and a frame that comes late is no one's
        if (reportGrabLeft <= 0.0f)
        {
            if (auto grab = ecsRef->getSystem<FrameGrab>())
                grab->cancel();

            reportScreen.clear();
            showReport();
        }
    }

    void LifeScene::showReport()
    {
        reportAsked = false;

        // Another question came up meanwhile: it has the page
        if (not dialog.empty() or leaving)
            return;

        const auto now = static_cast<int64_t>(wallClock());
        const ReportLimit limit = reportLimit();
        const bool limited = not limit.allowed(now);

        reportLine = limited ? waitLine(limit.waitSeconds(now)) : "";
        reportSending = false;

#ifdef __EMSCRIPTEN__
        reportOpen = true;

        auto theme = ecsRef->getSystem<ThemeSystem>();
        chronicle_report_open(reportScreen, reportFlow.message, reportLine, limited, theme and theme->currentTheme() == "candle");
#else
        if (not openDialog("report"))
            return;

        reportOpen = true;

        // His words of the last time, still there
        if (EntityRef field = dialog->get<Prefab>()->findEntity("reportWords"); not field.empty() and field->has<WordField>())
            field->get<WordField>()->setText(ecsRef, reportFlow.message);

        fillReport();
#endif

        placeReportButton();
    }

    void LifeScene::fillReport()
    {
        if (dialogKind != "report" or dialog.empty())
            return;

        auto prefab = dialog->get<Prefab>();

        if (EntityRef line = prefab->findEntity("reportLine"); not line.empty() and line->has<Label>())
        {
            const std::string said = reportLine.empty() ? " " : reportLine;

            if (line->get<Label>()->spec.text != said)
                line->get<Label>()->setText(ecsRef, said);
        }

        const auto now = static_cast<int64_t>(wallClock());
        const bool greyed = reportSending or not reportLimit().allowed(now);

        if (EntityRef send = prefab->findEntity("reportSend"); not send.empty() and send->has<Button>())
        {
            auto button = send->get<Button>();

            if (button->face->get<ButtonState>()->disabled != greyed)
                button->setDisabled(ecsRef, greyed);
        }
    }

    void LifeScene::closeReport()
    {
        reportAsked = false;
        reportSending = false;

        if (dialogKind == "report")
            closeDialog();

        reportOpen = false;

#ifdef __EMSCRIPTEN__
        chronicle_report_close();
#endif

        placeReportButton();
    }

    void LifeScene::sendFromReport(const std::string& message)
    {
        if (not reportOpen or reportSending)
            return;

        // His words are kept until they have gone
        reportFlow.message = message;

        const auto now = static_cast<int64_t>(wallClock());
        const ReportLimit limit = reportLimit();

        if (not limit.allowed(now))
        {
            reportAnswered(SendResult::Limited, limit.waitSeconds(now), "");
            return;
        }

        BugReport report;
        report.playerId = playerIdAt(reportDir());

        if (auto analytics = ecsRef->getSystem<Analytics>())
            report.sessionId = analytics->sessionId;

        report.version = gameVersion();
        report.message = clipMessage(message);
        report.screen = reportScreen;

        // The save as it was last written: the report never writes it, so it cannot take a step half made
        report.save = opt.noSave ? std::string() : fileText(opt.savePath);
        report.logs = recentLogs() + "\n--- life ---\n" + lifeLogTail(ReportLogLines);
        report.place = reportPlace();

#ifdef __EMSCRIPTEN__
        report.client = chronicle_report_client();
#else
        report.client = "native " + std::to_string(static_cast<int>(windowWidth)) + "x" + std::to_string(static_cast<int>(windowHeight));
#endif

        reportSending = true;
        reportLine.clear();
        fillReport();

        // Natively the answer comes at once, inside the call; on the web from pollReports, a pass later
        sendReport(report, [this](SendResult result, int waitS, std::string line) { reportAnswered(result, waitS, line); }, reportDir());
    }

    void LifeScene::reportAnswered(SendResult result, int waitS, const std::string& line)
    {
        reportSending = false;
        reportFlow.result(result);

        // Counted where it went: the limit is of reports sent, not of tries
        if (result == SendResult::Sent or result == SendResult::Written)
        {
            ReportLimit limit = reportLimit();
            limit.sent(static_cast<int64_t>(wallClock()));

            std::error_code ec;
            std::filesystem::create_directories(reportDir(), ec);

            std::ofstream file(std::filesystem::path(reportDir()) / "reports.limit", std::ios::trunc);
            file << limit.text();

            if (not file)
                LOG_ERROR(DOM, "Could not write the reports' limit to " << reportDir());
        }

        switch (result)
        {
        case SendResult::Sent:
            reportLine.clear();
#ifdef __EMSCRIPTEN__
            chronicle_report_result("sent", "", false);
#endif
            closeReport();
            toast("Sent. Thank you.");
            break;

        case SendResult::Written:
            reportLine.clear();
            closeReport();
            toast(line.empty() ? "Written beside the save." : line);
            break;

        case SendResult::Limited:
            reportLine = waitLine(waitS);
#ifdef __EMSCRIPTEN__
            chronicle_report_result("limited", reportLine, true);
#endif
            fillReport();
            break;

        case SendResult::Failed:
        case SendResult::TooLarge:
        default:
            reportLine = (line.empty() ? std::string("The report could not be sent") : line) + ". Your words are kept.";
#ifdef __EMSCRIPTEN__
            chronicle_report_result("failed", reportLine, false);
#endif
            fillReport();
            break;
        }
    }

    void LifeScene::pollReport()
    {
        // The sends' answers, on this thread
        pollReports();

#ifdef __EMSCRIPTEN__
        if (not reportOpen)
            return;

        const std::string pressed = chronicle_report_take();

        if (pressed.empty())
            return;

        const auto cut = pressed.find('\n');
        const std::string what = pressed.substr(0, cut);
        const std::string words = cut == std::string::npos ? std::string() : pressed.substr(cut + 1);

        if (what == "send")
            sendFromReport(words);
        else if (what == "cancel")
        {
            reportFlow.message = words;
            closeReport();
        }
#endif
    }

    void LifeScene::dropReport()
    {
        // Nothing asked for answers into a scene that is gone: no send, no frame
        cancelReports();

        if (reportAsked)
        {
            if (auto grab = ecsRef->getSystem<FrameGrab>())
                grab->cancel();
        }

#ifdef __EMSCRIPTEN__
        if (reportOpen)
            chronicle_report_close();
#endif

        reportOpen = false;
        reportAsked = false;
        reportSending = false;

        // It went with the scene, as a scene element
        reportButton = Button{};
        reportRight = -1.0f;
    }

    std::string LifeScene::reportDir() const
    {
        const auto parent = std::filesystem::path(opt.savePath).parent_path();

        return parent.empty() ? std::string(".") : parent.string();
    }

    ReportLimit LifeScene::reportLimit() const
    {
        return ReportLimit::fromText(fileText((std::filesystem::path(reportDir()) / "reports.limit").string()));
    }

    std::string LifeScene::lifeLogTail(size_t lines) const
    {
        const size_t from = save.log.size() > lines ? save.log.size() - lines : 0;
        std::string out;

        for (size_t i = from; i < save.log.size(); ++i)
        {
            const LogEntry& entry = save.log[i];

            out += ageText(entry.age) + "  " + entry.text;

            if (not entry.figure.empty())
                out += "  (" + entry.figure + ")";

            out += '\n';
        }

        return out;
    }

    std::string LifeScene::reportPlace() const
    {
        // page=life screen=fight age=12.4 running=glean zone=hollow lives=2
        const std::string pageName = optionsShown ? "options" : adventureShown ? "adventure" : townShown ? "town" : "life";

        std::string screen = "page";

        if (ended)
            screen = "ending";
        else if (save.walk.story())
            screen = "story";
        else if (not walkScreen.empty())
            screen = walkScreen;

        std::string place = "page=" + pageName + " screen=" + screen + " age=" + ageText(save.age) + " running=" + (save.running.empty() ? std::string("-") : save.running);

        if (auto zone = save.walk.run.find("zone"); zone != save.walk.run.end() and not zone->second.toString().empty())
            place += " zone=" + zone->second.toString();

        place += " lives=" + std::to_string(save.lives);
        place += compact ? (phone ? " layout=phone" : " layout=compact") : " layout=full";

        return place;
    }
}
