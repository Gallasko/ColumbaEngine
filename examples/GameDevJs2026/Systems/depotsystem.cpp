#include "depotsystem.h"
#include "playerinventory.h"

#include <cstdio>

void DepotSystem::save(Archive& archive)
{
    serialize(archive, "depots", depots);
    printf("DepotSystem: saved %zu depots\n", depots.size());
}

void DepotSystem::load(const UnserializedObject& serializedString)
{
    defaultDeserialize(serializedString, "depots", depots);
    printf("DepotSystem: loaded %zu depots\n", depots.size());
}

void DepotSystem::onEvent(const BuildingPlacedEvent& event)
{
    if (event.tileId == DEPOT_TILE_ID)
        registerDepot(event.x, event.y);
}

void DepotSystem::onEvent(const BuildingRemovedEvent& event)
{
    if (event.tileId == DEPOT_TILE_ID)
        unregisterDepot(event.x, event.y);
}

void DepotSystem::registerDepot(int x, int y)
{
    DepotData data;
    data.ownerX = x;
    data.ownerY = y;

    depots[machineKey(x, y)] = data;
    printf("DepotSystem: registered depot at (%d, %d)\n", x, y);
}

void DepotSystem::unregisterDepot(int x, int y)
{
    auto it = depots.find(machineKey(x, y));
    if (it != depots.end())
    {
        auto& depot = it->second;

        // Return all items to player
        for (auto& slot : depot.inventory.slots)
        {
            if (not slot.isEmpty())
            {
                sendEvent(PlayerGainItemEvent{slot.id, slot.count});
                slot.clear();
            }
        }

        depots.erase(it);
        printf("DepotSystem: unregistered depot at (%d, %d)\n", x, y);
    }
}
