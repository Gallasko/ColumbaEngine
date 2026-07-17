#pragma once

#include "Systems/basicsystems.h"
#include "Input/sdlevents.h"

#include "gridsystem.h"
#include "buildingregistry.h"

#include <cstdint>

class HotbarSystem;
class CameraSystem;

// When the player has a building selected in the hotbar, tints the grid
// cell(s) under the ghost preview green (placeable) or red (blocked).
// Mirrors the ghost so the player gets immediate feedback on whether the
// next click will succeed.
//
// Implementation: a pool of footprint-sized overlay squares (re-positioned
// each frame to follow the cursor) on the game viewport, hidden when no
// building is selected.
class PlacementOverlaySystem : public pg::System<pg::InitSys,
                                                  pg::QueuedListener<pg::OnSDLMouseMotion>,
                                                  pg::Listener<pg::TickEvent>,
                                                  pg::Listener<BuildingPlacedEvent>,
                                                  pg::Listener<BuildingRemovedEvent>>
{
public:
    static constexpr size_t GAME_VP = 1;
    static constexpr float  Z_OVERLAY = 8.0f;  // above terrain, below ghost (9)

    // Largest building footprint we expect to support. Conveyors / furnaces /
    // assemblers are at most 2x2 today; bumping this up costs almost nothing.
    static constexpr int MAX_FOOTPRINT_SIDE = 4;
    static constexpr int OVERLAY_POOL_SIZE  = MAX_FOOTPRINT_SIDE * MAX_FOOTPRINT_SIDE;

    PlacementOverlaySystem(float screenWidth, float screenHeight)
        : screenWidth(screenWidth), screenHeight(screenHeight) {}

    virtual std::string getSystemName() const override
        { return "Placement Overlay System"; }

    void init() override;

    virtual void onProcessEvent(const pg::OnSDLMouseMotion& event) override;
    virtual void onEvent(const pg::TickEvent& event) override;
    virtual void onEvent(const BuildingPlacedEvent& event) override;
    virtual void onEvent(const BuildingRemovedEvent& event) override;

    void execute() override {}

private:
    bool checkPlacementOK(int gx, int gy, const BuildingDef& def) const;
    void refreshGhostFootprint(const BuildingDef& def, int gx, int gy);
    void hideAll();

    float screenWidth  = 0.0f;
    float screenHeight = 0.0f;

    bool created = false;

    // Mouse cursor screen-space (set from queued OnSDLMouseMotion).
    float cursorX = 0.0f;
    float cursorY = 0.0f;

    const BuildingDef* lastDef = nullptr;
    int lastGhostGX = -1000;
    int lastGhostGY = -1000;
    bool pendingRefresh = true;

    uint64_t overlayIds[OVERLAY_POOL_SIZE] = {};
};
