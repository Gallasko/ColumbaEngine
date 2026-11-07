#pragma once

#include <string>
#include <vector>

#include "engine.h"

struct LaunchOptions
{
    /** Where the game's mods live. Empty: the source tree's mods folder, else mods/ here or next to the program. The player's own are read too */
    std::string modsDir;

    /** Console commands run once the arena is up (--exec "spawn base:brute") */
    std::vector<std::string> startupCommands;

    /** Multiplayer: host a game others join (--host [port]), or join one (--join address[:port]) */
    enum class Net { None, Host, Join };
    Net net = Net::None;
    std::string address = "127.0.0.1";
    uint16_t port = 7777;              // TCP; UDP is the next port
    std::string name = "player";
    bool upnp = true;                  // a host opens its ports on the router (--no-upnp: not)
    std::string upnpPorts;             // the outside ports to try on the router (--upnp-ports 7777,45000-45200)

    /** The lab (--lab [script.pg]): the content shown one by one, played by a pgscript */
    bool lab = false;
    std::string labScript;
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
