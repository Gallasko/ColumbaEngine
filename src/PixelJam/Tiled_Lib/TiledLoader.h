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
};

#endif //TILEDLOADER_H
