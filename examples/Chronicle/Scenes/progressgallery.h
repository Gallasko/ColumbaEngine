#pragma once

#include "Scene/scenemanager.h"

#include "UI/themesystem.h"
#include "UI/label.h"
#include "UI/progressrule.h"

namespace chronicle
{
    // Dev scene: static rules on the left, the running activity (rule + buttons that feed it) on
    // the right. T toggles theme; R toggles reduced motion.
    struct ProgressGallery : public pg::Scene
    {
        virtual void init() override;

    private:
        pg::ThemeSystem* theme = nullptr;

        pg::_unique_id backgroundId = 0;

        ProgressRule running;   // stable: its tween captures &running
        Label motionLabel;
        int month = 0;
    };
}
