#pragma once

#include "Systems/basicsystems.h"

#include "gridsystem.h"
#include "machinekey.h"
#include "saveserialization.h"

using namespace pg;

struct StorageData
{
    int ownerX = 0, ownerY = 0;
    Inventory inventory{8}; // 8 slots
};

class StorageSystem : public System<Listener<BuildingPlacedEvent>,
                                     Listener<BuildingRemovedEvent>,
                                     SaveSys>
{
public:
    StorageSystem(GridSystem* gridSystem, ItemRegistry* itemRegistry)
        : gridSystem(gridSystem), itemRegistry(itemRegistry) {}

    virtual std::string getSystemName() const override { return "Storage System"; }

    // SaveSys
    virtual void save(Archive& archive) override;
    virtual void load(const UnserializedObject& serializedString) override;

    virtual void onEvent(const BuildingPlacedEvent& event) override;
    virtual void onEvent(const BuildingRemovedEvent& event) override;

    StorageData* getStorage(int x, int y)
    {
        auto it = storages.find(machineKey(x, y));
        return it != storages.end() ? &it->second : nullptr;
    }

private:
    void registerStorage(int x, int y);
    void unregisterStorage(int x, int y);

    GridSystem* gridSystem = nullptr;
    ItemRegistry* itemRegistry = nullptr;

    std::unordered_map<uint32_t, StorageData> storages;
};
