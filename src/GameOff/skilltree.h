#pragma once

#include <string>

#include "characterstats.h"
#include "spells.h"
#include "passives.h"

namespace pg
{
    constexpr static size_t MAXLEVEL = 100;

    struct LevelProgression
    {
        size_t neededXp[MAXLEVEL + 1] = {0};
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

        LevelProgression requiredXpForNextLevel;

        LevelIncrease levelGains[MAXLEVEL + 1];
    };

    struct NoneSkillTree : public SkillTree 
    {
        NoneSkillTree() { name = "None"; }
    };
}