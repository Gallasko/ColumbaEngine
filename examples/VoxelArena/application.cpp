#include "application.h"

#include <filesystem>

#include "2D/simple2dobject.h"
#include "ECS/entitysystem.h"
#include "Renderer/renderer.h"
#include "UI/ttftext.h"
#include "logger.h"
#include "window.h"

#include "Game/arenagame.h"

using namespace pg;

namespace
{
    constexpr const char* DOM = "App";

    EngineConfig config()
    {
        EngineConfig c;
        c.width = 1280;
        c.height = 720;
        // Two ECS passes per rendered frame, phase-locked: the arena builds one set of render calls per frame
        c.ecsPassesPerFrame = 2;
        return c;
    }

    std::string pick(const std::string& asked, const char* sourceTree, const std::string& local)
    {
        if (not asked.empty())
            return asked;

        std::error_code ec;

        // Reading straight from the source tree lets an edited file be reloaded with F5, no rebuild
        if (sourceTree and std::filesystem::is_directory(sourceTree, ec))
            return sourceTree;

        return local;
    }
}

#ifndef VOXELARENA_MODS_DIR
    #define VOXELARENA_MODS_DIR nullptr
#endif

#ifndef VOXELARENA_SHADER_DIR
    #define VOXELARENA_SHADER_DIR nullptr
#endif

GameApp::GameApp(const std::string& appName, const LaunchOptions& opts) : engine(appName, config()), options(opts)
{
    engine.setSetupFunction([this](EntitySystem& ecs, Window& window)
    {
        auto* masterRenderer = window.masterRenderer;

        const std::string shaderDir = pick("", VOXELARENA_SHADER_DIR, "arena-shader");
        masterRenderer->registerShader("arena", shaderDir + "/arena.vs", shaderDir + "/arena.fs");

        // The engine made the 2D shape renderer already (with the shaders of res/setupRenderer.pg)
        auto* ttf = ecs.createSystem<TTFTextSystem>(masterRenderer);
        ttf->registerFont("res/font/Inter/static/Inter_18pt-SemiBold.ttf", arena::Hud::FONT, 20);
        ttf->registerFont("res/font/Inter/static/Inter_18pt-Regular.ttf", arena::Hud::MONO, 17);

        arena::ArenaOptions arenaOptions;
        arenaOptions.modsDir = pick(options.modsDir, VOXELARENA_MODS_DIR, "mods");
        arenaOptions.startupCommands = options.startupCommands;

        LOG_INFO(DOM, "Mods from " << arenaOptions.modsDir << ", shaders from " << shaderDir);

        ecs.createSystem<arena::ArenaGame>(masterRenderer, &window, arenaOptions);

        // The game first (it moves the HUD), then the 2D renderers, then the frame is put together
        ecs.succeed<Simple2DObjectSystem, arena::ArenaGame>();
        ecs.succeed<TTFTextSystem, arena::ArenaGame>();
        ecs.succeed<MasterRenderer, arena::ArenaGame>();
        ecs.succeed<MasterRenderer, Simple2DObjectSystem>();
        ecs.succeed<MasterRenderer, TTFTextSystem>();
    });
}

int GameApp::exec()
{
    return engine.exec();
}
