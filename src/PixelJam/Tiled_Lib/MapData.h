//
// Created by nicol on 5/12/2025.
//

#ifndef MAPDATA_H
#define MAPDATA_H

#include <string>
#include <vector>

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
         * Width in pixels of each tile
         */
    int tileWidth;
    /*!
     * Height in pixels of each tile
     */
    int tileHeight;
};

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
    /*!
     *
     */
    std::vector<TileSet> tilesets;
    /*!
         * Width in pixels of each tile
         */
    int tileWidth;
    /*!
     * Height in pixels of each tile
     */
    int tileHeight;
};

#endif //MAPDATA_H
