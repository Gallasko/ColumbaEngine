#pragma once

#include "Scene/scenemanager.h"

#include "Core/tokens.h"
#include "Core/textstyle.h"
#include "UI/paint.h"

namespace chronicle
{
    // Dev scene: the whole mark set at two sizes, marked-label pairs across the styles,
    // the status colours, and the missing-mark fallback.
    struct MarkGallery : public pg::Scene
    {
        MarkGallery(Tokens* tokens, TextStyles* styles) : tokens(tokens), styles(styles) {}

        virtual void init() override;

    private:
        Tokens* tokens = nullptr;
        TextStyles* styles = nullptr;

        pg::_unique_id backgroundId = 0;
    };
}
