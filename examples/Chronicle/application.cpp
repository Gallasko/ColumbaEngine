#include "application.h"

#include <cstdlib>

#include "window.h"
#include "ECS/entitysystem.h"
#include "UI/ttftext.h"
#include "Scene/scenemanager.h"

#include "Scenes/typespecimen.h"

using namespace pg;

namespace
{
    // The dev scenes this build knows. --dev must name one of these.
    bool isKnownScene(const std::string& name)
    {
        return name == "TypeSpecimen";
    }
}

namespace chronicle
{
    GameApp::GameApp(const std::string& appName, const LaunchOptions& options) : engine(appName), opt(options)
    {
        // Reject an unknown --dev name before opening a window.
        if (not opt.devScene.empty() and not isKnownScene(opt.devScene))
        {
            LOG_ERROR("Chronicle", "Unknown --dev scene '" << opt.devScene << "'. Known scenes: TypeSpecimen");
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

            // 3. the standard render/UI/input stack is created by the engine boot.

            // 4. scene
            ecs.getSystem<SceneElementSystem>()->loadSystemScene<TypeSpecimen>(&tokens, &styles);
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
