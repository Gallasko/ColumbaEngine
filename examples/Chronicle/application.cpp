#include "application.h"

#include <cstdio>
#include <cstdlib>

#include "window.h"
#include "ECS/entitysystem.h"
#include "UI/ttftext.h"
#include "Scene/scenemanager.h"

#include "Systems/tween.h"
#include "UI/gamedataview.h"

#include "UI/paint.h"
#include "UI/mark.h"
#include "UI/ornament.h"
#include "UI/button.h"
#include "UI/tabs.h"
#include "UI/gloss.h"
#include "ECS/entitysystem_fwd.h"   // ResizeEvent, used by tooltip.h
#include "UI/tooltip.h"
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
            // 1. tokens
            tokens = Tokens::load("res/chronicle/tokens.json");
            if (not tokens.ok())
            {
                for (const auto& e : tokens.errors())
                    LOG_ERROR("Chronicle", e);
            }
            tokens.setTheme(opt.theme == "candle" ? Theme::Candle : Theme::Day);

            // 2. text
            auto* ttfSys = ecs.createSystem<TTFTextSystem>(window.masterRenderer);
            styles = TextStyles::fromTokens(tokens);
            styles.registerAll(ttfSys, "res/font");

            // 3. paint system (repaints on theme change), then the rest of the
            //    standard render/UI/input stack created by the engine boot.
            ecs.createSystem<PaintSystem>(&tokens);

            // Animation and the data seam every phase-2 scene drives through.
            ecs.createSystem<TweenSystem>();
            ecs.createSystem<GameDataView>();

            // Register the icon sets (IconSystem comes from the engine boot).
            registerMarks(&ecs);
            registerOrnaments(&ecs);

            // One Tab order shared by every focusable face (buttons, tabs, later rows).
            ecs.createSystem<FocusOrderSystem>();

            // Buttons: react to the engine's hover/click/focus events. Order after the hover
            // system so a hover diff is seen the same frame.
            ecs.createSystem<ButtonSystem>(&tokens);
            ecs.succeed<MouseHoverSystem, ButtonSystem>();

            ecs.createSystem<TabsSystem>(&tokens);
            ecs.succeed<MouseHoverSystem, TabsSystem>();

            // Gloss tooltips go through the engine's TooltipSystem (created by the UI boot).
            if (auto* tip = ecs.getSystem<TooltipSystem>())
                tip->setDefaultFont("chr-body-sm");
            ecs.createSystem<GlossRegistry>(&tokens, &styles);

            // 4. scene (default TypeSpecimen when no --dev given)
            const std::string scene = opt.devScene.empty() ? "TypeSpecimen" : opt.devScene;
            loadDevScene(ecs.getSystem<SceneElementSystem>(), scene, &tokens, &styles);
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
