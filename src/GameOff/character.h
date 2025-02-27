#pragma once

#include <string>
#include <map>

#include "Helpers/helpers.h"
#include "serialization.h"

#include "characterstats.h"
#include "spells.h"
#include "passives.h"

namespace pg
{
    enum class CharacterType : uint8_t
    {
        Player = 0,
        Enemy
    };

    const static std::unordered_map<CharacterType, std::string> charaTypeToString = {
        {CharacterType::Player, "Player"},
        {CharacterType::Enemy, "Enemy"},
    };

    const static std::unordered_map<std::string, CharacterType> stringToCharaType = invertMap(charaTypeToString);

    enum class PlayingStatus : uint8_t
    {
        Alive = 0,
        Dead
    };

    enum class CharaBehaviourType : uint8_t
    {
        None = 0,
        Random,
        OnlyAutoAttack,
        Pattern,
    };

    enum class TargetingType : uint8_t
    {
        None,
        Random,
        RoundRobin,
        WeakestFirst,
        StrongestFirst,
    };

    // Todo add some passives that can change the behaviour of a character
    struct CharaBehaviour
    {
        CharaBehaviourType type = CharaBehaviourType::None;

        TargetingType onAllyTarget = TargetingType::None;
        TargetingType onEnemyTarget = TargetingType::None;

        std::vector<size_t> pattern = { };
    };

    struct Character
    {
        Character() {}
        Character(const std::string& name) : name(name) {}
        Character(const Character& other) : name(other.name), icon(other.icon), type(other.type), stat(other.stat), spells(other.spells), passives(other.passives), behaviour(other.behaviour), basicSpell(other.basicSpell), aggroMap(other.aggroMap), playingStatus(other.playingStatus), id(other.id) {}

        Character& operator=(const Character& other)
        {
            name = other.name;
            icon = other.icon;
            type = other.type;
            stat = other.stat;
            spells = other.spells;
            passives = other.passives;
            behaviour = other.behaviour;
            basicSpell = other.basicSpell;
            aggroMap = other.aggroMap;
            playingStatus = other.playingStatus;
            id = other.id;

            return *this;
        }

        std::string name = "Unknown";

        std::string icon = "NoneIcon";

        CharacterType type = CharacterType::Player;

        CharacterStat stat = BaseCharacterStat{};

        float speedUnits = 0;

        std::vector<Spell> spells = { };

        // Todo create addSpell also

        std::vector<PassiveEffect> passives = {};

        // Todo add behaviour and basic spell to serialize/deserialize
        CharaBehaviour behaviour = { CharaBehaviourType::Random, TargetingType::Random, TargetingType::Random };

        Spell basicSpell;

        void addPassive(const PassiveEffect& passive, EntitySystem *ecsRef);

        inline static std::string getType() { return "Character"; }

        // In combat charac
        // Todo make them private !

        /** Map containing the aggro of all the character relative to this one.
         * Key: id of the character
         * Value: Value of the aggro of the said character
         * 
         * Aggro increase with all the damage dealt
         * Base aggro = character.physicalAttack + character.magicalAttack
         * 
         * Todo make sure that this map is ordered with the higher aggro first and the worst aggro last
        */
        std::map<size_t, float> aggroMap = {};

        void receiveDmg(long long int amount, EntitySystem* ecsRef = nullptr);

        PlayingStatus playingStatus = PlayingStatus::Alive;

        size_t id = 0;
    };

    template <>
    void serialize(Archive& archive, const Character& value);

    template <>
    Character deserialize(const UnserializedObject& serializedString);

    struct CharacterLeftClicked
    {
        CharacterLeftClicked(Character *chara) : chara(chara) {}

        Character *chara;
    };
}