#pragma once

namespace pg
{
    class EntitySystem;
    class MasterRenderer;
    struct VM;

    MasterRenderer* registerRenderSystems(EntitySystem* ecs, VM* vm, int width, int height);
}
