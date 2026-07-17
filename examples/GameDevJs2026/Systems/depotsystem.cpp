#include "depotsystem.h"
#include "playerinventory.h"
#include "worldfacts.h"

using namespace pg;

void DepotSystem::save(Archive& archive)
{
    serialize(archive, "depots", depots);
    LOG_INFO("DepotSystem", "saved " << depots.size() << " depots");
}

void DepotSystem::load(const UnserializedObject& serializedString)
{
    defaultDeserialize(serializedString, "depots", depots);
    LOG_INFO("DepotSystem", "loaded " << depots.size() << " depots");
}

void DepotSystem::onEvent(const BuildingPlacedEvent& event)
{
    LOG_INFO("DepotSystem", "BuildingPlacedEvent " << event.tileName
            << " at (" << event.x << "," << event.y << ")");
    if (event.tileName == "Depot")
        registerDepot(event.x, event.y);
}

void DepotSystem::onEvent(const BuildingRemovedEvent& event)
{
    LOG_INFO("DepotSystem", "BuildingRemovedEvent " << event.tileName
            << " at (" << event.x << "," << event.y << ")");
    if (event.tileName == "Depot")
        unregisterDepot(event.x, event.y);
}

void DepotSystem::registerDepot(int x, int y)
{
    DepotData data;
    data.ownerX = x;
    data.ownerY = y;

    depots[machineKey(x, y)] = data;

    // First depot ever → unlock mission HUD button
    if (depots.size() == 1)
        sendEvent(pg::AddFact{"depot_placed", pg::ElementType(true)});

    LOG_INFO("DepotSystem", "registered depot at (" << x << ", " << y << ")");
}

void DepotSystem::unregisterDepot(int x, int y)
{
    auto it = depots.find(machineKey(x, y));
    if (it != depots.end())
    {
        auto& depot = it->second;

        // Return all items to player (input + output)
        for (auto& slot : depot.inventory.slots)
        {
            if (not slot.isEmpty())
            {
                sendEvent(PlayerGainItemEvent{slot.id, slot.count});
                slot.clear();
            }
        }
        for (auto& slot : depot.output.slots)
        {
            if (not slot.isEmpty())
            {
                sendEvent(PlayerGainItemEvent{slot.id, slot.count});
                slot.clear();
            }
        }

        depots.erase(it);
        LOG_INFO("DepotSystem", "unregistered depot at (" << x << ", " << y << ")");
    }
}
