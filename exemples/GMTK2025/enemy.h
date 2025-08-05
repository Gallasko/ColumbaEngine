#pragma once

#include "ECS/componentregistry.h"

namespace pg
{
    struct EnemyFlag : public Component
    {
        EnemyFlag(size_t hp) : hp(hp) {}

        DEFAULT_COMPONENT_MEMBERS(EnemyFlag)

        size_t hp = 1;
    };
}