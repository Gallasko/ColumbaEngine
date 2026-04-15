#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>

using ItemId = uint16_t;
static constexpr ItemId ITEM_NONE = 0;

enum class ItemCategory : uint8_t
{
    Resource,
    Intermediate,
    Product,
    Building
};

struct ItemDef
{
    ItemId       id          = ITEM_NONE;
    std::string  name;
    std::string  textureName; // Atlas frame name or "" for color fallback
    ItemCategory category    = ItemCategory::Resource;
    uint16_t     maxStack    = 50;
};

struct ItemStack
{
    ItemId   id    = ITEM_NONE;
    uint16_t count = 0;

    bool isEmpty() const { return id == ITEM_NONE or count == 0; }
    void clear() { id = ITEM_NONE; count = 0; }
};

struct ItemRegistry
{
    std::vector<ItemDef> items;
    std::unordered_map<std::string, ItemId> nameToId;

    ItemId addItem(const ItemDef& def)
    {
        ItemId assignedId = static_cast<ItemId>(items.size());
        ItemDef copy = def;
        copy.id = assignedId;
        nameToId[copy.name] = assignedId;
        items.push_back(copy);
        return assignedId;
    }

    const ItemDef& get(ItemId id) const { return items[id]; }

    const ItemDef* findByName(const std::string& name) const
    {
        auto it = nameToId.find(name);
        if (it != nameToId.end())
            return &items[it->second];
        return nullptr;
    }

    size_t count() const { return items.size(); }
};

inline ItemRegistry createDefaultItemRegistry()
{
    ItemRegistry reg;

    // Index 0 is the null item
    reg.items.push_back({ITEM_NONE, "None", "", ItemCategory::Resource, 0});

    // Raw resources (IDs 1-4)
    reg.addItem({0, "Iron Ore",    "Items.8",  ItemCategory::Resource, 50});
    reg.addItem({0, "Copper Ore",  "Items.9",  ItemCategory::Resource, 50});
    reg.addItem({0, "Coal",        "Items.6",  ItemCategory::Resource, 50});
    reg.addItem({0, "Stone",       "Items.7",  ItemCategory::Resource, 50});

    // Intermediates (IDs 5-8)
    reg.addItem({0, "Iron Plate",   "Items.10", ItemCategory::Intermediate, 100});
    reg.addItem({0, "Copper Plate", "Items.11", ItemCategory::Intermediate, 100});
    reg.addItem({0, "Iron Gear",    "Items.14", ItemCategory::Intermediate, 100});
    reg.addItem({0, "Copper Wire",  "Items.13", ItemCategory::Intermediate, 100});

    // Products (ID 9)
    reg.addItem({0, "Circuit", "Items.17", ItemCategory::Product, 100});

    // Liquids / fuels (IDs 10-14)
    reg.addItem({0, "Water",     "Items.0",  ItemCategory::Resource, 50});
    reg.addItem({0, "Petroleum", "Items.1",  ItemCategory::Resource, 50});
    reg.addItem({0, "Fuel",      "Items.2",  ItemCategory::Resource, 50});
    reg.addItem({0, "Acid",      "Items.3",  ItemCategory::Resource, 50});
    reg.addItem({0, "Biofuel",   "Items.4",  ItemCategory::Resource, 50});

    // Raw resources (IDs 15-17)
    reg.addItem({0, "Wood", "Items.5",  ItemCategory::Resource, 50});
    reg.addItem({0, "Rock", "Items.20", ItemCategory::Resource, 50});
    reg.addItem({0, "Ore",  "Items.21", ItemCategory::Resource, 50});

    // Intermediates (IDs 18-20)
    reg.addItem({0, "Steel",      "Items.12", ItemCategory::Intermediate, 100});
    reg.addItem({0, "Copper Rod", "Items.15", ItemCategory::Intermediate, 100});
    reg.addItem({0, "Iron Rod",   "Items.16", ItemCategory::Intermediate, 100});

    // Products (IDs 21-22)
    reg.addItem({0, "Processor",          "Items.18", ItemCategory::Product, 100});
    reg.addItem({0, "Advanced Processor", "Items.19", ItemCategory::Product, 50});

    // Utility (ID 23)
    reg.addItem({0, "Electricity", "Items.22", ItemCategory::Resource, 0});

    return reg;
}
