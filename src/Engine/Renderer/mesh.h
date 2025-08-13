#pragma once

#include <memory>
#include <unordered_map>

#include "pgconstant.h"
#include "logger.h"

namespace pg
{
    // Forward declarations
    class OpenGLVertexArrayObject;
    class OpenGLBuffer;

    struct OpenGLObject
    {
        OpenGLVertexArrayObject *VAO = nullptr;
        OpenGLBuffer *VBO = nullptr;
        OpenGLBuffer *instanceVBO = nullptr;
        OpenGLBuffer *EBO = nullptr;

        bool initialized = false;

        OpenGLObject();
        ~OpenGLObject();

        void initialize();
    };

    struct Mesh
    {
        Mesh() { LOG_THIS_MEMBER("Mesh"); }
        Mesh(const Mesh &other) = delete;
        Mesh& operator=(const Mesh &other) = delete;
        virtual ~Mesh() {}

        void bind();

        virtual void generateMesh() = 0;

        OpenGLObject openGLMesh;
        constant::ModelInfo modelInfo;
        bool initialized = false;
    };

    struct SimpleSquareMesh : public Mesh
    {
        SimpleSquareMesh(const std::vector<size_t>& attributes) : Mesh(), attributes(attributes)
        {
            LOG_THIS_MEMBER("Simple square mesh");
            modelInfo.vertices = new float[12];
            //              x                     y                              z
            modelInfo.vertices[0] =   0.0f; modelInfo.vertices[1] =   0.0f; modelInfo.vertices[2] =  1.0f;
            modelInfo.vertices[3] =   1.0f; modelInfo.vertices[4] =   0.0f; modelInfo.vertices[5] =  1.0f;
            modelInfo.vertices[6] =   0.0f; modelInfo.vertices[7] =  -1.0f; modelInfo.vertices[8] =  1.0f;
            modelInfo.vertices[9] =   1.0f; modelInfo.vertices[10] = -1.0f; modelInfo.vertices[11] = 1.0f;

            modelInfo.indices = new unsigned int[6];
            modelInfo.indices[0] = 0; modelInfo.indices[1] = 1; modelInfo.indices[2] = 2;
            modelInfo.indices[3] = 1; modelInfo.indices[4] = 2; modelInfo.indices[5] = 3;

            modelInfo.nbVertices = 12;
            modelInfo.nbIndices = 6;
        }

        virtual ~SimpleSquareMesh();

        void generateMesh();

        /**
         * @brief Vector responsible of generating the vertex attributes pointer of the instance VBO
         *
         * The size of the vector dictate the number of vertex attributes and the value of each entry dictate the size and the offset of the vertex attribute
         * E.G
         * if attibutes = {3, 2, 3}
         * then when the mesh is generated (through a call to generate mesh)
         * the vertex attibute will be populated with:
         * openGLMesh.instanceVBO->bind();
         * glEnableVertexAttribArray(1);
         * glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
         * glEnableVertexAttribArray(2);
         * glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
         * glEnableVertexAttribArray(3);
         * glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(5 * sizeof(float)));
         *
         * glBindBuffer(GL_ARRAY_BUFFER, 0);
         * glVertexAttribDivisor(1, 1); // tell OpenGL this is an instanced vertex attribute.
         * glVertexAttribDivisor(2, 1); // tell OpenGL this is an instanced vertex attribute.
         * glVertexAttribDivisor(3, 1); // tell OpenGL this is an instanced vertex attribute.
         */
        std::vector<size_t> attributes;
    };

    struct SimpleTexturedSquareMesh : public Mesh
    {
        SimpleTexturedSquareMesh(const std::vector<size_t>& attributes) : Mesh(), attributes(attributes)
        {
            LOG_THIS_MEMBER("Simple square mesh");
            modelInfo = constant::SquareInfo{};
        }

        virtual ~SimpleTexturedSquareMesh();

        void generateMesh();

        /**
         * @brief Vector responsible of generating the vertex attributes pointer of the instance VBO
         *
         * The size of the vector dictate the number of vertex attributes and the value of each entry dictate the size and the offset of the vertex attribute
         * E.G
         * if attibutes = {3, 2, 3}
         * then when the mesh is generated (through a call to generate mesh)
         * the vertex attibute will be populated with:
         * openGLMesh.instanceVBO->bind();
         * glEnableVertexAttribArray(1);
         * glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
         * glEnableVertexAttribArray(2);
         * glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
         * glEnableVertexAttribArray(3);
         * glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(5 * sizeof(float)));
         *
         * glBindBuffer(GL_ARRAY_BUFFER, 0);
         * glVertexAttribDivisor(1, 1); // tell OpenGL this is an instanced vertex attribute.
         * glVertexAttribDivisor(2, 1); // tell OpenGL this is an instanced vertex attribute.
         * glVertexAttribDivisor(3, 1); // tell OpenGL this is an instanced vertex attribute.
         */
        std::vector<size_t> attributes;
    };
}