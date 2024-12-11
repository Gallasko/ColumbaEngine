#pragma once

#include "Scene/scenemanager.h"

#include "character.h"

#include "item.h"

namespace pg
{
    struct DropChance
    {
        Item item;

        float dropChance = 1.0f;

        size_t quantity = 1;
    };

    struct CharacterList
    {
        std::vector<Character> characters;
    };

    struct DropChanceList
    {
        std::vector<DropChance> dropTable;
    };

    struct Encounter
    {
        CharacterList charaList;

        DropChanceList dropList;
    };

    struct Location
    {
        std::string name;

        size_t nbEncounterBeforeBoss = 10;

        bool randomEncounter = true;

        std::vector<Encounter> possibleEnounters;

        Encounter bossEncounter;

        bool finished = false;

        std::vector<std::string> prerequisiteEnounters;
    };

    template <>
    void serialize(Archive& archive, const DropChance& value);

    template <>
    void serialize(Archive& archive, const CharacterList& value);

    template <>
    void serialize(Archive& archive, const DropChanceList& value);

    template <>
    void serialize(Archive& archive, const Encounter& value);

    template <>
    void serialize(Archive& archive, const Location& value);
}