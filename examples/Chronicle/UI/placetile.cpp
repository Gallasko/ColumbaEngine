#include "placetile.h"

#include <algorithm>
#include <cmath>

#include "logger.h"

#include "ECS/callable.h"
#include "2D/position.h"
#include "2D/simple2dobject.h"
#include "Input/inputcomponent.h"
#include "UI/prefab.h"
#include "UI/themesystem.h"

#include "gloss.h"

using namespace pg;

namespace chronicle
{
    namespace
    {
        constexpr const char * const DOM = "Chronicle.PlaceTile";

        constexpr float Pad = 8.0f;              // space-2 padding, as an activity's tile
        constexpr float EdgeWidth = 3.0f;        // border-frame
        constexpr float MarkSide = 24.0f;
        constexpr float NameGap = 8.0f;          // Between the mark and the name
        constexpr float NameLines = 2.0f;
        constexpr float SealSide = 14.0f;
        constexpr float SealGap = 4.0f;          // Between two seals
        constexpr float BlockGap = 6.0f;         // Between the name, the seals and the line
        constexpr float LineLines = 3.0f;
        constexpr float TileGap = 8.0f;          // space-2 between two tiles, on a line and from line to line

        std::string groundElement(bool selected)
        {
            return selected ? "place.ground.selected" : "place.ground";
        }

        std::string edgeElement(bool selected)
        {
            return selected ? "place.edge.selected" : "place.edge";
        }

        std::string sealElement(bool earned)
        {
            return earned ? "place.seal.earned" : "place.seal.empty";
        }

        float lineHeightOf(EntitySystem* ecs, const std::string& style)
        {
            auto theme = ecs->getSystem<ThemeSystem>();

            return theme ? static_cast<float>(theme->style(style).lineHeightPx) : 16.0f;
        }

        float nameHeight(EntitySystem* ecs)
        {
            return std::max(MarkSide, NameLines * lineHeightOf(ecs, "control"));
        }

        // The seals of a tile, anchored under its name: made by makePlaceTile and again by setLevel
        void makeSeals(EntitySystem* ecs, PlaceTile& tile)
        {
            for (auto& seal : tile.seals)
                ecs->removeEntity(seal.entity.id);

            tile.seals.clear();

            const float top = Pad + nameHeight(ecs) + BlockGap;

            for (int i = 0; i < tile.spec.of; ++i)
            {
                Mark seal = makeMark(ecs, {"seal", MarkSize::S14, "ink", tile.spec.z + 3});

                seal.entity->get<ThemeComponent>()->setElement(sealElement(i < tile.spec.level));

                auto anchor = seal.entity->get<UiAnchor>();
                anchor->setTopAnchor(PosAnchor{tile.root.id, AnchorType::Top});
                anchor->setTopMargin(top);
                anchor->setLeftAnchor(PosAnchor{tile.root.id, AnchorType::Left});
                anchor->setLeftMargin(Pad + static_cast<float>(i) * (SealSide + SealGap));

                tile.root->get<Prefab>()->addToPrefab(seal.entity, "seal" + std::to_string(i));
                tile.seals.push_back(seal);
            }
        }
    }

    float placeTileHeight(EntitySystem* ecs)
    {
        return Pad + nameHeight(ecs) + BlockGap + SealSide + BlockGap + LineLines * lineHeightOf(ecs, "caption") + Pad;
    }

    PlaceTile makePlaceTile(EntitySystem* ecs, const PlaceTileSpec& specIn)
    {
        PlaceTile tile;
        tile.spec = specIn;

        if (tile.spec.id.empty())
            LOG_ERROR(DOM, "A place tile needs an id");

        tile.spec.of = std::max(1, tile.spec.of);
        tile.spec.level = std::clamp(tile.spec.level, 0, tile.spec.of);

        const PlaceTileSpec& spec = tile.spec;
        const float height = placeTileHeight(ecs);

        auto root = makeAnchoredPrefab(ecs, 0.0f, 0.0f, static_cast<float>(spec.z));

        root.get<PositionComponent>()->setWidth(spec.width);
        root.get<PositionComponent>()->setHeight(height);

        tile.root = root.entity;

        auto prefab = root.get<Prefab>();

        // The ground: the tile's whole box. It takes the click
        auto ground = makeUiSimple2DShape(ecs, Shape2D::Square, spec.width, height);

        ground.get<PositionComponent>()->setZ(static_cast<float>(spec.z));
        ground.get<UiAnchor>()->setTopAnchor(PosAnchor{tile.root.id, AnchorType::Top});
        ground.get<UiAnchor>()->setLeftAnchor(PosAnchor{tile.root.id, AnchorType::Left});
        ecs->attach<ThemeComponent>(ground.entity, groundElement(spec.selected));
        ecs->attach<MouseLeftClickComponent>(ground.entity, makeCallable<PlaceSelectedEvent>(PlaceSelectedEvent{spec.id}));
        prefab->addToPrefab(ground.entity, "ground");

        tile.ground = ground.entity;

        auto edge = makeUiSimple2DShape(ecs, Shape2D::Square, EdgeWidth, height);

        edge.get<PositionComponent>()->setZ(static_cast<float>(spec.z + 2));
        edge.get<UiAnchor>()->setTopAnchor(PosAnchor{tile.root.id, AnchorType::Top});
        edge.get<UiAnchor>()->setLeftAnchor(PosAnchor{tile.root.id, AnchorType::Left});
        ecs->attach<ThemeComponent>(edge.entity, edgeElement(spec.selected));
        prefab->addToPrefab(edge.entity, "edge");

        tile.edge = edge.entity;

        // Its mark, and its name beside it
        tile.mark = makeMark(ecs, {spec.glyph, MarkSize::S24, "ink-muted", spec.z + 3});

        auto markAnchor = tile.mark.entity->get<UiAnchor>();
        markAnchor->setTopAnchor(PosAnchor{tile.root.id, AnchorType::Top});
        markAnchor->setTopMargin(Pad);
        markAnchor->setLeftAnchor(PosAnchor{tile.root.id, AnchorType::Left});
        markAnchor->setLeftMargin(Pad);
        prefab->addToPrefab(tile.mark.entity, "mark");

        LabelSpec name;
        name.style = "control";
        name.text = spec.name;
        name.overflow = Overflow::Wrap;
        name.maxLines = static_cast<int>(NameLines);
        name.width = std::max(1.0f, spec.width - Pad - MarkSide - NameGap - Pad);
        name.z = spec.z + 4;

        tile.name = makeLabel(ecs, name);

        auto nameAnchor = tile.name.entity->get<UiAnchor>();
        nameAnchor->setTopAnchor(PosAnchor{tile.root.id, AnchorType::Top});
        nameAnchor->setTopMargin(Pad);
        nameAnchor->setLeftAnchor(PosAnchor{tile.root.id, AnchorType::Left});
        nameAnchor->setLeftMargin(Pad + MarkSide + NameGap);
        prefab->addToPrefab(tile.name.entity, "name");

        makeSeals(ecs, tile);

        // What it gives now
        LabelSpec line;
        line.style = "caption";
        line.color = "ink-muted";
        line.text = spec.line;
        line.overflow = Overflow::Wrap;
        line.maxLines = static_cast<int>(LineLines);
        line.width = std::max(1.0f, spec.width - 2.0f * Pad);
        line.z = spec.z + 4;

        tile.line = makeLabel(ecs, line);

        auto lineAnchor = tile.line.entity->get<UiAnchor>();
        lineAnchor->setTopAnchor(PosAnchor{tile.root.id, AnchorType::Top});
        lineAnchor->setTopMargin(Pad + nameHeight(ecs) + BlockGap + SealSide + BlockGap);
        lineAnchor->setLeftAnchor(PosAnchor{tile.root.id, AnchorType::Left});
        lineAnchor->setLeftMargin(Pad);
        prefab->addToPrefab(tile.line.entity, "line");

        if (not spec.glossKey.empty())
            attachGloss(ecs, tile.root, spec.glossKey);

        return tile;
    }

    void PlaceTile::setLevel(EntitySystem* ecs, int level, int of)
    {
        of = std::max(1, of);
        level = std::clamp(level, 0, of);

        const bool remake = of != spec.of;

        spec.level = level;
        spec.of = of;

        if (remake)
        {
            makeSeals(ecs, *this);
            return;
        }

        for (size_t i = 0; i < seals.size(); ++i)
            seals[i].entity->get<ThemeComponent>()->setElement(sealElement(static_cast<int>(i) < level));
    }

    void PlaceTile::setLine(EntitySystem* ecs, const std::string& text)
    {
        spec.line = text;
        line.setText(ecs, text);
    }

    void PlaceTile::setSelected(EntitySystem*, bool selected)
    {
        spec.selected = selected;

        ground->get<ThemeComponent>()->setElement(groundElement(selected));
        edge->get<ThemeComponent>()->setElement(edgeElement(selected));
    }

    void PlaceTile::setWidth(EntitySystem* ecs, float width)
    {
        spec.width = width;

        root->get<PositionComponent>()->setWidth(width);
        ground->get<PositionComponent>()->setWidth(width);

        name.setWidth(ecs, std::max(1.0f, width - Pad - MarkSide - NameGap - Pad));
        line.setWidth(ecs, std::max(1.0f, width - 2.0f * Pad));
    }

    float PlaceTile::height(EntitySystem* ecs) const
    {
        return ecs->getEntity(root.id)->get<PositionComponent>()->height;
    }

    PlaceGrid makePlaceGrid(EntitySystem* ecs, const PlaceGridSpec& specIn)
    {
        PlaceGrid grid;
        grid.spec = specIn;
        grid.spec.places.clear();

        auto root = makeAnchoredPrefab(ecs, 0.0f, 0.0f, static_cast<float>(specIn.z));

        root.get<PositionComponent>()->setWidth(specIn.width);

        grid.root = root.entity;

        grid.setPlaces(ecs, specIn.places);

        return grid;
    }

    int PlaceGrid::columns() const
    {
        const float tile = std::max(1.0f, spec.tileWidth);

        return std::max(1, static_cast<int>(std::floor((spec.width + TileGap) / (tile + TileGap))));
    }

    float PlaceGrid::tileSize() const
    {
        const int count = columns();

        return (spec.width - TileGap * static_cast<float>(count - 1)) / static_cast<float>(count);
    }

    void PlaceGrid::setPlaces(EntitySystem* ecs, const std::vector<PlaceTileSpec>& places)
    {
        for (auto& tile : tiles)
            ecs->removeEntity(tile.root.id);

        tiles.clear();

        const float size = tileSize();
        bool kept = false;

        for (const auto& place : places)
        {
            PlaceTileSpec tileSpec = place;
            tileSpec.width = size;
            tileSpec.z = spec.z;
            tileSpec.selected = not chosen.empty() and place.id == chosen;

            kept = kept or tileSpec.selected;

            PlaceTile tile = makePlaceTile(ecs, tileSpec);

            root->get<Prefab>()->addToPrefab(tile.root, "place." + place.id);
            tiles.push_back(tile);
        }

        if (not kept)
            chosen.clear();

        lay(ecs);
    }

    void PlaceGrid::lay(EntitySystem* ecs)
    {
        const int count = columns();
        const float size = tileSize();
        const float height = placeTileHeight(ecs);

        for (size_t i = 0; i < tiles.size(); ++i)
        {
            const int column = static_cast<int>(i) % count;
            const int row = static_cast<int>(i) / count;

            if (std::abs(tiles[i].spec.width - size) > 0.01f)
                tiles[i].setWidth(ecs, size);

            auto anchor = tiles[i].root->get<UiAnchor>();
            anchor->setTopAnchor(PosAnchor{root.id, AnchorType::Top});
            anchor->setTopMargin(static_cast<float>(row) * (height + TileGap));
            anchor->setLeftAnchor(PosAnchor{root.id, AnchorType::Left});
            anchor->setLeftMargin(static_cast<float>(column) * (size + TileGap));
        }

        const int rows = tiles.empty() ? 0 : (static_cast<int>(tiles.size()) + count - 1) / count;

        root->get<PositionComponent>()->setHeight(rows == 0 ? 0.0f : static_cast<float>(rows) * height + static_cast<float>(rows - 1) * TileGap);
    }

    void PlaceGrid::setWidth(EntitySystem* ecs, float width)
    {
        spec.width = width;

        root->get<PositionComponent>()->setWidth(width);

        lay(ecs);
    }

    void PlaceGrid::select(EntitySystem* ecs, const std::string& id)
    {
        chosen = tile(id) ? id : std::string();

        for (auto& t : tiles)
            t.setSelected(ecs, t.spec.id == chosen);
    }

    const std::string& PlaceGrid::selected() const
    {
        return chosen;
    }

    PlaceTile* PlaceGrid::tile(const std::string& id)
    {
        for (auto& t : tiles)
        {
            if (t.spec.id == id)
                return &t;
        }

        return nullptr;
    }
}
