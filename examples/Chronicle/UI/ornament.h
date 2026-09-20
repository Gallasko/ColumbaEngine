#pragma once

#include <string>

#include "ECS/entitysystem.h"

namespace chronicle
{
    // Second icon set, "chronicle-ornaments", registered at {22, 28, 72, 120}. registerIconSet
    // rasterises every file at every size (28 rasters, < 120 k px) - accepted; one call, one atlas.
    // Returns false (and logs) if the IconSystem is missing. Idempotent per ECS.
    bool registerOrnaments(pg::EntitySystem*, const std::string& iconRoot = "res/icons/chronicle-ornaments");
}
