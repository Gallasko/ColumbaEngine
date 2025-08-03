#include "stdafx.h"

#include "polygonmesh.h"

#include "Helpers/openglobject.h"

namespace pg
{
    PolygonData PolygonMesh::prepareMesh()
    {
        PolygonData data;
        size_t N = m_polygon.size();

        if (N < 3)
        {
            modelInfo.nbVertices = 0;
            modelInfo.nbIndices = 0;
            return data;
        }

        // Reserve space for vertices (x, y per vertex)
        data.verts.reserve(N * 2);

        // Build vertices (x, y)
        for (size_t i = 0; i < N; ++i)
        {
            data.verts.push_back(m_polygon[i].x);
            data.verts.push_back(m_polygon[i].y);
        }

        // Triangulate the polygon using ear clipping
        data.idx = triangulateEarClipping(m_polygon);

        return data;
    }

    void PolygonMesh::generateMesh()
    {
        auto data = prepareMesh();

        // 2) Copy into modelInfo
        modelInfo.nbVertices = static_cast<unsigned int>(data.verts.size());
        modelInfo.vertices = new float[data.verts.size()];
        
        std::copy(data.verts.begin(), data.verts.end(), modelInfo.vertices);
        
        modelInfo.nbIndices = static_cast<unsigned int>(data.idx.size());
        modelInfo.indices = new unsigned int[data.idx.size()];
        
        std::copy(data.idx.begin(), data.idx.end(), modelInfo.indices);

        // 3) Upload to GPU
        openGLMesh.initialize();
        openGLMesh.VAO->bind();

        // Vertex buffer
        openGLMesh.VBO->bind();
        openGLMesh.VBO->setUsagePattern(OpenGLBuffer::DynamicDraw);
        openGLMesh.VBO->allocate(modelInfo.vertices, modelInfo.nbVertices * sizeof(float));

        // Position attribute (location = 0, vec2)
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);

        // Attribute for instancing (location = 1, float)

        openGLMesh.instanceVBO->setUsagePattern(OpenGLBuffer::DynamicDraw);
        openGLMesh.instanceVBO->create();

        openGLMesh.instanceVBO->bind();

        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, 1 * sizeof(float), (void*)(0 * sizeof(float)));

        glVertexAttribDivisor(1, 1); // tell OpenGL this is an instanced vertex attribute.

        glBindBuffer(GL_ARRAY_BUFFER, 0);

        // -------------

        // Index buffer
        openGLMesh.EBO->bind();
        openGLMesh.EBO->setUsagePattern(OpenGLBuffer::DynamicDraw);
        openGLMesh.EBO->allocate(modelInfo.indices, modelInfo.nbIndices * sizeof(unsigned int));

        openGLMesh.VAO->release();
        initialized = true;
    }

    // Ear clipping triangulation implementation
    std::vector<unsigned int> PolygonMesh::triangulateEarClipping(const std::vector<Point2D>& polygon) const
    {
        std::vector<unsigned int> indices;
        size_t n = polygon.size();

        if (n < 3)
            return indices;

        if (n == 3)
        {
            // Simple triangle
            indices = {0, 1, 2};
            return indices;
        }

        // Create a list of vertex indices
        std::vector<int> vertexList;
        vertexList.reserve(n);
        for (int i = 0; i < static_cast<int>(n); ++i)
        {
            vertexList.push_back(i);
        }

        // Determine winding order and ensure counter-clockwise
        float area = 0.0f;
        for (size_t i = 0; i < n; ++i)
        {
            size_t j = (i + 1) % n;
            area += (polygon[j].x - polygon[i].x) * (polygon[j].y + polygon[i].y);
        }

        if (area > 0.0f)
        {
            // Clockwise, reverse the order
            std::reverse(vertexList.begin(), vertexList.end());
        }

        // Ear clipping algorithm
        int nv = static_cast<int>(n);
        int count = 2 * nv; // Prevent infinite loops
        int v = nv - 1;

        while (nv > 2)
        {
            if (--count <= 0)
            {
                // Something went wrong, return what we have
                break;
            }

            int u = v;
            if (nv <= u) u = 0;
            v = u + 1;
            if (nv <= v) v = 0;
            int w = v + 1;
            if (nv <= w) w = 0;

            if (isEar(polygon, vertexList[u], vertexList[v], vertexList[w]))
            {
                int a = vertexList[u];
                int b = vertexList[v];
                int c = vertexList[w];

                // Output triangle
                indices.push_back(static_cast<unsigned int>(a));
                indices.push_back(static_cast<unsigned int>(b));
                indices.push_back(static_cast<unsigned int>(c));

                // Remove vertex v from the list
                for (int s = v, t = v + 1; t < nv; s++, t++)
                {
                    vertexList[s] = vertexList[t];
                }
                nv--;

                // Reset counter and adjust v
                count = 2 * nv;
                if (v >= nv) v = 0;
            }
            else
            {
                // Move to next vertex
                v++;
                if (v >= nv) v = 0;
            }
        }

        return indices;
    }

    bool PolygonMesh::isEar(const std::vector<Point2D>& poly, int prev, int curr, int next) const
    {
        // Check if the triangle is convex (ear tip)
        if (!isConvexVertex(poly, prev, curr, next))
            return false;

        // Check if any other vertex lies inside this triangle
        const Point2D& a = poly[prev];
        const Point2D& b = poly[curr];
        const Point2D& c = poly[next];

        // Check triangle area - if too small, might cause issues
        float area = std::abs(cross2D(a, b, c));
        if (area < 1e-10f)
            return false;

        for (size_t i = 0; i < poly.size(); ++i)
        {
            int idx = static_cast<int>(i);
            if (idx == prev || idx == curr || idx == next)
                continue;

            if (isPointInTriangle(poly[i], a, b, c))
                return false;
        }

        return true;
    }

    bool PolygonMesh::isConvexVertex(const std::vector<Point2D>& poly, int prev, int curr, int next) const
    {
        return cross2D(poly[prev], poly[curr], poly[next]) > 0.0f;
    }

    float PolygonMesh::cross2D(const Point2D& a, const Point2D& b, const Point2D& c) const
    {
        return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
    }

    bool PolygonMesh::isPointInTriangle(const Point2D& p, const Point2D& a, const Point2D& b, const Point2D& c) const
    {
        // Use barycentric coordinates for more robust point-in-triangle test
        float denom = (b.y - c.y) * (a.x - c.x) + (c.x - b.x) * (a.y - c.y);
        
        if (std::abs(denom) < 1e-10f)
            return false; // Degenerate triangle
        
        float alpha = ((b.y - c.y) * (p.x - c.x) + (c.x - b.x) * (p.y - c.y)) / denom;
        float beta = ((c.y - a.y) * (p.x - c.x) + (a.x - c.x) * (p.y - c.y)) / denom;
        float gamma = 1.0f - alpha - beta;
        
        // Point is inside if all barycentric coordinates are non-negative
        // Use small epsilon to handle floating point precision issues
        const float eps = 1e-10f;
        return (alpha >= eps && beta >= eps && gamma >= eps);
    }
}