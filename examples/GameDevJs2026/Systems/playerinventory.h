#pragma once

#include "Systems/basicsystems.h"
#include "UI/gamedataview.h"
#include "inventory.h"
#include "worldfacts.h"
#include "saveserialization.h"

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

class PlayerInventorySystem : public pg::System<pg::InitSys,
                                                pg::Listener<PlayerGainItemEvent>,
                                                pg::Listener<PlayerLoseItemEvent>,
                                                pg::SaveSys>
{
public:
    static constexpr size_t NUM_SLOTS = 29;
    static constexpr size_t MAIN_SLOTS = 20;
    static constexpr size_t HOTBAR_START = 20;
    static constexpr size_t HOTBAR_COUNT = 9;
    static constexpr ItemId TICKET_ID = 35;

    explicit PlayerInventorySystem(ItemRegistry* itemRegistry)
        : itemRegistry(itemRegistry) {}

    virtual std::string getSystemName() const override { return "Player Inventory System"; }

    void init() override;

    // SaveSys
    virtual void save(pg::Archive& archive) override;
    virtual void load(const pg::UnserializedObject& serializedString) override;

    virtual void onEvent(const PlayerGainItemEvent& event) override;

    virtual void onEvent(const PlayerLoseItemEvent& event) override;

    const Inventory& getInventory() const { return inventory; }
    Inventory& getInventory() { return inventory; }

    bool hasItem(ItemId id, uint16_t count = 1) const
    {
        if (id == TICKET_ID)
            return ticketCount >= count;
        return inventory.hasAtLeast(id, count);
    }

    uint32_t getTickets() const { return ticketCount; }
    void addTickets(uint32_t amount)
    {
        ticketCount += amount;
        publishTickets();
    }
    bool spendTickets(uint32_t amount)
    {
        if (ticketCount < amount)
            return false;
        ticketCount -= amount;
        publishTickets();
        return true;
    }

    static constexpr const char* TICKETS_PATH = "player.tickets";

private:
    void publishTickets()
    {
        if (auto* view = ecsRef ? ecsRef->getSystem<pg::GameDataView>() : nullptr)
            view->set(TICKETS_PATH, static_cast<size_t>(ticketCount));
    }

    ItemRegistry* itemRegistry = nullptr;
    Inventory inventory;
    uint32_t ticketCount = 0;
};
