#include "stdafx.h"

#include "mesh.h"

#include "UI/sentencesystem.h"

#include "Helpers/openglobject.h"

namespace pg
{
    OpenGLObject::OpenGLObject()
    {
        LOG_THIS_MEMBER("OpenGLObject");
    }

    OpenGLObject::~OpenGLObject()
    {
        LOG_THIS_MEMBER("OpenGLObject");

        if (initialized)
        {
            delete VAO;
            delete VBO;
            delete instanceVBO;
            delete EBO;
        }
    }

    void OpenGLObject::initialize()
    {
        LOG_THIS_MEMBER("OpenGLObject");

        VAO = new OpenGLVertexArrayObject();
        VBO = new OpenGLBuffer(OpenGLBuffer::VertexBuffer);
        instanceVBO = new OpenGLBuffer(OpenGLBuffer::VertexBuffer);
        EBO = new OpenGLBuffer(OpenGLBuffer::IndexBuffer);

        VAO->create();
        VBO->create();
        instanceVBO->create();
        EBO->create();

        initialized = true;
    }

    void Mesh::bind()
    {
        LOG_THIS_MEMBER("Mesh");

        if (not initialized)
            generateMesh();

        openGLMesh.VAO->bind();
    }

    // Todo add a mutex to protect m_meshes of any race conditions

    void SimpleSquareMesh::generateMesh()
    {
        LOG_THIS_MEMBER("Simple square mesh");

        openGLMesh.initialize();

        openGLMesh.VAO->bind();

        openGLMesh.VBO->bind();
        openGLMesh.VBO->setUsagePattern(OpenGLBuffer::StaticDraw);
        openGLMesh.VBO->allocate(modelInfo.vertices, modelInfo.nbVertices * sizeof(float));

        // Position attribute
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);

        openGLMesh.instanceVBO->setUsagePattern(OpenGLBuffer::DynamicDraw);
        openGLMesh.instanceVBO->create();

        openGLMesh.instanceVBO->bind();

        // Calculate the total size of the attributes requested
        size_t totalSize = 0;

        for (auto att : attributes)
        {
            totalSize += att;
        }

        size_t currentSize = 0;

        for (size_t i = 0; i < attributes.size(); ++i)
        {
            glEnableVertexAttribArray(i + 1);
            glVertexAttribPointer(i + 1, attributes[i], GL_FLOAT, GL_FALSE, totalSize * sizeof(float), (void*)(currentSize * sizeof(float)));

            currentSize += attributes[i];

            glVertexAttribDivisor(i + 1, 1); // tell OpenGL this is an instanced vertex attribute.
        }

        glBindBuffer(GL_ARRAY_BUFFER, 0);

        openGLMesh.EBO->bind();
        openGLMesh.EBO->setUsagePattern(OpenGLBuffer::StaticDraw);
        openGLMesh.EBO->allocate(modelInfo.indices, modelInfo.nbIndices * sizeof(unsigned int));

        openGLMesh.VAO->release();

        initialized = true;
    }

    SimpleSquareMesh::~SimpleSquareMesh()
    {
        LOG_THIS_MEMBER("Simple square mesh");
    }

    void SimpleTexturedSquareMesh::generateMesh()
    {
        LOG_THIS_MEMBER("Simple square mesh");

        openGLMesh.initialize();

        openGLMesh.VAO->bind();

        openGLMesh.VBO->bind();
        openGLMesh.VBO->setUsagePattern(OpenGLBuffer::StaticDraw);
        openGLMesh.VBO->allocate(modelInfo.vertices, modelInfo.nbVertices * sizeof(float));

        // Position attribute
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);

        // texture coord attribute
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));

        openGLMesh.instanceVBO->setUsagePattern(OpenGLBuffer::DynamicDraw);
        openGLMesh.instanceVBO->create();

        openGLMesh.instanceVBO->bind();

        // Calculate the total size of the attributes requested
        size_t totalSize = 0;

        for (auto att : attributes)
        {
            totalSize += att;
        }

        size_t currentSize = 0;

        for (size_t i = 0; i < attributes.size(); ++i)
        {
            glEnableVertexAttribArray(i + 2);
            glVertexAttribPointer(i + 2, attributes[i], GL_FLOAT, GL_FALSE, totalSize * sizeof(float), (void*)(currentSize * sizeof(float)));

            currentSize += attributes[i];

            glVertexAttribDivisor(i + 2, 1); // tell OpenGL this is an instanced vertex attribute.
        }

        glBindBuffer(GL_ARRAY_BUFFER, 0);

        openGLMesh.EBO->bind();
        openGLMesh.EBO->setUsagePattern(OpenGLBuffer::StaticDraw);
        openGLMesh.EBO->allocate(modelInfo.indices, modelInfo.nbIndices * sizeof(unsigned int));

        openGLMesh.VAO->release();

        initialized = true;
    }

    SimpleTexturedSquareMesh::~SimpleTexturedSquareMesh()
    {
        LOG_THIS_MEMBER("Simple square mesh");
    }
}