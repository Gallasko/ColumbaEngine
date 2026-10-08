#pragma once

#include "ECS/entitysystem.h"
#include "2D/position.h"
#include "Systems/gamefacts.h"
#include "UI/prefab.h"
#include "UI/sizer.h"

#include "UI/activityrow.h"

namespace chronicle
{
    // The systems a change of the page goes through, in the order it goes through them: the facts
    // reach the widgets that follow them, a prefab shows or clips its parts, a list adopts its rows,
    // the layouts place what they hold and the solver moves what hangs on it. In the ecs' settle
    // phase they answer each other until the page is at rest, in the pass where the change was made
    // (pg::EntitySystem::addSettleSystem).
    inline void settleThePage(pg::EntitySystem* ecs)
    {
        ecs->addSettleSystem<pg::WorldFacts>();
        ecs->addSettleSystem<pg::PrefabSystem>();
        ecs->addSettleSystem<ActivitySystem>();
        ecs->addSettleSystem<pg::LayoutSystem>();
        ecs->addSettleSystem<pg::PositionComponentSystem>();
    }
}
