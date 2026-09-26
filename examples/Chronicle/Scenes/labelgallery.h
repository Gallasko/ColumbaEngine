#pragma once

#include "Scene/scenemanager.h"

#include "UI/themesystem.h"

namespace chronicle
{
    // Dev scene: four columns exercising Label's alignment and the three overflows.
    struct LabelGallery : public pg::Scene
    {
        virtual void init() override;

    private:
        pg::ThemeSystem* theme = nullptr;

        pg::_unique_id backgroundId = 0;
    };
}
