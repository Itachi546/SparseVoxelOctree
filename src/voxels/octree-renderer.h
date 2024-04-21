#pragma once

#include "rendering/rendering-device.h"
#include "gfx/mesh.h"

class ParallelOctree;
class RenderingDevice;

namespace gfx {
    struct Mesh;
    class Camera;
};

struct OctreeRenderer {

    OctreeRenderer(uint32_t width, uint32_t height);

    void Initialize();

    void Update(ParallelOctree *octree, gfx::Camera* camera);

    void Render(CommandBufferID commandBuffer, UniformSetID globalSet);

    void Shutdown();

    gfx::Mesh voxelMesh;
    PipelineID voxelRasterPipeline;

    TextureID colorAttachment;
    TextureID depthAttachment;
    RenderingDevice *device;

    BufferID instanceDataBuffer;
    void *instanceDataBufferPtr;
    uint32_t numVoxels;

    UniformSetID instancedUniformSet;

    uint32_t width, height;

    const uint32_t MAX_VOXELS = 10'000'000;

  private:
    void SetupRasterizer();
    void RasterizeVoxel(CommandBufferID commandBuffer, UniformSetID globalSet);
};