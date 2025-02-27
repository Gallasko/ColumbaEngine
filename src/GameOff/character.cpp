#include "character.h"
#include "fightscene.h"

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
            LOG_MILE(DOM, "Deserializing CharacterStat");

            defaultDeserialize(serializedString, "health", data.health);

            defaultDeserialize(serializedString, "ad", data.physicalAttack);
            defaultDeserialize(serializedString, "ap", data.magicalAttack);

            defaultDeserialize(serializedString, "armor", data.physicalDefense);
            defaultDeserialize(serializedString, "rm", data.magicalDefense);

            defaultDeserialize(serializedString, "speed", data.speed);

            defaultDeserialize(serializedString, "critChance", data.critChance);
            defaultDeserialize(serializedString, "critDamage", data.critDamage);
            defaultDeserialize(serializedString, "evasion", data.evasionRate);
        }

        return data;
    }

    template <>
    void serialize(Archive& archive, const CharaBehaviour& value)
    {
        archive.startSerialization("CharaBehaviour");

        serialize(archive, "type", charaBehaviourToString.at(value.type));
        serialize(archive, "onAlly", targetingTypeToString.at(value.onAllyTarget));
        serialize(archive, "onEnemy", targetingTypeToString.at(value.onEnemyTarget));
        serialize(archive, "pattern", value.pattern);

        archive.endSerialization();
    }

    template <>
    CharaBehaviour deserialize(const UnserializedObject& serializedString)
    {
        LOG_THIS(DOM);

        std::string type = "";

        if (serializedString.isNull())
        {
            LOG_ERROR(DOM, "Element is null");
        }
        else
        {
            LOG_INFO(DOM, "Deserializing CharaBehaviour");

            CharaBehaviour data;

            std::string type = charaBehaviourToString.at(data.type);
            std::string onAllyTarget = targetingTypeToString.at(data.onAllyTarget);
            std::string onEnemyTarget = targetingTypeToString.at(data.onEnemyTarget);

            defaultDeserialize(serializedString, "type", type);
            defaultDeserialize(serializedString, "onAlly", onAllyTarget);
            defaultDeserialize(serializedString, "onEnemy", onEnemyTarget);
            defaultDeserialize(serializedString, "pattern", data.pattern);

            data.type = stringToCharaBehaviour.at(type);
            data.onAllyTarget = stringToTargetingType.at(onAllyTarget);
            data.onEnemyTarget = stringToTargetingType.at(onEnemyTarget);

            return data;
        }

        return CharaBehaviour{};
    }

    template <>
    void serialize(Archive& archive, const Character& value)
    {
        archive.startSerialization(Character::getType());

        serialize(archive, "name", value.name);
        serialize(archive, "icon", value.icon);
        serialize(archive, "type", charaTypeToString.at(value.type));
        serialize(archive, "stat", value.stat);
        serialize(archive, "spells", value.spells);
        serialize(archive, "behaviour", value.behaviour);
        serialize(archive, "basicSpell", value.basicSpell);

        // Todo
        // serialize(archive, "passives", value.passives);

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

            defaultDeserialize(serializedString, "icon", data.icon);

            std::string temp = "Enemy";
            defaultDeserialize(serializedString, "type", temp);

            data.type = stringToCharaType.at(temp);

            defaultDeserialize(serializedString, "stat", data.stat);

            defaultDeserialize(serializedString, "spells", data.spells);

            defaultDeserialize(serializedString, "behaviour", data.behaviour);

            defaultDeserialize(serializedString, "basicSpell", data.basicSpell);

            // Todo
            // defaultDeserialize(serializedString, "passives", data.passives);

            return data;
        }

        return Character{};
    }

    void Character::addPassive(const PassiveEffect& passive, EntitySystem *ecsRef)
    {
        // Todo, stop applying the passive to the chara and instead recalculate at each turn the current stat of the player depending on his current boost
        // We try to apply the passive if it is a start of turn one
        if (passive.info.type == PassiveType::CharacterEffect and passive.info.trigger == TriggerType::StatBoost)
        {
            passive.effect.applyOnCharacter.apply(*this, ecsRef);
        }

        passives.push_back(passive);
    }

    void Character::receiveDmg(long long int amount, EntitySystem* ecsRef)
    {
        stat.health -= amount;

        if (ecsRef)
        {
            auto str = "Character " + name;
            
            if (amount < 0)
                str += " gained " + std::to_string(-amount) + " hp !";
            else
                str += " lost " + std::to_string(amount) + " hp !";

            ecsRef->sendEvent(FightMessageEvent{str});
        }

        if (stat.health <= 0.5f)
        {
            speedUnits = 0;
            playingStatus = PlayingStatus::Dead;
            
            if (ecsRef)
            {
                std::string message = name + " died !";

                ecsRef->sendEvent(FightMessageEvent{message});

                ecsRef->sendEvent(DeadPlayerEvent{*this});
            }
        }
    }
}