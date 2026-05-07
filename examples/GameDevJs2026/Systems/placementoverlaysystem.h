#pragma once

#include "Systems/basicsystems.h"

#include "gridsystem.h"
#include "buildingregistry.h"

#include <cstdint>

using namespace pg;

class HotbarSystem;

// When the player has a building selected in the hotbar, tints every grid
// cell green (placeable) or red (blocked). Provides immediate visual feedback
// for where the building can go before the player commits a click.
//
// Implementation: a pool of WIDTH*HEIGHT square overlays on the game viewport
// at z just above the terrain, hidden by default. On selection change or
// after any BuildingPlaced/BuildingRemoved event the overlays are recoloured.
class PlacementOverlaySystem : public System<InitSys,
                                              Listener<TickEvent>,
                                              Listener<BuildingPlacedEvent>,
                                              Listener<BuildingRemovedEvent>>
{
public:
    static constexpr size_t GAME_VP = 1;
    static constexpr float  Z_OVERLAY = 8.0f;  // above terrain, below ghost (9)

    PlacementOverlaySystem(GridSystem* gridSystem, HotbarSystem* hotbar)
        : gridSystem(gridSystem), hotbar(hotbar) {}

    virtual std::string getSystemName() const override
        { return "Placement Overlay System"; }

    void init() override;

    virtual void onEvent(const TickEvent& event) override;
    virtual void onEvent(const BuildingPlacedEvent& event) override;
    virtual void onEvent(const BuildingRemovedEvent& event) override;

    void execute() override {}

private:
    bool checkPlacementOK(int gx, int gy, const BuildingDef& def) const;
    void refreshAll(const BuildingDef* def);
    void hideAll();

    GridSystem*   gridSystem = nullptr;
    HotbarSystem* hotbar     = nullptr;

    bool created = false;

    const BuildingDef* lastDef = nullptr;
    bool pendingRefresh = true;

    uint64_t overlayIds[Grid::WIDTH * Grid::HEIGHT] = {};
};
