#pragma once

#include <memory>

#include "rendering/rendering-device.h"

class Voxelizer;
struct RenderScene;
struct VoxelRenderer;

namespace gfx {
    class Camera;
}

class OctreeBuilder {

  public:
    void Initialize(std::shared_ptr<RenderScene> scene);

    void Build(CommandPoolID commandPool, CommandBufferID commandBuffer);

    // @TODO Temp
    void Debug(CommandBufferID commandBuffer, const gfx::Camera *camera);

    void Shutdown();

    std::shared_ptr<Voxelizer> voxelizer;
    std::shared_ptr<RenderScene> scene;

    const uint32_t kDims = 512;
    const uint32_t kLevels = static_cast<uint32_t>(std::log2(kDims) + 1);

    BufferID octreeBuffer, buildInfoBuffer, dispatchIndirectBuffer;
    PipelineID pipelineInitNode, pipelineTagNode, pipelineAllocateNode, pipelineUpdateParams;
    UniformSetID initNodeSet, tagNodeSet, allocateNodeSet, updateParamsSet;

    RD *device = nullptr;
    std::shared_ptr<VoxelRenderer> renderer;

  private:
    void InitializeNode(CommandBufferID commandBuffer);
    void TagNode(CommandBufferID commandBuffer, uint32_t level);
    void AllocateNode(CommandBufferID commandBuffer);
    void UpdateParams(CommandBufferID commandBuffer);
};