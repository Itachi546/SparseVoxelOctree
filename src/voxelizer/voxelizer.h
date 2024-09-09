#pragma once

#include "rendering/rendering-device.h"

struct Voxelizer {
    virtual void Initialize(uint32_t voxelResolution) = 0;

    virtual void Voxelize(CommandPoolID commandPool, CommandBufferID commandBuffer) = 0;

    virtual void Shutdown() = 0;

    virtual ~Voxelizer() {}

    uint32_t voxelCount;
    BufferID voxelFragmentBuffer;
};