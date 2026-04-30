#pragma once

#include "itemregistry.h"

#include <vector>

struct Inventory
{
    std::vector<ItemStack> slots;

    explicit Inventory(size_t numSlots = 0)
        : slots(numSlots) {}

    size_t size() const { return slots.size(); }

    const ItemStack& getSlot(size_t index) const { return slots[index]; }
    ItemStack& getSlot(size_t index) { return slots[index]; }

    // Insert items, stacking into existing slots first then filling empty ones.
    // Returns the number of items that could NOT be inserted (overflow).
    uint16_t insert(ItemId id, uint16_t count, const ItemRegistry& reg);

    // Same as insert, but when filling empty slots, checks the priority range first.
    uint16_t insert(ItemId id, uint16_t count, const ItemRegistry& reg,
                    size_t priorityStart, size_t priorityCount);

    // Remove up to `count` of item `id`. Returns the number actually removed.
    uint16_t remove(ItemId id, uint16_t count);

    bool hasAtLeast(ItemId id, uint16_t count) const;

    uint16_t countItem(ItemId id) const;

    bool canAccept(ItemId id, const ItemRegistry& reg) const;

    bool hasEmptySlot() const;
};
