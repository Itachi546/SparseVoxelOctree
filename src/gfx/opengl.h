#pragma once

#include <glad/glad.h>
#include <stdint.h>

namespace gfx {
    constexpr const uint32_t InvalidShader = 0;

    using Buffer = uint32_t;
    constexpr const uint32_t InvalidBuffer = 0;

    struct Shader {
        void Create(const char *vs, const char *fs);
        void Create(const char *cs);

        inline void Destroy() { glDeleteProgram(handle); }

        void SetUniformInt(const char *name, int val);
        void SetUniformFloat(const char *name, float val);
        void SetUniformFloat2(const char *name, float *val);
        void SetUniformFloat3(const char *name, float *val);
        void SetUniformMat4(const char *name, float *val);

        void SetBuffer(int bindingIndex, Buffer buffer) { glBindBufferBase(GL_SHADER_STORAGE_BUFFER, bindingIndex, buffer); }
        void SetUniformBuffer(int bindingIndex, Buffer buffer) { glBindBufferBase(GL_UNIFORM_BUFFER, bindingIndex, buffer); }

        void Bind() { glUseProgram(handle); }

        void Unbind() { glUseProgram(0); }

        uint32_t handle;
    };

    Buffer CreateBuffer(void *data, size_t size, GLbitfield flag);
    inline void *MapBuffer(Buffer buffer, int offset, int length, GLbitfield access) { return glMapNamedBufferRange(buffer, offset, length, access); }
    inline void *MapBuffer(Buffer buffer, GLbitfield access) { return glMapNamedBuffer(buffer, access); }
    inline void CopyToBuffer(Buffer buffer, void *data, uint32_t size, GLenum usage) { glNamedBufferData(buffer, size, data, usage); }

    inline void DestroyBuffer(Buffer buffer) { glDeleteBuffers(1, &buffer); }

} // namespace gfx
