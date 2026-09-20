#pragma once

#include "Scene/scenemanager.h"

#include "Core/tokens.h"
#include "Core/textstyle.h"
#include "UI/paint.h"

namespace chronicle
{
    // Dev scene and phase-1 gate: the three design-system panels laid out to compare against the
    // design system's Panel preview, plus the engine-only checks (plain, ruled list, ellipsis,
    // nesting, marks-in-a-panel).
    struct PanelGallery : public pg::Scene
    {
        PanelGallery(Tokens* tokens, TextStyles* styles) : tokens(tokens), styles(styles) {}

        virtual void init() override;

    private:
        Tokens* tokens = nullptr;
        TextStyles* styles = nullptr;
        PaintSystem* paintSystem = nullptr;

        pg::_unique_id backgroundId = 0;
    };
}
