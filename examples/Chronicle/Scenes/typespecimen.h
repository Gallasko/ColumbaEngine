#pragma once

#include <string>
#include <utility>
#include <vector>

#include "Scene/scenemanager.h"

#include "UI/themesystem.h"

namespace chronicle
{
    // Dev scene: shows every text style on vellum so the fonts can be judged by eye.
    // T toggles the theme (the theme system repaints every ThemeComponent); S toggles a swatch column.
    struct TypeSpecimen : public pg::Scene
    {
        virtual void init() override;

    private:
        void toggleSwatches();

        pg::ThemeSystem* theme = nullptr;

        float screenWidth = 1320.0f;
        float screenHeight = 860.0f;

        pg::_unique_id backgroundId = 0;

        bool swatchesShown = false;
        std::vector<pg::_unique_id> swatchIds;
    };
}
