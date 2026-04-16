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
#include "worldfacts.h"
#include "handcraftingsystem.h"
#include "craftingui.h"

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

        // Environment tilesets for procedurally generated canvases.
        // Single-tile bases used as-is; multi-tile tilesets are 3x3 where frame 4 is the center.
        window.masterRenderer->registerAtlasTexture(
            "Ground",
            "res/ext/Tileset & Environment/Ground_Tile.png",
            "",
            std::make_unique<GridAtlas>("Ground_Tile.png", 16, 16, 16, 16, 1, 1));

        window.masterRenderer->registerAtlasTexture(
            "Grass_Tileset",
            "res/ext/Tileset & Environment/Grass_Tileset.png",
            "",
            std::make_unique<GridAtlas>("Grass_Tileset.png", 80, 48, 16, 16, 5, 15));

        // Multi-terrain environment tileset: 12 cols x 21 rows of 16x16 frames.
        // Frame layout is row-major (frame i -> col i%12, row i/12). The first
        // 3 rows (frames 0-35) are grass autotile tiles; other rows contain
        // water / sand / path variants we don't use yet.
        window.masterRenderer->registerAtlasTexture(
            "Environment_Tileset",
            "res/ext/Tileset & Environment/Environment_Tileset.png",
            "",
            std::make_unique<GridAtlas>("Environment_Tileset.png", 192, 336, 16, 16, 12, 252));

        window.masterRenderer->registerAtlasTexture(
            "Iron_Ore_Tiles",
            "res/ext/Tileset & Environment/Iron_Ore_Tiles.png",
            "",
            std::make_unique<GridAtlas>("Iron_Ore_Tiles.png", 48, 48, 16, 16, 3, 9));

        window.masterRenderer->registerAtlasTexture(
            "Coal_Tiles",
            "res/ext/Tileset & Environment/Coal_Tiles.png",
            "",
            std::make_unique<GridAtlas>("Coal_Tiles.png", 48, 48, 16, 16, 3, 9));

        window.masterRenderer->registerAtlasTexture(
            "Rock_Tiles",
            "res/ext/Tileset & Environment/Rock_Tiles.png",
            "",
            std::make_unique<GridAtlas>("Rock_Tiles.png", 48, 48, 16, 16, 3, 9));

        window.masterRenderer->registerAtlasTexture(
            "Copper_Rock",
            "res/ext/Tileset & Environment/Copper_Rock.png",
            "",
            std::make_unique<GridAtlas>("Copper_Rock.png", 16, 16, 16, 16, 1, 1));

        window.masterRenderer->registerAtlasTexture(
            "Rock_Tile",
            "res/ext/Tileset & Environment/Rock_Tile.png",
            "",
            std::make_unique<GridAtlas>("Rock_Tile.png", 16, 16, 16, 16, 1, 1));

        // Tree sprite: 32x48 single image; treated as a 1x1 atlas of a 32x48 frame.
        window.masterRenderer->registerAtlasTexture(
            "Tree",
            "res/ext/Tileset & Environment/Tree.png",
            "",
            std::make_unique<GridAtlas>("Tree.png", 32, 48, 32, 48, 1, 1));

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

        // World facts (progression/discovery state) must exist before the
        // hand-crafting system and crafting UI query it for unlock checks.
        auto* worldFacts = ecs.createSystem<WorldFacts>();
        worldFacts->setDefaultFact("discovered_coal", false);

        auto* handCrafting = ecs.createSystem<HandCraftingSystem>(
            playerInvSystem, &itemRegistry, &recipeRegistry, worldFacts);

        auto* craftingUI = ecs.createSystem<CraftingUISystem>(
            handCrafting, &recipeRegistry, &itemRegistry, playerInvSystem,
            worldFacts, inventoryUI, screenW, screenH);

        ecs.createSystem<GameSystem>(gridSystem, cameraSystem, toolbarSystem, &registry, transportSystem, inventoryUI, minerUI, craftingUI);
    });
}

GameApp::~GameApp()
{
}

int GameApp::exec()
{
    return engine.exec();
}
