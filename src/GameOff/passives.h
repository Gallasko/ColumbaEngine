#pragma once

#include <string>
#include <functional>

namespace pg
{
    // Type definitions
    struct Character;

    enum class TriggerType : uint8_t
    {
        TurnStart = 0,
        TurnEnd,
        OnHit,
        OnDamageDealt,
        StatBoost
    };

    enum class PassiveType : uint8_t
    {
        CharacterEffect = 0,
        SpellEffect,
        TurnEffect,
    };

    struct Passive
    {
        PassiveType type;

        TriggerType trigger;

        std::string name = "Passive"; 
        
        // -1 means permanent, once it reach 0, the passive is removed
        int32_t remainingTurns = -1;

        // 0 means that it is active everytime the trigger is triggered, any other value means that it need multiple triggers before activation
        size_t numberOfTriggerBeforeActivation = 0;
        size_t currentNbOfTriggerSinceLastActivation = 0;

        /** Flag indicating if this passive is hidden from view of the player */
        bool hidden = false;

        /** Keep track of the number of time this passive was activated */
        size_t nbSuccesfulActivation = 0;

        // Function to use and define when passiveType == CharacterEffect and trigger == StatBoost
        std::function<void(Character&)> applyOnCharacter;
        std::function<void(Character&)> removeFromCharacter;

        // Todo upgrade this to be more precise
        bool operator==(const Passive& other)
        {
            return name == other.name;
        }
    };

    enum class PlayerBoostType : uint8_t
    {
        Health = 0,
        PAtk,
        MAtk,
        PDef,
        MDef,
        Speed,
        CChance,
        CDamage,
        Evasion,
        Res
    };

    /**
     * @brief Factory function creating simple Player boost passive
     * 
     * @param type Type of player boost to apply
     * @param value Value of player boost to apply
     * @param duration Number of turn that the boost last (-1 to go infinite)
     * @param name Name of the boost
     * 
     * @return Passive A Passive that gives the "value" amount to a given character
     */
    Passive makeSimplePlayerBoostPassive(PlayerBoostType type, float value, int32_t duration = -1, std::string name = "PlayerBoost");
}