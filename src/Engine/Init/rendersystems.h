#pragma once

namespace pg
{
    class EntitySystem;
    class PgInterpreter;
    class MasterRenderer;

    MasterRenderer* registerRenderSystems(EntitySystem* ecs, PgInterpreter* interpreter, int width, int height);
}
