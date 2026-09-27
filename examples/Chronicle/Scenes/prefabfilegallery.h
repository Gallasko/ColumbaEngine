#pragma once

#include "Scene/scenemanager.h"
#include "UI/prefabbuilder.h"

#include "UI/label.h"

namespace chronicle
{
    // Dev scene: the whole page comes from res/chronicle/ui/prefabgallery.yaml, built through
    // the Chronicle prefab factories. The scene only wires the named handles: the fed
    // requirement list follows GameDataView paths, the seal button writes them, the tab row
    // and the button echo their events. T toggles theme.
    struct PrefabFileGallery : public pg::Scene
    {
        virtual void init() override;

    private:
        pg::ThemeSystem* theme = nullptr;

        pg::_unique_id backgroundId = 0;

        pg::EntityRef page;             // the file's root prefab
        Label echo;
    };
}
