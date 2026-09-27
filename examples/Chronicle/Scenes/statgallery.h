#pragma once

#include <array>

#include "Scene/scenemanager.h"

#include "UI/themesystem.h"
#include "UI/label.h"
#include "UI/statline.h"

namespace chronicle
{
    // Dev scene: a Parts panel of four StatLines on the left, fed entirely through WorldFacts;
    // buttons on the right write those facts (never a setter directly); a wide bare line below.
    // T toggles theme; R toggles reduced motion.
    struct StatGallery : public pg::Scene
    {
        virtual void init() override;

    private:
        pg::ThemeSystem* theme = nullptr;

        pg::_unique_id backgroundId = 0;

        // Stable addresses: each groove's tween captures its StatLine by pointer.
        std::array<StatLine, 4> parts;
        StatLine big;

        Label echo;
        Label motionLabel;
    };
}
