#pragma once

#include "prefabspec.h"
#include "ECS/entitysystem.h"

#include <any>
#include <string>
#include <unordered_map>

namespace pg
{
    /**
     * The outcome of `buildTree()`: the produced root entity plus every handle the factories
     * returned, keyed by node name. A handle is the factory's own result struct (whatever it
     * chose to put in `FactoryResult::handle`), retrieved with its exact type:
     *
     *     auto built = buildTree(ecs, spec);
     *     if (auto* panel = built.get<chronicle::Panel>("skills"))
     *         panel->setHeading(ecs, styles, "Skills");
     *
     * `get` returns nullptr when the name is unknown or the stored type differs.
     */
    struct PrefabBuildResult
    {
        EntityRef root;
        std::unordered_map<std::string, std::any> handles;

        template <typename T>
        T* get(const std::string& name)
        {
            auto it = handles.find(name);
            if (it == handles.end())
                return nullptr;

            return std::any_cast<T>(&it->second);
        }

        template <typename T>
        const T* get(const std::string& name) const
        {
            auto it = handles.find(name);
            if (it == handles.end())
                return nullptr;

            return std::any_cast<T>(&it->second);
        }

        bool has(const std::string& name) const { return handles.find(name) != handles.end(); }
    };

    /**
     * Materialise a NodeSpec into a live ECS entity tree.
     *
     * Single entry point for the unified spec. Dispatches by `kind`:
     *   - Primitive kinds (Shape2D / TTFText / Texture / user factories) are wrapped in a
     *     Prefab + UiAnchor + Position container; the leaf becomes the mainEntity, children
     *     sit alongside as anchored siblings (with optional Flow-synthesised default anchors)
     *     — or, when the factory exposes a slot, inside that slot's layout.
     *   - Layout kinds (Layout:Horizontal / Layout:Vertical) produce a single entity with the
     *     layout component attached; children are registered via `layout->addEntity(...)`
     *     and reflow at runtime.
     *   - Empty kind ("") + children produces a bare container prefab with no mainEntity —
     *     useful for grouping entities without a backdrop.
     *
     * `buildTree` returns the root and the named handles; `buildNode` is the root-only form.
     * For composite results, call `root->get<Prefab>()` to walk children, or use the layout
     * component's API.
     */
    PrefabBuildResult buildTree(EntitySystem* ecs, const NodeSpec& spec);
    EntityRef buildNode(EntitySystem* ecs, const NodeSpec& spec);
}
