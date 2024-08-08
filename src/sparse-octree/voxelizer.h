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
    PipelineID prepassPipeline, mainPipeline;
    UniformSetID prepassSet, mainSet;

    RD *device;

    BufferID voxelFragmentListBuffer;
    BufferID voxelCountBuffer;
    uint32_t *countBufferPtr;
    uint32_t voxelCount = 0;

    // @TEMP
    TextureID texture;

    const uint32_t VOXEL_GRID_SIZE = 128;

    void InitializePrepassResources();
    void InitializeMainResources();

    void DrawVoxelScene(CommandBufferID cb, PipelineID pipeline, UniformSetID *uniformSet, uint32_t uniformSetCount);

    void ExecuteVoxelPrepass(CommandPoolID cp, CommandBufferID cb, FenceID waitFence);

    void ExecuteMainPass(CommandPoolID cp, CommandBufferID cb, FenceID waitFence);
};