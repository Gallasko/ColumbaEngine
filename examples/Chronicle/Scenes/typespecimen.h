#pragma once

#include "Scene/scenemanager.h"

#include "Core/tokens.h"
#include "Core/textstyle.h"

namespace chronicle
{
    // Dev scene: shows every text style on vellum so the fonts can be judged by eye.
    struct TypeSpecimen : public pg::Scene
    {
        TypeSpecimen(Tokens* tokens, TextStyles* styles) : tokens(tokens), styles(styles) {}

        virtual void init() override;

    private:
        Tokens* tokens = nullptr;
        TextStyles* styles = nullptr;
    };
}
