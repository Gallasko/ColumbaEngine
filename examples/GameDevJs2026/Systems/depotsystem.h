#pragma once

#include "Systems/basicsystems.h"

#include "gridsystem.h"
#include "machinekey.h"
#include "saveserialization.h"

struct DepotData
{
    int ownerX = 0, ownerY = 0;
    Inventory inventory{4}; // 4 input slots (Robot Cores, delivery items)
    Inventory output{4};    // 4 output slots (mission rewards)
};

class DepotSystem : public pg::System<pg::Listener<BuildingPlacedEvent>,
                                       pg::Listener<BuildingRemovedEvent>,
                                       pg::SaveSys>
{
public:
    explicit DepotSystem(ItemRegistry* itemRegistry)
        : itemRegistry(itemRegistry) {}

    virtual std::string getSystemName() const override { return "Depot System"; }

    // SaveSys
    virtual void save(pg::Archive& archive) override;
    virtual void load(const pg::UnserializedObject& serializedString) override;

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

    ItemRegistry* itemRegistry = nullptr;

    std::unordered_map<uint32_t, DepotData> depots;
};
