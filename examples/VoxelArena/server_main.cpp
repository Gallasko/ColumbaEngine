// Voxel Arena dedicated server: the same simulation and server as a host, with no window, no
// player of its own and nothing drawn or played.
//
//   VoxelArenaServer [--port 7777] [--mods path] [--no-upnp] [--upnp-ports 7777,45000-45200]
//
// Players join with VoxelArena --join address[:port]; the run starts when everyone is ready.
// It opens its ports on the router itself (UPnP) unless told not to. Stop it with Ctrl+C.

#include <atomic>
#include <chrono>
#include <csignal>
#include <filesystem>
#include <iostream>
#include <string>
#include <thread>

#include "Networking/backend_sdl.h"
#include "Networking/network_system.h"

#include "Core/content.h"
#include "Core/scriptbrain.h"
#include "Core/tuning.h"
#include "Core/world.h"
#include "Core/net/server.h"
#include "Net/enginetransport.h"
#include "Net/portmapper.h"

#ifndef VOXELARENA_MODS_DIR
    #define VOXELARENA_MODS_DIR nullptr
#endif

namespace
{
    std::atomic<bool> running{true};

    void stop(int) { running = false; }

    void printReport(const arena::ContentReport& report)
    {
        for (const auto& e : report.errors)
            std::cout << "error: " << e.file << ":" << e.line << ": " << e.text << "\n";

        for (const auto& w : report.warnings)
            std::cout << "warning: " << w.file << ":" << w.line << ": " << w.text << "\n";
    }
}

int main(int argc, char* argv[])
{
    std::string modsDir;
    uint16_t port = 7777;
    bool upnp = true;
    std::string upnpPorts;   // the outside ports to try on the router ("": the server's, then 45000-45200)

    for (int i = 1; i < argc; ++i)
    {
        const std::string a = argv[i];

        if (a == "--port" and i + 1 < argc)
            port = static_cast<uint16_t>(std::stoi(argv[++i]));
        else if (a == "--mods" and i + 1 < argc)
            modsDir = argv[++i];
        else if (a == "--no-upnp")
            upnp = false;
        else if (a == "--upnp-ports" and i + 1 < argc)
            upnpPorts = argv[++i];
    }

    if (modsDir.empty())
    {
        std::error_code ec;
        modsDir = VOXELARENA_MODS_DIR and std::filesystem::is_directory(VOXELARENA_MODS_DIR, ec) ? VOXELARENA_MODS_DIR : "mods";
    }

    arena::Content content = arena::loadContent(modsDir);
    arena::Tuning tuning = arena::readTuning(content.tuning, content.tuningFile, content.report);
    arena::ScriptBrain brain;
    brain.load(content, content.report);
    printReport(content.report);

    arena::ArenaWorld world;
    world.start(content, tuning, false);
    world.setBrain(&brain);

    pg::NetworkConfig net;
    net.isServer = true;
    net.tcpPort = port;
    net.udpLocalPort = static_cast<uint16_t>(port + 1);
    net.emitEvents = false;

    pg::NetworkSystem network(new pg::SdlNetworkBackend(net), net);
    network.init();

    arena::net::EngineTransport transport(&network);
    arena::net::NetServer server(world, content, transport, true);
    server.maxClients = 8;
    server.offerMods(modsDir);

    std::signal(SIGINT, stop);
    std::signal(SIGTERM, stop);

    std::cout << "Voxel Arena server on port " << port << " (TCP) and " << port + 1 << " (UDP), mods from " << modsDir
              << ", content " << std::hex << content.fingerprint << std::dec << std::endl;

    const std::string lan = arena::net::PortMapper::localAddress();
    std::cout << "On this network: " << (lan.empty() ? "?" : lan) << ":" << port << std::endl;

    // The router's ports, opened for players from outside (said once it is known)
    arena::net::PortMapper portMapper;
    if (upnp)
        portMapper.start(port, static_cast<uint16_t>(port + 1), "Voxel Arena server", arena::net::PortMapper::parsePorts(upnpPorts));
    auto routerState = arena::net::PortMapper::State::Off;

    // 60 steps a second, on a fixed schedule (a late step is caught up, a far behind one skipped)
    using clock = std::chrono::steady_clock;
    const auto stepLength = std::chrono::duration_cast<clock::duration>(std::chrono::duration<double>(arena::STEP_SECONDS));
    auto next = clock::now();

    while (running)
    {
        network.execute();
        server.poll();
        server.step(nullptr);
        network.execute();

        for (const auto& line : server.log)
            std::cout << line << std::endl;
        server.log.clear();

        if (const auto st = portMapper.status(); st.state != routerState)
        {
            routerState = st.state;
            if (st.state == arena::net::PortMapper::State::Open)
                std::cout << "Router ports opened: players join with " << (st.externalIp.empty() ? "your public address" : st.externalIp) << ":"
                          << (st.externalPort ? st.externalPort : port) << std::endl;
            else if (st.state == arena::net::PortMapper::State::NoRouter or st.state == arena::net::PortMapper::State::Failed)
                std::cout << "Router ports not opened (" << st.error << "): forward TCP " << port << " and UDP " << port + 1 << " to this machine" << std::endl;
        }

        next += stepLength;

        if (clock::now() - next > std::chrono::milliseconds(250))
            next = clock::now();

        std::this_thread::sleep_until(next);
    }

    std::cout << "Server stopping" << std::endl;

    return 0;
}
