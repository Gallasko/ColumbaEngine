#ifndef APPLICATION_H
#define APPLICATION_H

#include "engine.h"

class GameApp
{
public:
    GameApp(const std::string& appName);
    ~GameApp();

    int exec();

private:
    pg::Engine engine;
};

#endif