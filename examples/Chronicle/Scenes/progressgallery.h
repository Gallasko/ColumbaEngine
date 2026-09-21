#pragma once

#include "Scene/scenemanager.h"

#include "Core/tokens.h"
#include "Core/textstyle.h"
#include "UI/paint.h"
#include "UI/label.h"
#include "UI/progressrule.h"

namespace chronicle
{
    // Dev scene: static rules on the left, the running activity (rule + buttons that feed it) on
    // the right. T toggles theme; R toggles reduced motion.
    struct ProgressGallery : public pg::Scene
    {
        ProgressGallery(Tokens* tokens, TextStyles* styles) : tokens(tokens), styles(styles) {}

        virtual void init() override;

    private:
        Tokens* tokens = nullptr;
        TextStyles* styles = nullptr;
        PaintSystem* paintSystem = nullptr;

        pg::_unique_id backgroundId = 0;

        ProgressRule running;   // stable: its tween captures &running
        Label motionLabel;
        int month = 0;
    };
}
