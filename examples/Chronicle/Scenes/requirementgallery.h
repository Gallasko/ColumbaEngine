#pragma once

#include "Scene/scenemanager.h"

#include "UI/themesystem.h"
#include "UI/label.h"
#include "UI/requirementlist.h"

namespace chronicle
{
    // Dev scene: a milestone panel with a roomy list (and its dense twin), a fed list bound
    // to GameDataView paths with buttons that write them, and the ellipsis / right-aligned
    // pair demonstrations. T toggles theme.
    struct RequirementGallery : public pg::Scene
    {
        virtual void init() override;

    private:
        pg::ThemeSystem* theme = nullptr;

        pg::_unique_id backgroundId = 0;

        RequirementList fed;   // bound to milestone.squire.reqs.* through subscriptions
        Label echo;
    };
}
