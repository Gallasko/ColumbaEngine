#pragma once

#include "Systems/basicsystems.h"
#include "Input/inputcomponent.h"

#include "demoscenarios.h"
#include "buildingregistry.h"
#include "itemregistry.h"
#include "gridsystem.h"

using namespace pg;

// ─────────────────────────────────────────────────────────────────────────────
// Internal simulation state
// ─────────────────────────────────────────────────────────────────────────────

struct DemoSimItem
{
    float x, y;              // Pixel position within demo panel (relative)
    uint16_t itemId;
    uint64_t entityId = 0;
    bool active = false;
};

enum class DemoInserterState : uint8_t { Idle, Swinging, Returning };

struct DemoSimInserter
{
    int gridX, gridY;
    uint8_t direction;
    DemoInserterState state = DemoInserterState::Idle;
    size_t animFrame = 0;
    int heldItemIndex = -1;  // Index into items array, -1 = empty
    uint64_t entityId = 0;
};

struct DemoSimBelt
{
    int gridX, gridY;
    uint8_t direction;
    int carriedItemIndex = -1;
    uint64_t entityId = 0;
};

struct DemoSimMachine
{
    int gridX, gridY;
    std::string tileName;
    int inputItemIndex = -1;
    int outputItemIndex = -1;
    int processTimer = 0;
    uint64_t entityId = 0;
};

// ─────────────────────────────────────────────────────────────────────────────
// MachineDemoSystem
// ─────────────────────────────────────────────────────────────────────────────

class MachineDemoSystem : public System<Listener<TickEvent>,
                                         QueuedListener<OnMouseClick>,
                                         Listener<OnSDLScanCode>>
{
public:
    static constexpr size_t UI_VP = 2;
    static constexpr float DEMO_TILE_SIZE = 32.0f;  // 2x scale
    static constexpr float PADDING = 16.0f;
    static constexpr float TITLE_HEIGHT = 40.0f;
    static constexpr float DESC_HEIGHT = 20.0f;
    static constexpr float CLOSE_BTN_SIZE = 24.0f;
    static constexpr float TITLE_SCALE = 0.45f;
    static constexpr float DESC_SCALE = 0.30f;
    static constexpr size_t TICK_INTERVAL_MS = 100;
    static constexpr size_t MAX_ITEMS = 16;
    static constexpr size_t MAX_INSERTERS = 4;
    static constexpr size_t MAX_BELTS = 12;
    static constexpr size_t MAX_MACHINES = 4;

    static constexpr size_t INSERTER_SWING_FRAMES = 5;
    static constexpr size_t INSERTER_TOTAL_FRAMES = 8;
    static constexpr size_t INSERTER_PICKUP_FRAME[4] = {0, 2, 4, 6};

    static constexpr const char* FONT_PATH =
        "res/font/Inter/static/Inter_28pt-Light.ttf";

    MachineDemoSystem(BuildingRegistry* buildingRegistry,
                      ItemRegistry* itemRegistry,
                      float screenW, float screenH)
        : buildingRegistry(buildingRegistry),
          itemRegistry(itemRegistry),
          screenW(screenW), screenH(screenH) {}

    virtual std::string getSystemName() const override { return "Machine Demo System"; }

    void onEvent(const TickEvent& event) override;
    void onEvent(const OnSDLScanCode& event) override;
    void onProcessEvent(const OnMouseClick& event) override;

    void execute() override;

    bool isOpen() const { return open; }
    bool hasDemoForTile(const std::string& tileName) const;
    void openDemo(const std::string& tileName);
    void closeDemo();

private:
    // Panel
    void createPanel();
    void destroyPanel();

    // Simulation
    void initSimulation(const DemoScenario& scenario);
    void simulationTick();
    void tickBelts();
    void tickInserters();
    void tickMachines();
    void tickSpawns();
    void resetSimulation();

    // Rendering
    void updateRendering();
    void updateInserterSprite(DemoSimInserter& ins);
    void updateItemPosition(DemoSimItem& item);

    // Helpers
    float panelX() const;
    float panelY() const;
    float panelW() const;
    float panelH() const;
    float gridOriginX() const;
    float gridOriginY() const;
    int findBeltAt(int x, int y) const;
    int findMachineAt(int x, int y) const;
    int allocateItem(uint16_t itemId, float x, float y);
    void freeItem(int index);
    bool isClickOnCloseBtn(float x, float y) const;

    static size_t getInserterSpriteFrame(uint8_t direction, size_t animFrame, bool returning);

    // ─── Members ─────────────────────────────────────────────────────────────

    BuildingRegistry* buildingRegistry = nullptr;
    ItemRegistry* itemRegistry = nullptr;
    float screenW, screenH;

    bool open = false;
    bool panelCreated = false;
    bool pendingOpen = false;
    bool pendingClose = false;
    std::string pendingTileName;
    size_t tickAccumulator = 0;
    size_t spawnTickCounter = 0;

    // Current scenario
    DemoScenario currentScenario;

    // Simulation state
    DemoSimItem items[MAX_ITEMS] = {};
    DemoSimInserter inserters[MAX_INSERTERS] = {};
    size_t inserterCount = 0;
    DemoSimBelt belts[MAX_BELTS] = {};
    size_t beltCount = 0;
    size_t beltAnimFrame = 0;
    DemoSimMachine machines[MAX_MACHINES] = {};
    size_t machineCount = 0;

    // Panel entities
    uint64_t backdropId = 0;
    uint64_t titleId = 0;
    uint64_t descId = 0;
    uint64_t closeBtnId = 0;
    uint64_t closeBtnTextId = 0;

};
