#pragma once

#include "ECS/component.h"
#include "Registries/itemregistry.h"

#include <cstdint>

using namespace pg;

enum class SlotCategory : uint8_t
{
    PlayerInventory,
    Hotbar,
    Input,
    Output,
};

enum class SlotFlags : uint8_t
{
    None       = 0,
    ReadOnly   = 1 << 0,
    OutputOnly = 1 << 1,
};

inline SlotFlags operator|(SlotFlags a, SlotFlags b)
{
    return static_cast<SlotFlags>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
}

inline bool operator&(SlotFlags a, SlotFlags b)
{
    return (static_cast<uint8_t>(a) & static_cast<uint8_t>(b)) != 0;
}

struct SlotComponent : public Component
{
    SlotComponent() = default;

    SlotComponent(SlotCategory category, uint8_t slotIndex = 0, SlotFlags flags = SlotFlags::None)
        : category(category), slotIndex(slotIndex), flags(flags) {}

    // --- Item Data (single source of truth) ---
    ItemStack stack;

    // --- Slot Identity ---
    SlotCategory category = SlotCategory::PlayerInventory;
    uint8_t      slotIndex = 0;

    // --- Behavior Flags ---
    SlotFlags flags = SlotFlags::None;

    // --- Helpers ---
    bool isEmpty() const { return stack.isEmpty(); }
    bool isReadOnly() const { return flags & SlotFlags::ReadOnly; }
    bool isOutputOnly() const { return flags & SlotFlags::OutputOnly; }
};
