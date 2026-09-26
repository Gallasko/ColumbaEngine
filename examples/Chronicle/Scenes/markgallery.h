#pragma once

#include "Scene/scenemanager.h"

#include "UI/themesystem.h"

namespace chronicle
{
    // Dev scene: the whole mark set at two sizes, marked-label pairs across the styles,
    // the status colours, and the missing-mark fallback.
    struct MarkGallery : public pg::Scene
    {
        virtual void init() override;

    private:
        pg::ThemeSystem* theme = nullptr;

        pg::_unique_id backgroundId = 0;
    };
}
