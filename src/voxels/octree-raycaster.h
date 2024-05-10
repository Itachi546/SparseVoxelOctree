#pragma once

#include "rendering/rendering-device.h"
#include <glm/glm.hpp>

#include <vector>

class ParallelOctree;

namespace gfx {
    class Camera;
};

struct OctreeRaycaster {

    OctreeRaycaster() = default;

    void Initialize(uint32_t outputWidth, uint32_t outputHeight, ParallelOctree *octree);

    void Render(CommandBufferID commandBuffer, UniformSetID globalSet);

    void Shutdown();

    virtual ~OctreeRaycaster() = default;

    PipelineID pipeline;
    BufferID nodesBuffer;
    BufferID brickBuffer;
    TextureID outputTexture;
    TextureID accumulationTexture;
    float spp = 0.0f;

    glm::vec3 minBound;
    glm::vec3 maxBound;

    UniformSetID resourceSet;
    std::vector<RD::TextureBarrier> outputImageBarrier;

    uint32_t width;
    uint32_t height;
};