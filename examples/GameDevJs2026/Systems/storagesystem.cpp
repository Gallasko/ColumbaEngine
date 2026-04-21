#include "storagesystem.h"
#include "playerinventory.h"

#include <cstdio>

void StorageSystem::save(Archive& archive)
{
    serialize(archive, "storages", storages);
    printf("StorageSystem: saved %zu storages\n", storages.size());
}

void StorageSystem::load(const UnserializedObject& serializedString)
{
    defaultDeserialize(serializedString, "storages", storages);
    printf("StorageSystem: loaded %zu storages\n", storages.size());
}

void StorageSystem::onEvent(const BuildingPlacedEvent& event)
{
    if (event.tileName == "Storage")
        registerStorage(event.x, event.y);
}

void StorageSystem::onEvent(const BuildingRemovedEvent& event)
{
    if (event.tileName == "Storage")
        unregisterStorage(event.x, event.y);
}

void StorageSystem::registerStorage(int x, int y)
{
    StorageData data;
    data.ownerX = x;
    data.ownerY = y;

    storages[machineKey(x, y)] = data;
}

void StorageSystem::unregisterStorage(int x, int y)
{
    auto it = storages.find(machineKey(x, y));
    if (it != storages.end())
    {
        auto& storage = it->second;

        // Return all items to player
        for (auto& slot : storage.inventory.slots)
        {
            if (not slot.isEmpty())
            {
                sendEvent(PlayerGainItemEvent{slot.id, slot.count});
                slot.clear();
            }
        }

        storages.erase(it);
    }
}
