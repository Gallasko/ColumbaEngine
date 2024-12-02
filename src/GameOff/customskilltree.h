#pragma once

#include "skilltree.h"
#include "customitem.h"

namespace pg
{
    struct SlashSpell : public Spell
    {
        SlashSpell()
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
                requiredMatForNextLevel.neededMat[i] = { XpStone { i + 1 } };
        
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

            levelGains[0].learntSpells = { SlashSpell{} };
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
                requiredMatForNextLevel.neededMat[i] = { XpStone { i + 1 } };
        
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

    struct AdventurerTree : public SkillTree
    {
        AdventurerTree()
        {
            name = "Adventurer";

            currentLevel = 0;

            maxLevel = 10;

            for (size_t i = 0; i < maxLevel; i++)
            {
                if (i == 0)
                {
                    requiredMatForNextLevel.neededMat[i] = { XpStone { i + 1 }, SlimeBall{} };
                }
                else
                {
                    requiredMatForNextLevel.neededMat[i] = { XpStone { i + 1 } };
                }
                
            }
        
            for (size_t i = 0; i < maxLevel; i++)
            {
                if (i % 5 == 0)
                {
                    levelGains[i].stats.health += 10;
                }
                else
                {
                    levelGains[i].stats.health += 5;
                }
            }

            levelGains[0].learntSpells = { };
        }
    };
}