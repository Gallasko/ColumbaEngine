#include "prefabfilegallery.h"

#ifdef __EMSCRIPTEN__
#include <SDL2/SDL.h>
#elif __linux__
#include <SDL2/SDL.h>
#elif _WIN32
#include <SDL.h>
#endif

#include <string>

#include "ECS/entitysystem.h"
#include "ECS/entitysystem_fwd.h"
#include "Input/sdlevents.h"
#include "2D/position.h"
#include "2D/simple2dobject.h"
#include "UI/prefab.h"
#include "UI/prefabloader.h"
#include "Systems/gamefacts.h"

#include "UI/themesystem.h"
#include "UI/button.h"
#include "UI/tabs.h"

using namespace pg;

namespace chronicle
{
    namespace
    {
        constexpr const char* FILE = "res/chronicle/ui/prefabgallery.yaml";

        // The Squire path, as in RequirementGallery: the scene owns the numbers.
        struct SquireReq { int start; int needed; };
        const SquireReq SQUIRE[3] = {{12, 18}, {2, 4}, {9, 8}};

        std::string curPath(size_t i) { return "milestone.squire.reqs." + std::to_string(i) + ".current"; }
        std::string metPath(size_t i) { return "milestone.squire.reqs." + std::to_string(i) + ".met"; }
    }

    void PrefabFileGallery::init()
    {
        theme = ecsRef->getSystem<ThemeSystem>();

        auto place = [](EntityRef e, float x, float y)
        {
            auto p = e->get<PositionComponent>();
            p->setX(x);
            p->setY(y);
        };

        auto caption = [&](float, float, const std::string& text, const std::string& color)
        {
            LabelSpec spec;
            spec.style = "caption";
            spec.text = text;
            spec.color = color;
            spec.z = 15;

            return makeLabel(ecsRef, spec);
        };

        const float margin = theme->space(7);   // 48

        // ── The page ─────────────────────────────────────────────────────────
        std::vector<std::string> errors;
        PrefabLoadOptions options;
        options.errors = &errors;

        auto spec = loadNodeSpec(ecsRef, FILE, options);
        if (not spec)
        {
            Label fail = caption(margin, 68.0f, std::string("could not load ") + FILE + " (see the log)", "status-loss");
            place(fail.entity, margin, 68.0f);
            return;
        }

        page = buildTree(ecsRef, *spec);

        if (page.id == 0)
        {
            Label fail = caption(margin, 68.0f, std::string("could not build ") + FILE + " (see the log)", "status-loss");
            place(fail.entity, margin, 68.0f);
            return;
        }

        // The page background is the file's first node; the resize handler below keeps it full-window.
        backgroundId = page->get<Prefab>()->getEntity("page").id;

        if (not errors.empty())
        {
            Label c = caption(margin, 40.0f, std::to_string(errors.size()) + " problem(s) in the file, see the log", "status-loss");
            place(c.entity, margin, 40.0f);
        }

        // Wiring: the fed list follows the facts, the button writes them
        // The list's setters are helpers on its prefab; the argument types must match the
        // helper's signature exactly (size_t index, int values, bool verdict).
        auto facts = ecsRef->getSystem<WorldFacts>();
        const _unique_id fedId = page->get<Prefab>()->findEntity("fed").id;
        auto fedEnt = ecsRef->getEntity(fedId);

        if (facts and fedEnt and fedEnt->has<Prefab>() and fedEnt->get<Prefab>()->hasHelper("setItem"))
        {
            for (size_t i = 0; i < 3; ++i)
                facts->setFact(curPath(i), SQUIRE[i].start);

            facts->setFact(metPath(3), false);

            listenToEvent<WorldFactsUpdate>([this, fedId](const WorldFactsUpdate& event) {
                auto fed = ecsRef->getEntity(fedId);

                if (not fed or not fed->has<Prefab>())
                    return;

                for (const auto& name : event.changedFacts)
                {
                    const auto& it = event.factMap->find(name);

                    if (it == event.factMap->end())
                        continue;

                    for (size_t i = 0; i < 3; ++i)
                    {
                        if (name == curPath(i))
                            fed->get<Prefab>()->callHelper("setItem", i, it->second.get<int>(), SQUIRE[i].needed);
                    }

                    if (name == metPath(3))
                        fed->get<Prefab>()->callHelper("setMet", size_t{3}, it->second.get<bool>());
                }
            });
        }

        LabelSpec es; es.style = "body-sm"; es.text = "ready"; es.color = "ink-muted"; es.z = 15;
        echo = makeLabel(ecsRef, es);
        place(echo.entity, margin, 820.0f);

        listenToEvent<ButtonActivatedEvent>([this](const ButtonActivatedEvent& e)
        {
            auto facts = ecsRef->getSystem<WorldFacts>();

            if (not facts)
                return;

            if (e.tag == "gallery.train")
            {
                const int str = facts->getFact<int>(curPath(0)) + 1;
                const int swd = facts->getFact<int>(curPath(1)) + 1;
                facts->setFact(curPath(0), str);
                facts->setFact(curPath(1), swd);
                echo.setText(ecsRef, "A season of training: Strength " + std::to_string(str) + ", Swordsmanship " + std::to_string(swd));
            }
            else if (e.tag == "gallery.letter")
            {
                const bool has = facts->getFact<bool>(metPath(3));
                facts->setFact(metPath(3), not has);
                echo.setText(ecsRef, has ? "The letter is revoked" : "The letter is granted");
            }
            else
            {
                echo.setText(ecsRef, "button '" + e.tag + "'");
            }
        });

        listenToEvent<TabSelectedEvent>([this](const TabSelectedEvent& e)
        {
            echo.setText(ecsRef, "tab " + std::to_string(e.index) + " of '" + e.tag + "'");
        });

        listenToEvent<OnSDLScanCode>([this](const OnSDLScanCode& event)
        {
            if (event.key == SDL_SCANCODE_T)
            {
                theme->setTheme(theme->currentTheme() == "day" ? "candle" : "day");
            }
        });

        listenToEvent<ResizeEvent>([this](const ResizeEvent& event)
        {
            auto entity = ecsRef->getEntity(backgroundId);
            if (entity and entity->has<PositionComponent>())
            {
                entity->get<PositionComponent>()->setWidth(event.width);
                entity->get<PositionComponent>()->setHeight(event.height);
            }
        });
    }
}
