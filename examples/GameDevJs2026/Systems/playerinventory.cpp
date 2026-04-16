#include "playerinventory.h"

void PlayerInventorySystem::init()
{
    inventory = Inventory(NUM_SLOTS);
}

void PlayerInventorySystem::onEvent(const PlayerGainItemEvent& event)
{
    inventory.insert(event.id, event.count, *itemRegistry);

    // Seed discovery facts on first pickup so recipes can unlock.
    const auto& def = itemRegistry->get(event.id);
    if (def.name == "Coal")
        sendEvent(AddFact{"discovered_coal", ElementType{true}});
}

void PlayerInventorySystem::onEvent(const PlayerLoseItemEvent& event)
{
    inventory.remove(event.id, event.count);
}
