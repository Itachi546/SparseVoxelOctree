#include "pch.h"
#include "voxelizer.h"

#include "gfx/render-scene.h"
#include "gfx/gltf-scene.h"
#include "rendering/rendering-utils.h"

void Voxelizer::Initialize(std::shared_ptr<RenderScene> scene, BufferID globalBuffer) {
    this->scene = scene;
    device = RD::GetInstance();

    voxelCountBuffer = device->CreateBuffer(static_cast<uint32_t>(sizeof(uint64_t)),
                                            RD::BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                            RD::MEMORY_ALLOCATION_TYPE_CPU,
                                            "Voxel Count Buffer");
    countBufferPtr = (uint64_t *)device->MapBuffer(voxelCountBuffer);
    *countBufferPtr = 0;

    RD::UniformBinding vsBindings[] = {
        {RD::BINDING_TYPE_UNIFORM_BUFFER, 0, 0},
        {RD::BINDING_TYPE_STORAGE_BUFFER, 1, 0},
        {RD::BINDING_TYPE_STORAGE_BUFFER, 1, 1},
        {RD::BINDING_TYPE_STORAGE_BUFFER, 1, 2},
    };

    RD::UniformBinding fsBindings[] = {
        {RD::BINDING_TYPE_STORAGE_BUFFER, 1, 3},
    };

    ShaderID shaders[2] = {
        RenderingUtils::CreateShaderModuleFromFile("assets/SPIRV/voxelizer-prepass.vert.spv", vsBindings, (uint32_t)std::size(vsBindings), nullptr, 0),
        RenderingUtils::CreateShaderModuleFromFile("assets/SPIRV/voxelizer-prepass.frag.spv", fsBindings, (uint32_t)std::size(fsBindings), nullptr, 0),
    };

    RD::RasterizationState rasterizationState = RD::RasterizationState::Create();
    rasterizationState.cullMode = RD::CULL_MODE_NONE;
    RD::DepthState depthState = RD::DepthState::Create();
    depthState.enableDepthWrite = false;
    depthState.enableDepthTest = false;

    prepassPipeline = device->CreateGraphicsPipeline(shaders,
                                                     (uint32_t)std::size(shaders),
                                                     RD::TOPOLOGY_TRIANGLE_LIST,
                                                     &rasterizationState,
                                                     &depthState,
                                                     nullptr,
                                                     nullptr,
                                                     0,
                                                     RD::FORMAT_UNDEFINED,
                                                     "Voxelizer Prepass");

    device->Destroy(shaders[0]);
    device->Destroy(shaders[1]);

    RD::BoundUniform globalBinding{
        .bindingType = RD::BINDING_TYPE_UNIFORM_BUFFER,
        .binding = 0,
        .resourceID = globalBuffer,
        .offset = 0,
    };

    globalSet = device->CreateUniformSet(prepassPipeline, &globalBinding, 1, 0, "GlobalUniformSet");

    std::shared_ptr<GLTFScene> gltfScene = std::static_pointer_cast<GLTFScene>(scene);
    RD::BoundUniform boundedUniform[] = {
        {RD::BINDING_TYPE_STORAGE_BUFFER, 0, gltfScene->vertexBuffer},
        {RD::BINDING_TYPE_STORAGE_BUFFER, 1, gltfScene->drawCommandBuffer},
        {RD::BINDING_TYPE_STORAGE_BUFFER, 2, gltfScene->transformBuffer},
        {RD::BINDING_TYPE_STORAGE_BUFFER, 3, voxelCountBuffer},
    };
    prepassSet = device->CreateUniformSet(prepassPipeline, boundedUniform, static_cast<uint32_t>(std::size(boundedUniform)), 1, "Prepass Mesh Binding");
}

void Voxelizer::Voxelize(CommandBufferID commandBuffer) {
    device->BindPipeline(commandBuffer, prepassPipeline);
    UniformSetID bindings[] = {globalSet, prepassSet};
    device->BindUniformSet(commandBuffer, prepassPipeline, bindings, static_cast<uint32_t>(std::size(bindings)));

    std::shared_ptr<GLTFScene> gltfScene = std::static_pointer_cast<GLTFScene>(scene);
    device->BindIndexBuffer(commandBuffer, gltfScene->indexBuffer);
    uint32_t drawCount = static_cast<uint32_t>(gltfScene->meshGroup.drawCommands.size());
    device->DrawIndexedIndirect(commandBuffer, gltfScene->drawCommandBuffer, 0, drawCount, sizeof(RD::DrawElementsIndirectCommand));
}

void Voxelizer::Shutdown() {
    device->Destroy(globalSet);
    device->Destroy(prepassSet);
    device->Destroy(prepassPipeline);
    device->Destroy(voxelCountBuffer);
}