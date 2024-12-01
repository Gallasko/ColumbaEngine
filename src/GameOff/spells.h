#pragma once

#include "commons.h"

#include <string>

namespace pg
{
    struct Spell
    {
        std::string name = "Unknown";

        float baseDmg = 1;

        float baseManaCost = 0;

        size_t baseCooldown = 1;

        Element elementType = Element::ElementLess;
        DamageType damageType = DamageType::Physical;

        bool selfOnly = false;

        size_t nbTargets = 1;

        bool canTargetSameCharacterMultipleTimes = false;

        // In combat charac
        
        size_t numberOfTurnsSinceLastUsed = 0;

        // Todo upgrade this to be more precise
        bool operator==(const Spell& other)
        {
            return name == other.name;
        }
    };
}