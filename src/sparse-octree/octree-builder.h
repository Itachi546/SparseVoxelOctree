#pragma once

#include <memory>

#include "rendering/rendering-device.h"

struct RenderScene;

namespace gfx {
    class Camera;
}

class OctreeBuilder {

  public:
    void Initialize(std::shared_ptr<RenderScene> scene);

    void Build(CommandPoolID commandPool, CommandBufferID commandBuffer);

    void Shutdown();

    std::shared_ptr<RenderScene> scene;

    const uint32_t kResolution = 1024;
    const uint32_t kLevels = static_cast<uint32_t>(std::log2(kResolution) + 1);

    BufferID octreeBuffer, buildInfoBuffer, dispatchIndirectBuffer;
    PipelineID pipelineInitNode, pipelineTagNode, pipelineAllocateNode, pipelineUpdateParams;
    UniformSetID initNodeSet, tagNodeSet, allocateNodeSet, updateParamsSet;

    RD *device = nullptr;
    const uint32_t VOXEL_DATA_SIZE = static_cast<uint32_t>(sizeof(uint32_t));
    uint32_t octreeElmCount = 0;

  private:
    void InitializeNode(CommandBufferID commandBuffer);
    void TagNode(CommandBufferID commandBuffer, uint32_t level, uint32_t voxelCount);
    void AllocateNode(CommandBufferID commandBuffer);
    void UpdateParams(CommandBufferID commandBuffer);
};