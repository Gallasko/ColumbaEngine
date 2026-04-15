#include "application.h"
#include "window.h"

#include "Systems/basicsystems.h"
#include "Loaders/Aseprite/asepriteloader.h"
#include "Loaders/Aseprite/asepritefileatlasloader.h"
#include "craftingsystem.h"
#include "playerinventory.h"
#include "gamesystem.h"

using namespace pg;

GameApp::GameApp(const std::string &appName) : engine(appName)
{
    registry = createDefaultRegistry();
    itemRegistry = createDefaultItemRegistry();
    recipeRegistry = createDefaultRecipeRegistry();

    engine.setSetupFunction([this](EntitySystem& ecs, Window& window)
    {
        auto config = engine.getConfig();

        // Aseprite loader for sprite atlas metadata
        auto* asepriteLoader = ecs.createSystem<AsepriteLoader>();

        // Load conveyor belt sprite atlas
        auto anim = asepriteLoader->loadAnim(
            "res/ext/Automation Components/Conveyor_Belt.json", "Conveyor_Belt");

        window.masterRenderer->registerAtlasTexture(
            anim.filename,
            anim.metadata.imagePath.c_str(),
            "",
            std::make_unique<AsepriteFileAtlasLoader>(anim));

        float screenW = static_cast<float>(config.width);
        float screenH = static_cast<float>(config.height);

        // Camera must be created first so it exists before grid renders
        auto* cameraSystem = ecs.createSystem<CameraSystem>(
            window.masterRenderer, screenW, screenH);

        auto* gridSystem = ecs.createSystem<GridSystem>(&registry);

        // Inventory and crafting systems (must come after GridSystem)
        auto* transportSystem = ecs.createSystem<TransportSystem>(gridSystem, &itemRegistry);
        ecs.createSystem<CraftingSystem>(
            gridSystem, transportSystem, &itemRegistry, &recipeRegistry);
        ecs.createSystem<PlayerInventorySystem>(&itemRegistry);

        auto* toolbarSystem = ecs.createSystem<ToolbarSystem>(
            &registry, window.masterRenderer, screenW, screenH);

        ecs.createSystem<GameSystem>(gridSystem, cameraSystem, toolbarSystem, &registry, transportSystem);
    });
}

GameApp::~GameApp()
{
}

int GameApp::exec()
{
    return engine.exec();
}
