#pragma once

namespace pg
{
    class EntitySystem;
    class Input;

    void registerInputSystems(EntitySystem* ecs, Input* inputHandler);
}
