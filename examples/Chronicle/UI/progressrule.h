#pragma once

#include <optional>
#include <string>

#include "ECS/entitysystem.h"

#include "Core/tokens.h"
#include "Core/textstyle.h"

#include "label.h"
#include "mark.h"

namespace chronicle
{
    struct ProgressRuleSpec
    {
        float width = 240.0f;          // required (> 0)
        bool small = false;            // 6 px track instead of 10
        float percent = 0.0f;          // 0-100, clamped
        float forecastPercent = 0.0f;  // 0 = no forecast
        bool nib = true;
        std::string caption = "";      // "" = none; the figures, in caps
        int z = 20;
        float trackHeight = 0.0f;      // 0 -> 10 (or 6 with `small`); explicit wins over `small`
    };

    struct ProgressRule
    {
        pg::EntityRef root;            // width = spec.width, height = track (+ 4 + 15 with a caption)
        pg::EntityRef track;           // Simple2DObject progress-track
        pg::EntityRef frame;           // StrokeRect 1 px rule-hair
        pg::EntityRef fill;            // Simple2DObject progress-ink (carries the tween)
        pg::EntityRef forecast;        // HatchRect progress-forecast
        std::optional<Mark> nib;       // quill S14
        std::optional<Label> caption;
        ProgressRuleSpec spec;
        float shown = 0.0f;            // the percent currently DRAWN (differs from spec.percent while animating)

        const Tokens* tokens = nullptr;       // for setCaption/setNib building parts
        const TextStyles* styles = nullptr;   // for setCaption re-fitting

        // Setters - the whole public surface. No getters that compute.
        void setPercent(pg::EntitySystem*, float percent, bool animate = true);
        void setForecast(pg::EntitySystem*, float forecastPercent);
        void setCaption(pg::EntitySystem*, const TextStyles&, const std::string&);
        void setNib(pg::EntitySystem*, const Tokens&, bool);
        void setWidth(pg::EntitySystem*, float);

        void layoutAt(float shown);    // moves fill/forecast/nib to a drawn percent (internal)
    };

    ProgressRule makeProgressRule(pg::EntitySystem*, const Tokens&, const TextStyles&, const ProgressRuleSpec&);
}
