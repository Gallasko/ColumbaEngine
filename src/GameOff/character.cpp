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

        serialize(archive, "critChance", value.critChance);
        serialize(archive, "critDamage", value.critDamage);
        serialize(archive, "evasion", value.evasionRate);

        // Todo change, make a lookup table
        for (size_t i = 0; i < NbElements; i++)
        {
            serialize(archive, "elementRes" + std::to_string(i), value.elementalRes[i]);
        }

        archive.endSerialization();
    }

    template <>
    CharacterStat deserialize(const UnserializedObject& serializedString)
    {
        LOG_THIS(DOM);

        CharacterStat data;

        if (serializedString.isNull())
        {
            LOG_ERROR(DOM, "Element is null");
        }
        else
        {
            LOG_INFO(DOM, "Deserializing CharacterStat");

            defaultDeserialize(serializedString, "health", data.health);

            defaultDeserialize(serializedString, "ad", data.physicalAttack);
            defaultDeserialize(serializedString, "ap", data.magicalAttack);

            defaultDeserialize(serializedString, "armor", data.physicalDefense);
            defaultDeserialize(serializedString, "rm", data.magicalDefense);

            defaultDeserialize(serializedString, "speed", data.speed);

            defaultDeserialize(serializedString, "critDamage", data.critChance);
            defaultDeserialize(serializedString, "critChance", data.critDamage);
            defaultDeserialize(serializedString, "evasion", data.evasionRate);
        }

        return data;
    }

    template <>
    void serialize(Archive& archive, const Character& value)
    {
        archive.startSerialization(Character::getType());

        serialize(archive, "name", value.name);
        serialize(archive, "type", charaTypeToString.at(value.type));
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

            std::string temp = "Enemy";
            defaultDeserialize(serializedString, "type", temp);

            data.type = stringToCharaType.at(temp);

            defaultDeserialize(serializedString, "stat", data.stat);

            return data;
        }

        return Character{};
    }

    // Passive makeSimplePlayerBoostPassive(PlayerBoostType type, float value, int32_t duration, std::string name)
    // {
    //     Passive passive;

    //     passive.info.type = PassiveType::CharacterEffect;

    //     passive.info.trigger = TriggerType::StatBoost;

    //     passive.info.name = name;
        
    //     // Careful need to do + 1 to duration if it is > -1 here as the duration of the buff start ticking down at the start of the player turn !
    //     // (until the start of the next turn -> set duration == 0, until the end of the next turn -> set duration == 1)
    //     passive.info.remainingTurns = duration == -1 ? -1 : duration + 1;

    //     passive.applyOnCharacter.function = [type, value](Character& character) { addStatOnCharacter(character, type, value); };
    //     passive.removeFromCharacter.function = [type, value](Character& character) { addStatOnCharacter(character, type, -value); };

    //     return passive;
    // }

    void Character::addPassive(const Passive& passive)
    {
        if (passive.info.type == PassiveType::CharacterEffect and passive.info.trigger == TriggerType::StatBoost)
        {
            // passive.applyOnCharacter(*this);
        }

        passives.push_back(passive);
    }
}