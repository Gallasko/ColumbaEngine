//
// Created by nicol on 5/12/2025.
//
#include "TiledLoader.h"

#include "logger.h"

MapData TiledLoader::loadMap(const std::string &path) {
    std::cout << "Loading map: " << path << std::endl;

    tson::Tileson t;
    const auto map = t.parse(path);
    if (map->getStatus() != tson::ParseStatus::OK) {
        std::ostringstream oss;
        oss << "Error loading map: " << map->getStatusMessage();
        throw std::runtime_error(oss.str());
    }

    std::cout << "Loaded map: " << path << std::endl;

    fs::path jsonPath = fs::absolute(path);                  // full path to JSON
    fs::path jsonDir = jsonPath.parent_path();               // directory of JSON

    MapData result;
    result.width = map->getSize().x;
    result.height = map->getSize().y;
    result.tileWidth = map->getTileSize().x;
    result.tileHeight = map->getTileSize().y;

    for (auto& tileset : map->getTilesets()) {
        TileSet myTileSet;

        myTileSet.width = tileset.getImageSize().x;
        myTileSet.height = tileset.getImageSize().y;

        myTileSet.name = tileset.getName();

        myTileSet.tileCount = tileset.getTileCount();

        myTileSet.columns = tileset.getColumns();

        myTileSet.tileWidth = map->getTileSize().x;
        myTileSet.tileHeight = map->getTileSize().y;


        fs::path imagePath = tileset.getImagePath();         // relative to JSON
        std::cout<<"A.1:" << imagePath<< std::endl;
        fs::path resolvedPath = fs::absolute(jsonDir / imagePath);  // full path
        std::cout<<"A.2:" << resolvedPath<< std::endl;

        // Now you can make it relative to current working dir if needed:
        fs::path relativeToCWD = fs::relative(resolvedPath, fs::current_path());
        std::cout<<"A.3:" << relativeToCWD<< std::endl;

        myTileSet.imagePath = relativeToCWD.string();

        std::cout<<"A.4:" << myTileSet.imagePath << std::endl;


        result.tilesets.push_back(myTileSet);
    }

    for (auto& layer : map->getLayers()) {
        switch (layer.getType()) {
            case tson::LayerType::Undefined:
                break;
            case tson::LayerType::TileLayer: {

                Layer l;
                l.name = layer.getName();
                l.opacity = layer.getOpacity();
                for (auto& [pos, tile] : layer.getTileData()) {
                    Tile myTile;

                    // POSITION
                    auto [x, y] = pos;
                    myTile.x = x;
                    myTile.y = y;

                    // SIZE
                    //myTile.tileWidth = map->getTileSize().x;
                   // myTile.tileHeight = map->getTileSize().y;

                    // IMAGE PATH
                    auto* tileset = tile->getTileset();

                    myTile.textureName = tileset->getName() + "." + std::to_string(tile->getId());

                    myTile.isWall = tile->get<bool>("wall");
                    LOG_INFO("TILED", myTile.isWall << " " << myTile.textureName);

                    l.tiles.push_back(myTile);
                }
                result.layers.push_back(std::move(l));
                break;
            }
            case tson::LayerType::ObjectGroup:
                break;
            case tson::LayerType::ImageLayer:
                break;
            case tson::LayerType::Group:
                break;
        }
    }
    return result;
}
