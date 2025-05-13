//
// Created by nicol on 5/12/2025.
//
#include "TiledLoader.h"

#include "logger.h"
#include <stdexcept>
#include <string>

#define PG_ASSERT(cond, msg)                                         \
if (!(cond)) {                                               \
throw std::runtime_error(std::string("Assertion failed: ") + #cond + " - " +  msg);           \
}

static const std::string TriggerRoom = "triggerRoom";
static const std::string Event = "event";
static const std::string RoomIndex = "roomIndex";

MapData TiledLoader::loadMap(const std::string &path) {
    std::cout << "------MAP LOADING------" << std::endl;
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
        fs::path resolvedPath = fs::absolute(jsonDir / imagePath);  // full path

        // Now you can make it relative to current working dir if needed:
        fs::path relativeToCWD = fs::relative(resolvedPath, fs::current_path());

        myTileSet.imagePath = relativeToCWD.string();

        result.tilesets.push_back(myTileSet);
    }

    for (auto& layer : map->getLayers()) {
        // Skip layers annotated with "__DEV__"
        if (layer.getName().find("__DEV__") != std::string::npos) {
            continue;
        }

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

                    // IMAGE PATH
                    auto* tileset = tile->getTileset();

                    myTile.textureName = tileset->getName() + "." + std::to_string(tile->getId());

                    myTile.isWall = tile->get<bool>("wall");
                    myTile.isHole = tile->get<bool>("hole");

                    l.tiles.push_back(myTile);
                }
                result.layers.push_back(std::move(l));
                break;
            }
            case tson::LayerType::ObjectGroup:
                for (auto& obj : layer.getObjects()) {
                    if (obj.get<bool>("isTriggerRect")) {
                        PG_ASSERT(obj.has(Event), "a trigger must have an event")
                        const auto& event = obj.get<std::string>(Event);
                        if (event == TriggerRoom) {
                            PG_ASSERT(obj.has(RoomIndex), "a room trigger must have a room index")
                            const auto& roomIndex = obj.get<int>(RoomIndex);
                            RoomTrigger roomTrigger;

                            roomTrigger.roomIndex = roomIndex;

                            TiledRect rect;
                            rect.topLeftCornerX = obj.getPosition().x;
                            rect.topLeftCornerY = obj.getPosition().y;
                            rect.width = obj.getSize().x;
                            rect.height = obj.getSize().y;

                            roomTrigger.rect = rect;

                            result.roomTriggers.push_back(roomTrigger);
                        }
                    }
                }
                break;
            case tson::LayerType::ImageLayer:
                break;
            case tson::LayerType::Group:
                break;
        }
    }

    std::cout << "------MAP LOADING------ END" << std::endl;
    return result;
}
