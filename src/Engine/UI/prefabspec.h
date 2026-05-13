#pragma once

#include "Memory/elementtype.h"
#include "2D/position.h"

#include <string>
#include <variant>
#include <vector>

namespace pg
{
    /**
     * Declarative description of a UI prefab tree. Built at runtime by
     * buildPrefab(). The structure is intentionally JSON-shaped: every field is
     * either a primitive, a string, an `ElementType`, or a vector of one of the
     * above. No code, no closures, no type-erased payloads. This is what an
     * editor will eventually round-trip.
     *
     * Composition model:
     *   - A PrefabSpec produces a Prefab entity wrapping a tree of children.
     *   - `mainNode` is the visually-dominant node (e.g. background); the
     *     PrefabSystem auto-anchors the prefab container to its size+position.
     *   - `children` are additional entities. Each entry is either a leaf
     *     NodeSpec or a nested PrefabSpec sub-tree (composition is recursive).
     *   - Each child can anchor to `mainNode` (target = "main"), to the prefab
     *     itself (target = "parent"), or to any sibling by name.
     *   - PrefabSpec itself also carries `name` and `anchors` so a PrefabSpec
     *     value can be dropped directly into another PrefabSpec's `children`
     *     and behave like any other child.
     *   - `kind` selects how a NodeSpec is realised:
     *       "Shape2D"          -> makeUiSimple2DShape
     *       "TTFText"          -> makeTTFText
     *       "Texture"          -> makeUiTexture
     *       "Factory:<name>"   -> PrefabFactoryRegistry::build(<name>, props)
     *
     * Anchor sides (`AnchorSpec::side`):
     *   - Top / Bottom / Left / Right       -> cardinal anchors with margin
     *   - Width / Height                    -> size constrain to target
     *   - VerticalCenter / HorizontalCenter -> center this node on target axis
     *     (use both sides to fully center an entity inside another)
     *
     * Anchor resolution (three-tier, applied uniformly to root, nested, and
     * leaf anchors):
     *   1. AnchorSpec::targetId  != 0 -> look up entity by id directly.
     *   2. AnchorSpec::target    in the prefab's local name map (siblings +
     *                            "main" + "parent") -> use it.
     *   3. AnchorSpec::target    in the global EntityNameSystem -> use it.
     *   4. otherwise             -> skip (empty target) or log error.
     */

    struct PrefabSpec;  // forward declaration for recursive variant

    struct AnchorSpec
    {
        AnchorSpec() = default;
        AnchorSpec(const std::string& target, AnchorType side, float margin) : target(target), side(side), margin(margin) {}
        AnchorSpec(const std::string& target, AnchorType side, AnchorType targetSide = AnchorType::None, float margin = 0.0f) : target(target), side(side), targetSide(targetSide), margin(margin) {}
        AnchorSpec(_unique_id targetId, AnchorType side, float margin) : targetId(targetId), side(side), margin(margin) {}
        AnchorSpec(_unique_id targetId, AnchorType side, AnchorType targetSide = AnchorType::None, float margin = 0.0f) : targetId(targetId), side(side), targetSide(targetSide), margin(margin) {}

        std::string target;            // "main", "parent", another node's `name`, or a globally-named entity
        _unique_id  targetId   = 0;    // when non-zero, bypasses name lookup and resolves the entity directly
        AnchorType  side       = AnchorType::None;  // which side of THIS node to anchor (Top/Bottom/Left/Right/Width/Height/VerticalCenter/HorizontalCenter)
        AnchorType  targetSide = AnchorType::None;  // which side of `target` to anchor to (defaults to `side`)
        float       margin     = 0.0f;
    };

    struct NodeSpec
    {
        std::string kind;              // see kind dispatch above
        ElementMap  props;             // editor-introspectable parameters
        std::string name;              // optional, enables anchor lookup from siblings
        std::vector<AnchorSpec> anchors;
    };

    /**
     * Build-time anchor sugar for the "row of widgets" / "column of rows" pattern.
     *
     * When a PrefabSpec sets `flow` to Horizontal or Vertical, every child whose
     * `anchors` vector is empty has anchors auto-generated:
     *   - cross-axis anchored to `main` with `padding`
     *   - main-axis anchored to either `main` (first in-flow child) or the
     *     previous in-flow sibling (via targetId) with `spacing`
     *
     * Children that set ANY anchor of their own are skipped entirely by the
     * flow and are transparent to the chain — the next in-flow child still
     * chains from the previous in-flow sibling, not the manually anchored one.
     *
     * For runtime-reactive layouts (children added/removed, sizes changing)
     * use HorizontalLayout / VerticalLayout instead — those reflow each tick.
     */
    enum class Flow
    {
        None,
        Horizontal,
        Vertical,
    };

    struct PrefabSpec
    {
        NodeSpec mainNode;                                          // becomes the prefab's MainEntity
        std::string name;                                           // optional; positions the prefab in its parent's name scope
        std::vector<AnchorSpec> anchors;                            // applied to the prefab container itself
        std::vector<std::variant<NodeSpec, PrefabSpec>> children;   // leaf nodes and nested sub-prefabs, in declaration order

        Flow  flow    = Flow::None;     // when non-None, auto-anchors children with empty anchor lists
        float padding = 0.0f;            // distance between parent edge and first/each child (both axes)
        float spacing = 0.0f;            // gap between adjacent in-flow children on the main axis
    };

    // One-line helper for the common "center this entity on target" pattern.
    // Expands to two AnchorSpecs (vertical + horizontal). Usage:
    //   node.anchors = centerInAnchors("main");
    //   spec.anchors = centerInAnchors(existingEnt->id);
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
