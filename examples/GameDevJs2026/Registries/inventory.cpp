#include "inventory.h"

#include <algorithm>

uint16_t Inventory::insert(ItemId id, uint16_t count, const ItemRegistry& reg)
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

uint16_t Inventory::remove(ItemId id, uint16_t count)
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

bool Inventory::hasAtLeast(ItemId id, uint16_t count) const
{
    uint16_t total = 0;
    for (const auto& slot : slots)
    {
        if (slot.id == id) total += slot.count;
        if (total >= count) return true;
    }
    return false;
}

uint16_t Inventory::countItem(ItemId id) const
{
    uint16_t total = 0;
    for (const auto& slot : slots)
        if (slot.id == id) total += slot.count;
    return total;
}

bool Inventory::canAccept(ItemId id, const ItemRegistry& reg) const
{
    uint16_t maxStack = reg.get(id).maxStack;
    for (const auto& slot : slots)
    {
        if (slot.isEmpty()) return true;
        if (slot.id == id and slot.count < maxStack) return true;
    }
    return false;
}

bool Inventory::hasEmptySlot() const
{
    for (const auto& slot : slots)
        if (slot.isEmpty()) return true;
    return false;
}
