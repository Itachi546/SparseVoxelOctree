#pragma once

#include "voxelizer.h"
#include <glm/glm.hpp>

#include <memory>

struct RenderScene;

namespace gfx {
    class Camera;
};

class SceneVoxelizer : public Voxelizer {

  public:
    SceneVoxelizer(std::shared_ptr<RenderScene> scene);
    void Initialize(uint32_t voxelResolution);

    void Voxelize(CommandPoolID commandPool, CommandBufferID commandBuffer);

    // void RayMarch(CommandBufferID commandBuffer, std::shared_ptr<gfx::Camera> camera);

    void Shutdown();

    ~SceneVoxelizer() {}

  private:
    std::shared_ptr<RenderScene> scene;
    PipelineID prepassPipeline, mainPipeline;
    UniformSetID prepassSet, mainSet;

    // PipelineID clearTexturePipeline, raymarchPipeline;
    // UniformSetID clearTextureSet, raymarchSet;

    RD *device;

    BufferID voxelCountBuffer;
    uint32_t *countBufferPtr;
    uint32_t voxelResolution;

    // TextureID texture;
    bool enableConservativeRasterization = true;

    void InitializePrepassResources();
    void InitializeMainResources();
    // void InitializeRayMarchResources();

    void DrawVoxelScene(CommandBufferID cb, PipelineID pipeline, UniformSetID *uniformSet, uint32_t uniformSetCount);

    void ExecuteVoxelPrepass(CommandPoolID cp, CommandBufferID cb, FenceID waitFence);

    void ExecuteMainPass(CommandPoolID cp, CommandBufferID cb, FenceID waitFence);
    /*
    struct RayMarchPushConstant {
        glm::mat4 uInvP;
        glm::mat4 uInvV;
        glm::vec3 uCamPos;
        float voxelResolution;
    } raymarchConstants;
    */
};