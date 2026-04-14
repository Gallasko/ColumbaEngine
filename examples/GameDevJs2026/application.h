#ifndef APPLICATION_H
#define APPLICATION_H

#include "engine.h"
#include "buildingregistry.h"

class GameApp
{
public:
    GameApp(const std::string& appName);
    ~GameApp();

    int exec();

private:
    pg::Engine engine;
    BuildingRegistry registry;
};

#endif
