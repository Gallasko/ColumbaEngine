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
     *   - PrefabSpec itself also carries `name`, `anchors` and `centerIn` so a
     *     PrefabSpec value can be dropped directly into another PrefabSpec's
     *     `children` and behave like any other child.
     *   - `kind` selects how a NodeSpec is realised:
     *       "Shape2D"          -> makeUiSimple2DShape
     *       "TTFText"          -> makeTTFText
     *       "Texture"          -> makeUiTexture
     *       "Factory:<name>"   -> PrefabFactoryRegistry::build(<name>, props)
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
        AnchorType  side       = AnchorType::None;  // which side of THIS node to anchor
        AnchorType  targetSide = AnchorType::None;  // which side of `target` to anchor to (defaults to `side`)
        float       margin     = 0.0f;
    };

    struct CenterInSpec
    {
        std::string target;            // node to center this node in
        _unique_id  targetId = 0;      // when non-zero, bypasses name lookup
    };

    struct NodeSpec
    {
        std::string kind;              // see kind dispatch above
        ElementMap  props;             // editor-introspectable parameters
        std::string name;              // optional, enables anchor lookup from siblings
        std::vector<AnchorSpec>   anchors;
        std::vector<CenterInSpec> centerIn;
    };

    struct PrefabSpec
    {
        NodeSpec mainNode;                                            // becomes the prefab's MainEntity
        std::string name;                                             // optional; positions the prefab in its parent's name scope
        std::vector<AnchorSpec>   anchors;                            // applied to the prefab container itself
        std::vector<CenterInSpec> centerIn;                           // applied to the prefab container itself
        std::vector<std::variant<NodeSpec, PrefabSpec>> children;     // leaf nodes and nested sub-prefabs, in declaration order
    };
}
