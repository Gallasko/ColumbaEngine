#pragma once

#include "Scene/scenemanager.h"

#include "Core/tokens.h"
#include "Core/textstyle.h"
#include "UI/paint.h"

namespace chronicle
{
    // Dev scene: the four ornament kinds, with dividers shown on both a folio leaf and bare
    // vellum so the knot's ground patch is checked against both surfaces.
    struct OrnamentGallery : public pg::Scene
    {
        OrnamentGallery(Tokens* tokens, TextStyles* styles) : tokens(tokens), styles(styles) {}

        virtual void init() override;

    private:
        Tokens* tokens = nullptr;
        TextStyles* styles = nullptr;

        pg::_unique_id backgroundId = 0;
    };
}
