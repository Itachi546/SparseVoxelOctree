#pragma once

#include "rendering/rendering-device.h"
#include "gfx/mesh.h"

class ParallelOctree;
class RenderingDevice;

namespace gfx {
    struct Mesh;
};

struct OctreeRenderer {

    OctreeRenderer(uint32_t width, uint32_t height);

    void Initialize(ParallelOctree *octree);

    void Render(CommandBufferID commandBuffer, UniformSetID globalSet);

    void Shutdown();

    gfx::Mesh voxelMesh;
    PipelineID voxelRasterPipeline;

    TextureID colorAttachment;
    TextureID depthAttachment;
    RenderingDevice *device;

    BufferID instanceDataBuffer;
    uint32_t numVoxels;

    UniformSetID instancedUniformSet;

    uint32_t width, height;

  private:
    void SetupRasterizer(ParallelOctree *octree);
    void RasterizeVoxel(CommandBufferID commandBuffer, UniformSetID globalSet);
};