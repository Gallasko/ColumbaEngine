#pragma once

#include "Scene/scenemanager.h"

#include "UI/themesystem.h"
#include "UI/label.h"

namespace chronicle
{
    // Dev scene: the Life screen's tab row, buttons sharing its Tab order, a Parts panel whose
    // rows carry tooltip glosses (one missing, to show the fallback), margin glosses, and a
    // static tooltip gloss for side-by-side comparison. T toggles theme.
    struct TabsGlossGallery : public pg::Scene
    {
        virtual void init() override;

    private:
        pg::ThemeSystem* theme = nullptr;

        pg::_unique_id backgroundId = 0;
        Label selected;   // echoes the last TabSelectedEvent
    };
}
