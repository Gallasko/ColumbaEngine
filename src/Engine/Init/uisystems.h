#pragma once

namespace pg
{
    class EntitySystem;

    void registerUiSystems(EntitySystem* ecs);

    // Opt in: the prefab, layout and position systems join the ecs' settle phase, so that a change of
    // the page reaches every position it moves in the pass where it was made (EntitySystem::addSettleSystem).
    // A system of the application that places things (it sets positions in its execute) is added after this
    void settleUiInBasicTask(EntitySystem* ecs);
}
