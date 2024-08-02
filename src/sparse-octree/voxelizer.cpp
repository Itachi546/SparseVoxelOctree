#include "pch.h"
#include "voxelizer.h"

#include "gfx/render-scene.h"
#include "rendering/rendering-utils.h"

void Voxelizer::Initialize(std::shared_ptr<Scene> scene) {
    this->scene = scene;
    device = RD::GetInstance();

    uint32_t bufferSize = static_cast<uint32_t>(512 * 512 * 512 * sizeof(uint64_t) * 0.75);

    voxelListBuffer = device->CreateBuffer(bufferSize, RD::BUFFER_USAGE_STORAGE_BUFFER_BIT, RD::MEMORY_ALLOCATION_TYPE_GPU, "VoxelList Buffer");

    RD::UniformBinding vsBindings[] = {
        {RD::BINDING_TYPE_UNIFORM_BUFFER, 0, 0},
        {RD::BINDING_TYPE_STORAGE_BUFFER, 1, 0},
        {RD::BINDING_TYPE_STORAGE_BUFFER, 1, 1},
        {RD::BINDING_TYPE_STORAGE_BUFFER, 1, 2},
    };

    RD::UniformBinding fsBindings[] = {
        {RD::BINDING_TYPE_STORAGE_BUFFER, 1, 3},
        {RD::BINDING_TYPE_STORAGE_BUFFER, 1, 4},
    };

    ShaderID shaders[2] = {
        RenderingUtils::CreateShaderModuleFromFile("assets/SPIRV/mesh.vert.spv", vsBindings, (uint32_t)std::size(vsBindings), nullptr, 0),
        RenderingUtils::CreateShaderModuleFromFile("assets/SPIRV/voxelizer.frag.spv", fsBindings, (uint32_t)std::size(fsBindings), nullptr, 0),
    };

    RD::Format colorAttachmentFormat[] = {RD::FORMAT_B8G8R8A8_UNORM};
    RD::BlendState blendState = RD::BlendState::Create();
    RD::RasterizationState rasterizationState = RD::RasterizationState::Create();
    rasterizationState.cullMode = RD::CULL_MODE_NONE;
    RD::DepthState depthState = RD::DepthState::Create();
    depthState.enableDepthWrite = false;
    depthState.enableDepthTest = false;

    pipeline = device->CreateGraphicsPipeline(shaders,
                                              (uint32_t)std::size(shaders),
                                              RD::TOPOLOGY_TRIANGLE_LIST,
                                              &rasterizationState,
                                              &depthState,
                                              colorAttachmentFormat,
                                              &blendState,
                                              1,
                                              RD::FORMAT_D24_UNORM_S8_UINT,
                                              "MeshDrawPipeline");

    device->Destroy(shaders[0]);
    device->Destroy(shaders[1]);
    /*
    RD::BoundUniform globalBinding{
        .bindingType = RD::BINDING_TYPE_UNIFORM_BUFFER,
        .binding = 0,
        .resourceID = globalUB,
        .offset = 0,
    };

    globalSet = device->CreateUniformSet(renderPipeline, &globalBinding, 1, 0, "GlobalUniformSet");
    */
}

void Voxelizer::Shutdown() {
    device->Destroy(pipeline);
}