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

MapData TiledLoader::loadMap(const std::string &path, int scaleFactor) {
    std::cout << "------MAP LOADING------" << std::endl;
    std::cout << "Loading map: " << path << std::endl;

    fs::path pathToUse = fs::path("res/tiled/Tiled_Levels.tiled-project");
    tson::Project project{pathToUse};

    tson::Tileson t{&project};
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
    result.tileWidthInTPixels = map->getTileSize().x;
    result.tileHeightInTPixels = map->getTileSize().y;
    result.tileWidthInSPixels = map->getTileSize().x * scaleFactor;
    result.tileHeightInSPixels = map->getTileSize().y * scaleFactor;

    for (auto& tileset : map->getTilesets()) {
        TileSet myTileSet;

        myTileSet.width = tileset.getImageSize().x;
        myTileSet.height = tileset.getImageSize().y;

        myTileSet.name = tileset.getName();

        myTileSet.tileCount = tileset.getTileCount();

        myTileSet.columns = tileset.getColumns();

        myTileSet.tileWidthInTPixels = map->getTileSize().x;
        myTileSet.tileHeightInTPixels = map->getTileSize().y;


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
                            rect.topLeftCornerX = obj.getPosition().x * scaleFactor;
                            rect.topLeftCornerY = obj.getPosition().y * scaleFactor;
                            rect.width = obj.getSize().x * scaleFactor;
                            rect.height = obj.getSize().y * scaleFactor;

                            roomTrigger.rectInSPixels = rect;

                            result.roomTriggers.push_back(roomTrigger);
                        }
                    }
                    else if (obj.get<bool>("door")) {
                        PG_ASSERT(obj.has(RoomIndex), "a door must have room index")
                        const auto& roomIndex = obj.get<int>(RoomIndex);
                        Door door;

                        door.roomIndex = roomIndex;

                        TiledRect rect;
                        rect.topLeftCornerX = obj.getPosition().x * scaleFactor;
                        rect.topLeftCornerY = obj.getPosition().y * scaleFactor;
                        rect.width = obj.getSize().x * scaleFactor;
                        rect.height = obj.getSize().y * scaleFactor;

                        door.rectInSPixels = rect;
                        door.rectTilesSpace = rect.toTileRectTilesSpace(result.tileWidthInSPixels, result.tileHeightInSPixels);

                        result.doors.push_back(door);
                    }
                    else if (obj.get<bool>("spikes")) {
                        PG_ASSERT(obj.has("inDuration"), "a spike must have inDuration")
                        PG_ASSERT(obj.has("outDuration"), "a spike must have outDuration")
                        PG_ASSERT(obj.has("showingDuration"), "a spike must have showingDuration")
                        PG_ASSERT(obj.has("timerOffset"), "a spike must have timerOffset")

                        Spike spike;

                        spike.inDuration = obj.get<float>("inDuration");
                        spike.outDuration = obj.get<float>("outDuration");
                        spike.showingDuration = obj.get<float>("showingDuration");
                        spike.timerOffset = obj.get<float>("timerOffset");

                        TiledRect rect;
                        rect.topLeftCornerX = obj.getPosition().x * scaleFactor;
                        rect.topLeftCornerY = obj.getPosition().y * scaleFactor;
                        rect.width = obj.getSize().x * scaleFactor;
                        rect.height = obj.getSize().y * scaleFactor;

                        spike.rectInSPixels = rect;
                        spike.rectTilesSpace = rect.toTileRectTilesSpace(result.tileWidthInSPixels, result.tileHeightInSPixels);

                        result.spikes.push_back(spike);
                    }
                    else if (obj.get<bool>("enemy")) {
                        EnemyData enemy;

                        enemy.name = obj.get<std::string>("name");
                        enemy.aiCooldownTimer = obj.get<int>("aiCooldownTimer");
                        enemy.attackDistance = obj.get<float>("attackDistance");
                        enemy.chaseSpeed = obj.get<float>("chaseSpeed");
                        enemy.cooldownTime = obj.get<int>("cooldownTime");
                        enemy.enemyBulletDamage = obj.get<float>("enemyBulletDamage");
                        enemy.idealDistance = obj.get<float>("idealDistance");
                        enemy.orbitThreshold = obj.get<float>("orbitThreshold");
                        enemy.spiralRate = obj.get<float>("spiralRate");
                        enemy.wideUpTime = obj.get<float>("windUpTime");

                        enemy.objId = obj.getId();
                        enemy.weaponId = obj.get<uint32_t>("weapon");
                        enemy.canSpawn = enemy.name != "noEnemy";

                        result.enemyTemplates.push_back(enemy);
                    }
                    else if (obj.get<bool>("spawner")) {
                        Spawner spawner;

                        const auto& props = obj.getProperties().getProperties();
                        for (const auto& [key, prop] : props) {
                            // key is a std::string
                            // prop is a tson::Property
                            // use key and value as needed

                            if (prop.getName().find("enemy") == std::string::npos) {
                                continue;
                            }
                            PG_ASSERT(prop.getPropertyType() == "SpawnData", "in a spawner, if a property name contains enemy, it must of type SpawnData")
                            //std::cout << "Prop :" << prop.getName() << std::endl;
                            //::cout << prop.getPropertyType() << std::endl;

                            auto value = prop.getValue<tson::TiledClass>();
                            auto proba = value.get<float>("spawnProba");
                            auto enemyId = value.get<uint32_t>("enemyData");

                            SpawnData data;
                            data.spawnProba =proba;
                            data.enemyId = enemyId;

                            spawner.spawns.push_back(data);
                        }

                        spawner.roomIndex = obj.get<int>(RoomIndex);
                        spawner.posXInSPixels = obj.getPosition().x * scaleFactor;
                        spawner.posYInSPixels = obj.getPosition().y * scaleFactor;

                        result.spawners.push_back(spawner);
                    }
                    else if (obj.get<bool>("room")) {
                        PG_ASSERT(obj.has(RoomIndex), "room needs a room index")
                        PG_ASSERT(obj.has("nbEnemy"), "room needs a number of enemies")

                        RoomData room;
                        room.roomIndex = obj.get<int>(RoomIndex);
                        room.nbEnemy = obj.get<int>("nbEnemy");

                        result.roomDatas.push_back(room);
                    }
                    else if (obj.get<bool>("weapon")) {
                        PG_ASSERT(obj.has("data"), "a weapon must have a data")
                        auto objClass = obj.get<tson::TiledClass>("data");

                        //auto val = objClass.get<tson::EnumValue>("pattern");
                        //std::cout << "ENUM " << val.getValueName() << std::endl;

                        auto val = objClass.get<uint32_t>("pattern");
                        //std::cout << "ENUM " << val << std::endl;

                        auto pattern = static_cast<pg::BulletPattern>(val);

                        WeaponData data;
                        data.ammo = objClass.get<int>("ammo");
                        data.barrelSize = objClass.get<int>("barrelSize");
                        data.bulletCount = objClass.get<int>("bulletCount");
                        data.bulletSpreadAngle = objClass.get<float>("bulletSpreadAngle");
                        data.damage = objClass.get<float>("damage");
                        data.name = objClass.get<std::string>("name");
                        data.pattern = pattern;
                        data.projectileLifeTime = objClass.get<float>("projectileLifeTime");
                        data.projectileSize = objClass.get<float>("projectileSize");
                        data.projectileSpeed = objClass.get<float>("projectileSpeed");
                        data.reloadTimeMs = objClass.get<float>("reloadTimeMs");

                        data.id = obj.getId();

                        result.weaponDatas.push_back(data);
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
