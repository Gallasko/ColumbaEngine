#pragma once

#include <string>

#include "logger.h"

#ifdef __EMSCRIPTEN__
#define GL_GLEXT_PROTOTYPES 1
#include <emscripten.h>
#include <SDL2/SDL.h>
#include <SDL_opengl.h>
// #include <SDL_opengl_glext.h>
#include <GLES2/gl2.h>
#include <GLFW/glfw3.h>
#else
#ifdef __linux__
#include <SDL2/SDL.h>
#elif _WIN32
#include <SDL.h>
#endif
#include <GL/glew.h>
#include <GL/gl.h>
#endif


namespace pg
{
    class OpenGLShaderProgram
    {
    public:
        unsigned int ID;
        // constructor generates the shader on the fly
        // ------------------------------------------------------------------------
        OpenGLShaderProgram(const std::string& vertexPath, const std::string& fragmentPath);
        // activate the shader
        // ------------------------------------------------------------------------
        void bind() const;
        // activate the shader
        // ------------------------------------------------------------------------
        void release() const;
        // utility uniform functions
        // ------------------------------------------------------------------------
        void setUniformValue(const std::string &name, bool value) const;
        // ------------------------------------------------------------------------
        void setUniformValue(const std::string &name, int value) const;
        // ------------------------------------------------------------------------
        void setUniformValue(const std::string &name, float value) const;
        // ------------------------------------------------------------------------
        void setUniformValue(const std::string &name, const glm::vec2 &value) const;
        void setUniformValue(const std::string &name, float x, float y) const;
        // ------------------------------------------------------------------------
        void setUniformValue(const std::string &name, const glm::vec3 &value) const;
        void setUniformValue(const std::string &name, float x, float y, float z) const;
        // ------------------------------------------------------------------------
        void setUniformValue(const std::string &name, const glm::vec4 &value) const;
        void setUniformValue(const std::string &name, float x, float y, float z, float w) const;
        // ------------------------------------------------------------------------
        void setUniformValue(const std::string &name, const glm::mat2 &mat) const;
        // ------------------------------------------------------------------------
        void setUniformValue(const std::string &name, const glm::mat3 &mat) const;
        // ------------------------------------------------------------------------
        void setUniformValue(const std::string &name, const glm::mat4 &mat) const;
    private:
        // utility function for checking shader compilation/linking errors.
        // ------------------------------------------------------------------------
        void checkCompileErrors(GLuint shader, std::string type);
    };

    class OpenGLVertexArrayObject
    {
    public:
        OpenGLVertexArrayObject() { LOG_THIS_MEMBER("OpenGLVertexArrayObject"); }

        ~OpenGLVertexArrayObject()
        {
            LOG_THIS_MEMBER("OpenGLVertexArrayObject");

            if (created and (not (VAO != static_cast<unsigned int>(-1))))
                glDeleteVertexArrays(1, &VAO);
        }

        OpenGLVertexArrayObject(const OpenGLVertexArrayObject&) = delete;
        OpenGLVertexArrayObject& operator=(const OpenGLVertexArrayObject&) = delete;

        void create()
        {
            LOG_THIS_MEMBER("OpenGLVertexArrayObject");

            /* Allocate and assign a Vertex Array Object to our handle */
            glGenVertexArrays(1, &VAO);

            created = true;
        }

        void bind()
        {
            LOG_THIS_MEMBER("OpenGLVertexArrayObject");

            /* Bind our Vertex Array Object as the current used object */
            if (created)
                glBindVertexArray(VAO);
            else
            {
                LOG_ERROR("OpenGLVertexArrayObject", "Trying to bind to a vertex array that is not initialized yet");
            }
        }

        void release()
        {
            LOG_THIS_MEMBER("OpenGLVertexArrayObject");

            if (created)
                glBindVertexArray(0);
            else
            {
                LOG_ERROR("OpenGLVertexArrayObject", "Trying to delete a vertex array that is not initialized");
            }
        }

    private:
        GLuint VAO;

        bool created = false;
    };

    class OpenGLBuffer
    {
    public:
        enum Type
        {
            VertexBuffer        = GL_ARRAY_BUFFER,
            IndexBuffer         = GL_ELEMENT_ARRAY_BUFFER,
            PixelPackBuffer     = GL_PIXEL_PACK_BUFFER,
            PixelUnpackBuffer   = GL_PIXEL_UNPACK_BUFFER
        };

        enum UsagePattern
        {
            StreamDraw          = GL_STREAM_DRAW,
            StreamRead          = GL_STREAM_READ,
            StreamCopy          = GL_STREAM_COPY,
            StaticDraw          = GL_STATIC_DRAW,
            StaticRead          = GL_STATIC_READ,
            StaticCopy          = GL_STATIC_COPY,
            DynamicDraw         = GL_DYNAMIC_DRAW,
            DynamicRead         = GL_DYNAMIC_READ,
            DynamicCopy         = GL_DYNAMIC_COPY
        };

        enum Access
        {
            ReadOnly            = GL_READ_ONLY,
            WriteOnly           = GL_WRITE_ONLY,
            ReadWrite           = GL_READ_WRITE
        };

        enum RangeAccessFlag
        {
            RangeRead             = GL_MAP_READ_BIT,
            RangeWrite            = GL_MAP_WRITE_BIT,
            RangeInvalidate       = GL_MAP_INVALIDATE_RANGE_BIT,
            RangeInvalidateBuffer = GL_MAP_INVALIDATE_BUFFER_BIT,
            RangeFlushExplicit    = GL_MAP_FLUSH_EXPLICIT_BIT,
            RangeUnsynchronized   = GL_MAP_UNSYNCHRONIZED_BIT
        };

    public:
        OpenGLBuffer(OpenGLBuffer::Type type) : type(type), usagePattern(UsagePattern::StaticDraw) { LOG_THIS_MEMBER("OpenGLBuffer"); }
        ~OpenGLBuffer() { LOG_THIS_MEMBER("OpenGLBuffer"); if(created) glDeleteBuffers(1, &buffer); }

        inline OpenGLBuffer::UsagePattern getUsagePattern() const { LOG_THIS_MEMBER("OpenGLBuffer"); return usagePattern; }

        void setUsagePattern(OpenGLBuffer::UsagePattern value)
        {
            LOG_THIS_MEMBER("OpenGLBuffer");

            usagePattern = value;
        }

        void create()
        {
            LOG_THIS_MEMBER("OpenGLBuffer");

            /* Allocate and assign a Vertex Array Object to our handle */
            glGenBuffers(1, &buffer);

            created = true;
        }

        void bind()
        {
            LOG_THIS_MEMBER("OpenGLBuffer");

            /* Bind our Vertex Buffer Object as the current used object */
            if(created)
                glBindBuffer(type, buffer);
            else
            {
                LOG_ERROR("OpenGLBuffer", "Trying to bind to a buffer that is not initialized yet");
            }
        }

        void release()
        {
            LOG_THIS_MEMBER("OpenGLBuffer");

            if(created)
                glBindBuffer(type, 0);
            else
            {
                LOG_ERROR("OpenGLBuffer", "Trying to delete a buffer that is not initialized");
            }
        }

        void allocate(const void *data, int count)
        {
            LOG_THIS_MEMBER("OpenGLBuffer");

            if(created)
            {
                glBindBuffer(type, buffer);
                glBufferData(type, count, data, usagePattern);
            }
            else
            {
                LOG_ERROR("OpenGLBuffer", "Trying to allocate a buffer that is not initialized");
            }
        }

        inline void allocate(int count) { allocate(nullptr, count); }

    private:
        GLuint buffer;

        OpenGLBuffer::Type type;
        OpenGLBuffer::UsagePattern usagePattern;

        bool created = false;
    };

}