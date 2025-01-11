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

    struct BasicStrike : public Spell
    {
        BasicStrike()
        {
            name = "BasicStrike";

            baseDmg = 10;
        }
    };

    struct Character
    {
        Character() {}
        Character(const Character& other) : name(other.name), type(other.type), stat(other.stat), spells(other.spells), passives(other.passives), aggroMap(other.aggroMap), playingStatus(other.playingStatus), id(other.id) {}

        std::string name = "Unknown";

        CharacterType type = CharacterType::Player;

        CharacterStat stat = BaseCharacterStat{};

        float speedUnits = 0;

        std::vector<Spell> spells = { BasicStrike{} };

        // Todo create addSpell also

        std::vector<Passive> passives = {};

        void addPassive(const Passive& passive);

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