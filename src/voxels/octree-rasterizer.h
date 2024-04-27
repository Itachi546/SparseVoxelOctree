#pragma once

#include "rendering/rendering-device.h"
#include "gfx/mesh.h"

namespace gfx {
    class Camera;
} // namespace gfx

class ParallelOctree;

struct OctreeRasterizer {

    OctreeRasterizer() = default;

    void Initialize(uint32_t width, uint32_t height, ParallelOctree *octree);

    void Render(CommandBufferID commandBuffer, UniformSetID uniformSet);

    void Shutdown();

    virtual ~OctreeRasterizer() = default;

    gfx::Mesh voxelMesh;
    PipelineID pipeline;

    TextureID colorAttachment;
    TextureID depthAttachment;
    RenderingDevice *device;

    BufferID instanceDataBuffer;
    uint32_t numVoxels;

    UniformSetID instancedUniformSet;
    uint32_t width, height;
};