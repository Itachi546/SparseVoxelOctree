#include "opengl.h"

#include "utils.h"

#include <glad/glad.h>
#include <cassert>
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
            // std::cerr << buffer << std::endl;
            assert(0);
            return InvalidShader;
        }
        return shader;
    }

    static void PrintProgramInfoLog(Shader handle) {
        char buffer[8192];
        GLsizei length = 0;

        glGetProgramInfoLog(handle, sizeof(buffer), &length, buffer);
        if (length > 0) {
            std::cerr << buffer << std::endl;
            assert(0);
        }
    }

    Shader CreateGraphicsShader(const char *vs, const char *fs) {
        GLuint vertexModule = CreateShaderModule(vs, GL_VERTEX_SHADER);
        GLuint fragmentModule = CreateShaderModule(fs, GL_FRAGMENT_SHADER);

        Shader handle = glCreateProgram();
        glAttachShader(handle, vertexModule);
        glAttachShader(handle, fragmentModule);
        glLinkProgram(handle);

        PrintProgramInfoLog(handle);

        return handle;
    }

    Shader CreateComputeShader(const char *cs) {
        GLuint computeModule = CreateShaderModule(cs, GL_COMPUTE_SHADER);
        Shader handle = glCreateProgram();
        glAttachShader(handle, computeModule);
        glLinkProgram(handle);
        PrintProgramInfoLog(handle);

        return handle;
    }

    constexpr const int InvalidShaderLocation = -1;
    GLint GetUniformLocation(Shader shader, const char *name) {
        GLint location = glGetUniformLocation(shader, name);
        if (location == -1) {
            std::cerr << "Failed to get uniform location: " << name << std::endl;
        }
        return location;
    }

    void SetUniformInt(Shader shader, const char *name, int val) {
        GLint location = GetUniformLocation(shader, name);
        if (location != InvalidShaderLocation) {
            glUniform1i(location, val);
        }
    }

    void SetUniformFloat(Shader shader, const char *name, float val) {
        GLint location = GetUniformLocation(shader, name);
        if (location != InvalidShaderLocation) {
            glUniform1f(location, val);
        }
    }

    void SetUniformFloat2(Shader shader, const char *name, float *val) {
        GLint location = GetUniformLocation(shader, name);
        if (location != InvalidShaderLocation) {
            glUniform1fv(location, 2, val);
        }
    }
    void SetUniformFloat3(Shader shader, const char *name, float *val) {
        GLint location = GetUniformLocation(shader, name);
        if (location != InvalidShaderLocation) {
            glUniform1fv(location, 3, val);
        }
    }
    void SetUniformMat4(Shader shader, const char *name, float *val) {
        GLint location = GetUniformLocation(shader, name);
        if (location != InvalidShaderLocation) {
            glUniformMatrix4fv(location, 1, GL_FALSE, val);
        }
    }

    Buffer CreateBuffer(void *data, uint32_t size, GLbitfield flag) {
        Buffer handle = 0;
        if (size == 0) {
            std::cerr << "Requested to create empty buffer" << std::endl;
            return InvalidBuffer;
        }
        glGenBuffers(1, &handle);
        glNamedBufferStorage(handle, size, data, flag);
        return handle;
    }
} // namespace gfx