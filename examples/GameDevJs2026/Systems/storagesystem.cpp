#include "storagesystem.h"
#include "playerinventory.h"

void StorageSystem::save(Archive& archive)
{
    serialize(archive, "storages", storages);
    LOG_INFO("StorageSystem", "saved " << storages.size() << " storages");
}

void StorageSystem::load(const UnserializedObject& serializedString)
{
    defaultDeserialize(serializedString, "storages", storages);
    LOG_INFO("StorageSystem", "loaded " << storages.size() << " storages");
}

void StorageSystem::onEvent(const BuildingPlacedEvent& event)
{
    LOG_INFO("StorageSystem", "BuildingPlacedEvent " << event.tileName
            << " at (" << event.x << "," << event.y << ")");
    if (event.tileName == "Storage")
        registerStorage(event.x, event.y);
}

void StorageSystem::onEvent(const BuildingRemovedEvent& event)
{
    LOG_INFO("StorageSystem", "BuildingRemovedEvent " << event.tileName
            << " at (" << event.x << "," << event.y << ")");
    if (event.tileName == "Storage")
        unregisterStorage(event.x, event.y);
}

void StorageSystem::registerStorage(int x, int y)
{
    StorageData data;
    data.ownerX = x;
    data.ownerY = y;

    LOG_INFO("StorageSystem", "registered storage at (" << x << "," << y << ")");

    storages[machineKey(x, y)] = data;
}

void StorageSystem::unregisterStorage(int x, int y)
{
    auto it = storages.find(machineKey(x, y));
    if (it != storages.end())
    {
        auto& storage = it->second;
        LOG_INFO("StorageSystem", "unregistering storage at (" << x << "," << y
                << ") — returning items to player");

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
    else
    {
        LOG_INFO("StorageSystem", "unregisterStorage at (" << x << "," << y << ") — not found");
    }
}
