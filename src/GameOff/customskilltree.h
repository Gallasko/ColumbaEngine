#pragma once

#include "skilltree.h"

namespace pg
{
    struct StabSpell : public Spell
    {
        StabSpell()
        {

        }
    };

    struct WarriorTree : public SkillTree
    {
        WarriorTree()
        {
            name = "Warrior";

            currentLevel = 2;

            maxLevel = 10;

            for (size_t i = 0; i < maxLevel; i++)
                requiredXpForNextLevel.neededXp[i] = i + 1;
        
            for (size_t i = 0; i < maxLevel; i++)
            {
                if (i % 5 == 0)
                {
                    levelGains[i].stats.health += 50;
                    levelGains[i].stats.physicalAttack += 1;
                    levelGains[i].stats.physicalDefense += 1;
                }
                else
                {
                    levelGains[i].stats.health += 10;
                }
            }
        }
    };

    struct MageTree : public SkillTree
    {
        MageTree()
        {
            name = "Mage";

            currentLevel = 5;

            maxLevel = 10;

            for (size_t i = 0; i < maxLevel; i++)
                requiredXpForNextLevel.neededXp[i] = i + 1;
        
            for (size_t i = 0; i < maxLevel; i++)
            {
                if (i % 5 == 0)
                {
                    levelGains[i].stats.magicalAttack += 5;
                }
                else
                {
                    levelGains[i].stats.magicalAttack += 1;
                }
            }
        }
    };
}