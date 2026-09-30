#include "windowgallery.h"

#ifdef __linux__
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

#include "UI/windowmeter.h"
#include "UI/button.h"
#include "Core/motion.h"

using namespace pg;

namespace chronicle
{
    namespace
    {
        constexpr const char* File = "res/chronicle/ui/windowgallery.yaml";

        constexpr const char* AgePath = "life.age";
        constexpr const char* StatePath = "window.squire.state";
        constexpr const char* NotePath = "window.squire.note";

        constexpr float StartAge = 20.2f;
        constexpr float SquireCloses = 22.0f;
        constexpr int SquireMonths = 12;       // One attempt at the Squire's trial

        const char* const MeterNames[] = {"squire", "academy", "choir", "squireWide", "academyWide", "choirWide", "academyLong"};
        const char* const SquireNames[] = {"squire", "squireWide"};

        float ageAt(int monthsLived)
        {
            return StartAge + static_cast<float>(monthsLived) / 12.0f;
        }

        std::string counted(int n, bool capital)
        {
            static const char* const Words[] = {"no", "one", "two", "three", "four", "five", "six"};

            std::string word = n >= 0 and n < 7 ? Words[n] : std::to_string(n);

            if (capital and not word.empty() and word[0] >= 'a' and word[0] <= 'z')
                word[0] = static_cast<char>(word[0] - 'a' + 'A');

            return word;
        }

        // What windows.pg will say: the months left, then the attempts, never a percentage.
        std::string squireNote(float age)
        {
            const int months = std::max(0, static_cast<int>(std::round((SquireCloses - age) * 12.0f)));
            const int fits = months / SquireMonths;

            std::string note = "Open " + std::to_string(months) + (months == 1 ? " more month. " : " more months. ");

            if (fits == 0)
                return note + "No attempt fits.";

            note += counted(fits, true) + (fits == 1 ? " attempt fits; " : " attempts fit; ");

            return note + counted(fits + 1, false) + " do not.";
        }

        WindowMeter* meterOf(EntitySystem* ecs, _unique_id id)
        {
            auto entity = ecs->getEntity(id);

            if (not entity or not entity->has<WindowMeter>())
                return nullptr;

            return entity->get<WindowMeter>().component;
        }

        WindowState stateFrom(const std::string& text)
        {
            if (text == "closed")
                return WindowState::Closed;

            if (text == "upcoming")
                return WindowState::Upcoming;

            return WindowState::Open;
        }
    }

    void WindowGallery::init()
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

        for (const char* name : MeterNames)
            meters.push_back(prefab->findEntity(name).id);

        for (const char* name : SquireNames)
            squires.push_back(prefab->findEntity(name).id);

        echo = caption(960.0f, 300.0f, "ready", "ink-muted");
        motionLabel = caption(960.0f, 320.0f, Motion::reduced() ? "motion: reduced" : "motion: full", "ink-muted");

        // Listen, then seed the facts: the buttons write facts, the meters follow them.
        listenToEvent<WorldFactsUpdate>([this](const WorldFactsUpdate& event) {
            for (const auto& name : event.changedFacts)
            {
                const auto& it = event.factMap->find(name);

                if (it == event.factMap->end())
                    continue;

                if (name == AgePath)
                {
                    for (auto id : meters)
                    {
                        if (auto meter = meterOf(ecsRef, id))
                            meter->setAge(ecsRef, it->second.get<float>());
                    }
                }
                else if (name == StatePath)
                {
                    for (auto id : squires)
                    {
                        if (auto meter = meterOf(ecsRef, id))
                            meter->setState(ecsRef, stateFrom(it->second.get<std::string>()));
                    }
                }
                else if (name == NotePath)
                {
                    for (auto id : squires)
                    {
                        if (auto meter = meterOf(ecsRef, id))
                            meter->setNote(ecsRef, it->second.get<std::string>());
                    }
                }
            }
        });

        writeSquire();

        listenToEvent<ButtonActivatedEvent>([this](const ButtonActivatedEvent& e) {
            auto facts = ecsRef->getSystem<WorldFacts>();

            if (not facts)
                return;

            if (e.tag == "window.age")
            {
                monthsLived += 6;
                facts->setFact(AgePath, ageAt(monthsLived));
                writeSquire();

                const int tenths = static_cast<int>(std::round(ageAt(monthsLived) * 10.0f));
                echo.setText(ecsRef, "Age " + std::to_string(tenths / 10) + "." + std::to_string(tenths % 10) + ": every fill moves, no note moves");
            }
            else if (e.tag == "window.close")
            {
                squireClosed = true;
                writeSquire();
                echo.setText(ecsRef, "The Squire door is closed: state-locked, a cross, still on the page");
            }
            else if (e.tag == "window.reopen")
            {
                squireClosed = false;
                writeSquire();
                echo.setText(ecsRef, "The Squire door is open again");
            }
            else if (e.tag == "window.reset")
            {
                monthsLived = 0;
                squireClosed = false;
                facts->setFact(AgePath, ageAt(monthsLived));
                writeSquire();
                echo.setText(ecsRef, "Reset to 20.2, the Squire door open");
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

    // The scene decides the Squire door, the way windows.pg will: shut by hand, or by the age.
    void WindowGallery::writeSquire()
    {
        auto facts = ecsRef->getSystem<WorldFacts>();

        if (not facts)
            return;

        const float age = ageAt(monthsLived);

        if (squireClosed)
        {
            facts->setFact(StatePath, std::string("closed"));
            facts->setFact(NotePath, std::string("The Squire door is shut."));
        }
        else if (age >= SquireCloses)
        {
            facts->setFact(StatePath, std::string("closed"));
            facts->setFact(NotePath, std::string("The Squire took boys until 22. That door is shut."));
        }
        else
        {
            facts->setFact(StatePath, std::string("open"));
            facts->setFact(NotePath, squireNote(age));
        }
    }
}
