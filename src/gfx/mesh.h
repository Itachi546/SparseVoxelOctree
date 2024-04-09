#pragma once

#include "rendering/rendering-device.h"
#include "math-utils.h"

#include <stdint.h>

namespace gfx {

    struct Vertex {
        glm::vec3 position;
        glm::vec3 normal;
        glm::vec2 uv;
    };

    struct Mesh {

        Mesh(float *vertices, uint32_t vertexCount, uint32_t *indices, uint32_t indexCount);
        Mesh() = default;

        BufferID vertexBuffer;
        BufferID indexBuffer;
        uint32_t numVertices;

        void Initialize(float *vertices, uint32_t vertexCount, uint32_t *indices, uint32_t indexCount);

        static void CreatePlane(Mesh *mesh, int width = 10, int height = 10);
        static void CreateCube(Mesh *mesh);

        ~Mesh();
    };
} // namespace gfx
