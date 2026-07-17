#include "stdafx.h"

#include "Init/coresystems.h"

#include "ECS/entitysystem.h"
#include "ECS/loggersystem.h"
#include "Systems/coresystems.h"
#include "UI/focusable.h"
#include "Interpreter/pginterpreter.h"

namespace pg
{
    void registerCoreSystems(EntitySystem* ecs)
    {
        ecs->createSystem<EntityNameSystem>();
        ecs->createSystem<TickingSystem>();
        ecs->createSystem<TimerSystem>();
        ecs->createSystem<TerminalLogSystem>();
        ecs->createSystem<FocusableSystem>();
        ecs->createSystem<OnEventComponentSystem>();

        // Ordering
        ecs->succeed<TickingSystem, PgInterpreter>();
        ecs->succeed<TimerSystem, TickingSystem>();
    }
}
