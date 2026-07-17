#pragma once

#include "ECS/component.h"
#include "Registries/itemregistry.h"

#include <cstdint>
#include <functional>

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
    NoPickUp   = 1 << 2,
};

inline SlotFlags operator|(SlotFlags a, SlotFlags b)
{
    return static_cast<SlotFlags>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
}

inline bool operator&(SlotFlags a, SlotFlags b)
{
    return (static_cast<uint8_t>(a) & static_cast<uint8_t>(b)) != 0;
}

struct SlotComponent : public pg::Component
{
    SlotComponent() = default;

    explicit SlotComponent(SlotCategory category, uint8_t slotIndex = 0, SlotFlags flags = SlotFlags::None)
        : category(category), slotIndex(slotIndex), flags(flags) {}

    // --- Item Data (single source of truth) ---
    ItemStack stack;

    // --- Slot Identity ---
    SlotCategory category = SlotCategory::PlayerInventory;
    uint8_t      slotIndex = 0;

    // --- Behavior Flags ---
    SlotFlags flags = SlotFlags::None;

    // Write-back hook. When set, SlotSystem invokes it after every mutation to
    // `stack` from pickUpFrom/dropOn/cancelHeld so the gameplay-side backing store
    // stays in sync without each consumer wiring SlotPickedUpEvent/SlotDroppedEvent.
    std::function<void(const ItemStack&)> onChange;

    // --- Helpers ---
    bool isEmpty() const { return stack.isEmpty(); }
    bool isReadOnly() const { return flags & SlotFlags::ReadOnly; }
    bool isOutputOnly() const { return flags & SlotFlags::OutputOnly; }
    bool isNoPickUp() const { return flags & SlotFlags::NoPickUp; }

    void setFlag(SlotFlags f) { flags = flags | f; }
    void clearFlag(SlotFlags f) { flags = static_cast<SlotFlags>(static_cast<uint8_t>(flags) & ~static_cast<uint8_t>(f)); }
};
