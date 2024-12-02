#pragma once

#include "location.h"
#include "customitem.h"

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

            stat.health = 30;

            stat.speed = 75;

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

                dropTable = { { XpStone{} } };
            }
        };

        SlimeForest()
        {
            name = "SlimeForest";

            possibleEnounters = { SoloSlimeEncounter{} };
        }
    };
}