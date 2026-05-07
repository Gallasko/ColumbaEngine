#pragma once

#include "prefabspec.h"
#include "ECS/entitysystem_fwd.h"

namespace pg
{
    /**
     * Materialise a PrefabSpec into a live ECS prefab entity.
     *
     *   - Wraps the tree in a Prefab + UiAnchor + Position container.
     *   - Realises each node via its `kind` (see prefabspec.h).
     *   - Resolves anchor targets by name. Reserved names: "main" (the
     *     prefab's main entity) and "parent" (the prefab container itself).
     *   - Names with empty strings are not registered and cannot be referenced.
     *
     * Returns the prefab container entity. Call `prefabRef->get<Prefab>()` to
     * walk children, register helpers, or look up named entities.
     */
    EntityRef buildPrefab(EntitySystem* ecs, const PrefabSpec& spec);

    EntityRef buildNode(EntitySystem* ecs, const NodeSpec& spec);
}
