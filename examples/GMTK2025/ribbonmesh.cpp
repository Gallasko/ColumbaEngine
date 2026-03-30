#include "stdafx.h"

#include "ribbonmesh.h"

#include "Helpers/openglobject.h"

namespace pg
{
    RibbonData TexturedRibbonMesh::prepareMesh()
    {
        RibbonData data;
        size_t N = m_path.size();

        if (N < 2)
        {
            modelInfo.nbVertices = 0;
            modelInfo.nbIndices = 0;

            return data;
        }

        data.verts.reserve(N * 4);
        data.idx.reserve((N - 1) * 6);

        std::vector<float> lengthAt(N, 0.0f);
        float totalLen = 0.0f;

        for (size_t i = 1; i < N; ++i)
        {
            float dx = m_path[i].x - m_path[i - 1].x;
            float dy = m_path[i].y - m_path[i - 1].y;

            float l = std::sqrt(dx * dx + dy * dy);

            totalLen += l;
            lengthAt[i] = totalLen;
        }

        // Build vertices (x,y,u,v)
        for (size_t i = 0; i < N; ++i)
        {
            // compute tangent
            Point2D tang;

            if (i == 0)
                tang = Segment2D(m_path[0], m_path[1]).normalizedDirection();
            else if (i == N - 1)
                tang = Segment2D(m_path[N - 2], m_path[N - 1]).normalizedDirection();
            else
            {
                Point2D d1 = Segment2D(m_path[i - 1], m_path[i]).normalizedDirection();
                Point2D d2 = Segment2D(m_path[i], m_path[i + 1]).normalizedDirection();
                tang = Point2D((d1.x + d2.x) * 0.5f, (d1.y + d2.y) * 0.5f);

                float ll = std::sqrt(tang.x * tang.x + tang.y * tang.y);

                if (ll > 1e-5f)
                {
                    tang.x /= ll;
                    tang.y /= ll;
                }
            }

            Point2D norm(-tang.y, tang.x);

            float u = 0.0f;

            if (m_repeatBySize and m_repeatValue > 1e-5f)
                u = lengthAt[i] / m_repeatValue;
            else if (!m_repeatBySize and totalLen > 1e-5f)
                u = lengthAt[i] / totalLen;

            // left
            data.verts.push_back(m_path[i].x + norm.x * m_halfWidth);
            data.verts.push_back(m_path[i].y + norm.y * m_halfWidth);
            data.verts.push_back(u);
            data.verts.push_back(0.0f);
            // right
            data.verts.push_back(m_path[i].x - norm.x * m_halfWidth);
            data.verts.push_back(m_path[i].y - norm.y * m_halfWidth);
            data.verts.push_back(u);
            data.verts.push_back(1.0f);
        }

        // Build indices
        for (unsigned int i = 0; i < N - 1; ++i)
        {
            unsigned int i0 = i*2;
            unsigned int i1 = i*2 + 1;
            unsigned int i2 = i*2 + 2;
            unsigned int i3 = i*2 + 3;
        
            data.idx.push_back(i0); data.idx.push_back(i1); data.idx.push_back(i2);
            data.idx.push_back(i2); data.idx.push_back(i1); data.idx.push_back(i3);
        }

        return data;
    }

    void TexturedRibbonMesh::generateMesh()
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

        // Position (loc=0), UV (loc=1)
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));

        // Index buffer
        openGLMesh.EBO->bind();
        openGLMesh.EBO->setUsagePattern(OpenGLBuffer::DynamicDraw);
        openGLMesh.EBO->allocate(modelInfo.indices, modelInfo.nbIndices * sizeof(unsigned int));

        openGLMesh.VAO->release();
        initialized = true;
    }

}