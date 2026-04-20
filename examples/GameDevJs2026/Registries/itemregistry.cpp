#include "itemregistry.h"

ItemId ItemRegistry::addItem(const ItemDef& def)
{
    ItemId assignedId = static_cast<ItemId>(items.size());
    ItemDef copy = def;
    copy.id = assignedId;
    nameToId[copy.name] = assignedId;
    items.push_back(copy);
    return assignedId;
}

const ItemDef* ItemRegistry::findByName(const std::string& name) const
{
    auto it = nameToId.find(name);
    if (it != nameToId.end())
        return &items[it->second];
    return nullptr;
}

const ItemDef* ItemRegistry::findByBuildingTileId(uint16_t tileId) const
{
    for (const auto& item : items)
    {
        if (item.buildingTileId == tileId)
            return &item;
    }
    return nullptr;
}

ItemRegistry createDefaultItemRegistry()
{
    ItemRegistry reg;

    // Index 0 is the null item
    reg.items.push_back({ITEM_NONE, "None", "", ItemCategory::Resource, 0});

    // Raw resources (IDs 1-4)
    // {id, name, texture, category, maxStack, bldTile, toolTier, speedMult, iconRatio, description, worldSourceTier}
    reg.addItem({0, "Iron Ore",   "Items.8", ItemCategory::Resource, 50, 0, 0, 1.0f, 1.0f, "A raw ore from iron deposits.", 1});
    reg.addItem({0, "Copper Ore", "Items.9", ItemCategory::Resource, 50, 0, 0, 1.0f, 1.0f, "A raw ore from copper deposits.", 1});
    reg.addItem({0, "Coal",       "Items.6", ItemCategory::Resource, 50, 0, 0, 1.0f, 1.0f, "Combustible fuel found underground.", 1});
    reg.addItem({0, "Stone",      "Items.7", ItemCategory::Resource, 50, 0, 0, 1.0f, 1.0f, "A common rock used in basic crafting.", 0});

    // Intermediates (IDs 5-8)
    reg.addItem({0, "Iron Plate",   "Items.10", ItemCategory::Intermediate, 100, 0, 0, 1.0f, 1.0f, "Smelted iron, the backbone of industry."});
    reg.addItem({0, "Copper Plate", "Items.11", ItemCategory::Intermediate, 100, 0, 0, 1.0f, 1.0f, "Smelted copper, an excellent conductor."});
    reg.addItem({0, "Iron Gear",    "Items.14", ItemCategory::Intermediate, 100, 0, 0, 1.0f, 1.0f, "A toothed gear used in many machines."});
    reg.addItem({0, "Copper Wire",  "Items.13", ItemCategory::Intermediate, 100, 0, 0, 1.0f, 1.0f, "Fine wire for electronics."});

    // Products (ID 9)
    reg.addItem({0, "Circuit", "Items.17", ItemCategory::Product, 100, 0, 0, 1.0f, 1.0f, "A basic electronic circuit board."});

    // Liquids / fuels (IDs 10-14)
    reg.addItem({0, "Water",     "Items.0", ItemCategory::Resource, 50, 0, 0, 1.0f, 1.0f, "Pure water."});
    reg.addItem({0, "Petroleum", "Items.1", ItemCategory::Resource, 50, 0, 0, 1.0f, 1.0f, "Crude petroleum oil."});
    reg.addItem({0, "Fuel",      "Items.2", ItemCategory::Resource, 50, 0, 0, 1.0f, 1.0f, "Refined fuel for machines."});
    reg.addItem({0, "Acid",      "Items.3", ItemCategory::Resource, 50, 0, 0, 1.0f, 1.0f, "Corrosive acid used in advanced crafting."});
    reg.addItem({0, "Biofuel",   "Items.4", ItemCategory::Resource, 50, 0, 0, 1.0f, 1.0f, "Organic fuel derived from biomass."});

    // Raw resources (IDs 15-17)
    reg.addItem({0, "Wood", "Items.5",  ItemCategory::Resource, 50, 0, 0, 1.0f, 1.0f, "Harvested from trees.", 0});
    reg.addItem({0, "Rock", "Items.20", ItemCategory::Resource, 50, 0, 0, 1.0f, 1.0f, "A rough chunk of stone.", 0});
    reg.addItem({0, "Ore",  "Items.21", ItemCategory::Resource, 50, 0, 0, 1.0f, 1.0f, "Generic ore fragment.", 0});

    // Intermediates (IDs 18-20)
    reg.addItem({0, "Steel",      "Items.12", ItemCategory::Intermediate, 100, 0, 0, 1.0f, 1.0f, "High-strength refined steel."});
    reg.addItem({0, "Copper Rod", "Items.15", ItemCategory::Intermediate, 100, 0, 0, 1.0f, 1.0f, "A copper rod for wiring."});
    reg.addItem({0, "Iron Rod",   "Items.16", ItemCategory::Intermediate, 100, 0, 0, 1.0f, 1.0f, "An iron rod for structural use."});

    // Products (IDs 21-22)
    reg.addItem({0, "Processor",          "Items.18", ItemCategory::Product, 100, 0, 0, 1.0f, 1.0f, "A processing chip for complex machines."});
    reg.addItem({0, "Advanced Processor", "Items.19", ItemCategory::Product,  50, 0, 0, 1.0f, 1.0f, "A high-performance computing module."});

    // Utility (ID 23)
    reg.addItem({0, "Electricity", "Items.22", ItemCategory::Resource, 0, 0, 0, 1.0f, 1.0f, "Electrical power unit."});

    // Buildings (IDs 24-28) — placeable from hotbar, linked to BuildingDef by tileId
    reg.addItem({0, "Conveyor Belt", "Conveyor_Belt.152", ItemCategory::Building, 100, 4, 0, 1.0f, 1.0f,    "Moves items automatically from one place to another."});
    reg.addItem({0, "Furnace",       "Stone_Furnace.0",        ItemCategory::Building, 50, 5, 0, 1.0f, 2.0f/3.0f, "Smelts ores into plates using heat."});
    reg.addItem({0, "Assembler",     "Assembler_Machine_1.0",  ItemCategory::Building, 50, 6, 0, 1.0f, 2.0f/3.0f, "Automates crafting of intermediate goods."});
    reg.addItem({0, "Miner",         "Miner_Machine_1.0",      ItemCategory::Building, 50, 7, 0, 1.0f, 1.0f,      "Automatically mines the terrain beneath it."});
    reg.addItem({0, "Inserter",      "Robotic_Arms_1.0",       ItemCategory::Building, 50, 8, 0, 1.0f, 1.0f,      "Transfers items between machines and belts."});

    // Tools (IDs 29-30)
    reg.addItem({0, "Stone Pickaxe", "PixelwoodIcons.72", ItemCategory::Product, 1, 0, 1, 1.5f, 1.0f, "Tier 1 mining tool. Unlocks coal, copper and iron ore."});
    reg.addItem({0, "Iron Pickaxe",  "PixelwoodIcons.70", ItemCategory::Product, 1, 0, 2, 2.0f, 1.0f, "Tier 2 mining tool. Mines faster."});

    // Storage (ID 31)
    reg.addItem({0, "Storage", "Crate.0", ItemCategory::Building, 50, 9, 0, 1.0f, 1.0f, "A simple chest that stores items."});

    return reg;
}
