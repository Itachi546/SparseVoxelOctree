#pragma once

#include <glad/glad.h>
#include <cstdint>

namespace gfx {
    using Shader = uint32_t;
    constexpr const uint32_t InvalidShader = 0;

    using Buffer = uint32_t;
    constexpr const uint32_t InvalidBuffer = 0;

    Shader CreateGraphicsShader(const char *vs, const char *fs);
    Shader CreateComputeShader(const char *cs);

    void SetUniformInt(Shader shader, const char *name, int val);
    void SetUniformFloat(Shader shader, const char *name, float val);
    void SetUniformFloat2(Shader shader, const char *name, float *val);
    void SetUniformFloat3(Shader shader, const char *name, float *val);
    void SetUniformMat4(Shader shader, const char *name, float *val);

    Buffer CreateBuffer(void *data, uint32_t size, GLbitfield flag);
    inline void *MapBuffer(Buffer buffer, int offset, int length, GLbitfield access) { return glMapNamedBufferRange(buffer, offset, length, access); }
    inline void *MapBuffer(Buffer buffer, GLbitfield access) { return glMapNamedBuffer(buffer, access); }
    inline void CopyToBuffer(Buffer buffer, void *data, uint32_t size, GLenum usage) { glNamedBufferData(buffer, size, data, usage); }

} // namespace gfx