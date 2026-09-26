#pragma once

#include "prefabspec.h"

#include <optional>
#include <string>
#include <vector>

namespace pg
{
    class EntitySystem;

    /**
     * Load a NodeSpec tree from a YAML-like text file.
     *
     * The file is parsed by the PgScript parser (`tools/yaml_parser.pg`), run through the
     * loader script (`res/scripts/ui_loader.pg` by default) on a throwaway VM set up by the
     * EntitySystem; the resulting table is then mapped onto a NodeSpec:
     *
     *   reserved keys      -> NodeSpec fields
     *     kind, name              strings
     *     x, y                    kept in props (the builder positions the wrap with them)
     *     anchors                 list of {side, target, targetSide?, margin?}
     *     children                list of nodes (recursive)
     *     flow                    "horizontal" | "vertical" | "none"
     *     padding, spacing        numbers (also copied into props for Layout:* kinds)
     *   other scalars      -> props (int / double / bool / string)
     *   list of maps       -> records[key] (each item a flat map of scalars)
     *   list of scalars    -> rejected with a log (not needed by any factory)
     *
     * Returns nullopt when the file cannot be read or does not hold a map. Every problem is
     * reported through the logger; `errors` (when given) receives the same messages.
     */
    struct PrefabLoadOptions
    {
        std::string loaderScript = "res/scripts/ui_loader.pg";
        std::vector<std::string>* errors = nullptr;
    };

    std::optional<NodeSpec> loadNodeSpec(EntitySystem* ecs, const std::string& yamlPath,
                                         const PrefabLoadOptions& options = PrefabLoadOptions{});
}
