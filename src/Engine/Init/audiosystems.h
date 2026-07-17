#pragma once

namespace pg
{
    class EntitySystem;
    struct AudioSystem;

    AudioSystem* registerAudioSystem(EntitySystem* ecs);
}
