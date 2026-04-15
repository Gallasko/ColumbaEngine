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
    reg.addItem({0, "Iron Ore",    "", ItemCategory::Resource, 50});
    reg.addItem({0, "Copper Ore",  "", ItemCategory::Resource, 50});
    reg.addItem({0, "Coal",        "", ItemCategory::Resource, 50});
    reg.addItem({0, "Stone",       "", ItemCategory::Resource, 50});

    // Intermediates (IDs 5-8)
    reg.addItem({0, "Iron Plate",   "", ItemCategory::Intermediate, 100});
    reg.addItem({0, "Copper Plate", "", ItemCategory::Intermediate, 100});
    reg.addItem({0, "Iron Gear",    "", ItemCategory::Intermediate, 100});
    reg.addItem({0, "Copper Wire",  "", ItemCategory::Intermediate, 100});

    // Products (ID 9)
    reg.addItem({0, "Circuit", "", ItemCategory::Product, 100});

    return reg;
}
