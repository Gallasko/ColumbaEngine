#pragma once

namespace pg
{
    class PrefabFactoryRegistry;

    /**
     * Register the engine's built-in prefab factories with the given registry.
     * Should be called once during application startup, after the registry
     * itself has been created.
     *
     * Currently registers:
     *   "Panel"     — backdrop rectangle, optionally centered in a target.
     *   "TitleBar"  — backdrop with a title text anchored top-left.
     *
     * Games are free to override any of these by re-registering under the same
     * name with a different schema/factory.
     */
    void registerEnginePrefabFactories(PrefabFactoryRegistry* registry);
}
