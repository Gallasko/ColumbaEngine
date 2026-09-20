#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "ECS/entitysystem.h"

#include "Core/tokens.h"
#include "Core/textstyle.h"

#include "label.h"

namespace chronicle
{
    // Second icon set, "chronicle-ornaments", registered at {22, 28, 72, 120}. registerIconSet
    // rasterises every file at every size (28 rasters, < 120 k px) - accepted; one call, one atlas.
    // Returns false (and logs) if the IconSystem is missing. Idempotent per ECS.
    bool registerOrnaments(pg::EntitySystem*, const std::string& iconRoot = "res/icons/chronicle-ornaments");

    enum class OrnamentKind : uint8_t { Divider, Flourish, Corner, Versal };
    enum class DividerWeight : uint8_t { Hair, Rule };           // 1 px rule-hair | 2 px rule-ruled
    enum class CornerPos : uint8_t { TL, TR, BL, BR };
    enum class VersalTone : uint8_t { Vermilion, Gold, Lapis };  // frame and letter share the tone

    struct OrnamentSpec
    {
        OrnamentKind kind = OrnamentKind::Divider;
        // Divider
        DividerWeight weight = DividerWeight::Hair;
        bool knot = true;                 // default on; pass false for the quiet rule inside lists
        float width = 0.0f;               // 0 -> caller anchors left+right and the divider stretches
        std::string ground = "folio";     // the token the knot's patch is filled with: the surface it sits on
        // Flourish / override
        std::string colour = "";          // "" -> rule-ruled for divider/flourish, gold-edge for corner, tone for versal
        // Corner
        CornerPos corner = CornerPos::TL;
        // Versal
        std::string letter = "A";         // first code point only
        VersalTone tone = VersalTone::Vermilion;
        int z = 0;                        // root z; children z+1 (patch/frame/curls) and z+2 (knot/letter)
    };

    struct Ornament
    {
        pg::EntityRef root;               // PositionComponent + UiAnchor + Prefab: anchor and size this
        OrnamentKind kind = OrnamentKind::Divider;
        std::vector<pg::EntityRef> parts; // every child, for tests
        std::optional<Label> letter;      // Versal only

        void setColour(pg::EntitySystem*, const std::string& token);   // all inked parts (not the knot's ground patch)
        void setLetter(pg::EntitySystem*, const TextStyles&, const std::string&);   // Versal only

        // The parts that carry the ornament's colour (excludes the knot's ground patch).
        std::vector<pg::EntityRef> inked;
    };

    Ornament makeOrnament(pg::EntitySystem*, const Tokens&, const TextStyles&, const OrnamentSpec&);
}
