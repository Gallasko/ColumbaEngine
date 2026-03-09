#pragma once

#include <string>
#include <functional>

#include "Helpers/helpers.h"
#include "serialization.h"

#include "Memory/elementtype.h"

#include "ECS/system.h"

namespace pg
{
    // Type definitions
    struct Character;
    struct CallableIntepretedFunction;

    static const char * const NOOPPASSIVE = "Noop"; 

    enum class TriggerType : uint8_t
    {
        Noop = 0,
        TurnStart,
        TurnEnd,
        OnHit,
        OnDamageDealt,
        StatBoost
    };

    const static std::unordered_map<TriggerType, std::string> triggerTypeToString = {
        {TriggerType::Noop, NOOPPASSIVE},
        {TriggerType::TurnStart, "TurnStart"},
        {TriggerType::TurnEnd, "TurnEnd"},
        {TriggerType::OnHit, "OnHit"},
        {TriggerType::OnDamageDealt, "OnDamageDealt"},
        {TriggerType::StatBoost, "StatBoost"},
    };

    const static std::unordered_map<std::string, TriggerType> stringToTriggerType = invertMap(triggerTypeToString);

    enum class PassiveType : uint8_t
    {
        Noop = 0,
        CharacterEffect,
        SpellEffect,
        TurnEffect,
    };

    const static std::unordered_map<PassiveType, std::string> passiveTypeToString = {
        {PassiveType::Noop, NOOPPASSIVE},
        {PassiveType::CharacterEffect, "CharacterEffect"},
        {PassiveType::SpellEffect, "SpellEffect"},
        {PassiveType::TurnEffect, "TurnEffect"},
    };

    const static std::unordered_map<std::string, PassiveType> stringToPassiveType = invertMap(passiveTypeToString);

    enum class ApplicableFunctionType : uint8_t
    {
        Noop = 0,
        Script,
        Functional
    };

    typedef std::unordered_map<std::string, ElementType> ElementMap;

    template <typename Type>
    struct ApplicablePassive
    {
        ApplicableFunctionType type = ApplicableFunctionType::Noop;

        ElementMap args;

        // Todo make it an union
        std::function<void(Type&, const ElementMap&, EntitySystem*)> func = [](Type&, const ElementMap&, EntitySystem*) { LOG_ERROR("Passive", "Trying to call a non function"); };
        std::shared_ptr<CallableIntepretedFunction> scriptFunction = nullptr;

        ApplicablePassive& operator=(const std::function<void(Type&, const ElementMap&, EntitySystem*)>& f)
        {
            type = ApplicableFunctionType::Functional;

            func = f;

            return *this;
        }

        void apply(Type& chara, EntitySystem *ecsRef) const
        {
            if (type == ApplicableFunctionType::Functional)
            {
                func(chara, args, ecsRef);
            }
            else
            {
                LOG_ERROR("Passive", "Passive function type is not supported !");
            }
        };
    };

    struct PassiveInfo
    {
        PassiveType type;

        TriggerType trigger;

        // std::string name = "Passive"; 
        
        // -1 means permanent, once it reach 0, the passive is removed
        int32_t remainingTurns = -1;

        // 0 means that it is active everytime the trigger is triggered, any other value means that it need multiple triggers before activation
        size_t numberOfTriggerBeforeActivation = 0;
        size_t currentNbOfTriggerSinceLastActivation = 0;

        /** Flag indicating if this passive is hidden from view of the player */
        bool hidden = false;

        /** Keep track of the number of time this passive was activated */
        size_t nbSuccesfulActivation = 0;

        // Todo
        // size_t acc = 2.0f / 3.0f;
        size_t activationChance = 100;

        size_t activationMaxChance = 100;
    };

    template <>
    void serialize(Archive& archive, const PassiveInfo& value);

    template <>
    PassiveInfo deserialize(const UnserializedObject& serializedString);

    struct Passive
    {
        std::string name = NOOPPASSIVE;

        // Function to use and define when passiveType == CharacterEffect
        ApplicablePassive<Character> applyOnCharacter;
        ApplicablePassive<Character> removeFromCharacter;

        // Todo upgrade this to be more precise
        bool operator==(const Passive& other)
        {
            return this->name == other.name;
        }

        bool operator==(const std::string& name)
        {
            return this->name == name;
        }
    };

    struct PassiveCall
    {
        PassiveCall() {};
        PassiveCall(const PassiveCall& other) : passiveName(other.passiveName), info(other.info), args(other.args) {}

        PassiveCall& operator=(const PassiveCall& other)
        {
            passiveName = other.passiveName;
            info = other.info;
            args = other.args;

            return *this;
        }

        std::string passiveName;

        PassiveInfo info;

        ElementMap args;
    };

    template <>
    void serialize(Archive& archive, const PassiveCall& value);

    template <>
    PassiveCall deserialize(const UnserializedObject& serializedString);

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

    struct PassiveEffect
    {
        PassiveCall call;
        PassiveInfo info;
        Passive effect;
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
    // Passive makeSimplePlayerBoostPassive(PlayerBoostType type, float value, int32_t duration = -1, std::string name = "PlayerBoost");

    struct PassiveDatabase : public System<StoragePolicy>
    {
        std::unordered_map<std::string, Passive> database;

        void storePassive(const Passive& passive);

        Passive resolvePassive(const PassiveCall& call) const;
    };
}