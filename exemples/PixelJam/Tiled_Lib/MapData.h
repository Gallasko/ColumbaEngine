//
// Created by nicol on 5/12/2025.
//

#ifndef MAPDATA_H
#define MAPDATA_H

#include <string>
#include <vector>


#include "Weapons/weapon.h"

// T Pixels : Tiled Pixels. So raw data from Tiled. For example a Tile size is 16x16 TPixels
// S Pixels : Screen Pixels. We take the data from Tiled and scale it. So a 16x16 tile can take 32x32 real screen pixels, by a factor of 2

struct Tile {
    /*!
     * X position of the tile. If the map width is 30 tiles, then the position is between 0 and 29
     */
    int x;
    /*!
     * Y position of the tile. If the map height is 30 tiles, then the position is between 0 and 29
     */
    int y;

    bool isWall;
    bool isHole;

    std::string textureName;
};

/*!
 * Represents a list of tiles
 */
struct Layer {
    std::string name;
    std::vector<Tile> tiles;
    /*!
     * A number between 0 and 1, each tile in the layer has the same opacity
     */
    float opacity;
};

struct TileSet {
    std::string name;
    /*!
     * The path the texture atlas. Local to the std::filesystem::current_path().
     */
    std::string imagePath;
    int imageWidthInPixels;
    int imageHeightInPixels;
    int tileCount;
    int columns;
    /*!
         * Width in Tpixels of each tile
         */
    int tileWidthInTPixels;
    /*!
     * Height in Tpixels of each tile
     */
    int tileHeightInTPixels;
};

struct SpawnPoint {
    pg::constant::Vector2D positionSPixels;
};

/*!
 * Same as TiledRect but works in tile space. So if the tile map size is 30x20 tiles, the coordinates are grid, int tile positions
 */
struct TileRectTilesSpace {
    int topLeftCornerX;
    int topLeftCornerY;
    int widthInTiles;
    int heightInTiles;
};

struct TiledRect {
    float topLeftCornerX;
    float topLeftCornerY;
    float width;
    float height;

    TileRectTilesSpace toTileRectTilesSpace(int tileSizeX, int tileSizeY) const {
        TileRectTilesSpace result;
        result.topLeftCornerX = static_cast<int>(topLeftCornerX / tileSizeX);
        result.topLeftCornerY = static_cast<int>(topLeftCornerY / tileSizeY);
        result.widthInTiles = static_cast<int>(width / tileSizeX);
        result.heightInTiles = static_cast<int>(height / tileSizeY);
        return result;
    }
};

inline std::ostream &operator<<(std::ostream &os, const TiledRect &rect) {
    os << "TiledRect {\n"
            << "  topLeftCornerX: " << rect.topLeftCornerX << "\n"
            << "  topLeftCornerY: " << rect.topLeftCornerY << "\n"
            << "  width: " << rect.width << "\n"
            << "  height: " << rect.height << "\n"
            << "}";
    return os;
}


inline std::ostream &operator<<(std::ostream &os, const TileRectTilesSpace &rect) {
    os << "TileRectTilesSpace {\n"
            << "  topLeftCornerX (tile space): " << rect.topLeftCornerX << "\n"
            << "  topLeftCornerY (tile space): " << rect.topLeftCornerY << "\n"
            << "  width in tiles: " << rect.widthInTiles << "\n"
            << "  height in tiles: " << rect.heightInTiles << "\n"
            << "}";
    return os;
}

struct RoomTrigger {
    TiledRect rectInSPixels;
    int roomIndex;
};

inline std::ostream &operator<<(std::ostream &os, const RoomTrigger &trigger) {
    os << "RoomTrigger {\n"
            << "  rectInSPixels: " << trigger.rectInSPixels << "\n"
            << "  roomIndex: " << trigger.roomIndex << "\n"
            << "}";
    return os;
}

struct Door {
    TiledRect rectInSPixels;
    TileRectTilesSpace rectTilesSpace;
    int roomIndex;
    bool vertical;
};

inline std::ostream &operator<<(std::ostream &os, const Door &trigger) {
    os << "Door {\n"
            << "  rectInSPixels: " << trigger.rectInSPixels << "\n"
            << "  rectIn Tile: " << trigger.rectTilesSpace << "\n"
            << "  roomIndex: " << trigger.roomIndex << "\n"
    << "  vertical: " << trigger.vertical << "\n"
            << "}";
    return os;
}

struct Spike {
    TiledRect rectInSPixels;
    TileRectTilesSpace rectTilesSpace;
    // in seconds
    float inDuration;
    // in seconds
    float showingDuration;
    // in seconds
    float outDuration;
    // in seconds
    float timerOffset;
    float scaledTileWidth;
    float scaledTileHeight;
};

inline std::ostream &operator<<(std::ostream &os, const Spike &trigger) {
    os << "Spike {\n"
            << "  rectInSPixels: " << trigger.rectInSPixels << "\n"
            << "  rectIn Tile: " << trigger.rectTilesSpace << "\n"
            << "  inDuration: " << trigger.inDuration << "\n"
            << "  showingDuration: " << trigger.showingDuration << "\n"
            << "  outDuration: " << trigger.outDuration << "\n"
            << "  timerOffset: " << trigger.timerOffset << "\n"
            << "}";
    return os;
}

struct SpikeImage {
    std::string textureName;
    int spikeStep;
};

inline std::ostream &operator<<(std::ostream &os, const SpikeImage &spike) {
    os << "SpikeImage {\n"
            << "  Texture name: " << spike.textureName << "\n"
            << "  Step : " << spike.spikeStep << "\n"
            << "}";
    return os;
}

struct SpawnData {
    /*!
     * which enemy to spawn
     */
    int enemyId;
    /*!
     * not a proba but a weight, so can be greater than 1 !!!!
     */
    float spawnProba;
};

inline std::ostream &operator<<(std::ostream &os, const SpawnData &data) {
    os << "SpawnData {\n"
            << "  enemyId: " << data.enemyId << "\n"
            << "  spawnProba (weight): " << data.spawnProba << "\n"
            << "}";
    return os;
}

struct Spawner {
    /*!
     * All the possible spawns : which enemy with which weight ?
     */
    std::vector<SpawnData> spawns;
    int roomIndex;
    float posXInSPixels;
    float posYInSPixels;
};

inline std::ostream &operator<<(std::ostream &os, const Spawner &spawner) {
    os << "Spawner {\n"
            << "  roomIndex: " << spawner.roomIndex << "\n"
            << "  posXInSPixels: " << spawner.posXInSPixels << "\n"
            << "  posYInSPixels: " << spawner.posYInSPixels << "\n"
            << "  spawns: [\n";
    for (const auto &spawn: spawner.spawns) {
        os << "    " << spawn << "\n";
    }
    os << "  ]\n}";
    return os;
}

struct Gold {
    float posXInSPixels;
    float posYInSPixels;
};

inline std::ostream &operator<<(std::ostream &os, const Gold &spawner) {
    os << "Gold {\n"

            << "  posXInSPixels: " << spawner.posXInSPixels << "\n"
            << "  posYInSPixels: " << spawner.posYInSPixels << "\n";
    return os;
}

struct WeaponData {
    std::string name = "Unknown";

    float damage = 1.0f;

    float projectileSpeed = 300.0f;
    float projectileLifeTime = 1000.0f;

    float projectileSize = 10.0f;

    pg::BulletPattern pattern = pg::BulletPattern::AtPlayer;

    size_t bulletCount = 1;
    float bulletSpreadAngle = 0.0f;

    float reloadTimeMs = 1000.0f;
    size_t barrelSize = 6;

    size_t ammo = 18;

    int id;
};

inline std::ostream &operator<<(std::ostream &os, const WeaponData &data) {
    os << "WeaponData {\n"
            << "  name: " << data.name << "\n"
            << "  damage: " << data.damage << "\n"
            << "  projectileSpeed: " << data.projectileSpeed << "\n"
            << "  projectileLifeTime: " << data.projectileLifeTime << "\n"
            << "  projectileSize: " << data.projectileSize << "\n"
            << "  pattern: " << static_cast<int>(data.pattern) << "\n"
            << "  bulletCount: " << data.bulletCount << "\n"
            << "  bulletSpreadAngle: " << data.bulletSpreadAngle << "\n"
            << "  reloadTimeMs: " << data.reloadTimeMs << "\n"
            << "  barrelSize: " << data.barrelSize << "\n"
            << "  ammo: " << data.ammo << "\n"
            << "  id: " << data.id << "\n"
            << "}";
    return os;
}


struct RoomData {
    int roomIndex;
    int nbEnemy;
};

inline std::ostream &operator<<(std::ostream &os, const RoomData &data) {
    os << "RoomData {\n"
            << "  roomIndex: " << data.roomIndex << "\n"
            << "  nbEnemy: " << data.nbEnemy << "\n"
            << "}";
    return os;
}

struct EnemyData {
    std::string name;
    /*!
     * if false, don't spawn
     */
    bool canSpawn;
    float chaseSpeed = 1.5f;
    float idealDistance = 250.f; // px
    float orbitThreshold = 20.f; // px
    float attackDistance = 200.f;
    float cooldownTime = 1000.0f; // ms
    float wideUpTime = 500.0f;

    bool isBoss;
    int hp;

    /*!
     * Enemy template i. SpawnData references it.
     */
    int objId;
    /*!
     * The weapon id used by this enemy. Found in WeaponData.
     */
    int weaponId;
};

inline std::ostream &operator<<(std::ostream &os, const EnemyData &data) {
    os << "EnemyData {\n"
            << "  name: " << data.name << "\n"
            << "  Id: " << data.objId << "\n"
            << "  isBoss: " << data.isBoss << "\n"
            << "  HP: " << data.hp << "\n"
            << "  chaseSpeed: " << data.chaseSpeed << "\n"
            << "  idealDistance: " << data.idealDistance << "\n"
            << "  orbitThreshold: " << data.orbitThreshold << "\n"
            << "  attackDistance: " << data.attackDistance << "\n"
            << "  cooldownTime: " << data.cooldownTime << "\n"
            << "  wideUpTime: " << data.wideUpTime << "\n"
            << "  weapon id: " << data.weaponId << "\n"
            << "  can spawn: " << data.canSpawn << "\n"
            << "}";
    return os;
}

struct MapData {
    SpawnPoint playerSpawn;
    /*!
     * Number of tiles horizontally
     */
    int width;
    /*!
     * Number of tiles vertically
     */
    int height;
    /*!
     * The list of layers. The index 0 is renderer first, then the index 1 and so on
     */
    std::vector<Layer> layers;
    /*!
     * All the room triggers to detect when a player enters a room
     */
    std::vector<RoomTrigger> roomTriggers;
    /*!
     * All the enemy types. When spawned, we can use data from this class. Find with objId.
     */
    std::vector<EnemyData> enemyTemplates;
    /*!
     * All the spawners. You can filter per room with the roomIndex.
     */
    std::vector<Spawner> spawners;
    /*!
     * All the rooms.
     */
    std::vector<RoomData> roomDatas;
    /*!
     * All the weapon types. an enemy can have a weapon, referencing it by id.
     */
    std::vector<WeaponData> weaponDatas;
    /*!
     * All the doors. You can filter per room with the roomIndex.
     */
    std::vector<Door> doors;
    /*!
     * All the spikes.
     */
    std::vector<Spike> spikes;
    /*!
     * All the spikes images
     */
    std::vector<SpikeImage> spike_images;
    std::vector<Gold> golds;
    /*!
     *
     */
    std::vector<TileSet> tilesets;
    /*!
         * Width in t pixels of each tile
         */
    int tileWidthInTPixels;
    /*!
     * Height in t pixels of each tile
     */
    int tileHeightInTPixels;
    /*!
         * Width in s pixels of each tile in S Pixels
         */
    int tileWidthInSPixels;
    /*!
     * Height in s pixels of each tile in S Pixels
     */
    int tileHeightInSPixels;
};

#endif //MAPDATA_H
