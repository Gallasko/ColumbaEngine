#include "application.h"
#include "window.h"

#include "Systems/basicsystems.h"
#include "Renderer/camera.h"
#include "Loaders/Aseprite/asepriteloader.h"
#include "Loaders/Aseprite/asepritefileatlasloader.h"
#include "UI/ttftext.h"
#include "gridatlas.h"
#include "craftingsystem.h"
#include "minersystem.h"
#include "playerinventory.h"
#include "inventoryui.h"
#include "minerui.h"
#include "machineui.h"
#include "insertersystem.h"
#include "storagesystem.h"
#include "storageui.h"
#include "depotsystem.h"
#include "depotui.h"
#include "saveserialization.h"
#include "gamesystem.h"
#include "hotbarsystem.h"
#include "manualmining.h"
#include "worldfacts.h"
#include "handcraftingsystem.h"
#include "craftingui.h"
#include "tooltipsystem.h"
#include "tutorialsystem.h"
#include "autosavesystem.h"
#include "analyticssystem.h"
#include "machinedemosystem.h"
#include "hudbarsystem.h"
#include "missionsystem.h"
#include "missionui.h"
#include "Systems/tween.h"

using namespace pg;

GameApp::GameApp(const std::string &appName) : engine(appName)
{
    registry = createDefaultBuildingRegistry();
    itemRegistry = createDefaultItemRegistry();
    missionRegistry = createDefaultMissionRegistry();
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

        // Load furnace idle sprite atlas (single frame 32x48)
        window.masterRenderer->registerAtlasTexture(
            "Stone_Furnace",
            "res/ext/Structures & Machines/Stone_Furnace.png",
            "",
            std::make_unique<GridAtlas>("Stone_Furnace.png", 32, 48, 32, 48, 1, 1));

        // Load furnace active animation atlas (3 frames of 32x64 in a 96x64 strip)
        window.masterRenderer->registerAtlasTexture(
            "Stone_Furnace_Active",
            "res/ext/Structures & Machines/Stone_Furnace_Active.png",
            "",
            std::make_unique<GridAtlas>("Stone_Furnace_Active.png", 96, 64, 32, 64, 3, 3));

        // Load assembler idle sprite atlas (single frame 32x48)
        window.masterRenderer->registerAtlasTexture(
            "Assembler_Machine_1",
            "res/ext/Structures & Machines/Assembler_Machine_1.png",
            "",
            std::make_unique<GridAtlas>("Assembler_Machine_1.png", 32, 48, 32, 48, 1, 1));

        // Load assembler running animation atlas (4 frames of 32x48 in a 128x48 strip)
        window.masterRenderer->registerAtlasTexture(
            "Assembler_Machine_1_Running",
            "res/ext/Structures & Machines/Assembler_Machine_1_Running.png",
            "",
            std::make_unique<GridAtlas>("Assembler_Machine_1_Running.png", 128, 48, 32, 48, 4, 4));

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

        // Load crate sprite (16x16 single frame) for storage building
        window.masterRenderer->registerAtlasTexture(
            "Crate",
            "res/ext/Automation Components/Crate.png",
            "",
            std::make_unique<GridAtlas>("Crate.png", 16, 16, 16, 16, 1, 1));

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

        // Pixelwood Valley icon pack: 21 cols × 15 rows of 16×16 icons (315 total)
        window.masterRenderer->registerAtlasTexture(
            "PixelwoodIcons",
            "res/ext/Pixelwood Valley Icon Pack 1.0/1.0/Items 16x16.png",
            "",
            std::make_unique<GridAtlas>("Items 16x16.png", 336, 240, 16, 16, 21, 315));

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
        auto* storageSystem = ecs.createSystem<StorageSystem>(gridSystem, &itemRegistry);
        auto* depotSystem = ecs.createSystem<DepotSystem>(gridSystem, &itemRegistry);
        ecs.createSystem<InserterSystem>(
            gridSystem, transportSystem, minerSystem, craftingSystem, storageSystem, depotSystem, &itemRegistry);
        auto* playerInvSystem = ecs.createSystem<PlayerInventorySystem>(&itemRegistry);

        // UI camera at viewport 2 — needed by hotbar, inventory panel, crafting UI, etc.
        auto uiCam = ecs.createEntity();
        {
            auto cam = ecs._attach<BaseCamera2D>(uiCam);
            cam->setWidth(screenW);
            cam->setHeight(screenH);
            window.masterRenderer->queueRegisterCamera(uiCam->id);
        }
        cameraSystem->setUiCameraEntity(uiCam);

        auto* hotbar = ecs.createSystem<HotbarSystem>(
            playerInvSystem, &itemRegistry, &registry, screenW, screenH);

        auto* inventoryUI = ecs.createSystem<InventoryUISystem>(
            playerInvSystem, &itemRegistry, screenW, screenH);

        hotbar->setInventoryUI(inventoryUI);
        cameraSystem->setInventoryUI(inventoryUI);

        auto* minerUI = ecs.createSystem<MinerUISystem>(
            minerSystem, &itemRegistry, playerInvSystem, inventoryUI, screenW, screenH);

        auto* machineUI = ecs.createSystem<MachineUISystem>(
            craftingSystem, &itemRegistry, playerInvSystem, inventoryUI, screenW, screenH);

        auto* storageUI = ecs.createSystem<StorageUISystem>(
            storageSystem, &itemRegistry, playerInvSystem, inventoryUI, screenW, screenH);

        // World facts (progression/discovery state) must exist before the
        // hand-crafting system and crafting UI query it for unlock checks.
        // Discovery facts (discovered_<item>) are set generically by
        // PlayerInventorySystem on first pickup — no defaults needed.
        auto* worldFacts = ecs.createSystem<WorldFacts>();

        // MissionSystem must exist before DepotUI (depot panel shows mission section)
        auto* missionSystem = ecs.createSystem<MissionSystem>(&missionRegistry, depotSystem, worldFacts);

        auto* depotUI = ecs.createSystem<DepotUISystem>(
            depotSystem, &itemRegistry, playerInvSystem, inventoryUI,
            missionSystem, screenW, screenH);

        auto* handCrafting = ecs.createSystem<HandCraftingSystem>(
            playerInvSystem, &itemRegistry, &recipeRegistry, worldFacts);

        auto* craftingUI = ecs.createSystem<CraftingUISystem>(
            handCrafting, &recipeRegistry, &itemRegistry, playerInvSystem,
            worldFacts, inventoryUI, screenW, screenH);

        machineUI->setCraftingUI(craftingUI);
        depotUI->setCraftingUI(craftingUI);

        ecs.createSystem<TweenSystem>();

        auto* manualMining = ecs.createSystem<ManualMiningSystem>(
            gridSystem, cameraSystem, playerInvSystem, &itemRegistry, hotbar, screenW, screenH);

        ecs.createSystem<TooltipSystem>(
            inventoryUI, hotbar, machineUI, playerInvSystem,
            &itemRegistry, &recipeRegistry, screenW, screenH);

        ecs.createSystem<TutorialSystem>(worldFacts, &recipeRegistry);

        auto* machineDemo = ecs.createSystem<MachineDemoSystem>(
            &registry, &itemRegistry, screenW, screenH);

        machineUI->setMachineDemo(machineDemo);
        craftingUI->setMachineDemo(machineDemo);

        auto* missionUI = ecs.createSystem<MissionUISystem>(
            missionSystem, depotSystem, playerInvSystem, screenW, screenH);

        auto* hudBar = ecs.createSystem<HudBarSystem>(inventoryUI, playerInvSystem, worldFacts, screenW, screenH);
        hudBar->setMissionUIToggle([missionUI]() { missionUI->toggle(); });

        ecs.createSystem<AutoSaveSystem>();
        ecs.createSystem<AnalyticsSystem>();

        ecs.createSystem<GameSystem>(gridSystem, cameraSystem, hotbar, &registry, &itemRegistry, transportSystem, inventoryUI, minerUI, craftingUI, manualMining, machineUI, storageUI, depotUI, machineDemo, missionUI);
    });
}

GameApp::~GameApp()
{
}

int GameApp::exec()
{
    return engine.exec();
}
