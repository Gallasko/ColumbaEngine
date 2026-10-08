#include "application.h"

#include <cstdio>
#include <cstdlib>

#include "window.h"
#include "ECS/entitysystem.h"
#include "UI/ttftext.h"
#include "Scene/scenemanager.h"

#include "Systems/tween.h"
#include "Systems/gamefacts.h"
#include "Systems/achievement.h"

#include "UI/themesystem.h"

#include "UI/mark.h"
#include "UI/ornament.h"
#include "UI/button.h"
#include "UI/tabs.h"
#include "UI/activityrow.h"
#include "UI/gloss.h"
#include "ECS/entitysystem_fwd.h"   // ResizeEvent, used by tooltip.h
#include "UI/tooltip.h"
#include "UI/prefabfactory.h"
#include "UI/enginefactories.h"
#include "UI/factories.h"
#include "Scenes/devscenes.h"
#include "Scenes/lifescene.h"
#include "UI/sizer.h"   // LayoutSystem
#include "Core/factrouter.h"
#include "Core/settle.h"
#include "Core/analytics.h"

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
        config.width = opt.width;
        config.height = opt.height;
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
            ecs.createSystem<FactRouter>();   // Paths to the widgets that follow them

            // How long a session lasts and where the life stood when it stopped (a browser build only)
            ecs.createSystem<Analytics>();

            // The deeds of a life, watched against the facts. What is reached is kept by the
            // life's own save, not by the systems' file.
            auto* achievements = ecs.createSystem<AchievementSys>();
            ecs.getComponentRegistry()->unregisterSystemSave(achievements->getSystemName());
            achievements->clear();
            ecs.succeed<AchievementSys, WorldFacts>();

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

            // Activity rows and lists: hover, selection and confirm, and the rows' adoption by their list.
            ecs.createSystem<ActivitySystem>();
            ecs.succeed<MouseHoverSystem, ActivitySystem>();
            // It walks the lists' layouts every frame: never while the layout system changes them
            ecs.succeed<LayoutSystem, ActivitySystem>();

            // The page comes to rest in the pass where it changes: the facts reach their widgets, the
            // rows their list, the layouts and the solver each other, before anything is drawn. Without
            // this a month's end took six passes to place, each one drawn
            settleThePage(&ecs);

            // Gloss tooltips go through the engine's TooltipSystem (created by the UI boot).
            if (auto* tip = ecs.getSystem<TooltipSystem>())
                tip->setDefaultFont("body-sm");
            ecs.createSystem<GlossRegistry>();

            // Prefab factories: the engine primitives plus every Chronicle kind, so a
            // NodeSpec tree (hand-built or loaded from res/chronicle/ui/*.yaml) builds.
            auto* factories = ecs.createSystem<PrefabFactoryRegistry>();
            registerEnginePrefabFactories(factories);
            registerChronicleFactories(factories);

            // 3. scene: the Life scene, unless --dev names a gallery
            if (opt.devScene.empty() or opt.devScene == "LifeScene")
            {
                LifeSceneOptions life;
                life.fresh = opt.fresh;
                life.noSave = opt.noSave;
                life.freshWithoutSave = opt.freshWithoutSave;
                life.savePath = opt.savePath;
                life.monthMs = opt.monthMs;

                ecs.getSystem<SceneElementSystem>()->loadSystemScene<LifeScene>(life);
            }
            else
            {
                loadDevScene(ecs.getSystem<SceneElementSystem>(), opt.devScene);
            }
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
