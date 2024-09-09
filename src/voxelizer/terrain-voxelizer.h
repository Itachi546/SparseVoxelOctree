#pragma once

#include "voxelizer.h"

class TerrainVoxelizer : public Voxelizer {

  public:
    void Initialize(uint32_t voxelResolution) override;

    void Voxelize(CommandPoolID commandPool, CommandBufferID commandBuffer) override;

    void Shutdown() override;

    ~TerrainVoxelizer() {}

  private:
    uint32_t voxelResolution;

    BufferID voxelCountBuffer;
    uint32_t *countBufferPtr;

    RD *device;

    PipelineID prepassPipeline, mainPipeline;
};