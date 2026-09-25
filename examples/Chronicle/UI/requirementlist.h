#pragma once

#include <optional>
#include <string>
#include <vector>

#include "ECS/entitysystem.h"

#include "Core/tokens.h"
#include "Core/textstyle.h"

#include "label.h"
#include "mark.h"

namespace chronicle
{
    // One prerequisite: a label, and either the numeric pair "current / needed" or an
    // explicit verdict. The list never decides anything - `met` is computed by the scene,
    // or, for numeric rows with no explicit `met`, by the trivial display rule below.
    struct Requirement
    {
        std::string label;             // "Swordsmanship", "Has the Guild's letter"
        int current = -1;              // < 0 = non-numeric: no "current / needed" shown; `met` must then be given
        int needed = 0;
        int met = -1;                  // -1 = derive (current >= needed); 0/1 = as the scene says. Explicit wins.

        bool isMet() const { return met >= 0 ? met != 0 : (current >= 0 and current >= needed); }
    };

    struct RequirementListSpec
    {
        float width = 256.0f;
        bool dense = false;            // tick (13/16) instead of body-sm (14/20); rows 4 apart either way
        std::vector<Requirement> items;
        int z = 20;                    // root z; rows z+0, marks/labels z+1
    };

    // Prerequisites as a checked list: each row carries a mark as well as a colour -
    // `check` in status-gain when met, `cross` in status-loss when not - and shows the
    // pair "current / needed", never a verdict. The mark and the value take the tone;
    // the label stays ink (unmet) or recedes to ink-muted (met): the mark carries the
    // state, the label carries the words.
    struct RequirementList
    {
        pg::EntityRef root;            // PositionComponent + UiAnchor + Prefab; height = n x lineH + (n - 1) x 4
        struct Row
        {
            MarkedLabel name;
            std::optional<Label> value;
            Requirement item;
        };
        std::vector<Row> rows;
        RequirementListSpec spec;

        const Tokens* tokens = nullptr;
        const TextStyles* styles = nullptr;

        void setItems(pg::EntitySystem*, const Tokens&, const TextStyles&, const std::vector<Requirement>&);
        void setItem(pg::EntitySystem*, const TextStyles&, size_t index, int current, int needed);
        void setMet(pg::EntitySystem*, size_t index, bool met);
        void clearMet(pg::EntitySystem*, size_t index);
        float height(pg::EntitySystem*) const;
        size_t size() const { return rows.size(); }

        void repaintRow(pg::EntitySystem*, size_t index);   // tokens only; nothing moves (internal)
    };

    RequirementList makeRequirementList(pg::EntitySystem*, const Tokens&, const TextStyles&, const RequirementListSpec&);
}
