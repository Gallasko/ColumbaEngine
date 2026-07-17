#include "stdafx.h"

#include "voxelserializer.h"
#include "editorstate.h"
#include "voxelcomponents.h"
#include "ECS/entitysystem.h"
#include "logger.h"

#include <fstream>
#include <sstream>
#include <map>
#include <cmath>

namespace pg
{
    namespace
    {
        constexpr const char* DOM = "Serialization";

        // Find the palette index whose color matches (exact match on RGBA).
        int findPaletteIndex(const std::vector<glm::vec4>& palette, const glm::vec4& color)
        {
            for (size_t i = 0; i < palette.size(); ++i)
            {
                if (palette[i] == color)
                    return static_cast<int>(i);
            }
            return -1;
        }
    }

    // =========================================================================
    // Gather project data from canvas
    // =========================================================================

    ProjectData gatherProjectData(Canvas& canvas)
    {
        ProjectData data;
        data.W = canvas.W;
        data.H = canvas.H;
        data.D = canvas.D;
        data.palette = canvas.palette;

        // Layers
        for (const auto& layer : canvas.layers)
            data.layers.push_back({ layer.name, layer.visible });

        // Visible cells
        const int total = canvas.W * canvas.H * canvas.D;
        for (int idx = 0; idx < total; ++idx)
        {
            auto& ent = canvas.cells[static_cast<size_t>(idx)];
            if (ent.empty())
                continue;

            const int x = idx % canvas.W;
            const int y = (idx / canvas.W) % canvas.H;
            const int z = idx / (canvas.W * canvas.H);
            const int layerIdx = canvas.cellLayer[static_cast<size_t>(idx)];

            glm::vec4 color{255, 255, 255, 255};
            if (ent.has<VoxelComponent>())
                color = ent->get<VoxelComponent>()->color;

            int ci = findPaletteIndex(canvas.palette, color);
            if (ci < 0) ci = 0; // fallback

            data.cells.push_back({ {x, y, z}, ci, layerIdx });
        }

        // Hidden layer cells (detached from ECS)
        for (size_t li = 0; li < canvas.layers.size(); ++li)
        {
            const auto& layer = canvas.layers[li];
            for (const auto& hc : layer.hiddenData)
            {
                int ci = findPaletteIndex(canvas.palette, hc.color);
                if (ci < 0) ci = 0;
                data.cells.push_back({ hc.cell, ci, static_cast<int>(li) });
            }
        }

        return data;
    }

    // =========================================================================
    // Save to JSON
    // =========================================================================

    std::string saveProjectToString(const ProjectData& data)
    {
        std::ostringstream out;

        out << "{\n";
        out << "  \"version\": 1,\n";
        out << "  \"canvas\": { \"w\": " << data.W << ", \"h\": " << data.H << ", \"d\": " << data.D << " },\n";

        // Palette
        out << "  \"palette\": [\n";
        for (size_t i = 0; i < data.palette.size(); ++i)
        {
            const auto& c = data.palette[i];
            out << "    [" << static_cast<int>(c.r) << ", "
                           << static_cast<int>(c.g) << ", "
                           << static_cast<int>(c.b) << ", "
                           << static_cast<int>(c.a) << "]";
            if (i + 1 < data.palette.size()) out << ",";
            out << "\n";
        }
        out << "  ],\n";

        // Layers
        out << "  \"layers\": [\n";
        for (size_t i = 0; i < data.layers.size(); ++i)
        {
            const auto& l = data.layers[i];
            out << "    { \"name\": \"" << l.name << "\", \"visible\": " << (l.visible ? "true" : "false") << " }";
            if (i + 1 < data.layers.size()) out << ",";
            out << "\n";
        }
        out << "  ],\n";

        // Cells
        out << "  \"cells\": [\n";
        for (size_t i = 0; i < data.cells.size(); ++i)
        {
            const auto& cell = data.cells[i];
            out << "    { \"x\": " << cell.pos.x
                << ", \"y\": " << cell.pos.y
                << ", \"z\": " << cell.pos.z
                << ", \"c\": " << cell.colorIndex
                << ", \"l\": " << cell.layerIndex << " }";
            if (i + 1 < data.cells.size()) out << ",";
            out << "\n";
        }
        out << "  ]\n";

        out << "}\n";

        return out.str();
    }

    bool saveProjectToFile(const ProjectData& data, const std::string& path)
    {
        std::ofstream file(path);
        if (!file.is_open())
        {
            LOG_ERROR(DOM, "Cannot open file for writing: " << path);
            return false;
        }

        file << saveProjectToString(data);

        LOG_INFO(DOM, "Project saved to " << path << " (" << data.cells.size() << " cells)");
        return true;
    }

    // =========================================================================
    // Load from JSON — minimal hand-rolled parser
    // =========================================================================

    namespace
    {
        // Skip whitespace, return current char or EOF
        char skipWs(std::istream& in)
        {
            char c;
            while (in.get(c))
            {
                if (c != ' ' && c != '\t' && c != '\n' && c != '\r')
                    return c;
            }
            return '\0';
        }

        // Read a quoted string (assumes opening " already consumed)
        std::string readString(std::istream& in)
        {
            std::string s;
            char c;
            while (in.get(c))
            {
                if (c == '"') break;
                if (c == '\\') { in.get(c); } // escaped char
                s += c;
            }
            return s;
        }

        // Read a number (integer or float)
        double readNumber(std::istream& in, char first)
        {
            std::string s(1, first);
            char c;
            while (in.get(c))
            {
                if ((c >= '0' && c <= '9') || c == '.' || c == '-' || c == '+' || c == 'e' || c == 'E')
                    s += c;
                else
                { in.putback(c); break; }
            }
            return std::stod(s);
        }

        // Skip until a specific character is found
        void skipUntil(std::istream& in, char target)
        {
            char c;
            while (in.get(c))
            {
                if (c == target) return;
            }
        }
    }

    bool loadProjectFromString(const std::string& json, ProjectData& out)
    {
        std::istringstream in(json);

        out = ProjectData{};

        // Very simple parser: scan for known keys and parse their values.
        // This isn't a general JSON parser but handles our specific format.
        enum class Section { None, Canvas, Palette, PaletteEntry, Layers, LayerEntry, Cells, CellEntry };
        Section section = Section::None;

        char c;
        std::string currentKey;
        LayerData currentLayer;
        CellData currentCell{};
        std::vector<int> paletteEntry;

        while ((c = skipWs(in)) != '\0')
        {
            if (c == '"')
            {
                std::string key = readString(in);

                // Skip the ':'
                skipWs(in); // consume ':'

                if (key == "version")
                {
                    char fc = skipWs(in);
                    readNumber(in, fc); // consume version number
                }
                else if (key == "canvas")
                {
                    section = Section::Canvas;
                }
                else if (key == "palette")
                {
                    section = Section::Palette;
                }
                else if (key == "layers")
                {
                    section = Section::Layers;
                }
                else if (key == "cells")
                {
                    section = Section::Cells;
                }
                else if (key == "w")
                {
                    char fc = skipWs(in);
                    out.W = static_cast<int>(readNumber(in, fc));
                }
                else if (key == "h")
                {
                    char fc = skipWs(in);
                    out.H = static_cast<int>(readNumber(in, fc));
                }
                else if (key == "d")
                {
                    char fc = skipWs(in);
                    out.D = static_cast<int>(readNumber(in, fc));
                }
                else if (key == "name")
                {
                    skipWs(in); // consume opening "
                    currentLayer.name = readString(in);
                }
                else if (key == "visible")
                {
                    // Read true/false
                    char fc = skipWs(in);
                    std::string val(1, fc);
                    char ch;
                    while (in.get(ch))
                    {
                        if (ch == ',' || ch == '}' || ch == ']') { in.putback(ch); break; }
                        if (ch != ' ' && ch != '\n' && ch != '\r' && ch != '\t')
                            val += ch;
                    }
                    currentLayer.visible = (val == "true");
                }
                else if (key == "x")
                {
                    char fc = skipWs(in);
                    currentCell.pos.x = static_cast<int>(readNumber(in, fc));
                }
                else if (key == "y")
                {
                    char fc = skipWs(in);
                    currentCell.pos.y = static_cast<int>(readNumber(in, fc));
                }
                else if (key == "z")
                {
                    char fc = skipWs(in);
                    currentCell.pos.z = static_cast<int>(readNumber(in, fc));
                }
                else if (key == "c")
                {
                    char fc = skipWs(in);
                    currentCell.colorIndex = static_cast<int>(readNumber(in, fc));
                }
                else if (key == "l")
                {
                    char fc = skipWs(in);
                    currentCell.layerIndex = static_cast<int>(readNumber(in, fc));
                }

                currentKey = key;
            }
            else if (c == '[')
            {
                if (section == Section::Palette)
                {
                    // Could be outer array or inner [r,g,b,a]
                    // Peek to distinguish: if next non-ws is a digit, it's a color entry
                    char fc = skipWs(in);
                    if (fc == '[')
                    {
                        // Outer array, first inner array starts
                        in.putback(fc);
                    }
                    else if (fc >= '0' && fc <= '9' || fc == '-')
                    {
                        // Inner palette entry [r, g, b, a]
                        paletteEntry.clear();
                        paletteEntry.push_back(static_cast<int>(readNumber(in, fc)));
                    }
                    else
                    {
                        in.putback(fc);
                    }
                }
            }
            else if (c == ']')
            {
                if (section == Section::Palette && !paletteEntry.empty())
                {
                    // End of a palette entry
                    while (paletteEntry.size() < 4)
                        paletteEntry.push_back(255);
                    out.palette.push_back({
                        static_cast<float>(paletteEntry[0]),
                        static_cast<float>(paletteEntry[1]),
                        static_cast<float>(paletteEntry[2]),
                        static_cast<float>(paletteEntry[3])
                    });
                    paletteEntry.clear();
                }
            }
            else if (c == '{')
            {
                if (section == Section::Layers)
                {
                    currentLayer = LayerData{};
                    section = Section::LayerEntry;
                }
                else if (section == Section::Cells)
                {
                    currentCell = CellData{};
                    section = Section::CellEntry;
                }
            }
            else if (c == '}')
            {
                if (section == Section::LayerEntry && !currentLayer.name.empty())
                {
                    out.layers.push_back(currentLayer);
                    section = Section::Layers;
                }
                else if (section == Section::CellEntry)
                {
                    out.cells.push_back(currentCell);
                    section = Section::Cells;
                }
            }
            else if (c == ',')
            {
                if (section == Section::Palette && !paletteEntry.empty())
                {
                    // Next number in palette entry
                    char fc = skipWs(in);
                    if (fc >= '0' && fc <= '9' || fc == '-')
                        paletteEntry.push_back(static_cast<int>(readNumber(in, fc)));
                    else
                        in.putback(fc);
                }
            }
            else if (c >= '0' && c <= '9' || c == '-')
            {
                // Stray number (shouldn't happen normally with our key-based parser)
            }
        }

        return true;
    }

    bool loadProjectFromFile(const std::string& path, ProjectData& out)
    {
        std::ifstream file(path);
        if (!file.is_open())
        {
            LOG_ERROR(DOM, "Cannot open file for reading: " << path);
            return false;
        }

        std::string content((std::istreambuf_iterator<char>(file)),
                             std::istreambuf_iterator<char>());

        if (!loadProjectFromString(content, out))
            return false;

        LOG_INFO(DOM, "Project loaded from " << path << " (" << out.cells.size() << " cells, "
                 << out.layers.size() << " layers)");
        return true;
    }

    // =========================================================================
    // OBJ Export
    // =========================================================================

    bool exportOBJ(Canvas& canvas, const std::string& basePath)
    {
        const std::string objPath = basePath + ".obj";
        const std::string mtlPath = basePath + ".mtl";

        // Extract the MTL filename (no directory prefix)
        std::string mtlName = basePath;
        auto lastSlash = mtlName.find_last_of("/\\");
        if (lastSlash != std::string::npos)
            mtlName = mtlName.substr(lastSlash + 1);
        mtlName += ".mtl";

        // Gather all occupied cells with their colors
        struct VoxelData { glm::ivec3 pos; glm::vec4 color; };
        std::vector<VoxelData> voxels;

        const int total = canvas.W * canvas.H * canvas.D;
        for (int idx = 0; idx < total; ++idx)
        {
            auto& ent = canvas.cells[static_cast<size_t>(idx)];
            if (ent.empty() || !ent.has<VoxelComponent>())
                continue;

            const int x = idx % canvas.W;
            const int y = (idx / canvas.W) % canvas.H;
            const int z = idx / (canvas.W * canvas.H);

            voxels.push_back({ {x, y, z}, ent->get<VoxelComponent>()->color });
        }

        // Also include hidden layer cells
        for (const auto& layer : canvas.layers)
        {
            for (const auto& hc : layer.hiddenData)
                voxels.push_back({ hc.cell, hc.color });
        }

        if (voxels.empty())
        {
            LOG_INFO(DOM, "Nothing to export (canvas is empty)");
            return false;
        }

        // Build unique color map → material name
        struct ColorKey
        {
            int r, g, b, a;
            bool operator<(const ColorKey& o) const
            {
                if (r != o.r) return r < o.r;
                if (g != o.g) return g < o.g;
                if (b != o.b) return b < o.b;
                return a < o.a;
            }
        };

        std::map<ColorKey, std::string> materialMap;
        int matIdx = 0;

        for (const auto& v : voxels)
        {
            ColorKey ck{ static_cast<int>(v.color.r), static_cast<int>(v.color.g),
                         static_cast<int>(v.color.b), static_cast<int>(v.color.a) };
            if (materialMap.find(ck) == materialMap.end())
                materialMap[ck] = "mat_" + std::to_string(matIdx++);
        }

        // Write MTL file
        {
            std::ofstream mtl(mtlPath);
            if (!mtl.is_open())
            {
                LOG_ERROR(DOM, "Cannot open MTL file for writing: " << mtlPath);
                return false;
            }

            mtl << "# Voxel Editor MTL export\n\n";

            for (const auto& [ck, name] : materialMap)
            {
                const float r = ck.r / 255.0f;
                const float g = ck.g / 255.0f;
                const float b = ck.b / 255.0f;

                mtl << "newmtl " << name << "\n";
                mtl << "Kd " << r << " " << g << " " << b << "\n";
                mtl << "Ka " << r * 0.2f << " " << g * 0.2f << " " << b * 0.2f << "\n";
                mtl << "Ks 0.1 0.1 0.1\n";
                mtl << "Ns 32.0\n";
                mtl << "d " << (ck.a / 255.0f) << "\n";
                mtl << "illum 2\n\n";
            }
        }

        // Write OBJ file
        {
            std::ofstream obj(objPath);
            if (!obj.is_open())
            {
                LOG_ERROR(DOM, "Cannot open OBJ file for writing: " << objPath);
                return false;
            }

            obj << "# Voxel Editor OBJ export\n";
            obj << "mtllib " << mtlName << "\n\n";

            // 6 face normals
            obj << "vn  0  0 -1\n";  // 1: front  (-Z)
            obj << "vn  0  0  1\n";  // 2: back   (+Z)
            obj << "vn  0 -1  0\n";  // 3: bottom (-Y)
            obj << "vn  0  1  0\n";  // 4: top    (+Y)
            obj << "vn -1  0  0\n";  // 5: left   (-X)
            obj << "vn  1  0  0\n";  // 6: right  (+X)
            obj << "\n";

            // Group voxels by material for fewer usemtl switches
            std::map<ColorKey, std::vector<size_t>> byMaterial;
            for (size_t i = 0; i < voxels.size(); ++i)
            {
                const auto& v = voxels[i];
                ColorKey ck{ static_cast<int>(v.color.r), static_cast<int>(v.color.g),
                             static_cast<int>(v.color.b), static_cast<int>(v.color.a) };
                byMaterial[ck].push_back(i);
            }

            // Emit all vertices first (8 per voxel)
            // Vertex ordering per cube:
            //   0: (x,   y,   z  )   4: (x,   y,   z+1)
            //   1: (x+1, y,   z  )   5: (x+1, y,   z+1)
            //   2: (x+1, y+1, z  )   6: (x+1, y+1, z+1)
            //   3: (x,   y+1, z  )   7: (x,   y+1, z+1)
            for (const auto& v : voxels)
            {
                const float x = static_cast<float>(v.pos.x);
                const float y = static_cast<float>(v.pos.y);
                const float z = static_cast<float>(v.pos.z);

                obj << "v " << x     << " " << y     << " " << z     << "\n";
                obj << "v " << x+1.f << " " << y     << " " << z     << "\n";
                obj << "v " << x+1.f << " " << y+1.f << " " << z     << "\n";
                obj << "v " << x     << " " << y+1.f << " " << z     << "\n";
                obj << "v " << x     << " " << y     << " " << z+1.f << "\n";
                obj << "v " << x+1.f << " " << y     << " " << z+1.f << "\n";
                obj << "v " << x+1.f << " " << y+1.f << " " << z+1.f << "\n";
                obj << "v " << x     << " " << y+1.f << " " << z+1.f << "\n";
            }
            obj << "\n";

            // Emit faces grouped by material
            for (const auto& [ck, indices] : byMaterial)
            {
                obj << "usemtl " << materialMap[ck] << "\n";

                for (size_t vi : indices)
                {
                    // Base vertex index (1-based)
                    const int b = static_cast<int>(vi) * 8 + 1;

                    // Front face (-Z): vertices 0,1,2,3 → normal 1
                    obj << "f " << b   << "//1 " << b+1 << "//1 " << b+2 << "//1\n";
                    obj << "f " << b   << "//1 " << b+2 << "//1 " << b+3 << "//1\n";

                    // Back face (+Z): vertices 4,7,6,5 → normal 2
                    obj << "f " << b+4 << "//2 " << b+7 << "//2 " << b+6 << "//2\n";
                    obj << "f " << b+4 << "//2 " << b+6 << "//2 " << b+5 << "//2\n";

                    // Bottom face (-Y): vertices 0,4,5,1 → normal 3
                    obj << "f " << b   << "//3 " << b+4 << "//3 " << b+5 << "//3\n";
                    obj << "f " << b   << "//3 " << b+5 << "//3 " << b+1 << "//3\n";

                    // Top face (+Y): vertices 3,2,6,7 → normal 4
                    obj << "f " << b+3 << "//4 " << b+2 << "//4 " << b+6 << "//4\n";
                    obj << "f " << b+3 << "//4 " << b+6 << "//4 " << b+7 << "//4\n";

                    // Left face (-X): vertices 0,3,7,4 → normal 5
                    obj << "f " << b   << "//5 " << b+3 << "//5 " << b+7 << "//5\n";
                    obj << "f " << b   << "//5 " << b+7 << "//5 " << b+4 << "//5\n";

                    // Right face (+X): vertices 1,5,6,2 → normal 6
                    obj << "f " << b+1 << "//6 " << b+5 << "//6 " << b+6 << "//6\n";
                    obj << "f " << b+1 << "//6 " << b+6 << "//6 " << b+2 << "//6\n";
                }
                obj << "\n";
            }
        }

        LOG_INFO(DOM, "OBJ exported to " << objPath << " (" << voxels.size() << " voxels, "
                 << materialMap.size() << " materials)");
        return true;
    }

} // namespace pg
