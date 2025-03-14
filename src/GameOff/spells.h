#pragma once

#include "commons.h"

#include "passives.h"

#include <string>
#include <set>

namespace pg
{
    struct Spell
    {
        std::string name = "Unknown";

        float baseDmg = 1;

        float baseManaCost = 0;

        size_t baseCooldown = 1;

        float physicalMultipler = 0.0f;
        float magicalMultipler = 0.0f;

        std::set<Element> elementType = {};
        DamageType damageType = DamageType::Physical;

        // Todo add every other non vector attributes in properties
        std::unordered_map<std::string, ElementType> properties = { {"SpellType", ElementType{"Damage"} }};

        std::vector<PassiveCall> applyToTarget = {};
        std::vector<PassiveCall> applyToSelf = {};
        
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

    template <>
    void serialize(Archive& archive, const Spell& value);

    struct BasicStrike : public Spell
    {
        BasicStrike()
        {
            name = "Basic Strike";

            baseDmg = 10;

            physicalMultipler = 0.4f;
        }
    };
}