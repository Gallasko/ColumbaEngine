#pragma once

#include <string>
#include <vector>

#include "engine.h"

struct LaunchOptions
{
    /** Where the mods live. Empty: the source tree's mods folder, else ./mods */
    std::string modsDir;

    /** Console commands run once the arena is up (--exec "spawn base:brute") */
    std::vector<std::string> startupCommands;

    /** Multiplayer: host a game others join (--host [port]), or join one (--join address[:port]) */
    enum class Net { None, Host, Join };
    Net net = Net::None;
    std::string address = "127.0.0.1";
    uint16_t port = 7777;              // TCP; UDP is the next port
    std::string name = "player";
};

class GameApp
{
public:
    GameApp(const std::string& appName, const LaunchOptions& options);

    int exec();

private:
    pg::Engine engine;
    LaunchOptions options;
};
