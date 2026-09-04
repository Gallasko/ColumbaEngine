#ifndef APPLICATION_H
#define APPLICATION_H

#include "engine.h"

/**
 * BlockBreaker: a game example driven entirely by PgScript.
 *
 * The C++ side only boots the engine and hands control to main.pg;
 * all game logic (paddle, ball, bricks, scoring) lives in the script.
 */
class GameApp
{
public:
    GameApp(const std::string& appName, const std::string& scriptPath);
    ~GameApp();

    int exec();

private:
    pg::Engine engine;
};

#endif
