#pragma once

#include <cstdint>
#include <string>

#include "Helpers/registry.h"

using ItemId = uint16_t;
inline constexpr ItemId ITEM_NONE = 0;

enum class ItemCategory : uint8_t
{
    Resource,
    Intermediate,
    Product,
    Building
};

// Todo maybe cache the building id when loading the registry
struct ItemDef
{
    ItemId       id              = ITEM_NONE;
    std::string  name;
    std::string  textureName;     // Atlas frame name or "" for color fallback
    ItemCategory category        = ItemCategory::Resource;
    uint16_t     maxStack        = 50;
    std::string  buildingName;        // Non-empty = placeable building (maps to BuildingDef name)
    uint8_t      toolTier        = 0;   // 0=not a tool, 1=stone, 2=iron, etc.
    float        miningSpeedMult = 1.0f; // Multiplier: 2.0 = halves required hits
    float        iconWidthRatio  = 1.0f; // Width:height ratio for icon display (e.g. 2/3 for furnace)
    std::string  description;            // Short tooltip description (empty = no description shown)
    uint8_t      worldSourceTier = 255;  // Tier of tool needed to mine from world:
                                         //   0=bare hands, 1=stone pickaxe, 2=iron pickaxe,
                                         //   255=not obtainable from world
};

struct ItemStack
{
    ItemId   id    = ITEM_NONE;
    uint16_t count = 0;

    bool isEmpty() const { return id == ITEM_NONE or count == 0; }
    void clear() { id = ITEM_NONE; count = 0; }
};

struct ItemRegistry : public pg::Registry<ItemDef, ItemId>
{
    ItemId addItem(const ItemDef& def)
    {
        ItemId assignedId = add(def);

        // Todo is the .id here really needed
        m_entries[static_cast<size_t>(assignedId)].id = assignedId;

        return assignedId;
    }

    const ItemDef* findByBuildingName(const std::string& name) const
    {
        for (const auto& entry : m_entries)
        {
            if (entry.buildingName == name)
                return &entry;
        }

        return nullptr;
    }
};

ItemRegistry createDefaultItemRegistry();
