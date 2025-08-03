#pragma once

#include "ECS/componentregistry.h"

namespace pg
{
    struct EnemyFlag : public Component
    {
        size_t hp = 1;
        
        EnemyFlag() = default;
        EnemyFlag(size_t hp) : hp(hp) {}

        EnemyFlag(const EnemyFlag& other) : hp(other.hp) {}
        
        EnemyFlag& operator=(const EnemyFlag& other)
        {
            hp = other.hp;

            return *this;
        }
    };
}