#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>

using ItemId = uint16_t;
inline constexpr ItemId ITEM_NONE = 0;

enum class ItemCategory : uint8_t
{
    Resource,
    Intermediate,
    Product,
    Building
};

struct ItemDef
{
    ItemId       id              = ITEM_NONE;
    std::string  name;
    std::string  textureName;     // Atlas frame name or "" for color fallback
    ItemCategory category        = ItemCategory::Resource;
    uint16_t     maxStack        = 50;
    uint16_t     buildingTileId  = 0; // Non-zero = placeable building (maps to BuildingDef tileId)
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

    ItemId addItem(const ItemDef& def);

    const ItemDef& get(ItemId id) const { return items[id]; }

    const ItemDef* findByName(const std::string& name) const;

    const ItemDef* findByBuildingTileId(uint16_t tileId) const;

    size_t count() const { return items.size(); }
};

ItemRegistry createDefaultItemRegistry();
