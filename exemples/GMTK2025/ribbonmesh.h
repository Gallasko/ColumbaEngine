#pragma once

#include "Renderer/mesh.h"
#include "Maths/geometry.h"   // For Point2D, Segment2D
#include <vector>

namespace pg {

/**
 * @brief A dynamic textured ribbon mesh that follows a polyline path.
 */
struct TexturedRibbonMesh : public Mesh
{
    TexturedRibbonMesh(const std::vector<Point2D>& path, float halfWidth, bool repeatBySize = false, float repeatValue = 1.0f) : Mesh(),
        m_path(path),
        m_halfWidth(halfWidth),
        m_repeatBySize(repeatBySize),
        m_repeatValue(repeatValue)
    {
        // No pre-population of modelInfo here; done in generateMesh()
    }

    virtual ~TexturedRibbonMesh() {}

    /**
     * Build the ribbon vertex & index buffers and upload to OpenGL.
     */
    void generateMesh() override;

private:
    std::vector<Point2D> m_path;
    float m_halfWidth;
    bool  m_repeatBySize;
    float m_repeatValue;
};

} // namespace pg
