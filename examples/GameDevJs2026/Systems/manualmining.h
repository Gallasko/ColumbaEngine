#pragma once

#include "Systems/basicsystems.h"
#include "Systems/tween.h"
#include "Input/inputcomponent.h"

#include "gridsystem.h"
#include "camerasystem.h"
#include "playerinventory.h"
#include "itemregistry.h"
#include "terrain.h"

using namespace pg;

// Event emitted when the player finishes mining a terrain tile by hand.
struct ManualMineCompletedEvent
{
    int gridX, gridY;
    TerrainType terrain;
    ItemId itemGained;
    uint16_t count;
};

class ManualMiningSystem : public System<InitSys,
                                          Listener<TickEvent>,
                                          QueuedListener<OnMouseClick>>
{
public:
    ManualMiningSystem(GridSystem* gridSystem, CameraSystem* cameraSystem,
                       PlayerInventorySystem* playerInv, ItemRegistry* itemRegistry,
                       float screenWidth, float screenHeight)
        : gridSystem(gridSystem), cameraSystem(cameraSystem),
          playerInv(playerInv), itemRegistry(itemRegistry),
          screenWidth(screenWidth), screenHeight(screenHeight) {}

    virtual std::string getSystemName() const override { return "Manual Mining System"; }

    void init() override;
    void execute() override;

    virtual void onEvent(const TickEvent& event) override;
    virtual void onProcessEvent(const OnMouseClick& event) override;

    // Called by GameSystem to enable/disable mining (disabled when a building is selected)
    void setEnabled(bool enabled) { miningEnabled = enabled; }

    // Set the hotbar height so we can ignore clicks on the hotbar
    void setHotbarHeight(float h) { hotbarHeight = h; }

private:
    static constexpr size_t DECAY_TIMEOUT_MS = 2000;
    static constexpr float BAR_WIDTH = 20.0f;
    static constexpr float BAR_HEIGHT = 3.0f;
    static constexpr float BAR_OUTLINE = 1.0f;
    static constexpr float BAR_OFFSET_Y = -5.0f;
    static constexpr float BAR_FADE_TOTAL_MS = 1000.0f;

    // Progress bar
    void createProgressBar();
    void updateProgressBar();
    void startBarFadeOut();
    void cancelBarFade();
    void hideProgressBar();

    // Ghost float animation
    void spawnGhostAnimation(float worldX, float worldY, ItemId itemId);
    void tickGhostAnimations(size_t deltaMs);

    struct GhostAnim
    {
        uint64_t entityId;
        float startY;
        float endY;
        float elapsed = 0.0f;
        static constexpr float DURATION = 800.0f;
    };
    std::vector<GhostAnim> activeGhosts;

    // Mining state
    int targetGridX = -1;
    int targetGridY = -1;
    int currentHits = 0;
    int requiredHits = 0;
    size_t decayTimer = 0;

    // Progress bar entities
    uint64_t progressOutlineEntityId = 0;
    uint64_t progressBgEntityId = 0;
    uint64_t progressFillEntityId = 0;
    uint64_t fadeTweenEntityId = 0;

    // Dependencies
    GridSystem* gridSystem = nullptr;
    CameraSystem* cameraSystem = nullptr;
    PlayerInventorySystem* playerInv = nullptr;
    ItemRegistry* itemRegistry = nullptr;
    float screenWidth = 0.0f;
    float screenHeight = 0.0f;
    float hotbarHeight = 48.0f;

    bool miningEnabled = true;
    size_t tickAccumulator = 0;
    size_t frameDelta = 0; // Raw delta for ghost animations
};
