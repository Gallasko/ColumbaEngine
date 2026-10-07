#pragma once

#include "ECS/entitysystem.h"
#include "2D/position.h"

namespace chronicle
{
    // Where a part made while the game runs is born. Such a part is drawn once where it was made,
    // before its anchors or its layout move it: made off the stage, that frame is not seen. What
    // places it afterwards (an anchor, a layout, a setX) is not hindered by it.
    constexpr float Offstage = -10000.0f;

    inline void offstage(pg::EntityRef entity)
    {
        auto pos = entity->get<pg::PositionComponent>();

        pos->setX(Offstage);
        pos->setY(Offstage);
    }
}
