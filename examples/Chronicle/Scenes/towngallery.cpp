#include "towngallery.h"

#ifdef __EMSCRIPTEN__
#include <SDL2/SDL.h>
#elif __linux__
#include <SDL2/SDL.h>
#elif _WIN32
#include <SDL.h>
#endif

#include <string>
#include <vector>

#include "logger.h"

#include "ECS/entitysystem.h"
#include "ECS/entitysystem_fwd.h"
#include "Input/sdlevents.h"
#include "2D/position.h"
#include "UI/prefab.h"
#include "UI/prefabloader.h"
#include "UI/prefabbuilder.h"

#include "UI/placetile.h"

using namespace pg;

namespace chronicle
{
    namespace
    {
        constexpr const char * const DOM = "Chronicle.TownGallery";

        constexpr const char * const File = "res/chronicle/ui/towngallery.yaml";

        const char * const GridNames[] = {"places", "narrow"};
    }

    void TownGallery::init()
    {
        theme = ecsRef->getSystem<ThemeSystem>();

        std::vector<std::string> errors;
        PrefabLoadOptions options;
        options.errors = &errors;

        auto spec = loadNodeSpec(ecsRef, File, options);

        if (not spec)
        {
            LOG_ERROR(DOM, "Could not load " << File);
            return;
        }

        for (const auto& e : errors)
            LOG_ERROR(DOM, File << ": " << e);

        EntityRef page = buildTree(ecsRef, *spec);

        if (page.empty())
        {
            LOG_ERROR(DOM, "Could not build " << File);
            return;
        }

        ecsRef->attach<SceneElement>(page);

        auto prefab = page->get<Prefab>();
        backgroundId = prefab->getEntity("page").id;

        for (const char* name : GridNames)
            grids.push_back(prefab->findEntity(name).id);

        // A tile clicked is the one chosen, in the grid that holds it
        listenToEvent<PlaceSelectedEvent>([this](const PlaceSelectedEvent& e) {
            for (auto id : grids)
            {
                auto entity = ecsRef->getEntity(id);

                if (entity and entity->has<PlaceGrid>())
                    entity->get<PlaceGrid>()->select(ecsRef, e.id);
            }
        });

        listenToEvent<OnSDLScanCode>([this](const OnSDLScanCode& e) {
            if (e.key == SDL_SCANCODE_T and theme)
                theme->setTheme(theme->currentTheme() == "day" ? "candle" : "day");
        });

        listenToEvent<ResizeEvent>([this](const ResizeEvent& e) {
            if (auto background = ecsRef->getEntity(backgroundId))
            {
                background->get<PositionComponent>()->setWidth(e.width);
                background->get<PositionComponent>()->setHeight(e.height);
            }
        });
    }
}
