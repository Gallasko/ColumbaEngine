#include "application.h"

#include <cstdio>
#include <cstdlib>

#include "window.h"
#include "ECS/entitysystem.h"
#include "UI/ttftext.h"
#include "Scene/scenemanager.h"

#include "Systems/tween.h"
#include "Systems/gamefacts.h"

#include "UI/themesystem.h"

#include "UI/mark.h"
#include "UI/ornament.h"
#include "UI/button.h"
#include "UI/tabs.h"
#include "UI/gloss.h"
#include "ECS/entitysystem_fwd.h"   // ResizeEvent, used by tooltip.h
#include "UI/tooltip.h"
#include "UI/prefabfactory.h"
#include "UI/enginefactories.h"
#include "UI/factories.h"
#include "Scenes/devscenes.h"

using namespace pg;

namespace
{
    std::string knownSceneNames()
    {
        std::string names;

        for (const auto& [name, loader] : chronicle::devScenes())
            names += (names.empty() ? "" : ", ") + name;

        return names;
    }
}

namespace chronicle
{
    GameApp::GameApp(const std::string& appName, const LaunchOptions& options) : engine(appName), opt(options)
    {
        // Reject an unknown --dev name before opening a window. This runs before the
        // engine (and its log sink) exist, so write straight to stderr.
        if (not opt.devScene.empty() and devScenes().count(opt.devScene) == 0)
        {
            std::fprintf(stderr, "Chronicle: unknown --dev scene '%s'. Known scenes: %s\n",
                         opt.devScene.c_str(), knownSceneNames().c_str());
            std::exit(2);
        }

        auto config = engine.getConfig();
        config.width = 1320;
        config.height = 860;
        config.manifestPath = "res/chronicle/manifest.json";
        engine.setConfig(config);

        engine.setSetupFunction([this](EntitySystem& ecs, Window& window)
        {
            // 1. text, then the theme: the tokens, text styles and elements of res/chronicle/tokens.json.
            //    The engine boot created the ThemeSystem; loading the file registers every style's
            //    font atlas on the TTFTextSystem and paints every themed entity from then on.
            ecs.createSystem<TTFTextSystem>(window.masterRenderer);

            auto* theme = ecs.getSystem<ThemeSystem>();
            theme->loadTheme("res/chronicle/tokens.json", "res/font");
            theme->setTheme(opt.theme);

            // 2. animation and the data seam every phase-2 scene drives through.
            ecs.createSystem<TweenSystem>();
            ecs.createSystem<WorldFacts>();

            // Register the icon sets (IconSystem comes from the engine boot).
            registerMarks(&ecs);
            registerOrnaments(&ecs);

            // One Tab order shared by every focusable face (buttons, tabs, later rows).
            ecs.createSystem<FocusOrderSystem>();

            // Buttons: react to the engine's hover/click/focus events. Order after the hover
            // system so a hover diff is seen the same frame.
            ecs.createSystem<ButtonSystem>();
            ecs.succeed<MouseHoverSystem, ButtonSystem>();

            ecs.createSystem<TabsSystem>();
            ecs.succeed<MouseHoverSystem, TabsSystem>();

            // Gloss tooltips go through the engine's TooltipSystem (created by the UI boot).
            if (auto* tip = ecs.getSystem<TooltipSystem>())
                tip->setDefaultFont("body-sm");
            ecs.createSystem<GlossRegistry>();

            // Prefab factories: the engine primitives plus every Chronicle kind, so a
            // NodeSpec tree (hand-built or loaded from res/chronicle/ui/*.yaml) builds.
            auto* factories = ecs.createSystem<PrefabFactoryRegistry>();
            registerEnginePrefabFactories(factories);
            registerChronicleFactories(factories);

            // 3. scene (default TypeSpecimen when no --dev given)
            const std::string scene = opt.devScene.empty() ? "TypeSpecimen" : opt.devScene;
            loadDevScene(ecs.getSystem<SceneElementSystem>(), scene);
        });
    }

    GameApp::~GameApp()
    {
    }

    int GameApp::exec()
    {
        return engine.exec();
    }
}
