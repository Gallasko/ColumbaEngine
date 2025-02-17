#pragma once

#include "Scene/scenemanager.h"

#include "character.h"

#include "item.h"

#include "gamefacts.h"

namespace pg
{
    struct DropChance
    {
        Item item;

        float dropChance = 1.0f;

        size_t quantity = 1;
    };

    struct Encounter
    {
        std::vector<Character> characters;

        std::vector<DropChance> dropTable;
    };

    struct Location
    {
        std::string name;

        size_t nbEncounterBeforeBoss = 10;

        bool randomEncounter = true;

        std::vector<Encounter> possibleEnounters;

        Encounter bossEncounter;

        bool finished = false;

        std::vector<FactChecker> prerequisiteFacts;
    };

    template <>
    void serialize(Archive& archive, const DropChance& value);

    template <>
    void serialize(Archive& archive, const Encounter& value);

    template <>
    void serialize(Archive& archive, const Location& value);
}