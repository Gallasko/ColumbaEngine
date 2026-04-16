#include "application.h"
#include "window.h"

#include "Systems/basicsystems.h"
#include "Loaders/Aseprite/asepriteloader.h"
#include "Loaders/Aseprite/asepritefileatlasloader.h"
#include "UI/ttftext.h"
#include "gridatlas.h"
#include "craftingsystem.h"
#include "minersystem.h"
#include "playerinventory.h"
#include "inventoryui.h"
#include "minerui.h"
#include "insertersystem.h"
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

        // Load miner idle sprite atlas (single frame)
        auto minerIdle = asepriteLoader->loadAnim(
            "res/ext/Structures & Machines/Miner_Machine_1.json", "Miner_Idle");

        window.masterRenderer->registerAtlasTexture(
            minerIdle.filename,
            minerIdle.metadata.imagePath.c_str(),
            "",
            std::make_unique<AsepriteFileAtlasLoader>(minerIdle));

        // Load miner mining animation atlas (4 frames)
        auto minerMining = asepriteLoader->loadAnim(
            "res/ext/Structures & Machines/Miner_Machine_1_Mining.json", "Miner_Mining");

        window.masterRenderer->registerAtlasTexture(
            minerMining.filename,
            minerMining.metadata.imagePath.c_str(),
            "",
            std::make_unique<AsepriteFileAtlasLoader>(minerMining));

        // Load item icons as a grid atlas (5 cols × 5 rows of 16×16 icons, 23 used)
        window.masterRenderer->registerAtlasTexture(
            "Items",
            "res/ext/Item Icons/Items.png",
            "",
            std::make_unique<GridAtlas>("Items.png", 80, 80, 16, 16, 5, 23));

        // Load robotic arm sprite sheet (8 frames of 48x48 in a 384x48 strip)
        window.masterRenderer->registerAtlasTexture(
            "Robotic_Arms_1",
            "res/ext/Automation Components/Robotic_Arms_1.png",
            "",
            std::make_unique<GridAtlas>("Robotic_Arms_1.png", 384, 48, 48, 48, 8, 8));

        float screenW = static_cast<float>(config.width);
        float screenH = static_cast<float>(config.height);

        // TTF text system for UI text rendering
        auto* ttfSys = ecs.createSystem<TTFTextSystem>(window.masterRenderer);
        ttfSys->registerFont("res/font/Inter/static/Inter_28pt-Light.ttf");

        // Camera must be created first so it exists before grid renders
        auto* cameraSystem = ecs.createSystem<CameraSystem>(
            window.masterRenderer, screenW, screenH);

        auto* gridSystem = ecs.createSystem<GridSystem>(&registry);

        // Inventory and crafting systems (must come after GridSystem)
        auto* transportSystem = ecs.createSystem<TransportSystem>(gridSystem, &itemRegistry);
        auto* craftingSystem = ecs.createSystem<CraftingSystem>(
            gridSystem, transportSystem, &itemRegistry, &recipeRegistry);
        auto* minerSystem = ecs.createSystem<MinerSystem>(gridSystem, transportSystem, &itemRegistry);
        ecs.createSystem<InserterSystem>(
            gridSystem, transportSystem, minerSystem, craftingSystem, &itemRegistry);
        auto* playerInvSystem = ecs.createSystem<PlayerInventorySystem>(&itemRegistry);

        auto* toolbarSystem = ecs.createSystem<ToolbarSystem>(
            &registry, window.masterRenderer, screenW, screenH);

        auto* inventoryUI = ecs.createSystem<InventoryUISystem>(
            playerInvSystem, &itemRegistry, screenW, screenH);

        auto* minerUI = ecs.createSystem<MinerUISystem>(
            minerSystem, &itemRegistry, playerInvSystem, inventoryUI, screenW, screenH);

        ecs.createSystem<GameSystem>(gridSystem, cameraSystem, toolbarSystem, &registry, transportSystem, inventoryUI, minerUI);
    });
}

GameApp::~GameApp()
{
}

int GameApp::exec()
{
    return engine.exec();
}
