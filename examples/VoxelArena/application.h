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
