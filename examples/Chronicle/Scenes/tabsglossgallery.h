#pragma once

#include "Scene/scenemanager.h"

#include "Core/tokens.h"
#include "Core/textstyle.h"
#include "UI/paint.h"
#include "UI/label.h"

namespace chronicle
{
    // Dev scene: the Life screen's tab row, buttons sharing its Tab order, a Parts panel whose
    // rows carry tooltip glosses (one missing, to show the fallback), margin glosses, and a
    // static tooltip gloss for side-by-side comparison. T toggles theme.
    struct TabsGlossGallery : public pg::Scene
    {
        TabsGlossGallery(Tokens* tokens, TextStyles* styles) : tokens(tokens), styles(styles) {}

        virtual void init() override;

    private:
        Tokens* tokens = nullptr;
        TextStyles* styles = nullptr;
        PaintSystem* paintSystem = nullptr;

        pg::_unique_id backgroundId = 0;
        Label selected;   // echoes the last TabSelectedEvent
    };
}
