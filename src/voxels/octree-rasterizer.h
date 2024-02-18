#pragma once

#include "gfx/opengl.h"

namespace gfx {
    struct Mesh;
    class Camera;
} // namespace gfx

struct Octree;

struct OctreeRasterizer {

    OctreeRasterizer() = default;

    void Initialize(Octree *octree);

    void Render(gfx::Camera *camera);

    void Shutdown();

    virtual ~OctreeRasterizer() = default;

    gfx::Shader shader;
    gfx::Mesh *cubeMesh;
    gfx::Buffer instanceBuffer;
    uint32_t voxelCount;
};