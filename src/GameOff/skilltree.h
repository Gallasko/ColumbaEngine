#pragma once

#include <string>

#include "characterstats.h"
#include "spells.h"
#include "passives.h"
#include "item.h"

namespace pg
{
    constexpr static size_t MAXLEVEL = 100;

    struct LevelProgression
    {
        std::vector<Item> neededMat[MAXLEVEL + 1] = {};
    };

    struct LevelIncrease
    {
        CharacterStat stats;

        std::vector<Spell> learntSpells;

        std::vector<Passive> learntPassive;
    };

    struct SkillTree
    {
        std::string name;

        size_t currentLevel = 0;

        size_t maxLevel = 0;

        size_t currentXp = 0;

        LevelProgression requiredMatForNextLevel;

        LevelIncrease levelGains[MAXLEVEL + 1];

        bool operator==(const SkillTree& other) const
        {
            return name == other.name;
        }
    };

    struct NoneSkillTree : public SkillTree 
    {
        NoneSkillTree() { name = "None"; }
    };

    struct SkillTreeDatabase : public System<StoragePolicy>
    {
        void addSkillTree(const SkillTree& sTree) { database[sTree.name] = sTree; }

        std::unordered_map<std::string, SkillTree> database;
    };
}