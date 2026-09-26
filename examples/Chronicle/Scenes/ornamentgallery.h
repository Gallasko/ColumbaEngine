#pragma once

#include "Scene/scenemanager.h"

#include "UI/themesystem.h"

namespace chronicle
{
    // Dev scene: the four ornament kinds, with dividers shown on both a folio leaf and bare
    // vellum so the knot's ground patch is checked against both surfaces.
    struct OrnamentGallery : public pg::Scene
    {
        virtual void init() override;

    private:
        pg::ThemeSystem* theme = nullptr;

        pg::_unique_id backgroundId = 0;
    };
}
