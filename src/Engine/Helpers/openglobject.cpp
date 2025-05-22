#include "stdafx.h"

#include "openglobject.h"

#include <fstream>
#include <sstream>
#include <iostream>

#include "logger.h"

#include "Files/filemanager.h"

namespace
{
    static const char* const DOM = "Opengl Objects";

    GLenum glCheckError_(const char *file, int line)
    {
        GLenum errorCode;
        while ((errorCode = glGetError()) != GL_NO_ERROR)
        {
            std::string error;
            switch (errorCode)
            {
                case GL_INVALID_ENUM:                  error = "INVALID_ENUM"; break;
                case GL_INVALID_VALUE:                 error = "INVALID_VALUE"; break;
                case GL_INVALID_OPERATION:             error = "INVALID_OPERATION"; break;
                case GL_STACK_OVERFLOW:                error = "STACK_OVERFLOW"; break;
                case GL_STACK_UNDERFLOW:               error = "STACK_UNDERFLOW"; break;
                case GL_OUT_OF_MEMORY:                 error = "OUT_OF_MEMORY"; break;
                case GL_INVALID_FRAMEBUFFER_OPERATION: error = "INVALID_FRAMEBUFFER_OPERATION"; break;
            }

            LOG_ERROR(DOM, error << " | " << file << " (" << line << ")");
        }
        return errorCode;
    }
}

#ifdef DEBUG
#define glCheckError() glCheckError_(__FILE__, __LINE__)
#else
#define glCheckError()
#endif


namespace pg
{
    // Todo change this with our custom file opener
    OpenGLShaderProgram::OpenGLShaderProgram(const std::string& vertexPath, const std::string& fragmentPath)
    {
        // 1. retrieve the vertex/fragment source code from filePath
        auto vertexFile = UniversalFileAccessor::openTextFile(vertexPath);
        auto fragmentFile = UniversalFileAccessor::openTextFile(fragmentPath);

        if (vertexFile.data == "" or fragmentFile.data == "")
        {
            LOG_ERROR(DOM, "Couldn't open files to create shader: '" << vertexPath << ", " << fragmentPath << "'");
            return;
        }

        const char* vShaderCode = vertexFile.data.c_str();
        const char * fShaderCode = fragmentFile.data.c_str();
        // 2. compile shaders
        unsigned int vertex, fragment;
        // vertex shader
        vertex = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vertex, 1, &vShaderCode, NULL);
        glCompileShader(vertex);
        checkCompileErrors(vertex, "VERTEX");
        // fragment Shader
        fragment = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fragment, 1, &fShaderCode, NULL);
        glCompileShader(fragment);
        checkCompileErrors(fragment, "FRAGMENT");
        // shader Program
        ID = glCreateProgram();
        glAttachShader(ID, vertex);
        glAttachShader(ID, fragment);
        glLinkProgram(ID);
        checkCompileErrors(ID, "PROGRAM");
        // delete the shaders as they're linked into our program now and no longer necessary
        glDeleteShader(vertex);
        glDeleteShader(fragment);

    }
    // activate the shader
    // ------------------------------------------------------------------------
    void OpenGLShaderProgram::bind() const
    {
        glUseProgram(ID);

        glCheckError();
    }
    // release the shader
    // ------------------------------------------------------------------------
    void OpenGLShaderProgram::release() const
    {
        glUseProgram(0);
    }
    // utility uniform functions
    // ------------------------------------------------------------------------
    void OpenGLShaderProgram::setUniformValue(const std::string &name, bool value) const
    {
        glUniform1i(glGetUniformLocation(ID, name.c_str()), (int)value);

        glCheckError();
    }
    // ------------------------------------------------------------------------
    void OpenGLShaderProgram::setUniformValue(const std::string &name, int value) const
    {
        glUniform1i(glGetUniformLocation(ID, name.c_str()), value);

        glCheckError();
    }
    // ------------------------------------------------------------------------
    void OpenGLShaderProgram::setUniformValue(const std::string &name, float value) const
    {
        glUniform1f(glGetUniformLocation(ID, name.c_str()), value);

        glCheckError();
    }
    // ------------------------------------------------------------------------
    void OpenGLShaderProgram::setUniformValue(const std::string &name, const glm::vec2 &value) const
    {
        glUniform2fv(glGetUniformLocation(ID, name.c_str()), 1, &value[0]);

        glCheckError();
    }
    void OpenGLShaderProgram::setUniformValue(const std::string &name, float x, float y) const
    {
        glUniform2f(glGetUniformLocation(ID, name.c_str()), x, y);

        glCheckError();
    }
    // ------------------------------------------------------------------------
    void OpenGLShaderProgram::setUniformValue(const std::string &name, const glm::vec3 &value) const
    {
        glUniform3fv(glGetUniformLocation(ID, name.c_str()), 1, &value[0]);

        glCheckError();
    }
    void OpenGLShaderProgram::setUniformValue(const std::string &name, float x, float y, float z) const
    {
        glUniform3f(glGetUniformLocation(ID, name.c_str()), x, y, z);

        glCheckError();
    }
    // ------------------------------------------------------------------------
    void OpenGLShaderProgram::setUniformValue(const std::string &name, const glm::vec4 &value) const
    {
        glUniform4fv(glGetUniformLocation(ID, name.c_str()), 1, &value[0]);

        glCheckError();
    }
    void OpenGLShaderProgram::setUniformValue(const std::string &name, float x, float y, float z, float w) const
    {
        glUniform4f(glGetUniformLocation(ID, name.c_str()), x, y, z, w);

        glCheckError();
    }
    // ------------------------------------------------------------------------
    void OpenGLShaderProgram::setUniformValue(const std::string &name, const glm::mat2 &mat) const
    {
        glUniformMatrix2fv(glGetUniformLocation(ID, name.c_str()), 1, GL_FALSE, &mat[0][0]);

        glCheckError();
    }
    // ------------------------------------------------------------------------
    void OpenGLShaderProgram::setUniformValue(const std::string &name, const glm::mat3 &mat) const
    {
        glUniformMatrix3fv(glGetUniformLocation(ID, name.c_str()), 1, GL_FALSE, &mat[0][0]);

        glCheckError();
    }
    // ------------------------------------------------------------------------
    void OpenGLShaderProgram::setUniformValue(const std::string &name, const glm::mat4 &mat) const
    {
        glUniformMatrix4fv(glGetUniformLocation(ID, name.c_str()), 1, GL_FALSE, &mat[0][0]);

        glCheckError();
    }

    void OpenGLShaderProgram::checkCompileErrors(GLuint shader, std::string type)
    {
        GLint success;
        GLchar infoLog[1024];
        if (type != "PROGRAM")
        {
            glGetShaderiv(shader, GL_COMPILE_STATUS, &success);

            if (not success)
            {
                glGetShaderInfoLog(shader, 1024, NULL, infoLog);

                LOG_ERROR(DOM, "ERROR::SHADER_COMPILATION_ERROR of type: " << type << "\n" << &infoLog[0] << "\n -- --------------------------------------------------- -- ");
            }
        }
        else
        {
            glGetProgramiv(shader, GL_LINK_STATUS, &success);

            if (not success)
            {
                glGetProgramInfoLog(shader, 1024, NULL, infoLog);

                LOG_ERROR(DOM, "ERROR::PROGRAM_LINKING_ERROR of type: " << type << "\n" << &infoLog[0] << "\n -- --------------------------------------------------- -- ");
            }
        }
    }

}