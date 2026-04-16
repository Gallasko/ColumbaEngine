#pragma once

#include "Systems/basicsystems.h"
#include "Input/inputcomponent.h"

#include "gridsystem.h"
#include "camerasystem.h"
#include "toolbarsystem.h"
#include "buildingregistry.h"
#include "transportsystem.h"
#include "inventoryui.h"
#include "minerui.h"
#include "craftingui.h"

using namespace pg;

class GameSystem : public System<InitSys, QueuedListener<OnMouseClick>, QueuedListener<OnMouseRelease>, QueuedListener<OnSDLScanCode>, QueuedListener<OnSDLMouseMotion>>
{
public:
    GameSystem(GridSystem* gridSystem, CameraSystem* cameraSystem, ToolbarSystem* toolbarSystem, BuildingRegistry* registry, TransportSystem* transportSystem = nullptr, InventoryUISystem* inventoryUI = nullptr, MinerUISystem* minerUI = nullptr, CraftingUISystem* craftingUI = nullptr)
        : gridSystem(gridSystem), cameraSystem(cameraSystem), toolbarSystem(toolbarSystem), registry(registry), transportSystem(transportSystem), inventoryUI(inventoryUI), minerUI(minerUI), craftingUI(craftingUI) {}

    virtual std::string getSystemName() const override { return "Game System"; }

    void init() override;

    virtual void onProcessEvent(const OnSDLScanCode& event) override;
    virtual void onProcessEvent(const OnMouseClick& event) override;
    virtual void onProcessEvent(const OnMouseRelease& event) override;
    virtual void onProcessEvent(const OnSDLMouseMotion& event) override;

    size_t getSelectedSlot() const { return toolbarSystem->getSelectedSlot(); }

private:
    // Direction constants
    static constexpr size_t DIRECTION_TILE_INDEX[4] = {LINE_RIGHT_1, LINE_DOWN_1, LINE_LEFT_1, LINE_UP_1};
    static constexpr const char* directionNames[4] = {"Right", "Down", "Left", "Up"};

    // Corner tile mapping: cornerTileMap[enterDir][exitDir]
    // enterDir/exitDir: 0=Right, 1=Down, 2=Left, 3=Up
    //
    // (X,Y) in corner_XX_X_Y = the OPEN/EMPTY corner position:
    //   (0,0) empty top-left    → belt bottom-right → connects Bottom+Right
    //   (1,0) empty top-right   → belt bottom-left  → connects Bottom+Left
    //   (0,1) empty bottom-left → belt top-right    → connects Top+Right
    //   (1,1) empty bottom-right→ belt top-left     → connects Top+Left
    //
    // CW turns:  R→D=1_0, D→L=1_1, L→U=0_1, U→R=0_0
    // CCW turns: R→U=1_1, D→R=0_1, L→D=0_0, U→L=1_0
    static constexpr size_t INVALID_CORNER = SIZE_MAX;
    static constexpr size_t cornerTileMap[4][4] = {
        //                exit: Right          Down             Left             Up
        /* enter Right */ {INVALID_CORNER, CORNER_CW_1_0,  INVALID_CORNER, CORNER_CCW_1_1},
        /* enter Down  */ {CORNER_CCW_0_1, INVALID_CORNER, CORNER_CW_1_1,  INVALID_CORNER},
        /* enter Left  */ {INVALID_CORNER, CORNER_CCW_0_0, INVALID_CORNER, CORNER_CW_0_1},
        /* enter Up    */ {CORNER_CW_0_0,  INVALID_CORNER, CORNER_CCW_1_0, INVALID_CORNER},
    };

    static constexpr float TOOLBAR_HEIGHT = 48.0f;

    const BuildingDef& getSelectedDef() const
    {
        return registry->get(getSelectedSlot());
    }

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
    bool isMouseOverToolbar() const;

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
    ToolbarSystem* toolbarSystem = nullptr;
    BuildingRegistry* registry = nullptr;
    TransportSystem* transportSystem = nullptr;
    InventoryUISystem* inventoryUI = nullptr;
    MinerUISystem* minerUI = nullptr;
    CraftingUISystem* craftingUI = nullptr;

    size_t currentDirection = 0; // 0=Right, 1=Down, 2=Left, 3=Up
    bool leftMouseDown = false;
    size_t lastSelectedSlot = 0;

    uint64_t cursorEntityId = 0;
    uint64_t ghostEntityId = 0;

    // Line-drag state
    bool isDragging = false;
    std::vector<std::pair<int, int>> dragPath;
    std::vector<uint64_t> dragGhostEntityIds;
};
