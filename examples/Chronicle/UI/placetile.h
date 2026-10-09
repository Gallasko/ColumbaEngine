#pragma once

#include <string>
#include <vector>

#include "ECS/entitysystem.h"

#include "label.h"
#include "mark.h"

namespace chronicle
{
    struct PlaceTileSpec
    {
        std::string id;                // The place's key ("market"), carried by PlaceSelectedEvent; required
        std::string name;              // "The Market"
        std::string glyph = "town";
        int level = 0;                 // The seals earned
        int of = 3;                    // The seals in all
        std::string line;              // What the place gives now, in caps; "NOT YET BUILT" at level 0
        bool selected = false;
        std::string glossKey;          // "" = none; else attachGloss(root, key)
        float width = 148.0f;
        int z = 20;                    // Root and ground; the edge z+2; the marks z+3, the texts z+4
    };

    // Sent when a place's tile is clicked.
    struct PlaceSelectedEvent
    {
        std::string id;
    };

    // A place of the town, as large and as plain as an activity's tile so the two pages read as
    // one book: its mark and its name, a seal for each of its levels (gold for the ones earned),
    // and a line for what it gives now. Its gloss says the rest. A click sends PlaceSelectedEvent;
    // the tile itself does nothing with it: whoever holds it lights the one chosen.
    struct PlaceTile
    {
        pg::EntityRef root;            // PositionComponent + UiAnchor + Prefab: anchor this. spec.width wide, height() tall
        pg::EntityRef ground;          // Simple2DObject, place.ground (.selected): it takes the click
        pg::EntityRef edge;            // Simple2DObject 3 px at the left, place.edge (.selected)
        Mark mark;
        Label name;                    // Two lines at most
        std::vector<Mark> seals;       // spec.of of them: place.seal.earned, then place.seal.empty
        Label line;                    // Three lines at most
        PlaceTileSpec spec;

        void setLevel(pg::EntitySystem*, int level, int of);   // The seals: made again when `of` changes
        void setLine(pg::EntitySystem*, const std::string&);
        void setSelected(pg::EntitySystem*, bool);
        void setWidth(pg::EntitySystem*, float);                // The ground and the texts follow
        float height(pg::EntitySystem*) const;
    };

    PlaceTile makePlaceTile(pg::EntitySystem*, const PlaceTileSpec&);

    // The height every place tile has: it does not follow what the tile says.
    float placeTileHeight(pg::EntitySystem*);

    struct PlaceGridSpec
    {
        float width = 572.0f;
        float tileWidth = 148.0f;      // As many tiles to a line as fit at that width or more, 8 apart
        std::vector<PlaceTileSpec> places;
        int z = 20;
    };

    // The places of the town, laid as an activity list lays its tiles: lines of as many as fit,
    // 8 apart, each sharing the width out. One tile at most is selected.
    struct PlaceGrid
    {
        pg::EntityRef root;            // PositionComponent + UiAnchor + Prefab: anchor this. Width spec.width, height the lines'
        std::vector<PlaceTile> tiles;  // In the order given
        PlaceGridSpec spec;            // width, tileWidth and z; the places live in `tiles`

        void setPlaces(pg::EntitySystem*, const std::vector<PlaceTileSpec>& places);   // Rebuilds; the selection is kept when its place is still there
        void setWidth(pg::EntitySystem*, float);    // The tiles are laid again
        void select(pg::EntitySystem*, const std::string& id);   // "" clears
        const std::string& selected() const;
        PlaceTile* tile(const std::string& id);     // nullptr when unknown
        int columns() const;           // The tiles a line holds at this width, one at least
        float tileSize() const;        // And how wide each is

        // Internal
        void lay(pg::EntitySystem*);   // Every tile to its place, the root to their height
        std::string chosen;
    };

    PlaceGrid makePlaceGrid(pg::EntitySystem*, const PlaceGridSpec&);
}
