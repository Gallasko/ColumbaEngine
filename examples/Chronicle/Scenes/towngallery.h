#pragma once

#include "Scene/scenemanager.h"
#include "UI/themesystem.h"

namespace chronicle
{
    // Dev scene: the places of the town from res/chronicle/ui/towngallery.yaml, nine tiles at
    // every level from none to three, one of them chosen, and a narrow grid beside them. A click
    // on a tile chooses it, as the Town page does. T toggles the theme.
    struct TownGallery : public pg::Scene
    {
        virtual void init() override;

    private:
        pg::ThemeSystem* theme = nullptr;

        pg::_unique_id backgroundId = 0;

        std::vector<pg::_unique_id> grids;
    };
}
