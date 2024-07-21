#pragma once

#include "rendering/rendering-device.h"
#include "gfx/mesh.h"
#include "octree-raycaster.h"

class ParallelOctree;

namespace gfx {
    class Camera;
}; // namespace gfx

enum OctreeRenderMode {
    RenderMode_Rasterizer = 0,
    RenderMode_Raycaster
};

struct OctreeRenderer {

    OctreeRenderer();

    void Initialize(uint32_t width, uint32_t height, ParallelOctree *octree);

    // void Update(ParallelOctree *octree, gfx::Camera *camera);

    void AddUI();

    void Render(CommandBufferID commandBuffer, UniformSetID globalSet);

    TextureID GetColorAttachment() const {
        return raycaster->outputTexture;
    }

    void Shutdown();

    OctreeRaycaster *raycaster;
    OctreeRenderMode renderMode = RenderMode_Rasterizer;
};