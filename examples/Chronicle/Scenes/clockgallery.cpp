#include "clockgallery.h"

#ifdef __EMSCRIPTEN__
#include <SDL2/SDL.h>
#elif __linux__
#include <SDL2/SDL.h>
#elif _WIN32
#include <SDL.h>
#endif

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "ECS/entitysystem.h"
#include "ECS/entitysystem_fwd.h"
#include "Input/sdlevents.h"
#include "2D/position.h"
#include "UI/prefab.h"
#include "UI/prefabloader.h"
#include "UI/prefabbuilder.h"
#include "Systems/gamefacts.h"

#include "UI/lifeclock.h"
#include "UI/button.h"
#include "Core/motion.h"

using namespace pg;

namespace chronicle
{
    namespace
    {
        constexpr const char* File = "res/chronicle/ui/clockgallery.yaml";

        constexpr const char* AgePath = "life.age";
        constexpr const char* RunningPath = "activity.running.months";
        constexpr const char* NextLabelPath = "life.next.label";
        constexpr const char* NextInPath = "life.next.in";

        constexpr float StartAge = 17.4f;
        constexpr int StartRunning = 9;

        // What milestones.pg will say; the scene owns it, the clock only draws two strings.
        struct Milestone
        {
            float age;
            const char* label;
        };

        const Milestone Milestones[] = {
            {14.0f, "Apprenticeship"},
            {18.0f, "Choose a path"},
            {25.0f, "Journeyman"},
            {40.0f, "Elder"},
            {43.0f, "The end"},
        };

        constexpr size_t NbMilestones = sizeof(Milestones) / sizeof(Milestones[0]);

        // From the months lived, so a year of advances lands on a whole age, not on a float sum.
        float ageAt(int monthsLived)
        {
            return StartAge + static_cast<float>(monthsLived) / 12.0f;
        }

        LifeClock* lifeClock(EntitySystem* ecs, _unique_id id)
        {
            auto entity = ecs->getEntity(id);

            if (not entity or not entity->has<LifeClock>())
                return nullptr;

            return entity->get<LifeClock>().component;
        }
    }

    void ClockGallery::init()
    {
        theme = ecsRef->getSystem<ThemeSystem>();

        auto place = [](EntityRef e, float x, float y) {
            auto p = e->get<PositionComponent>();
            p->setX(x);
            p->setY(y);
        };

        auto caption = [&](float x, float y, const std::string& text, const std::string& color) {
            LabelSpec spec;
            spec.style = "caption";
            spec.text = text;
            spec.color = color;
            spec.z = 15;

            Label label = makeLabel(ecsRef, spec);
            place(label.entity, x, y);

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
        lifeId = prefab->getEntity("life").id;
        choirButtonId = prefab->getEntity("choir").id;

        if (auto clock = lifeClock(ecsRef, lifeId))
        {
            for (size_t i = 0; i < clock->windows.size(); ++i)
            {
                if (clock->windows[i].spec.label == "Choir")
                    choirIndex = i;
            }
        }

        echo = caption(760.0f, 300.0f, "ready", "ink-muted");
        motionLabel = caption(760.0f, 320.0f, Motion::reduced() ? "motion: reduced" : "motion: full", "ink-muted");

        // Seed the facts, then listen: the buttons write facts, the clock follows them.
        auto facts = ecsRef->getSystem<WorldFacts>();

        if (facts)
        {
            facts->setFact(AgePath, StartAge);
            facts->setFact(RunningPath, static_cast<float>(StartRunning));
            facts->setFact(NextLabelPath, std::string("Choose a path"));
            facts->setFact(NextInPath, 7);
        }

        listenToEvent<WorldFactsUpdate>([this](const WorldFactsUpdate& event) {
            auto clock = lifeClock(ecsRef, lifeId);

            if (not clock)
                return;

            for (const auto& name : event.changedFacts)
            {
                const auto& it = event.factMap->find(name);

                if (it == event.factMap->end())
                    continue;

                if (name == AgePath)
                    clock->setAge(ecsRef, it->second.get<float>());
                else if (name == RunningPath)
                    clock->setRunning(ecsRef, it->second.get<float>());
                else if (name == NextLabelPath)
                    clock->setNext(ecsRef, it->second.get<std::string>(), clock->spec.nextIn);
                else if (name == NextInPath)
                    clock->setNext(ecsRef, clock->spec.nextLabel, it->second.get<int>());
            }
        });

        updateNext(true);

        listenToEvent<ButtonActivatedEvent>([this](const ButtonActivatedEvent& e) {
            auto facts = ecsRef->getSystem<WorldFacts>();

            if (not facts)
                return;

            if (e.tag == "clock.advance")
            {
                advanceMonth();
            }
            else if (e.tag == "clock.season")
            {
                runningMonths = 6;
                facts->setFact(RunningPath, static_cast<float>(runningMonths));
                echo.setText(ecsRef, "A season starts: 6 months hatched ahead of the fill");
            }
            else if (e.tag == "clock.choir")
            {
                setChoir(not choirClosed);
                echo.setText(ecsRef, choirClosed ? "The Choir window is closed: ruled edges, the wash stays" : "The Choir window is open again");
            }
            else if (e.tag == "clock.reset")
            {
                monthsLived = 0;
                runningMonths = StartRunning;
                facts->setFact(AgePath, ageAt(monthsLived));
                facts->setFact(RunningPath, static_cast<float>(runningMonths));
                updateNext(true);
                setChoir(true);
                echo.setText(ecsRef, "Reset to 17.4, 9 months running");
            }
        });

        listenToEvent<OnSDLScanCode>([this](const OnSDLScanCode& event) {
            if (event.key == SDL_SCANCODE_T)
            {
                theme->setTheme(theme->currentTheme() == "day" ? "candle" : "day");
            }
            else if (event.key == SDL_SCANCODE_R)
            {
                Motion::setReduced(not Motion::reduced());
                motionLabel.setText(ecsRef, Motion::reduced() ? "motion: reduced" : "motion: full");
            }
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

    void ClockGallery::advanceMonth()
    {
        auto facts = ecsRef->getSystem<WorldFacts>();

        if (not facts)
            return;

        ++monthsLived;

        const bool wasRunning = runningMonths > 0;

        if (wasRunning)
            --runningMonths;

        facts->setFact(AgePath, ageAt(monthsLived));
        facts->setFact(RunningPath, static_cast<float>(runningMonths));

        // The activity is done: the game recomputes the next milestone, and so does the scene.
        const bool finished = wasRunning and runningMonths == 0;
        updateNext(finished);

        std::string text = std::to_string(monthsLived) + " month(s) past 17.4";

        if (finished)
            text += ": the activity is done, the fill ends where the hatch ended";
        else if (runningMonths > 0)
            text += ", " + std::to_string(runningMonths) + " running";

        echo.setText(ecsRef, text);
    }

    void ClockGallery::updateNext(bool relabel)
    {
        auto facts = ecsRef->getSystem<WorldFacts>();

        if (not facts)
            return;

        const float age = ageAt(monthsLived);

        if (relabel)
        {
            nextIndex = NbMilestones;

            for (size_t i = 0; i < NbMilestones; ++i)
            {
                if (Milestones[i].age > age)
                {
                    nextIndex = i;
                    break;
                }
            }

            facts->setFact(NextLabelPath, std::string(nextIndex < NbMilestones ? Milestones[nextIndex].label : ""));
        }

        if (nextIndex >= NbMilestones)
        {
            facts->setFact(NextInPath, -1);
            return;
        }

        const int months = static_cast<int>(std::round((Milestones[nextIndex].age - age) * 12.0f));

        facts->setFact(NextInPath, std::max(0, months));
    }

    void ClockGallery::setChoir(bool closed)
    {
        choirClosed = closed;

        if (auto clock = lifeClock(ecsRef, lifeId))
            clock->setWindowClosed(ecsRef, choirIndex, closed);

        auto button = ecsRef->getEntity(choirButtonId);

        if (button and button->has<Prefab>())
            button->get<Prefab>()->callHelper("setLabel", std::string(closed ? "Open the Choir window" : "Close the Choir window"));
    }
}
