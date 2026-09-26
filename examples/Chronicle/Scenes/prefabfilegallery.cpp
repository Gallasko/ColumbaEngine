#include "prefabfilegallery.h"

#ifdef __linux__
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
#include "UI/prefabloader.h"

#include "UI/themesystem.h"
#include "UI/requirementlist.h"
#include "UI/button.h"
#include "UI/tabs.h"
#include "UI/gamedataview.h"

using namespace pg;

namespace chronicle
{
    namespace
    {
        constexpr float PAGE_W = 1320.0f;
        constexpr float PAGE_H = 860.0f;
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

        auto bg = makeUiSimple2DShape(ecsRef, Shape2D::Square, PAGE_W, PAGE_H, theme->color("vellum"));
        bg.get<PositionComponent>()->setZ(0.0f);
        backgroundId = bg.entity.id;
        ecsRef->attach<ThemeComponent>(bg.entity, "scene.background");

        auto place = [](EntityRef e, float x, float y)
        {
            auto p = e->get<PositionComponent>();
            p->setX(x);
            p->setY(y);
        };

        auto caption = [&](float x, float y, const std::string& text, const std::string& color)
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

        built = buildTree(ecsRef, *spec);

        {
            Label c = caption(margin, 68.0f, std::string("every panel on this page comes from ") + FILE, "ink-muted");
            place(c.entity, margin, 68.0f);
        }

        if (not errors.empty())
        {
            Label c = caption(margin, 40.0f, std::to_string(errors.size()) + " problem(s) in the file, see the log", "status-loss");
            place(c.entity, margin, 40.0f);
        }

        // ── Wiring: the fed list follows the view, the button writes it ──────
        auto* view = ecsRef->getSystem<GameDataView>();
        RequirementList* fed = built.get<RequirementList>("fed");

        if (view and fed)
        {
            for (size_t i = 0; i < 3; ++i)
                view->set(curPath(i), ElementType{SQUIRE[i].start});
            view->set(metPath(3), ElementType{false});

            for (size_t i = 0; i < 3; ++i)
            {
                const int needed = SQUIRE[i].needed;
                view->subscribe(curPath(i), [this, fed, i, needed](const ElementType& v) {
                    fed->setItem(ecsRef, i, v.get<int>(), needed);
                });
            }
            view->subscribe(metPath(3), [this, fed](const ElementType& v) {
                fed->setMet(ecsRef, 3, v.get<bool>());
            });
        }

        LabelSpec es; es.style = "body-sm"; es.text = "ready"; es.color = "ink-muted"; es.z = 15;
        echo = makeLabel(ecsRef, es);
        place(echo.entity, margin, 820.0f);

        listenToEvent<ButtonActivatedEvent>([this](const ButtonActivatedEvent& e)
        {
            auto* v = ecsRef->getSystem<GameDataView>();
            if (not v)
                return;

            if (e.tag == "gallery.train")
            {
                const int str = v->get(curPath(0)).get<int>() + 1;
                const int swd = v->get(curPath(1)).get<int>() + 1;
                v->set(curPath(0), ElementType{str});
                v->set(curPath(1), ElementType{swd});
                echo.setText(ecsRef, "A season of training: Strength " + std::to_string(str) + ", Swordsmanship " + std::to_string(swd));
            }
            else if (e.tag == "gallery.letter")
            {
                const bool has = v->get(metPath(3)).get<bool>();
                v->set(metPath(3), ElementType{not has});
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
