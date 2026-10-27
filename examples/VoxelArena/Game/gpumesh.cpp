#include "stdafx.h"

#include "gpumesh.h"

#include "Helpers/openglobject.h"

namespace arena
{
    void GpuMesh::setData(MeshData data)
    {
        const size_t faces = data.faceCount();

        {
            std::lock_guard<std::mutex> lock(mutex);
            pending = std::move(data);
        }

        faceCount = faces;
        hasPending = true;
    }

    void GpuMesh::generateMesh()
    {
        using pg::OpenGLBuffer;

        if (not glReady)
        {
            openGLMesh.initialize();
            openGLMesh.VAO->bind();

            openGLMesh.VBO->bind();
            openGLMesh.VBO->setUsagePattern(OpenGLBuffer::DynamicDraw);

            const GLsizei stride = MeshData::FLOATS_PER_VERTEX * sizeof(float);
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
            glEnableVertexAttribArray(1);
            glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));
            glEnableVertexAttribArray(2);
            glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, stride, (void*)(6 * sizeof(float)));
            glEnableVertexAttribArray(3);
            glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, stride, (void*)(9 * sizeof(float)));

            openGLMesh.instanceVBO->setUsagePattern(OpenGLBuffer::DynamicDraw);
            openGLMesh.instanceVBO->bind();

            const GLsizei istride = INSTANCE_FLOATS * sizeof(float);

            // Model matrix then view * model: eight vec4 columns, locations 4 to 11; tint 12, flash 13
            for (int i = 0; i < 10; ++i)
            {
                glEnableVertexAttribArray(4 + i);
                glVertexAttribPointer(4 + i, 4, GL_FLOAT, GL_FALSE, istride, (void*)(i * 4 * sizeof(float)));
                glVertexAttribDivisor(4 + i, 1);
            }

            glBindBuffer(GL_ARRAY_BUFFER, 0);

            openGLMesh.EBO->bind();
            openGLMesh.EBO->setUsagePattern(OpenGLBuffer::DynamicDraw);

            openGLMesh.VAO->release();

            modelInfo.nbIndices = 0;
            glReady = true;
        }

        if (hasPending.exchange(false))
        {
            std::lock_guard<std::mutex> lock(mutex);

            openGLMesh.VAO->bind();
            openGLMesh.VBO->allocate(pending.vertices.data(), static_cast<int>(pending.vertices.size() * sizeof(float)));
            openGLMesh.EBO->allocate(pending.indices.data(), static_cast<int>(pending.indices.size() * sizeof(uint32_t)));
            openGLMesh.VAO->release();

            modelInfo.nbIndices = pending.indices.size();

            pending = MeshData{};
        }
    }
}
