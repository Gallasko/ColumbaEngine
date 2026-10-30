#include "application.h"

#include <filesystem>

#include "2D/position.h"
#include "2D/simple2dobject.h"
#include "Audio/audiosystem.h"
#include "Networking/backend_sdl.h"
#include "Networking/network_system.h"
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

        // The engine starts with 6 sound channels: a fight plays many sounds at once
        if (auto* audio = ecs.getSystem<AudioSystem>())
            audio->setNumberOfChannel(24);

        // The engine made the 2D shape renderer already (with the shaders of res/setupRenderer.pg)
        auto* ttf = ecs.createSystem<TTFTextSystem>(masterRenderer);
        arena::Ui::registerFonts(ttf);

        arena::ArenaOptions arenaOptions;
        arenaOptions.modsDir = pick(options.modsDir, VOXELARENA_MODS_DIR, "mods");
        arenaOptions.startupCommands = options.startupCommands;

        LOG_INFO(DOM, "Mods from " << arenaOptions.modsDir << ", shaders from " << shaderDir);

        // Multiplayer: the engine's network system carries the messages, the game reads them each step
        if (options.net != LaunchOptions::Net::None)
        {
            NetworkConfig net;
            net.isServer = options.net == LaunchOptions::Net::Host;
            net.peerAddress = options.address;
            net.tcpPort = options.port;
            net.udpLocalPort = net.isServer ? static_cast<uint16_t>(options.port + 1) : 0;
            net.udpPeerPort = static_cast<uint16_t>(options.port + 1);
            net.emitEvents = false;      // the game polls
            net.autoReconnect = false;   // a lost client says so instead of rejoining a world it no longer knows

            arenaOptions.network = ecs.createSystem<NetworkSystem>(new SdlNetworkBackend(net), net);
            arenaOptions.mode = net.isServer ? arena::ArenaOptions::Mode::Host : arena::ArenaOptions::Mode::Client;
            arenaOptions.playerName = options.name;
            arenaOptions.address = options.address;
            arenaOptions.port = options.port;
            arenaOptions.upnp = options.upnp;

            LOG_INFO(DOM, (net.isServer ? "Hosting on port " : "Joining ") << (net.isServer ? "" : options.address + ":") << options.port);
        }

        ecs.createSystem<arena::ArenaGame>(masterRenderer, &window, arenaOptions);

        // The game first (it moves the HUD), then the positions settle, then the 2D renderers, then the
        // frame is put together. Positions settle in their own system: unordered, a piece's new colour
        // or text could reach the frame a pass before its new place (the UI flickered)
        ecs.succeed<PositionComponentSystem, arena::ArenaGame>();
        ecs.succeed<Simple2DObjectSystem, arena::ArenaGame>();
        ecs.succeed<TTFTextSystem, arena::ArenaGame>();
        ecs.succeed<Simple2DObjectSystem, PositionComponentSystem>();
        ecs.succeed<TTFTextSystem, PositionComponentSystem>();
        ecs.succeed<MasterRenderer, arena::ArenaGame>();
        ecs.succeed<MasterRenderer, Simple2DObjectSystem>();
        ecs.succeed<MasterRenderer, TTFTextSystem>();

        // The game reads what the network received this pass
        if (arenaOptions.network)
            ecs.succeed<arena::ArenaGame, NetworkSystem>();
    });
}

int GameApp::exec()
{
    return engine.exec();
}
