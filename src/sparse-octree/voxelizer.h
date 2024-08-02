#pragma once

#include "rendering/rendering-device.h"

#include <memory>

struct RenderScene;

class Voxelizer {

  public:
    void Initialize(std::shared_ptr<RenderScene> scene);

    void Voxelize(CommandPoolID commandPool, CommandBufferID commandBuffer);

    void Shutdown();

  private:
    std::shared_ptr<RenderScene> scene;
    PipelineID prepassPipeline;
    UniformSetID prepassSet;

    RD *device;
    // BufferID voxelFragmentListBuffer;
    BufferID voxelCountBuffer;
    uint64_t *countBufferPtr;
};