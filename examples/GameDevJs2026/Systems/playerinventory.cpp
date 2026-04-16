#include "playerinventory.h"

#include <cstdio>

void PlayerInventorySystem::save(Archive& archive)
{
    serialize(archive, "slots", inventory.slots);
    printf("PlayerInventory: saved %zu slots\n", inventory.slots.size());
}

void PlayerInventorySystem::load(const UnserializedObject& serializedString)
{
    defaultDeserialize(serializedString, "slots", pendingSlots);
    printf("PlayerInventory: loaded %zu slots\n", pendingSlots.size());
}

void PlayerInventorySystem::init()
{
    inventory = Inventory(NUM_SLOTS);

    // Apply pending save data if we loaded from file
    if (not pendingSlots.empty())
    {
        // Ensure we don't exceed the slot count
        size_t count = std::min(pendingSlots.size(), static_cast<size_t>(NUM_SLOTS));
        for (size_t i = 0; i < count; ++i)
            inventory.slots[i] = pendingSlots[i];
        pendingSlots.clear();
    }
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
