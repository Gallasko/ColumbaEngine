#pragma once

#include "ECS/system.h"
#include "ECS/entitysystem.h"
#include "ECS/scriptregistry.h"

#include "Systems/coresystems.h"

namespace pg
{
    /**
     * @brief Polls the ScriptRegistry for on-disk script changes (hot reload).
     *
     * Opt-in, native-dev feature: create it once at startup and every script
     * loaded through the registry becomes hot reloadable. Not useful under
     * Emscripten where scripts ship inside the preloaded data bundle.
     *
     * @code
     * #ifndef __EMSCRIPTEN__
     *     ecs.createSystem<ScriptWatcherSystem>();
     * #endif
     * @endcode
     */
    struct ScriptWatcherSystem : public System<DeltaTime>
    {
        /** @brief Time between mtime sweeps, in seconds. */
        float pollInterval = 0.25f;

        virtual std::string getSystemName() const override { return "Script Watcher System"; }

        virtual void onExecute(float deltaTime) override
        {
            accumulated += deltaTime;

            if (accumulated < pollInterval)
                return;

            accumulated = 0.0f;

            ecsRef->scripts().pollForChanges();
        }

    private:
        float accumulated = 0.0f;
    };
}
