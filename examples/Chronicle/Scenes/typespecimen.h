#pragma once

#include <string>
#include <utility>
#include <vector>

#include "Scene/scenemanager.h"

#include "Core/tokens.h"
#include "Core/textstyle.h"

namespace chronicle
{
    // Dev scene: shows every text style on vellum so the fonts can be judged by eye.
    // T toggles the theme (the scene owns its refs and repaints); S toggles a swatch column.
    struct TypeSpecimen : public pg::Scene
    {
        TypeSpecimen(Tokens* tokens, TextStyles* styles) : tokens(tokens), styles(styles) {}

        virtual void init() override;

    private:
        void repaint();
        void toggleSwatches();

        Tokens* tokens = nullptr;
        TextStyles* styles = nullptr;

        float screenWidth = 1320.0f;
        float screenHeight = 860.0f;

        // Each painted text and the colour token it draws in, so a theme change repaints it.
        std::vector<std::pair<pg::_unique_id, std::string>> paintedTexts;
        pg::_unique_id backgroundId = 0;

        bool swatchesShown = false;
        std::vector<pg::_unique_id> swatchIds;
    };
}
