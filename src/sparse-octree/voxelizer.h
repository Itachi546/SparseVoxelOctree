#pragma once

#include "rendering/rendering-device.h"

#include <glm/glm.hpp>

#include <memory>

struct RenderScene;

namespace gfx {
    class Camera;
};

class Voxelizer {

  public:
    void Initialize(std::shared_ptr<RenderScene> scene, uint32_t size);

    void Voxelize(CommandPoolID commandPool, CommandBufferID commandBuffer);

    void RayMarch(CommandBufferID commandBuffer, const gfx::Camera *camera);

    void Shutdown();

    uint32_t voxelCount = 0;
    BufferID voxelFragmentListBuffer;

  private:
    std::shared_ptr<RenderScene> scene;
    PipelineID prepassPipeline, mainPipeline, clearTexturePipeline, raymarchPipeline;
    UniformSetID prepassSet, mainSet, clearTextureSet, raymarchSet;

    RD *device;

    BufferID voxelCountBuffer;
    uint32_t *countBufferPtr;
    uint32_t size;

    // @TEMP
    TextureID texture;
    bool enableConservativeRasterization = true;

    void InitializePrepassResources();
    void InitializeMainResources();
    void InitializeRayMarchResources();

    void DrawVoxelScene(CommandBufferID cb, PipelineID pipeline, UniformSetID *uniformSet, uint32_t uniformSetCount);

    void ExecuteVoxelPrepass(CommandPoolID cp, CommandBufferID cb, FenceID waitFence);

    void ExecuteMainPass(CommandPoolID cp, CommandBufferID cb, FenceID waitFence);

    struct RayMarchPushConstant {
        glm::mat4 uInvP;
        glm::mat4 uInvV;
        glm::vec4 uCamPos;
    } raymarchConstants;
};