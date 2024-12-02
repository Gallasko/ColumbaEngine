#pragma once

#include <string>
#include <map>

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
        std::string name = "Unknown";

        CharacterType type = CharacterType::Player;

        CharacterStat stat = BaseCharacterStat{};

        float speedUnits = 0;

        std::vector<Spell> spells = { BasicStrike{} };

        // Todo create addSpell also

        std::vector<Passive> passives = {};

        void addPassive(const Passive& passive);

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

    struct CharacterLeftClicked
    {
        CharacterLeftClicked(Character *chara) : chara(chara) {}

        Character *chara;
    };
}