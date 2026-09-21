#pragma once

#include "Scene/scenemanager.h"

#include "Core/tokens.h"
#include "Core/textstyle.h"
#include "UI/paint.h"
#include "UI/label.h"

namespace chronicle
{
    // Dev scene: the three button variants, the cost pair, disabled-with-reason, a dense
    // tab-order row, and an overlap check. T toggles theme; the last activated tag is shown.
    struct ButtonGallery : public pg::Scene
    {
        ButtonGallery(Tokens* tokens, TextStyles* styles) : tokens(tokens), styles(styles) {}

        virtual void init() override;

    private:
        Tokens* tokens = nullptr;
        TextStyles* styles = nullptr;
        PaintSystem* paintSystem = nullptr;

        pg::_unique_id backgroundId = 0;
        Label lastTag;   // updated on ButtonActivatedEvent
    };
}
