#pragma once

#include "Systems/basicsystems.h"
#include "Systems/tween.h"
#include "Input/inputcomponent.h"

#include "gridsystem.h"
#include "camerasystem.h"
#include "playerinventory.h"
#include "itemregistry.h"
#include "inventoryui.h"
#include "hotbarsystem.h"
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
                                          Listener<InventoryOpenedEvent>,
                                          Listener<InventoryClosedEvent>,
                                          QueuedListener<OnMouseClick>>
{
public:
    ManualMiningSystem(GridSystem* gridSystem, CameraSystem* cameraSystem,
                       PlayerInventorySystem* playerInv, ItemRegistry* itemRegistry,
                       HotbarSystem* hotbar,
                       float screenWidth, float screenHeight)
        : gridSystem(gridSystem), cameraSystem(cameraSystem),
          playerInv(playerInv), itemRegistry(itemRegistry), hotbar(hotbar),
          screenWidth(screenWidth), screenHeight(screenHeight) {}

    virtual std::string getSystemName() const override { return "Manual Mining System"; }

    void init() override;
    void execute() override;

    virtual void onEvent(const TickEvent& event) override;
    virtual void onEvent(const InventoryOpenedEvent&) override;
    virtual void onEvent(const InventoryClosedEvent&) override;
    virtual void onProcessEvent(const OnMouseClick& event) override;

    // Called by GameSystem to enable/disable mining (disabled when a building is selected)
    void setEnabled(bool enabled) { miningEnabled = enabled; }

    // Set the hotbar height so we can ignore clicks on the hotbar
    void setHotbarHeight(float h) { hotbarHeight = h; }

private:
    static constexpr size_t DECAY_TIMEOUT_MS = 2000;  // Progress resets after 2s
    static constexpr size_t BAR_IDLE_MS = 500;         // Bar starts fading after 500ms
    static constexpr float BAR_FADE_DURATION_MS = 500.0f;
    static constexpr float BAR_WIDTH = 20.0f;
    static constexpr float BAR_HEIGHT = 3.0f;
    static constexpr float BAR_OUTLINE = 1.0f;
    static constexpr float BAR_OFFSET_Y = -5.0f;

    // Progress bar
    void createProgressBar();
    void updateProgressBar();
    void startBarFadeOut();
    void cancelBarFade();
    void hideProgressBar();

    // Tool queries
    uint8_t getEquippedToolTier() const;
    float getEquippedMiningSpeed() const;

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
    size_t barIdleTimer = 0;
    bool barFadeStarted = false;

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
    HotbarSystem* hotbar = nullptr;
    float screenWidth = 0.0f;
    float screenHeight = 0.0f;
    float hotbarHeight = 48.0f;

    bool miningEnabled = true;
    bool uiOpen = false;
    size_t tickAccumulator = 0;
    size_t frameDelta = 0; // Raw delta for ghost animations
};
