#pragma once

#include "rendering/rendering-device.h"
#include "gfx/mesh.h"

namespace gfx {
    class Camera;
} // namespace gfx

class ParallelOctree;

struct VoxelRenderer {

    VoxelRenderer() = default;

    void Initialize(std::vector<glm::vec4> &voxels);

    void Render(CommandBufferID commandBuffer, glm::mat4 VP);

    void Shutdown();

    virtual ~VoxelRenderer() = default;

    PipelineID pipeline;
    RenderingDevice *device;

    BufferID vertexBuffer;
    BufferID indexBuffer;
    BufferID instanceDataBuffer;

    uint32_t numVoxels;
    UniformSetID uniformSet;
    uint32_t numVertices;
};