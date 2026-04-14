#pragma once

#include <string>
#include <vector>

#include "glm/glm.hpp"

namespace pg
{
    struct Canvas;

    // --------------------------------------------------------------------------
    // Project save/load  (.vxl.json)
    // --------------------------------------------------------------------------

    struct CellData
    {
        glm::ivec3 pos;
        int        colorIndex;
        int        layerIndex;
    };

    struct LayerData
    {
        std::string name;
        bool        visible;
    };

    struct ProjectData
    {
        int W, H, D;
        std::vector<glm::vec4> palette;
        std::vector<LayerData> layers;
        std::vector<CellData>  cells;
    };

    // Gather all canvas data (including hidden layers) into a ProjectData.
    ProjectData gatherProjectData(Canvas& canvas);

    // Serialize ProjectData to a JSON string.
    std::string saveProjectToString(const ProjectData& data);

    // Write ProjectData to a JSON file.
    bool saveProjectToFile(const ProjectData& data, const std::string& path);

    // Parse ProjectData from a JSON string.
    bool loadProjectFromString(const std::string& json, ProjectData& out);

    // Read ProjectData from a JSON file.
    bool loadProjectFromFile(const std::string& path, ProjectData& out);

    // --------------------------------------------------------------------------
    // OBJ export
    // --------------------------------------------------------------------------

    // Export canvas geometry as Wavefront OBJ + MTL.
    // basePath: writes <basePath>.obj and <basePath>.mtl
    bool exportOBJ(Canvas& canvas, const std::string& basePath);

} // namespace pg
