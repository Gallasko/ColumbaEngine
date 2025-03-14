#pragma once

#include <stdint.h>

#include "Helpers/helpers.h"
#include "Memory/elementtype.h"

#include "serialization.h"

namespace pg
{
    enum class ItemType : uint8_t
    {
        Weapon = 0,
        Armor,
        Consomable,
        SkillBook,
        SkillXp,
        Material,
        SpellStone,
        Key
    };

    const static std::unordered_map<ItemType, std::string> itemTypeToString = {
        {ItemType::Weapon, "Weapon"},
        {ItemType::Armor, "Armor"},
        {ItemType::Consomable, "Consomable"},
        {ItemType::SkillBook, "SkillBook"},
        {ItemType::SkillXp, "SkillXp"},
        {ItemType::Material, "Material"},
        {ItemType::SpellStone, "SpellStone"},
        {ItemType::Key, "Key"}
    };

    const static std::unordered_map<std::string, ItemType> stringToItemType = invertMap(itemTypeToString);

    enum class ItemRarity : uint8_t
    {
        Common = 0,
        Uncommon,
        Rare,
        Epic,
        Legendary,
        Mythic,
        Unique
    };

    struct Item
    {
        Item() {}
        Item(const Item& other) : name(other.name), type(other.type), stacksize(other.stacksize), nbItems(other.nbItems), rarity(other.rarity), attributes(other.attributes), description(other.description), icon(other.icon) {}

        Item& operator=(const Item& other)
        {
            name = other.name;
            type = other.type;
            stacksize = other.stacksize;
            nbItems = other.nbItems;
            rarity = other.rarity;
            attributes = other.attributes;
            description = other.description;
            icon = other.icon;

            return *this;
        }

        /** Name of the item */
        std::string name = "None";
        
        /** Type of the item */
        ItemType type = ItemType::Key;

        /** Number of items in a stack */
        int stacksize = 1;
        /** Current number of items in this stack */
        size_t nbItems = 1;

        /** Rarity of the item */
        ItemRarity rarity = ItemRarity::Common;

        /**
         * Map of all the attributes of an item
         * 
         * Exemple of attributes available
         * ConsumedUponUsed -> bool
         * StatsBoost (for weapons and armor) -> <"MATK", "5">
         * Spell -> "Spell1;Spell2;Spell3" (spellname string in csv format)
         */
        std::unordered_map<std::string, ElementType> attributes = {};
        
        // Todo maybe add a pointer to a character (For weapons and armors)

        /** Description of the item */
        std::string description = "An item";

        /** Icon of the item */
        std::string icon = "NoneIcon";

        bool operator==(const Item& rhs) const
        {
            return name == rhs.name and stacksize == rhs.stacksize and type == rhs.type and rarity == rhs.rarity and attributes == rhs.attributes;
        }
    };

    template <>
    void serialize(Archive& archive, const Item& value);
}