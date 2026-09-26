#pragma once

#include "Scene/scenemanager.h"

#include "UI/themesystem.h"
#include "UI/label.h"

namespace chronicle
{
    // Dev scene: the three button variants, the cost pair, disabled-with-reason, a dense
    // tab-order row, and an overlap check. T toggles theme; the last activated tag is shown.
    struct ButtonGallery : public pg::Scene
    {
        virtual void init() override;

    private:
        pg::ThemeSystem* theme = nullptr;

        pg::_unique_id backgroundId = 0;
        Label lastTag;   // updated on ButtonActivatedEvent
    };
}
