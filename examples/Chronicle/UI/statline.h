#pragma once

#include <optional>
#include <string>

#include "ECS/entitysystem.h"

#include "Core/tokens.h"
#include "Core/textstyle.h"

#include "label.h"
#include "mark.h"
#include "progressrule.h"

namespace chronicle
{
    // A stat line: the figure is the point, the bar is the glance. A name with its mark at the
    // left, the figure at the right with the running activity's projection beside it (`-> 17`),
    // then a groove (a ProgressRule, nib off, 8 px track) whose solid fill is the present value
    // and hatched ghost the projected gain, with a status-time tick standing on the track where
    // the next milestone's requirement sits and a note beneath naming it. Fed, not driven:
    // setValue / setProjected / setThreshold / setNote.
    struct StatLineSpec
    {
        float width = 288.0f;          // the left column's inner width (320 - 2 x 16)
        std::string label = "Strength";// upper-cased for display (ASCII only; `label` is the caps style)
        std::string glyph = "strength";
        int value = 0;                 // clamped to [0, max]
        int max = 30;                  // > 0; the groove's full length
        int projected = -1;            // < 0 = none; clamped to [0, max]; shown only when > value
        int threshold = 0;             // 0 = none; clamped to [1, max]
        std::string note = "";         // "" = none; caps ("WARRIOR AT 18 ASKS 18")
        std::string glossKey = "";     // "" = none; else attachGloss(root, key)
        int z = 20;                    // root z; head parts z+1 (texts z+2); groove z+1; threshold z+6
    };

    struct StatLine
    {
        pg::EntityRef root;            // PositionComponent + UiAnchor + Prefab: width = spec.width; height 34 (53 w/ note)
        MarkedLabel name;              // mark S16 + label style, both ink-muted
        Label figure;                  // `figure`, ink, right edge is the anchor (figure does not reflow)
        std::optional<Label> projected;// `tick`, progress-forecast, "-> 17"
        ProgressRule groove;           // nib false, trackHeight 8, width = spec.width
        pg::EntityRef threshold;       // Simple2DObject 2 x 14, status-time; hidden when none
        std::optional<Label> note;     // `caption`, ink-faint
        StatLineSpec spec;

        const Tokens* tokens = nullptr;
        const TextStyles* styles = nullptr;

        void setValue(pg::EntitySystem*, const TextStyles&, int value, bool animate = true);
        void setProjected(pg::EntitySystem*, const TextStyles&, int projected);
        void setThreshold(pg::EntitySystem*, int threshold);
        void setNote(pg::EntitySystem*, const TextStyles&, const std::string&);
        float height(pg::EntitySystem*) const;

        // internal
        void placeFigureRight(pg::EntitySystem*);   // right edge -> projection's left, else root right
        void placeThreshold();                      // re-place the tick and set its visibility
    };

    StatLine makeStatLine(pg::EntitySystem*, const Tokens&, const TextStyles&, const StatLineSpec&);
}
