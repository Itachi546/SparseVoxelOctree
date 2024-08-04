#include "pch.h"
#include "voxelizer.h"

#include "gfx/render-scene.h"
#include "gfx/gltf-scene.h"
#include "rendering/rendering-utils.h"

void Voxelizer::Initialize(std::shared_ptr<RenderScene> scene) {
    this->scene = scene;
    device = RD::GetInstance();

    voxelCountBuffer = device->CreateBuffer(static_cast<uint32_t>(sizeof(uint64_t)),
                                            RD::BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                            RD::MEMORY_ALLOCATION_TYPE_CPU,
                                            "Voxel Count Buffer");
    countBufferPtr = (uint64_t *)device->MapBuffer(voxelCountBuffer);
    *countBufferPtr = 0;

    RD::UniformBinding vsBindings[] = {
        {RD::BINDING_TYPE_STORAGE_BUFFER, 0, 1},
        {RD::BINDING_TYPE_STORAGE_BUFFER, 0, 2},
        {RD::BINDING_TYPE_STORAGE_BUFFER, 0, 3},
    };

    RD::UniformBinding fsBindings[] = {
        {RD::BINDING_TYPE_STORAGE_BUFFER, 0, 4},
        {RD::BINDING_TYPE_STORAGE_BUFFER, 0, 5},
        {RD::BINDING_TYPE_IMAGE, 0, 6},
    };

    ShaderID shaders[3] = {
        RenderingUtils::CreateShaderModuleFromFile("assets/SPIRV/voxelizer.vert.spv", vsBindings, (uint32_t)std::size(vsBindings), nullptr, 0),
        RenderingUtils::CreateShaderModuleFromFile("assets/SPIRV/voxelizer.geom.spv", nullptr, 0, nullptr, 0),
        RenderingUtils::CreateShaderModuleFromFile("assets/SPIRV/voxelizer.frag.spv", fsBindings, (uint32_t)std::size(fsBindings), nullptr, 0),
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
    device->Destroy(shaders[2]);

    RD::TextureDescription description = RD::TextureDescription::Initialize(VOXEL_GRID_SIZE, VOXEL_GRID_SIZE, VOXEL_GRID_SIZE);
    description.format = RD::FORMAT_R8_UNORM;
    description.usageFlags = RD::TEXTURE_USAGE_STORAGE_BIT;
    description.textureType = RD::TEXTURE_TYPE_3D;
    texture = device->CreateTexture(&description, "VoxelTexture");

    std::shared_ptr<GLTFScene> gltfScene = std::static_pointer_cast<GLTFScene>(scene);
    RD::BoundUniform boundedUniform[] = {
        {RD::BINDING_TYPE_STORAGE_BUFFER, 1, gltfScene->vertexBuffer},
        {RD::BINDING_TYPE_STORAGE_BUFFER, 2, gltfScene->drawCommandBuffer},
        {RD::BINDING_TYPE_STORAGE_BUFFER, 3, gltfScene->transformBuffer},
        {RD::BINDING_TYPE_STORAGE_BUFFER, 4, gltfScene->materialBuffer},
        {RD::BINDING_TYPE_STORAGE_BUFFER, 5, voxelCountBuffer},
        {RD::BINDING_TYPE_IMAGE, 6, texture},
    };
    prepassSet = device->CreateUniformSet(prepassPipeline, boundedUniform, static_cast<uint32_t>(std::size(boundedUniform)), 0, "Prepass Mesh Binding");
}

void Voxelizer::Voxelize(CommandPoolID cp, CommandBufferID cb) {
    FenceID waitFence = device->CreateFence("TempFence");
    RD::ImmediateSubmitInfo submitInfo = {
        .queue = device->GetDeviceQueue(RD::QUEUE_TYPE_GRAPHICS),
        .commandPool = cp,
        .commandBuffer = cb,
        .fence = waitFence,
    };

    RD::RenderingInfo renderingInfo = {
        .width = VOXEL_GRID_SIZE,
        .height = VOXEL_GRID_SIZE,
        .layerCount = 1,
        .colorAttachmentCount = 0,
        .pColorAttachments = nullptr,
        .pDepthStencilAttachment = nullptr,
    };
    UniformSetID bindings[] = {prepassSet};

    glm::mat4 VP = glm::mat4(1.0f);
    device->ImmediateSubmit([&](CommandBufferID commandBuffer) {
        RD::TextureBarrier barrier{
            .texture = texture,
            .srcAccess = 0,
            .dstAccess = RD::BARRIER_ACCESS_SHADER_WRITE_BIT,
            .newLayout = RD::TEXTURE_LAYOUT_GENERAL,
            .srcQueueFamily = QUEUE_FAMILY_IGNORED,
            .dstQueueFamily = QUEUE_FAMILY_IGNORED,
            .baseMipLevel = 0,
            .baseArrayLayer = 0,
            .levelCount = 1,
            .layerCount = 1,
        };
        device->PipelineBarrier(commandBuffer, RD::PIPELINE_STAGE_TOP_OF_PIPE_BIT, RD::PIPELINE_STAGE_FRAGMENT_SHADER_BIT, &barrier, 1);

        device->BeginRenderPass(commandBuffer, &renderingInfo);
        device->SetViewport(commandBuffer, 0, 0, static_cast<float>(renderingInfo.width), static_cast<float>(renderingInfo.height));
        device->SetScissor(commandBuffer, 0, 0, renderingInfo.width, renderingInfo.height);

        device->BindPipeline(commandBuffer, prepassPipeline);
        device->BindUniformSet(commandBuffer, prepassPipeline, bindings, static_cast<uint32_t>(std::size(bindings)));

        std::shared_ptr<GLTFScene> gltfScene = std::static_pointer_cast<GLTFScene>(scene);
        device->BindIndexBuffer(commandBuffer, gltfScene->indexBuffer);
        uint32_t drawCount = static_cast<uint32_t>(gltfScene->meshGroup.drawCommands.size());
        device->DrawIndexedIndirect(commandBuffer, gltfScene->drawCommandBuffer, 0, drawCount, sizeof(RD::DrawElementsIndirectCommand));

        device->EndRenderPass(commandBuffer);
    },
                            &submitInfo);

    device->WaitForFence(&waitFence, 1, UINT64_MAX);

    LOG("Total voxels: " + std::to_string(*countBufferPtr));

    device->Destroy(waitFence);
    device->ResetCommandPool(cp);
}

void Voxelizer::Shutdown() {
    device->Destroy(texture);
    device->Destroy(prepassSet);
    device->Destroy(prepassPipeline);
    device->Destroy(voxelCountBuffer);
}