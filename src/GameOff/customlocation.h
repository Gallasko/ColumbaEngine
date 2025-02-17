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

    struct MetalSlime : public Character
    {
        MetalSlime()
        {
            name = "MetalSlime";

            type = CharacterType::Enemy;

            stat.health = 300;

            stat.speed = 75;

            spells = { Bite{} };
        }
    };

    struct SoloSlimeEncounter : public Encounter
    {
        SoloSlimeEncounter() 
        {
            characters = { Slime{} };

            dropTable = { { XpStone {}, 1.0f, 2}, { SlimeBall {}, 0.5f }, { WarriorGrimoire{} } };
        }
    };

    struct SlimeForest : public Location
    {
        struct DuoSlimeEncounter : public Encounter
        {
            DuoSlimeEncounter() 
            {
                characters = { Slime{}, Slime{} };

                dropTable = { { XpStone {}, 1.0f, 4}, { SlimeBall {}, 0.5f }, { SlimeBall {}, 0.5f } };
            }
        };

        SlimeForest()
        {
            name = "SlimeForest";

            possibleEnounters = { SoloSlimeEncounter{}, DuoSlimeEncounter{} };
        }
    };

    struct SlimeDen : public Location
    {
        struct MetalSlimeEncounter : public Encounter
        {
            MetalSlimeEncounter() 
            {
                characters = { MetalSlime{} };

                dropTable = { { XpStone {}, 1.0f, 1000 }, { SlimeBall {}, 0.5f }, { WarriorGrimoire{} } };
            }
        };

        SlimeDen()
        {
            name = "SlimeDen";

            possibleEnounters = { SoloSlimeEncounter{}, MetalSlimeEncounter{} };

            prerequisiteFacts = { FactChecker{ "Slime_defeated", 5, FactCheckEquality::GreaterEqual } };
        }
    };
}