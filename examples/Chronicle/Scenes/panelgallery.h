#pragma once

#include "Scene/scenemanager.h"

#include "UI/themesystem.h"

namespace chronicle
{
    // Dev scene and phase-1 gate: the three design-system panels laid out to compare against the
    // design system's Panel preview, plus the engine-only checks (plain, ruled list, ellipsis,
    // nesting, marks-in-a-panel).
    struct PanelGallery : public pg::Scene
    {
        virtual void init() override;

    private:
        pg::ThemeSystem* theme = nullptr;

        pg::_unique_id backgroundId = 0;
    };
}
