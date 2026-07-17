#pragma once

#include "Systems/basicsystems.h"
#include "Input/inputcomponent.h"

#include "gridsystem.h"
#include "2D/camerasystem.h"
#include "hotbarsystem.h"
#include "buildingregistry.h"
#include "transportsystem.h"
#include "inventoryui.h"
#include "craftingui.h"
#include "manualmining.h"
#include "machinedemosystem.h"
#include "missionui.h"
#include "worldfacts.h"
#include "machineuicoordinator.h"

// UI exclusivity groups. The InventoryGroup (inventory + crafting + miner +
// machine + storage + depot) panels are designed to coexist side-by-side; the
// Mission panel is a fullscreen modal that must not overlap with them.
enum class UIPanel { None, InventoryGroup, Mission };

class GameSystem : public pg::System<pg::InitSys, pg::QueuedListener<pg::OnMouseClick>, pg::QueuedListener<pg::OnMouseRelease>, pg::QueuedListener<pg::OnSDLScanCode>, pg::QueuedListener<pg::OnSDLMouseMotion>, pg::Listener<PanelWasClickedEvent>>
{
public:
    GameSystem(BuildingRegistry* registry, ItemRegistry* itemRegistry)
        : registry(registry), itemRegistry(itemRegistry) {}

    virtual std::string getSystemName() const override { return "Game System"; }

    void init() override;

    virtual void onProcessEvent(const pg::OnSDLScanCode& event) override;
    virtual void onProcessEvent(const pg::OnMouseClick& event) override;
    virtual void onProcessEvent(const pg::OnMouseRelease& event) override;
    virtual void onProcessEvent(const pg::OnSDLMouseMotion& event) override;

    virtual void onEvent(const PanelWasClickedEvent&) override
    {
        panelClickedThisFrame = true;
    }

    // Closes every UI panel that does not belong to `keep`. Call before
    // opening a panel from a new group to enforce mutual exclusion.
    void closeOtherGroup(UIPanel keep);

    // HUD button entry points. These wrap closeOtherGroup + open/toggle so
    // HudBarSystem doesn't have to know about every UI.
    void toggleInventoryFromHud();
    void toggleMissionFromHud();

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
        auto* hotbar = ecsRef->getSystem<HotbarSystem>();
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

    BuildingRegistry* registry = nullptr;
    ItemRegistry* itemRegistry = nullptr;

    bool panelClickedThisFrame = false;
    size_t currentDirection = 0; // 0=Right, 1=Down, 2=Left, 3=Up
    bool leftMouseDown = false;
    const BuildingDef* lastBuildingDef = nullptr; // Track for ghost rebuild

    // Hold the EntityRef alongside the id so destruction works even when the
    // entity is still pending in cmdDispatcher (id-based lookup misses it →
    // orphaned entity that gets created later with no one tracking it).
    pg::EntityRef cursorEntity;
    uint64_t  cursorEntityId = 0;
    pg::EntityRef ghostEntity;
    uint64_t  ghostEntityId = 0;

    // Line-drag state
    bool isDragging = false;
    std::vector<std::pair<int, int>> dragPath;
    std::vector<pg::EntityRef> dragGhostEntities;
};
