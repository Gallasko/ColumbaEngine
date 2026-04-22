#pragma once

#include "Systems/basicsystems.h"
#include "Input/inputcomponent.h"

#include "gridsystem.h"
#include "camerasystem.h"
#include "hotbarsystem.h"
#include "buildingregistry.h"
#include "transportsystem.h"
#include "inventoryui.h"
#include "minerui.h"
#include "craftingui.h"
#include "machineui.h"
#include "storageui.h"
#include "depotui.h"
#include "manualmining.h"
#include "machinedemosystem.h"
#include "missionui.h"

using namespace pg;

class GameSystem : public System<InitSys, QueuedListener<OnMouseClick>, QueuedListener<OnMouseRelease>, QueuedListener<OnSDLScanCode>, QueuedListener<OnSDLMouseMotion>, Listener<PanelWasClickedEvent>>
{
public:
    GameSystem(GridSystem* gridSystem, CameraSystem* cameraSystem, HotbarSystem* hotbar, BuildingRegistry* registry, ItemRegistry* itemRegistry, TransportSystem* transportSystem = nullptr, InventoryUISystem* inventoryUI = nullptr, MinerUISystem* minerUI = nullptr, CraftingUISystem* craftingUI = nullptr, ManualMiningSystem* manualMining = nullptr, MachineUISystem* machineUI = nullptr, StorageUISystem* storageUI = nullptr, DepotUISystem* depotUI = nullptr, MachineDemoSystem* machineDemo = nullptr, MissionUISystem* missionUI = nullptr)
        : gridSystem(gridSystem), cameraSystem(cameraSystem), hotbar(hotbar), registry(registry), itemRegistry(itemRegistry), transportSystem(transportSystem), inventoryUI(inventoryUI), minerUI(minerUI), craftingUI(craftingUI), manualMining(manualMining), machineUI(machineUI), storageUI(storageUI), depotUI(depotUI), machineDemo(machineDemo), missionUI(missionUI) {}

    virtual std::string getSystemName() const override { return "Game System"; }

    void init() override;

    virtual void onProcessEvent(const OnSDLScanCode& event) override;
    virtual void onProcessEvent(const OnMouseClick& event) override;
    virtual void onProcessEvent(const OnMouseRelease& event) override;
    virtual void onProcessEvent(const OnSDLMouseMotion& event) override;

    virtual void onEvent(const PanelWasClickedEvent&) override
    {
        panelClickedThisFrame = true;
    }

private:
    // Direction constants
    static constexpr size_t DIRECTION_TILE_INDEX[4] = {LINE_RIGHT_1, LINE_DOWN_1, LINE_LEFT_1, LINE_UP_1};
    static constexpr const char* directionNames[4] = {"Right", "Down", "Left", "Up"};

    // Corner tile mapping: cornerTileMap[enterDir][exitDir]
    static constexpr size_t INVALID_CORNER = SIZE_MAX;
    static constexpr size_t cornerTileMap[4][4] = {
        /* enter Right */ {INVALID_CORNER, CORNER_CW_1_0,  INVALID_CORNER, CORNER_CCW_1_1},
        /* enter Down  */ {CORNER_CCW_0_1, INVALID_CORNER, CORNER_CW_1_1,  INVALID_CORNER},
        /* enter Left  */ {INVALID_CORNER, CORNER_CCW_0_0, INVALID_CORNER, CORNER_CW_0_1},
        /* enter Up    */ {CORNER_CW_0_0,  INVALID_CORNER, CORNER_CCW_1_0, INVALID_CORNER},
    };

    // Returns the BuildingDef for the current hotbar selection, or nullptr
    const BuildingDef* getSelectedBuildingDef() const
    {
        return hotbar ? hotbar->getSelectedBuildingDef() : nullptr;
    }

    bool hasBuildingSelected() const { return getSelectedBuildingDef() != nullptr; }

    // --- Cursor & Ghost ---

    void createCursorEntities();
    void rebuildGhostForSelectedBuilding();
    void updateCursorPosition();
    void updateGhostTexture();

    // --- Placement ---

    bool canPlaceAt(int gx, int gy, const BuildingDef& def) const;
    std::pair<int, int> getMouseGridPos() const;
    void placeAtMouse();
    void removeAtMouse();
    bool isMouseOverHotbar() const;

    // --- Line Drag ---

    static uint8_t directionFromTo(int ax, int ay, int bx, int by);
    size_t resolveTileIndex(uint8_t enterDir, uint8_t exitDir) const;
    void updateDragPathWithMouse();
    int findInPath(int x, int y) const;
    void updateDragGhosts();
    void clearDragGhosts();
    void commitDragPath();

    // --- Members ---

    GridSystem* gridSystem = nullptr;
    CameraSystem* cameraSystem = nullptr;
    HotbarSystem* hotbar = nullptr;
    BuildingRegistry* registry = nullptr;
    ItemRegistry* itemRegistry = nullptr;
    TransportSystem* transportSystem = nullptr;
    InventoryUISystem* inventoryUI = nullptr;
    MinerUISystem* minerUI = nullptr;
    CraftingUISystem* craftingUI = nullptr;
    ManualMiningSystem* manualMining = nullptr;
    MachineUISystem* machineUI = nullptr;
    StorageUISystem* storageUI = nullptr;
    DepotUISystem* depotUI = nullptr;
    MachineDemoSystem* machineDemo = nullptr;
    MissionUISystem* missionUI = nullptr;

    bool panelClickedThisFrame = false;
    size_t currentDirection = 0; // 0=Right, 1=Down, 2=Left, 3=Up
    bool leftMouseDown = false;
    const BuildingDef* lastBuildingDef = nullptr; // Track for ghost rebuild

    uint64_t cursorEntityId = 0;
    uint64_t ghostEntityId = 0;

    // Line-drag state
    bool isDragging = false;
    std::vector<std::pair<int, int>> dragPath;
    std::vector<uint64_t> dragGhostEntityIds;
};
