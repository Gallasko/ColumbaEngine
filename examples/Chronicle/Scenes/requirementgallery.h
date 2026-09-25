#pragma once

#include "Scene/scenemanager.h"

#include "Core/tokens.h"
#include "Core/textstyle.h"
#include "UI/paint.h"
#include "UI/label.h"
#include "UI/requirementlist.h"

namespace chronicle
{
    // Dev scene: a milestone panel with a roomy list (and its dense twin), a fed list bound
    // to GameDataView paths with buttons that write them, and the ellipsis / right-aligned
    // pair demonstrations. T toggles theme.
    struct RequirementGallery : public pg::Scene
    {
        RequirementGallery(Tokens* tokens, TextStyles* styles) : tokens(tokens), styles(styles) {}

        virtual void init() override;

    private:
        Tokens* tokens = nullptr;
        TextStyles* styles = nullptr;

        pg::_unique_id backgroundId = 0;

        RequirementList fed;   // bound to milestone.squire.reqs.* through subscriptions
        Label echo;
    };
}
