#pragma once

#include <memory>

#include "rendering/rendering-device.h"

class Voxelizer;
struct RenderScene;

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

    uint32_t kDims = 128;
    uint32_t kLevels = static_cast<uint32_t>(std::log2(128) + 1);

    BufferID octreeBuffer, buildInfoBuffer;
    PipelineID pipelineInitNode, pipelineTagNode, pipelineAllocateNode;
    UniformSetID initNodeSet, tagNodeSet, allocateNodeSet;

    RD *device = nullptr;

  private:
    void InitializeNode(CommandBufferID commandBuffer);
    void TagNode(CommandBufferID commandBuffer);
    void AllocateNode(CommandBufferID commandBuffer);
};