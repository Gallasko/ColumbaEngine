#pragma once

#include "Memory/elementtype.h"
#include "2D/position.h"

#include <string>
#include <vector>

namespace pg
{
    /**
     * Declarative description of a UI tree. Built at runtime by `buildNode()`.
     *
     * One struct, one entry point. A `NodeSpec` can be:
     *   - A primitive leaf  (`kind = "Shape2D" | "TTFText" | "Texture" | "Factory:<name>"`)
     *   - A composite       (any leaf kind + non-empty `children`): the leaf becomes the
     *                       prefab's mainEntity and children sit alongside in a prefab container.
     *   - A layout          (`kind = "Layout:Horizontal" | "Layout:Vertical"`): produces a
     *                       single entity with HorizontalLayout / VerticalLayout attached;
     *                       children are added via the layout (reflowed at runtime by LayoutSystem).
     *   - A bare container  (`kind = ""` + children): a prefab container with no mainEntity
     *                       and no backdrop — useful for "just group these entities together".
     *
     * `kind` dispatch:
     *   "Shape2D"          -> makeUiSimple2DShape
     *   "TTFText"          -> makeTTFText
     *   "Texture"          -> makeUiTexture
     *   "Factory:<name>"   -> PrefabFactoryRegistry::build(<name>, props)
     *   "Layout:Horizontal" / "Layout:Vertical" -> makeHorizontal/VerticalLayout, children added via layout->addEntity
     *   "Prefab"           -> ALWAYS wraps in a Prefab container, even with zero children.
     *                         When non-empty, children[0] becomes the mainEntity (auto-anchored
     *                         to the container top-left, container size constrained to it) and
     *                         children[1..] become anchored siblings. Use this when you want a
     *                         Prefab wrapper but don't need its leaf to carry kind-specific
     *                         visuals (e.g. the engine's "Panel" factory).
     *   ""                 -> no leaf entity; only meaningful when `children` is non-empty
     *
     * Wrapping rules:
     *   - Leaf kind + children non-empty: shorthand. Leaf becomes mainEntity, children are siblings.
     *   - kind == "Prefab": canonical wrap. children[0] is mainEntity, rest are siblings.
     *   - Leaf kind + empty children: just the leaf entity, no wrap.
     *
     * Anchor sides (`AnchorSpec::side`):
     *   - Top / Bottom / Left / Right       -> cardinal anchor with margin
     *   - Width / Height                    -> size constrain to target
     *   - VerticalCenter / HorizontalCenter -> center this node on target axis
     *     (combine both to fully center; see `centerInAnchors()` below)
     *
     * Anchor target resolution (three-tier, applied uniformly to root and children):
     *   1. AnchorSpec::targetId != 0 -> look up entity by id directly.
     *   2. AnchorSpec::target found in the local sibling/main/parent map -> use it.
     *   3. AnchorSpec::target found in the global EntityNameSystem -> use it.
     *   4. otherwise -> skip (empty target) or log + skip (non-empty target).
     *
     * Flow synthesis (build-time anchor sugar for "row of widgets" / "column of rows"):
     *   - `flow != None` auto-anchors children whose `anchors` is empty.
     *   - Children with any user-anchor are transparent to the chain (CSS `position: absolute`).
     *   - `padding` separates first child from `main`; `spacing` separates adjacent in-flow siblings.
     *   - For runtime-reactive layouts (children appearing/disappearing, size-dependent reflow,
     *     scroll), use the `Layout:Horizontal` / `Layout:Vertical` kinds instead — those produce
     *     a real LayoutSystem entity rather than baked anchors.
     *
     * `name` semantics:
     *   - When the node is nested as a child, `name` registers the produced entity in the
     *     PARENT prefab's child map (so siblings can anchor to it by name).
     *   - When the node also has its own children (prefab wrapping), the leaf (mainEntity) is
     *     additionally registered under the same `name` inside this node's own Prefab — so
     *     `parentPrefab->getEntity("bg")` returns the leaf, matching the old PrefabSpec idiom.
     */

    enum class Flow
    {
        None,
        Horizontal,
        Vertical,
    };

    struct AnchorSpec
    {
        AnchorSpec() = default;
        AnchorSpec(const std::string& target, AnchorType side, float margin) : target(target), side(side), margin(margin) {}
        AnchorSpec(const std::string& target, AnchorType side, AnchorType targetSide = AnchorType::None, float margin = 0.0f) : target(target), side(side), targetSide(targetSide), margin(margin) {}
        AnchorSpec(_unique_id targetId, AnchorType side, float margin) : targetId(targetId), side(side), margin(margin) {}
        AnchorSpec(_unique_id targetId, AnchorType side, AnchorType targetSide = AnchorType::None, float margin = 0.0f) : targetId(targetId), side(side), targetSide(targetSide), margin(margin) {}

        std::string target;            // "main", "parent", a sibling's `name`, or a globally-named entity
        _unique_id  targetId   = 0;    // when non-zero, bypasses name lookup
        AnchorType  side       = AnchorType::None;  // which side of THIS node to anchor (Top/Bottom/Left/Right/Width/Height/VerticalCenter/HorizontalCenter)
        AnchorType  targetSide = AnchorType::None;  // which side of `target` to anchor to (defaults to `side`)
        float       margin     = 0.0f;
    };

    struct NodeSpec
    {
        std::string kind;                       // see kind dispatch above
        ElementMap  props;                      // editor-introspectable parameters for the leaf
        std::string name;                       // optional; see name semantics above
        std::vector<AnchorSpec> anchors;        // applied to THIS node's produced entity
        std::vector<NodeSpec>   children;       // recursive composition

        Flow  flow    = Flow::None;             // build-time anchor sugar for children with empty anchors
        float padding = 0.0f;                   // first in-flow child distance from `main`
        float spacing = 0.0f;                   // gap between adjacent in-flow children
    };

    // Helper: returns the pair of anchors needed to fully center the entity on `target`.
    // Use as:
    //   node.anchors = centerInAnchors("main");
    //   node.anchors = centerInAnchors(existingEnt->id);
    inline std::vector<AnchorSpec> centerInAnchors(const std::string& target)
    {
        return {
            AnchorSpec{target, AnchorType::VerticalCenter},
            AnchorSpec{target, AnchorType::HorizontalCenter},
        };
    }

    inline std::vector<AnchorSpec> centerInAnchors(_unique_id targetId)
    {
        return {
            AnchorSpec{targetId, AnchorType::VerticalCenter},
            AnchorSpec{targetId, AnchorType::HorizontalCenter},
        };
    }
}
