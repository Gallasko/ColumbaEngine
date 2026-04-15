#pragma once

#include "Systems/basicsystems.h"
#include "inventory.h"

using namespace pg;

struct PlayerGainItemEvent
{
    ItemId   id;
    uint16_t count;
};

struct PlayerLoseItemEvent
{
    ItemId   id;
    uint16_t count;
};

class PlayerInventorySystem : public System<InitSys,
                                            Listener<PlayerGainItemEvent>,
                                            Listener<PlayerLoseItemEvent>>
{
public:
    static constexpr size_t NUM_SLOTS = 20;

    PlayerInventorySystem(ItemRegistry* itemRegistry)
        : itemRegistry(itemRegistry) {}

    virtual std::string getSystemName() const override { return "Player Inventory System"; }

    void init() override
    {
        inventory = Inventory(NUM_SLOTS);
    }

    virtual void onEvent(const PlayerGainItemEvent& event) override
    {
        inventory.insert(event.id, event.count, *itemRegistry);
    }

    virtual void onEvent(const PlayerLoseItemEvent& event) override
    {
        inventory.remove(event.id, event.count);
    }

    const Inventory& getInventory() const { return inventory; }
    Inventory& getInventory() { return inventory; }

    bool hasItem(ItemId id, uint16_t count = 1) const
    {
        return inventory.hasAtLeast(id, count);
    }

private:
    ItemRegistry* itemRegistry = nullptr;
    Inventory inventory;
};
