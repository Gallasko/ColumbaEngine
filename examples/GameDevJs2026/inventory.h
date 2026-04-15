#pragma once

#include "itemregistry.h"

#include <vector>
#include <algorithm>

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
    uint16_t insert(ItemId id, uint16_t count, const ItemRegistry& reg)
    {
        if (id == ITEM_NONE or count == 0) return 0;
        uint16_t maxStack = reg.get(id).maxStack;
        uint16_t remaining = count;

        // Phase 1: stack into existing matching slots
        for (auto& slot : slots)
        {
            if (remaining == 0) break;
            if (slot.id == id and slot.count < maxStack)
            {
                uint16_t space = maxStack - slot.count;
                uint16_t toAdd = std::min(remaining, space);
                slot.count += toAdd;
                remaining -= toAdd;
            }
        }

        // Phase 2: fill empty slots
        for (auto& slot : slots)
        {
            if (remaining == 0) break;
            if (slot.isEmpty())
            {
                uint16_t toAdd = std::min(remaining, maxStack);
                slot.id = id;
                slot.count = toAdd;
                remaining -= toAdd;
            }
        }

        return remaining;
    }

    // Remove up to `count` of item `id`. Returns the number actually removed.
    uint16_t remove(ItemId id, uint16_t count)
    {
        uint16_t remaining = count;
        for (auto& slot : slots)
        {
            if (remaining == 0) break;
            if (slot.id == id)
            {
                uint16_t toRemove = std::min(remaining, slot.count);
                slot.count -= toRemove;
                remaining -= toRemove;
                if (slot.count == 0) slot.clear();
            }
        }
        return count - remaining;
    }

    bool hasAtLeast(ItemId id, uint16_t count) const
    {
        uint16_t total = 0;
        for (const auto& slot : slots)
        {
            if (slot.id == id) total += slot.count;
            if (total >= count) return true;
        }
        return false;
    }

    uint16_t countItem(ItemId id) const
    {
        uint16_t total = 0;
        for (const auto& slot : slots)
            if (slot.id == id) total += slot.count;
        return total;
    }

    bool canAccept(ItemId id, const ItemRegistry& reg) const
    {
        uint16_t maxStack = reg.get(id).maxStack;
        for (const auto& slot : slots)
        {
            if (slot.isEmpty()) return true;
            if (slot.id == id and slot.count < maxStack) return true;
        }
        return false;
    }

    bool hasEmptySlot() const
    {
        for (const auto& slot : slots)
            if (slot.isEmpty()) return true;
        return false;
    }
};
