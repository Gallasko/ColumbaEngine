#include "playerinventory.h"

using namespace pg;

void PlayerInventorySystem::save(Archive& archive)
{
    serialize(archive, "slots", inventory.slots);
    serialize(archive, "ticketCount", ticketCount);
    LOG_INFO("PlayerInventory", "saved " << inventory.slots.size() << " slots, " << ticketCount << " tickets");
}

void PlayerInventorySystem::load(const UnserializedObject& serializedString)
{
    std::vector<ItemStack> slots;
    defaultDeserialize(serializedString, "slots", slots);
    defaultDeserialize(serializedString, "ticketCount", ticketCount);

    // init() already ran and created the 29-slot inventory — fill directly
    // (old saves with fewer slots are handled by the min below)
    size_t count = std::min(slots.size(), static_cast<size_t>(NUM_SLOTS));
    for (size_t i = 0; i < count; ++i)
        inventory.slots[i] = slots[i];

    // Migration: move any tickets from slots to the currency counter
    for (auto& slot : inventory.slots)
    {
        if (slot.id == TICKET_ID)
        {
            ticketCount += slot.count;
            slot.clear();
        }
    }

    LOG_INFO("PlayerInventory", "loaded " << slots.size() << " slots, " << ticketCount << " tickets");

    publishTickets();
}

void PlayerInventorySystem::init()
{
    LOG_INFO("PlayerInventory", "init() — creating inventory with " << NUM_SLOTS << " slots");
    inventory = Inventory(NUM_SLOTS);

    publishTickets();
}

void PlayerInventorySystem::onEvent(const PlayerGainItemEvent& event)
{
    LOG_INFO("PlayerInventory", "PlayerGainItemEvent id=" << event.id << " count=" << event.count);

    // Tickets bypass the inventory — stored as a currency counter
    if (event.id == TICKET_ID)
    {
        ticketCount += event.count;
        LOG_INFO("PlayerInventory", "tickets +=" << event.count << " — total=" << ticketCount);
        publishTickets();
        sendEvent(AddFact{"discovered_ticket", ElementType{true}});
        return;
    }

    // Route to main inventory first (slots 0-19), then overflow to hotbar.
    // Keeps the hotbar reserved for things the player explicitly equips, and
    // makes mined/crafted items visible in the inventory grid where players
    // expect them.
    inventory.insert(event.id, event.count, *itemRegistry);

    // Fire a discovered_<name> fact on every item pickup so recipes,
    // quests and tutorial steps can gate on any item without hardcoding.
    if (event.id != ITEM_NONE)
    {
        const auto& def = itemRegistry->get(event.id);
        std::string factName = "discovered_";
        for (char c : def.name)
        {
            if (c == ' ')
                factName.push_back('_');
            else if (c >= 'A' && c <= 'Z')
                factName.push_back(static_cast<char>(c - 'A' + 'a'));
            else
                factName.push_back(c);
        }
        sendEvent(AddFact{factName, ElementType{true}});
    }
}

void PlayerInventorySystem::onEvent(const PlayerLoseItemEvent& event)
{
    LOG_INFO("PlayerInventory", "PlayerLoseItemEvent id=" << event.id << " count=" << event.count);

    if (event.id == TICKET_ID)
    {
        if (ticketCount >= event.count)
            ticketCount -= event.count;
        else
            ticketCount = 0;
        LOG_INFO("PlayerInventory", "tickets after lose: " << ticketCount);
        publishTickets();
        return;
    }
    inventory.remove(event.id, event.count);
}
