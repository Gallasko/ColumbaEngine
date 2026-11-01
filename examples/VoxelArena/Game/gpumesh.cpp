#include "stdafx.h"

#include "gpumesh.h"

#include "Helpers/openglobject.h"

namespace arena
{
    void GpuTextureArray::setData(int size, int layers, std::vector<uint8_t> rgba)
    {
        std::lock_guard<std::mutex> lock(mutex);
        pending = std::move(rgba);
        pendingSize = size;
        pendingLayers = layers;
        hasPending = true;
    }

    void GpuTextureArray::bind()
    {
        glActiveTexture(GL_TEXTURE0);

        if (hasPending.exchange(false))
        {
            std::lock_guard<std::mutex> lock(mutex);

            if (pendingLayers > 0 and pending.size() >= static_cast<size_t>(pendingSize) * pendingSize * pendingLayers * 4)
            {
                if (id == 0)
                    glGenTextures(1, &id);

                glBindTexture(GL_TEXTURE_2D_ARRAY, id);
                glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
                glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA8, pendingSize, pendingSize, pendingLayers, 0, GL_RGBA, GL_UNSIGNED_BYTE, pending.data());
                glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                // Crisp pixels up close, mipmaps far away so the ground does not shimmer
                glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_LINEAR);
                glGenerateMipmap(GL_TEXTURE_2D_ARRAY);
            }

            pending = {};
        }

        if (id != 0)
            glBindTexture(GL_TEXTURE_2D_ARRAY, id);
    }

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
            glEnableVertexAttribArray(4);
            glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, stride, (void*)(10 * sizeof(float)));

            openGLMesh.instanceVBO->setUsagePattern(OpenGLBuffer::DynamicDraw);
            openGLMesh.instanceVBO->bind();

            const GLsizei istride = INSTANCE_FLOATS * sizeof(float);

            // Model matrix then view * model: eight vec4 columns, locations 5 to 12; tint 13, flash 14
            for (int i = 0; i < 10; ++i)
            {
                glEnableVertexAttribArray(5 + i);
                glVertexAttribPointer(5 + i, 4, GL_FLOAT, GL_FALSE, istride, (void*)(i * 4 * sizeof(float)));
                glVertexAttribDivisor(5 + i, 1);
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

        if (textures)
            textures->bind();
    }
}
