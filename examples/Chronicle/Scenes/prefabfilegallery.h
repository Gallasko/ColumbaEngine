#pragma once

#include "Scene/scenemanager.h"
#include "UI/prefabbuilder.h"

#include "Core/tokens.h"
#include "Core/textstyle.h"
#include "UI/label.h"

namespace chronicle
{
    // Dev scene: the whole page comes from res/chronicle/ui/prefabgallery.yaml, built through
    // the Chronicle prefab factories. The scene only wires the named handles: the fed
    // requirement list follows GameDataView paths, the seal button writes them, the tab row
    // and the button echo their events. T toggles theme.
    struct PrefabFileGallery : public pg::Scene
    {
        PrefabFileGallery(Tokens* tokens, TextStyles* styles) : tokens(tokens), styles(styles) {}

        virtual void init() override;

    private:
        Tokens* tokens = nullptr;
        TextStyles* styles = nullptr;

        pg::_unique_id backgroundId = 0;

        pg::PrefabBuildResult built;   // owns the handles the lambdas below point into
        Label echo;
    };
}
