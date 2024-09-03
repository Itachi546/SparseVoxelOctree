#include "pch.h"
#include "octree-tracer.h"
#include "gfx/camera.h"
#include "rendering/rendering-utils.h"
#include "octree-builder.h"

void OctreeTracer::Initialize(std::shared_ptr<OctreeBuilder> builder) {
    this->builder = builder;
    RD::UniformBinding bindings[] = {
        {RD::BINDING_TYPE_STORAGE_BUFFER, 0},
    };
    RD::PushConstant pushConstants = {0, sizeof(PushConstants)};

    ShaderID shaders[2] = {
        RenderingUtils::CreateShaderModuleFromFile("assets/SPIRV/raycast-grid.vert.spv", nullptr, 0, nullptr, 0),
        RenderingUtils::CreateShaderModuleFromFile("assets/SPIRV/octree-raycast.frag.spv", bindings, (uint32_t)std::size(bindings), &pushConstants, 1),
    };
    RD::RasterizationState rasterizationState = RD::RasterizationState::Create();
    RD::DepthState depthState = RD::DepthState::Create();
    depthState.enableDepthWrite = false;
    depthState.enableDepthTest = false;

    RD::Format colorAttachmentFormat = RD::FORMAT_B8G8R8A8_UNORM;
    RD::BlendState blendState = RD::BlendState::Create();

    RD *device = RD::GetInstance();
    pipeline = device->CreateGraphicsPipeline(shaders,
                                              static_cast<uint32_t>(std::size(shaders)),
                                              RD::TOPOLOGY_TRIANGLE_LIST,
                                              &rasterizationState,
                                              &depthState,
                                              &colorAttachmentFormat,
                                              &blendState,
                                              1,
                                              RD::FORMAT_D24_UNORM_S8_UINT,
                                              false,
                                              "Octree Raymarch");
    device->Destroy(shaders[0]);
    device->Destroy(shaders[1]);

    RD::BoundUniform boundUniform = {RD::BINDING_TYPE_STORAGE_BUFFER, 0, builder->octreeBuffer};
    uniformSet = device->CreateUniformSet(pipeline, &boundUniform, 1, 0, "Octree Raymarch Set");
}

void OctreeTracer::Trace(CommandBufferID commandBuffer, std::shared_ptr<gfx::Camera> camera) {
    pushConstants.invP = camera->GetInvProjectionMatrix();
    pushConstants.invV = camera->GetInvViewMatrix();
    pushConstants.camPos = glm::vec4(camera->GetPosition(), 0.0f);

    RD *device = RD::GetInstance();
    device->BindPipeline(commandBuffer, pipeline);
    device->BindUniformSet(commandBuffer, pipeline, &uniformSet, 1);
    device->BindPushConstants(commandBuffer, pipeline, RD::SHADER_STAGE_FRAGMENT, &pushConstants, 0, sizeof(PushConstants));

    device->Draw(commandBuffer, 6, 1, 0, 0);
}

void OctreeTracer::Shutdown() {
    RD *device = RD::GetInstance();
    device->Destroy(pipeline);
    device->Destroy(uniformSet);
}
