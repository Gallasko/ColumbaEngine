#pragma once

#include "location.h"

namespace pg
{
    struct Bite : public Spell
    {
        Bite()
        {
            name = "Bite";

            baseDmg = 10;
        }
    };


    struct Slime : public Character
    {
        Slime()
        {
            name = "Slime";

            type = CharacterType::Enemy;

            spells = { Bite{} };
        }
    };

    struct SlimeForest : public Location
    {
        struct SoloSlimeEncounter : public Encounter
        {
            SoloSlimeEncounter() 
            {
                characters = { Slime{} };
            }
        };

        SlimeForest()
        {
            name = "SlimeForest";

            possibleEnounters = { SoloSlimeEncounter{} };
        }
    };
}