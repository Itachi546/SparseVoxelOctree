#include "opengl.h"

#include "utils.h"

#include <assert.h>
#include <iostream>

namespace gfx {

    static GLuint CreateShaderModule(const char *file, GLuint shaderType) {
        std::optional<std::string> fileContent = utils::ReadFile(file);
        if (!fileContent.has_value()) {
            std::cerr << "Failed to open file: " << file << std::endl;
            assert(0);
            return InvalidShader;
        }
        const char *shaderCode = fileContent.value().c_str();
        GLuint shader = glCreateShader(shaderType);
        glShaderSource(shader, 1, &shaderCode, nullptr);
        glCompileShader(shader);

        char buffer[8192];
        GLsizei length = 0;
        glGetShaderInfoLog(shader, sizeof(buffer), &length, buffer);
        if (length > 0) {
            std::cerr << "Error loading file: " << file << std::endl;
            std::cerr << buffer << std::endl;
            assert(0);
            return InvalidShader;
        }
        return shader;
    }

    static void PrintProgramInfoLog(GLuint handle) {
        char buffer[8192];
        GLsizei length = 0;

        glGetProgramInfoLog(handle, sizeof(buffer), &length, buffer);
        if (length > 0) {
            std::cerr << buffer << std::endl;
            assert(0);
        }
    }

    void Shader::Create(const char *vs, const char *fs) {
        GLuint vertexModule = CreateShaderModule(vs, GL_VERTEX_SHADER);
        GLuint fragmentModule = CreateShaderModule(fs, GL_FRAGMENT_SHADER);

        handle = glCreateProgram();
        glAttachShader(handle, vertexModule);
        glAttachShader(handle, fragmentModule);
        glLinkProgram(handle);

        PrintProgramInfoLog(handle);

        glDeleteShader(vertexModule);
        glDeleteShader(fragmentModule);
    }

    void Shader::Create(const char *cs) {
        GLuint computeModule = CreateShaderModule(cs, GL_COMPUTE_SHADER);
        handle = glCreateProgram();
        glAttachShader(handle, computeModule);
        glLinkProgram(handle);
        PrintProgramInfoLog(handle);

        glDeleteShader(computeModule);
    }

    constexpr const int InvalidShaderLocation = -1;
    GLint GetUniformLocation(GLuint shader, const char *name) {
        GLint location = glGetUniformLocation(shader, name);
        if (location == -1) {
            std::cerr << "Failed to get uniform location: " << name << std::endl;
        }
        return location;
    }

    void Shader::SetUniformInt(const char *name, int val) {
        GLint location = GetUniformLocation(handle, name);
        if (location != InvalidShaderLocation) {
            glUniform1i(location, val);
        }
    }

    void Shader::SetUniformFloat(const char *name, float val) {
        GLint location = GetUniformLocation(handle, name);
        if (location != InvalidShaderLocation) {
            glUniform1f(location, val);
        }
    }

    void Shader::SetUniformFloat2(const char *name, float *val) {
        GLint location = GetUniformLocation(handle, name);
        if (location != InvalidShaderLocation) {
            glUniform2fv(location, 1, val);
        }
    }

    void Shader::SetUniformFloat3(const char *name, float *val) {
        GLint location = GetUniformLocation(handle, name);
        if (location != InvalidShaderLocation) {
            glUniform3fv(location, 1, val);
        }
    }

    void Shader::SetUniformMat4(const char *name, float *val) {
        GLint location = GetUniformLocation(handle, name);
        if (location != InvalidShaderLocation) {
            glUniformMatrix4fv(location, 1, GL_FALSE, val);
        }
    }

    Buffer CreateBuffer(void *data, size_t size, GLbitfield flag) {
        Buffer handle = 0;
        if (size == 0) {
            std::cerr << "Requested to create empty buffer" << std::endl;
            return InvalidBuffer;
        }
        glCreateBuffers(1, &handle);
        glNamedBufferStorage(handle, size, data, flag);
        return handle;
    }
} // namespace gfx