#pragma once

#include "Scene/scenemanager.h"

#include "Core/tokens.h"
#include "Core/textstyle.h"
#include "UI/paint.h"

namespace chronicle
{
    // Dev scene: four columns exercising Label's alignment and the three overflows.
    struct LabelGallery : public pg::Scene
    {
        LabelGallery(Tokens* tokens, TextStyles* styles) : tokens(tokens), styles(styles) {}

        virtual void init() override;

    private:
        Tokens* tokens = nullptr;
        TextStyles* styles = nullptr;
        PaintSystem* paintSystem = nullptr;

        pg::_unique_id backgroundId = 0;
    };
}
