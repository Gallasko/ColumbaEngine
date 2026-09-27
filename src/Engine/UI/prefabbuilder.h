#pragma once

#include "prefabspec.h"
#include "ECS/entitysystem.h"

#include <string>
#include <unordered_map>

namespace pg
{
    /**
     * Materialise a NodeSpec into a live ECS entity tree and return its root.
     *
     * The root always carries a Prefab: every kind, including a top-level layout, is wrapped in a
     * Prefab + UiAnchor + Position container whose main entity is what the kind produced. Named
     * nodes are reachable through it:
     *
     *     EntityRef page = buildTree(ecs, spec);
     *     auto fed = page->get<Prefab>()->getEntity("fed");
     *     fed->get<Prefab>()->callHelper("setItem", size_t{0}, 18, 18);
     *
     * Dispatch by `kind`:
     *   - Primitive kinds (Shape2D / TTFText / Texture / user factories) are wrapped; the leaf
     *     becomes the mainEntity, children sit alongside as anchored siblings (with optional
     *     Flow-synthesised default anchors), or inside the factory's slot layout.
     *   - Layout kinds (Layout:Horizontal / Layout:Vertical) produce a layout entity; children
     *     are added through `addEntity` and reflow at runtime. The names of a layout's children
     *     land on the nearest enclosing Prefab.
     *   - Empty kind ("") + children produces a bare container prefab with no mainEntity.
     *
     * Returns an empty ref when the root kind is unknown.
     */
    EntityRef buildTree(EntitySystem* ecs, const NodeSpec& spec);
}
