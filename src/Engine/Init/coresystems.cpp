#include "stdafx.h"

#include "Init/coresystems.h"

#include "ECS/entitysystem.h"
#include "ECS/loggersystem.h"
#include "Systems/coresystems.h"
#include "Systems/scriptrunner.h"
#include "UI/focusable.h"

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
        ecs->createSystem<ScriptRunnerSystem>();

        // Ordering
        ecs->succeed<TickingSystem, ScriptRunnerSystem>();
        ecs->succeed<TimerSystem, TickingSystem>();
    }
}
