#pragma once

#include "commons.h"

namespace pg
{
    struct CharacterStat
    {
        int health = 0;

        int physicalAttack = 0;
        int magicalAttack = 0;

        int physicalDefense = 0;
        int magicalDefense = 0;

        int speed = 0;

        // In percentage
        int critChance = 0;
        int critDamage = 0;
        int evasionRate = 0;

        // Todo
        // std::map<std::string, int> values = { {"health", 0}, };

        int elementalRes[NbElements] = {0};

        CharacterStat& operator+=(const CharacterStat& other)
        {
            health += other.health;
            physicalAttack += other.physicalAttack;
            magicalAttack += other.magicalAttack;
            physicalDefense += other.physicalDefense;
            magicalDefense += other.magicalDefense;
            speed += other.speed;
            critChance += other.critChance;
            critDamage += other.critDamage;
            evasionRate += other.evasionRate;

            for (size_t i = 0; i < NbElements; i++)
            {
                elementalRes[i] += other.elementalRes[i];
            }

            return *this;
        }

        CharacterStat& operator-=(const CharacterStat& other)
        {
            health -= other.health;
            physicalAttack -= other.physicalAttack;
            magicalAttack -= other.magicalAttack;
            physicalDefense -= other.physicalDefense;
            magicalDefense -= other.magicalDefense;
            speed -= other.speed;
            critChance -= other.critChance;
            critDamage -= other.critDamage;
            evasionRate -= other.evasionRate;

            for (size_t i = 0; i < NbElements; i++)
            {
                elementalRes[i] -= elementalRes[i];
            }

            return *this;
        }
    };

    template <>
    void serialize(Archive& archive, const CharacterStat& value);

    template <>
    CharacterStat deserialize(const UnserializedObject& serializedString);

    struct BaseCharacterStat : public CharacterStat
    {
        BaseCharacterStat()
        {
            health = 100;
            physicalAttack = 10;
            magicalAttack = 10;

            physicalDefense = 1;
            magicalDefense = 1;
            
            speed = 100;

            critChance = 5;
            critDamage = 150;
            evasionRate = 5;
        }
    };
}