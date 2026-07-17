#pragma once

#include "prefabspec.h"
#include "ECS/entitysystem_fwd.h"

namespace pg
{
    /**
     * Materialise a NodeSpec into a live ECS entity.
     *
     * Single entry point for the unified spec. Dispatches by `kind`:
     *   - Primitive kinds (Shape2D / TTFText / Texture / Factory:*) with no children
     *     produce a leaf entity directly.
     *   - Primitive kinds with children are wrapped in a Prefab + UiAnchor + Position
     *     container; the leaf becomes the mainEntity, children sit alongside as
     *     anchored siblings (with optional Flow-synthesised default anchors).
     *   - Layout kinds (Layout:Horizontal / Layout:Vertical) produce a single
     *     entity with the layout component attached; children are registered via
     *     `layout->addEntity(...)` and reflow at runtime.
     *   - Empty kind ("") + children produces a bare container prefab with no
     *     mainEntity — useful for grouping entities without a backdrop.
     *
     * Returns the produced entity (container for prefabs, layout entity for layouts,
     * leaf entity for primitives). For composite results, call `entity->get<Prefab>()`
     * to walk children, or use the layout component's API.
     */
    EntityRef buildNode(EntitySystem* ecs, const NodeSpec& spec);
}
