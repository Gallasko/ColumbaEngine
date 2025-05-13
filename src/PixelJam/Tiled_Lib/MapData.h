//
// Created by nicol on 5/12/2025.
//

#ifndef MAPDATA_H
#define MAPDATA_H

#include <string>
#include <vector>

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
    /*!
     * Number of tiles horizontally
     */
    int width;
    /*!
     * Number of tiles vertically
     */
    int height;
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

struct TiledRect {
    float topLeftCornerX;
    float topLeftCornerY;
    float width;
    float height;
};

struct RoomTrigger {
    TiledRect rectInSPixels;
    int roomIndex;
};

struct SpawnData {
    int enemyId;
    float spawnProba;
};

struct Spawner {
    std::vector<SpawnData> spawns;
    int roomIndex;
    float posXInSPixels;
    float posYInSPixels;
};

struct RoomData {
    int roomIndex;
    int nbEnemy;
};

struct EnemyData {
    std::string name;
    float chaseSpeed = 1.5f;
    float idealDistance = 250.f; // px
    float orbitThreshold = 20.f; // px
    float attackDistance = 200.f;
    int cooldownTime = 1000; // ms
    int wideUpTime = 500;
    long aiCooldownTimer = 0;
    float spiralRate = 0.1f;
    float enemyBulletDamage = 1.f;
    int objId;
};

inline std::ostream &operator<<(std::ostream &os, const EnemyData &data) {
    os << "EnemyData {\n"
            << "  name: " << data.name << "\n"
            << "  Id: " << data.objId << "\n"
            << "  chaseSpeed: " << data.chaseSpeed << "\n"
            << "  idealDistance: " << data.idealDistance << "\n"
            << "  orbitThreshold: " << data.orbitThreshold << "\n"
            << "  attackDistance: " << data.attackDistance << "\n"
            << "  cooldownTime: " << data.cooldownTime << "\n"
            << "  wideUpTime: " << data.wideUpTime << "\n"
            << "  aiCooldownTimer: " << data.aiCooldownTimer << "\n"
            << "  spiralRate: " << data.spiralRate << "\n"
            << "  enemyBulletDamage: " << data.enemyBulletDamage << "\n"
            << "}";
    return os;
}

struct MapData {
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
    std::vector<RoomTrigger> roomTriggers;
    std::vector<EnemyData> enemyTemplates;
    std::vector<Spawner> spawners;
    std::vector<RoomData> roomData;
    /*!
     *
     */
    std::vector<TileSet> tilesets;
    /*!
         * Width in pixels of each tile
         */
    int tileWidthInTPixels;
    /*!
     * Height in pixels of each tile
     */
    int tileHeightInTPixels;
    /*!
         * Width in pixels of each tile in S Pixels
         */
    int tileWidthInSPixels;
    /*!
     * Height in pixels of each tile in S Pixels
     */
    int tileHeightInSPixels;
};

#endif //MAPDATA_H
