#include "playerinventory.h"

#include <cstdio>

void PlayerInventorySystem::save(Archive& archive)
{
    serialize(archive, "slots", inventory.slots);
    printf("PlayerInventory: saved %zu slots\n", inventory.slots.size());
}

void PlayerInventorySystem::load(const UnserializedObject& serializedString)
{
    std::vector<ItemStack> slots;
    defaultDeserialize(serializedString, "slots", slots);
    printf("PlayerInventory: loaded %zu slots\n", slots.size());

    // init() already ran and created the 29-slot inventory — fill directly
    // (old saves with fewer slots are handled by the min below)
    size_t count = std::min(slots.size(), static_cast<size_t>(NUM_SLOTS));
    for (size_t i = 0; i < count; ++i)
        inventory.slots[i] = slots[i];
}

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
