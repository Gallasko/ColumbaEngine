#include "stdafx.h"

#include "Init/inputsystems.h"

#include "ECS/entitysystem.h"
#include "Input/inputcomponent.h"
#include "Input/input.h"
#include "UI/textinput.h"
#include "Systems/coresystems.h"

namespace pg
{
    void registerInputSystems(EntitySystem* ecs, Input* inputHandler)
    {
        ecs->createSystem<MouseClickSystem>(inputHandler);
        ecs->createSystem<MouseLeaveClickSystem>(inputHandler);
        ecs->createSystem<MouseWheelSystem>(inputHandler);
        ecs->createSystem<MouseHoverSystem>();
        ecs->createSystem<TextInputSystem>(inputHandler);

        // Ordering
        ecs->succeed<MouseClickSystem, TickingSystem>();
    }
}
