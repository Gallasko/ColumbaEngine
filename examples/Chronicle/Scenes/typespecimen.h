#pragma once

#include <string>
#include <utility>
#include <vector>

#include "Scene/scenemanager.h"

#include "Core/tokens.h"
#include "Core/textstyle.h"
#include "UI/paint.h"

namespace chronicle
{
    // Dev scene: shows every text style on vellum so the fonts can be judged by eye.
    // T toggles the theme (PaintSystem repaints every PaintComponent); S toggles a swatch column.
    struct TypeSpecimen : public pg::Scene
    {
        TypeSpecimen(Tokens* tokens, TextStyles* styles) : tokens(tokens), styles(styles) {}

        virtual void init() override;

    private:
        void toggleSwatches();

        Tokens* tokens = nullptr;
        TextStyles* styles = nullptr;
        PaintSystem* paintSystem = nullptr;

        float screenWidth = 1320.0f;
        float screenHeight = 860.0f;

        pg::_unique_id backgroundId = 0;

        bool swatchesShown = false;
        std::vector<pg::_unique_id> swatchIds;
    };
}
