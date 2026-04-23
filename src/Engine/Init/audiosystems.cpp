#include "stdafx.h"

#include "Init/audiosystems.h"

#include "ECS/entitysystem.h"
#include "Audio/audiosystem.h"

namespace pg
{
    AudioSystem* registerAudioSystem(EntitySystem* ecs)
    {
        return ecs->createSystem<AudioSystem>();
    }
}
