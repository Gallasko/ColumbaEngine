#ifndef APPLICATION_H
#define APPLICATION_H

#include "engine.h"
#include "buildingregistry.h"
#include "itemregistry.h"
#include "missionregistry.h"
#include "reciperegistry.h"

class GameApp
{
public:
    GameApp(const std::string& appName);
    ~GameApp();

    int exec();

private:
    BuildingRegistry registry;
    ItemRegistry itemRegistry;
    MissionRegistry missionRegistry;
    RecipeRegistry recipeRegistry;
    pg::Engine engine;
};

#endif
