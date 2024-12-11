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

            baseDmg = 25;
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
                charaList.characters = { Slime{} };

                dropList.dropTable = { { XpStone {}, 1.0f, 2}, { SlimeBall {}, 0.5f }, { WarriorGrimoire{} } };
            }
        };

        struct DuoSlimeEncounter : public Encounter
        {
            DuoSlimeEncounter() 
            {
                charaList.characters = { Slime{}, Slime{} };

                dropList.dropTable = { { XpStone {}, 1.0f, 4}, { SlimeBall {}, 0.5f }, { SlimeBall {}, 0.5f } };
            }
        };

        struct MetalSlimeEncounter : public Encounter
        {

        };

        SlimeForest()
        {
            name = "SlimeForest";

            possibleEnounters = { SoloSlimeEncounter{}, DuoSlimeEncounter{} };
        }
    };
}