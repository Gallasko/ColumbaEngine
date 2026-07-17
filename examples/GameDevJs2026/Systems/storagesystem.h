#pragma once

#include "Systems/basicsystems.h"

#include "gridsystem.h"
#include "machinekey.h"
#include "saveserialization.h"

struct StorageData
{
    int ownerX = 0, ownerY = 0;
    Inventory inventory{8}; // 8 slots
};

class StorageSystem : public pg::System<pg::Listener<BuildingPlacedEvent>,
                                         pg::Listener<BuildingRemovedEvent>,
                                         pg::SaveSys>
{
public:
    explicit StorageSystem(ItemRegistry* itemRegistry)
        : itemRegistry(itemRegistry) {}

    virtual std::string getSystemName() const override { return "Storage System"; }

    // SaveSys
    virtual void save(pg::Archive& archive) override;
    virtual void load(const pg::UnserializedObject& serializedString) override;

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

    ItemRegistry* itemRegistry = nullptr;

    std::unordered_map<uint32_t, StorageData> storages;
};
