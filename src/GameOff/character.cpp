#include "character.h"

namespace pg
{
    namespace
    {
        constexpr const char * const DOM = "Character";
    }

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

    template <>
    void serialize(Archive& archive, const CharacterStat& value)
    {
        archive.startSerialization("CharacterStat");

        serialize(archive, "health", value.health);

        serialize(archive, "ad", value.physicalAttack);
        serialize(archive, "ap", value.magicalAttack);

        serialize(archive, "armor", value.physicalDefense);
        serialize(archive, "rm", value.magicalDefense);

        serialize(archive, "speed", value.speed);

        archive.endSerialization();
    }

    template <>
    CharacterStat deserialize(const UnserializedObject& serializedString)
    {
        return CharacterStat{};
    }

    template <>
    void serialize(Archive& archive, const Character& value)
    {
        archive.startSerialization(Character::getType());

        serialize(archive, "name", value.name);
        serialize(archive, "stat", value.stat);

        archive.endSerialization();
    }

    template <>
    Character deserialize(const UnserializedObject& serializedString)
    {
        LOG_THIS(DOM);

        std::string type = "";

        if (serializedString.isNull())
        {
            LOG_ERROR(DOM, "Element is null");
        }
        else
        {
            LOG_INFO(DOM, "Deserializing Character");

            Character data;

            data.name = deserialize<std::string>(serializedString["name"]);

            return data;
        }

        return Character{};
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