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
     * One rule:
     *   - `kind == "Layout:Horizontal" | "Layout:Vertical"` produces a layout entity; children
     *     are added via `addEntity` and the runtime LayoutSystem reflows them.
     *   - Every other kind ALWAYS produces a Prefab container. The kind drives what becomes the
     *     container's mainEntity:
     *       * non-empty kind -> looked up in `PrefabFactoryRegistry` (built-ins like `Shape2D`,
     *         `TTFText`, `Texture` are registered at engine init alongside user factories like
     *         `Panel`, `Slot`, etc.). The factory's returned entity is the mainEntity.
     *       * empty kind     -> no mainEntity; `children` are all siblings of a bare container.
     *     `children` are realised recursively (each becomes its own wrap unless it's a layout)
     *     and anchored as siblings of the mainEntity inside this prefab.
     *
     * Name resolution (sibling/parent anchor scope):
     *   Each child registers under `name` in its parent's name map. The stored entity is the
     *   child's mainEntity (unwrapped from its Prefab container) so that
     *   `parent->get<Prefab>()->getEntity("bg")->get<Simple2DObject>()` works ergonomically.
     *   Anchoring a sibling to `"bg"` resolves to the leaf — geometrically identical to
     *   targeting the wrap (the leaf is auto-anchored top-left and width/height-constrained
     *   to its wrap container).
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
