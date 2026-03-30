//
// Created by nicol on 5/12/2025.
//

#ifndef TILEDLOADER_H
#define TILEDLOADER_H

#include "tileson.hpp"
#include "MapData.h"

class TiledLoader {
public:
    /*!
     * Loads a tiled map from file.
     * @attention The map must be exported from Tiled as json, the file must end with .json and the TILESETS must be EMBEDDED, and not in a separate file.
     * @param path the path the to file. Local to the std::filesystem::current_path(). Ends in .json. Example : "tiled/MyMap.json"
     * @param scaleFactor scales all the coords by this amount. So if a tile is 16x16 in Tiled and factor is 2, it will be 32x32 pixels on screen
     * @return All the data of the map
     */
    MapData loadMap(const std::string& path, int scaleFactor);
private:
    /*!
     * Converts a tile distance to Spixels.
     * Let's say the tile distance is 3, factor is 2 and tile size is 16, then distance in SPixels is 3 * 2 * 16
     * @param tileDistance Distance in tiles
     * @param scaleFactor the factor to go from TPixels to SPixels
     * @param tileSize tile size in TPixels
     * @return the distance it SPixels
     */
    float tileDistanceToSPixels(float tileDistance, float scaleFactor, int tileSize);
};

#endif //TILEDLOADER_H
