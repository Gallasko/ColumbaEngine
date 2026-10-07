#include "loggallery.h"

#ifdef __linux__
#include <SDL2/SDL.h>
#elif _WIN32
#include <SDL.h>
#endif

#include <string>
#include <vector>

#include "ECS/entitysystem.h"
#include "ECS/entitysystem_fwd.h"
#include "Input/sdlevents.h"
#include "2D/position.h"
#include "UI/prefab.h"
#include "UI/prefabloader.h"
#include "UI/prefabbuilder.h"
#include "Systems/coresystems.h"

#include "UI/eventlog.h"
#include "UI/button.h"

using namespace pg;

namespace chronicle
{
    namespace
    {
        constexpr const char* File = "res/chronicle/ui/loggallery.yaml";

        const char* const LogNames[] = {"log", "wide"};

        constexpr float StartAge = 16.9f;

        const std::string Minus = "\xE2\x88\x92";   // U+2212

        // What the rules will hand the scene month by month: the log only draws it.
        struct Month
        {
            const char* text;
            LogKind kind;
            const char* figure;
        };

        const Month Months[] = {
            {"Mucked out the knight's stable", LogKind::Note, ""},
            {"Drilled with the sword at dawn", LogKind::Gain, "+1 swd"},
            {"A squire's stipend", LogKind::Coin, "+20"},
            {"Rain on the road to the keep", LogKind::Note, ""},
            {"Carried the knight's shield all day", LogKind::Gain, "+1 str"},
            {"Won a wager on the tilt", LogKind::Coin, "+6"},
        };

        constexpr size_t NbMonths = sizeof(Months) / sizeof(Months[0]);

        float ageAt(int months)
        {
            return StartAge + static_cast<float>(months) / 12.0f;
        }

        EventLog* logOf(EntitySystem* ecs, _unique_id id)
        {
            auto entity = ecs->getEntity(id);

            if (not entity or not entity->has<EventLog>())
                return nullptr;

            return entity->get<EventLog>().component;
        }
    }

    void LogGallery::init()
    {
        theme = ecsRef->getSystem<ThemeSystem>();

        auto caption = [&](float x, float y, const std::string& text, const std::string& color) {
            LabelSpec spec;
            spec.style = "caption";
            spec.text = text;
            spec.color = color;
            spec.z = 15;

            Label label = makeLabel(ecsRef, spec);
            auto p = label.entity->get<PositionComponent>();
            p->setX(x);
            p->setY(y);

            return label;
        };

        const float margin = theme->space(7);   // 48

        std::vector<std::string> errors;
        PrefabLoadOptions options;
        options.errors = &errors;

        auto spec = loadNodeSpec(ecsRef, File, options);

        if (not spec)
        {
            caption(margin, 68.0f, std::string("could not load ") + File + " (see the log)", "status-loss");
            return;
        }

        EntityRef page = buildTree(ecsRef, *spec);

        if (page.empty())
        {
            caption(margin, 68.0f, std::string("could not build ") + File + " (see the log)", "status-loss");
            return;
        }

        if (not errors.empty())
            caption(margin, 68.0f, std::to_string(errors.size()) + " problem(s) in the file, see the log", "status-loss");

        auto prefab = page->get<Prefab>();
        backgroundId = prefab->getEntity("page").id;

        for (const char* name : LogNames)
            logs.push_back(prefab->findEntity(name).id);

        echo = caption(940.0f, 344.0f, "ready", "ink-muted");
        endLabel = caption(940.0f, 364.0f, "", "ink-muted");

        listenToEvent<ButtonActivatedEvent>([this](const ButtonActivatedEvent& e) {
            if (e.tag == "log.month")
            {
                const Month& month = Months[static_cast<size_t>(monthsPassed) % NbMonths];

                ++monthsPassed;
                appendToAll(month.text, static_cast<int>(month.kind), month.figure);
                echo.setText(ecsRef, std::to_string(monthsPassed) + " month(s) past 16.9");
            }
            else if (e.tag == "log.milestone")
            {
                appendToAll("Knighted in the field", static_cast<int>(LogKind::Milestone), "");
                echo.setText(ecsRef, "A milestone: full weight and a gold seal");
            }
            else if (e.tag == "log.loss")
            {
                appendToAll("Thrown from his horse at the ford", static_cast<int>(LogKind::Loss), Minus + "3 vit");
                echo.setText(ecsRef, "A loss: cross, vermilion, and the minus");
            }
            else if (e.tag == "log.end")
            {
                for (auto id : logs)
                {
                    if (auto log = logOf(ecsRef, id))
                        log->scrollToEnd(ecsRef);
                }

                echo.setText(ecsRef, "Scrolled to the end: new rows are followed again");
            }
            else if (e.tag == "log.clear")
            {
                for (auto id : logs)
                {
                    if (auto log = logOf(ecsRef, id))
                        log->clear(ecsRef);
                }

                monthsPassed = 0;
                echo.setText(ecsRef, "Cleared: the next entry starts with its year");
            }
        });

        // The echo follows the column's view, however it moved (wheel, append, scroll to end).
        listenToEvent<TickEvent>([this](const TickEvent&) { showEnd(); });

        listenToEvent<OnSDLScanCode>([this](const OnSDLScanCode& event) {
            if (event.key == SDL_SCANCODE_T)
                theme->setTheme(theme->currentTheme() == "day" ? "candle" : "day");
        });

        listenToEvent<ResizeEvent>([this](const ResizeEvent& event) {
            auto entity = ecsRef->getEntity(backgroundId);

            if (entity and entity->has<PositionComponent>())
            {
                entity->get<PositionComponent>()->setWidth(event.width);
                entity->get<PositionComponent>()->setHeight(event.height);
            }
        });
    }

    void LogGallery::appendToAll(const std::string& text, int kind, const std::string& figure)
    {
        LogEntry entry;
        entry.age = ageAt(monthsPassed);
        entry.text = text;
        entry.kind = static_cast<LogKind>(kind);
        entry.figure = figure;

        for (auto id : logs)
        {
            if (auto log = logOf(ecsRef, id))
                log->append(ecsRef, entry);
        }
    }

    void LogGallery::showEnd()
    {
        auto log = logs.empty() ? nullptr : logOf(ecsRef, logs.front());

        if (not log)
            return;

        const int atEnd = log->atEnd(ecsRef) ? 1 : 0;

        if (atEnd == shownEnd)
            return;

        shownEnd = atEnd;
        endLabel.setText(ecsRef, atEnd ? "atEnd(): true - new rows are followed" : "atEnd(): false - reading further up, the view stays");
    }
}
