#pragma once

#include "Systems/basicsystems.h"

#include "gridsystem.h"
#include "machinekey.h"
#include "saveserialization.h"

using namespace pg;

struct DepotData
{
    int ownerX = 0, ownerY = 0;
    Inventory inventory{4}; // 4 slots for Robot Cores in / rewards out
};

class DepotSystem : public System<Listener<BuildingPlacedEvent>,
                                   Listener<BuildingRemovedEvent>,
                                   SaveSys>
{
public:
    static constexpr uint16_t DEPOT_TILE_ID = 10;

    DepotSystem(GridSystem* gridSystem, ItemRegistry* itemRegistry)
        : gridSystem(gridSystem), itemRegistry(itemRegistry) {}

    virtual std::string getSystemName() const override { return "Depot System"; }

    // SaveSys
    virtual void save(Archive& archive) override;
    virtual void load(const UnserializedObject& serializedString) override;

    virtual void onEvent(const BuildingPlacedEvent& event) override;
    virtual void onEvent(const BuildingRemovedEvent& event) override;

    DepotData* getDepot(int x, int y)
    {
        auto it = depots.find(machineKey(x, y));
        return it != depots.end() ? &it->second : nullptr;
    }

    const std::unordered_map<uint32_t, DepotData>& getAllDepots() const { return depots; }

    bool hasAnyDepot() const { return !depots.empty(); }

private:
    void registerDepot(int x, int y);
    void unregisterDepot(int x, int y);

    GridSystem* gridSystem = nullptr;
    ItemRegistry* itemRegistry = nullptr;

    std::unordered_map<uint32_t, DepotData> depots;
};
