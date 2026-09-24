#pragma once

#include <array>

#include "Scene/scenemanager.h"

#include "Core/tokens.h"
#include "Core/textstyle.h"
#include "UI/paint.h"
#include "UI/label.h"
#include "UI/statline.h"

namespace chronicle
{
    // Dev scene: a Parts panel of four StatLines on the left, fed entirely through GameDataView;
    // buttons on the right write those paths (never a setter directly); a wide bare line below.
    // T toggles theme; R toggles reduced motion.
    struct StatGallery : public pg::Scene
    {
        StatGallery(Tokens* tokens, TextStyles* styles) : tokens(tokens), styles(styles) {}

        virtual void init() override;

    private:
        Tokens* tokens = nullptr;
        TextStyles* styles = nullptr;

        pg::_unique_id backgroundId = 0;

        // Stable addresses: each groove's tween captures its StatLine by pointer.
        std::array<StatLine, 4> parts;
        StatLine big;

        Label echo;
        Label motionLabel;
    };
}
