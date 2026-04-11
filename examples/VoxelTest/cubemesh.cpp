#include "stdafx.h"

#include "cubemesh.h"

#include "Helpers/openglobject.h"

namespace pg
{
    CubeMesh::CubeMesh() : Mesh()
    {
        // 8 vertices × 3 floats each (x, y, z only).
        modelInfo.nbVertices = 8 * 3;
        modelInfo.vertices = new float[24]
        {
            0.0f, 0.0f, 0.0f, // 0 — bottom-near-left
            1.0f, 0.0f, 0.0f, // 1 — bottom-near-right
            1.0f, 1.0f, 0.0f, // 2 — top-near-right
            0.0f, 1.0f, 0.0f, // 3 — top-near-left
            0.0f, 0.0f, 1.0f, // 4 — bottom-far-left
            1.0f, 0.0f, 1.0f, // 5 — bottom-far-right
            1.0f, 1.0f, 1.0f, // 6 — top-far-right
            0.0f, 1.0f, 1.0f, // 7 — top-far-left
        };

        // 6 faces × 2 triangles × 3 indices = 36 indices.
        modelInfo.nbIndices = 36;
        modelInfo.indices = new unsigned int[36]
        {
            // z = 0 (near)
            0, 1, 2,   0, 2, 3,
            // z = 1 (far)
            4, 6, 5,   4, 7, 6,
            // x = 0 (left)
            0, 3, 7,   0, 7, 4,
            // x = 1 (right)
            1, 5, 6,   1, 6, 2,
            // y = 0 (bottom)
            0, 4, 5,   0, 5, 1,
            // y = 1 (top)
            3, 2, 6,   3, 6, 7,
        };
    }

    CubeMesh::~CubeMesh()
    {
        // ModelInfo's destructor frees `vertices` and `indices`.
    }

    void CubeMesh::generateMesh()
    {
        openGLMesh.initialize();

        openGLMesh.VAO->bind();

        // --- Per-vertex positions (static VBO) ---
        openGLMesh.VBO->bind();
        openGLMesh.VBO->setUsagePattern(OpenGLBuffer::StaticDraw);
        openGLMesh.VBO->allocate(modelInfo.vertices, modelInfo.nbVertices * sizeof(float));

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);

        // --- Per-instance data (dynamic VBO) ---
        openGLMesh.instanceVBO->setUsagePattern(OpenGLBuffer::DynamicDraw);
        openGLMesh.instanceVBO->create();
        openGLMesh.instanceVBO->bind();

        const size_t stride = INSTANCE_STRIDE_FLOATS * sizeof(float);

        // location 1: aWorldPos (vec3), offset 0
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)(0));
        glVertexAttribDivisor(1, 1);

        // location 2: aSize (vec3), offset 3 floats
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));
        glVertexAttribDivisor(2, 1);

        // location 3: aColors (vec4), offset 6 floats
        glEnableVertexAttribArray(3);
        glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, stride, (void*)(6 * sizeof(float)));
        glVertexAttribDivisor(3, 1);

        glBindBuffer(GL_ARRAY_BUFFER, 0);

        // --- Index buffer ---
        openGLMesh.EBO->bind();
        openGLMesh.EBO->setUsagePattern(OpenGLBuffer::StaticDraw);
        openGLMesh.EBO->allocate(modelInfo.indices, modelInfo.nbIndices * sizeof(unsigned int));

        openGLMesh.VAO->release();

        initialized = true;
    }
}
