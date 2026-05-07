#pragma once

#include "Memory/elementtype.h"
#include "2D/position.h"

#include <string>
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
     *   - `children` are additional named entities. Each child can anchor to
     *     `mainNode` (target = "main"), to the prefab itself (target = "parent")
     *     or to any sibling by name.
     *   - `kind` selects how the node is realised:
     *       "Shape2D"          -> makeUiSimple2DShape
     *       "TTFText"          -> makeTTFText
     *       "Texture"          -> makeUiTexture
     *       "Factory:<name>"   -> PrefabFactoryRegistry::build(<name>, props)
     */

    struct AnchorSpec
    {
        std::string target;            // "main", "parent", or another node's `name`
        AnchorType  side       = AnchorType::None;  // which side of THIS node to anchor
        AnchorType  targetSide = AnchorType::None;  // which side of `target` to anchor to (defaults to `side`)
        float       margin     = 0.0f;
    };

    struct CenterInSpec
    {
        std::string target;            // node to center this node in
    };

    struct NodeSpec
    {
        std::string kind;              // see kind dispatch above
        ElementMap  props;             // editor-introspectable parameters
        std::string name;              // optional, enables anchor lookup from siblings
        std::vector<AnchorSpec>   anchors;
        std::vector<CenterInSpec> centerIn;
        std::vector<NodeSpec>     children;  // honoured by kinds that compose (factories may ignore)
    };

    struct PrefabSpec
    {
        NodeSpec mainNode;             // becomes the prefab's MainEntity
        std::vector<NodeSpec> children;
    };
}
