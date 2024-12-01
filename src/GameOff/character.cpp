#include "character.h"

namespace pg
{
    void addStatOnCharacter(Character& chara, PlayerBoostType type, float value)
    {
        switch (type)
        {
            case PlayerBoostType::Health:
                chara.stat.health += value;
                break;

            case PlayerBoostType::PAtk:
                chara.stat.physicalAttack += value;
                break;

            case PlayerBoostType::MAtk:
                chara.stat.magicalAttack += value;
                break;

            case PlayerBoostType::PDef:
                chara.stat.physicalDefense += value;
                break;

            case PlayerBoostType::MDef:
                chara.stat.physicalAttack += value;
                break;

            case PlayerBoostType::Speed:
                chara.stat.speed += value;
                break;

            case PlayerBoostType::CChance:
                chara.stat.critChance += value;
                break;

            case PlayerBoostType::CDamage:
                chara.stat.critDamage += value;
                break;

            case PlayerBoostType::Evasion:
                chara.stat.evasionRate += value;
                break;

            case PlayerBoostType::Res:
                // chara.elementalRes += value;
                break;

        }
    }

    Passive makeSimplePlayerBoostPassive(PlayerBoostType type, float value, int32_t duration, std::string name)
    {
        Passive passive;

        passive.type = PassiveType::CharacterEffect;

        passive.trigger = TriggerType::StatBoost;

        passive.name = name;
        
        // Careful need to do + 1 to duration if it is > -1 here as the duration of the buff start ticking down at the start of the player turn !
        // (until the start of the next turn -> set duration == 0, until the end of the next turn -> set duration == 1)
        passive.remainingTurns = duration == -1 ? -1 : duration + 1;

        passive.applyOnCharacter = [type, value](Character& character) { addStatOnCharacter(character, type, value); };
        passive.removeFromCharacter = [type, value](Character& character) { addStatOnCharacter(character, type, -value); };

        return passive;
    }

    void Character::addPassive(const Passive& passive)
    {
        if (passive.type == PassiveType::CharacterEffect and passive.trigger == TriggerType::StatBoost)
        {
            passive.applyOnCharacter(*this);
        }

        passives.push_back(passive);
    }
}