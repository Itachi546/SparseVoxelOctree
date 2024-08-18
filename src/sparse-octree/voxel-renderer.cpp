#include "voxel-renderer.h"

#include "gfx/camera.h"
#include "rendering/rendering-utils.h"
#include "gfx/geometry.h"
#include "gfx/async-loader.h"

#include <algorithm>
#include <vector>

void VoxelRenderer::Initialize(std::vector<glm::vec4> &voxels) {
    device = RD::GetInstance();

    RD::UniformBinding bindings[] = {
        RD::UniformBinding{RD::BINDING_TYPE_UNIFORM_BUFFER, 0, 0},
        RD::UniformBinding{RD::BINDING_TYPE_STORAGE_BUFFER, 1, 0},
        RD::UniformBinding{RD::BINDING_TYPE_STORAGE_BUFFER, 1, 1},
    };

    RD::PushConstant pushConstant = {0, sizeof(glm::mat4)};

    ShaderID shaders[2] = {
        RenderingUtils::CreateShaderModuleFromFile("assets/SPIRV/instanced-voxel.vert.spv", bindings, static_cast<uint32_t>(std::size(bindings)), &pushConstant, 1),
        RenderingUtils::CreateShaderModuleFromFile("assets/SPIRV/instanced-voxel.frag.spv", nullptr, 0, nullptr, 0),
    };

    RD::Format colorAttachmentFormats[] = {RD::FORMAT_B8G8R8A8_UNORM};
    RD::BlendState blendState = RD::BlendState::Create();

    RD::RasterizationState rs = RD::RasterizationState::Create();
    // @NOTE indices are in wrong order :)
    rs.cullMode = RD::CULL_MODE_FRONT;
    RD::DepthState ds = RD::DepthState::Create();
    ds.enableDepthTest = true;
    ds.enableDepthWrite = true;
    pipeline = device->CreateGraphicsPipeline(shaders, static_cast<uint32_t>(std::size(shaders)),
                                              RD::Topology::TOPOLOGY_TRIANGLE_LIST,
                                              &rs,
                                              &ds,
                                              colorAttachmentFormats,
                                              &blendState,
                                              static_cast<uint32_t>(std::size(colorAttachmentFormats)),
                                              RD::FORMAT_D24_UNORM_S8_UINT,
                                              false,
                                              "Voxel Rasterization Pipeline");

    device->Destroy(shaders[0]);
    device->Destroy(shaders[1]);

    std::vector<Vertex> vertices = {};
    std::vector<uint32_t> indices = {};
    gfx::CreateCubeGeometry(vertices, indices);

    numVertices = static_cast<uint32_t>(indices.size());
    numVoxels = static_cast<uint32_t>(voxels.size());

    uint32_t vertexDataSize = static_cast<uint32_t>(numVertices * sizeof(Vertex));
    vertexBuffer = device->CreateBuffer(vertexDataSize, RD::BUFFER_USAGE_TRANSFER_DST_BIT | RD::BUFFER_USAGE_STORAGE_BUFFER_BIT, RD::MEMORY_ALLOCATION_TYPE_GPU, "CubeVertexBuffer");

    uint32_t indexDataSize = static_cast<uint32_t>(numVertices * sizeof(uint32_t));
    indexBuffer = device->CreateBuffer(indexDataSize, RD::BUFFER_USAGE_TRANSFER_DST_BIT | RD::BUFFER_USAGE_INDEX_BUFFER_BIT, RD::MEMORY_ALLOCATION_TYPE_GPU, "CubeIndexBuffer");

    uint32_t instancedDataSize = static_cast<uint32_t>(voxels.size() * sizeof(glm::vec4));
    instanceDataBuffer = device->CreateBuffer(instancedDataSize, RD::BUFFER_USAGE_TRANSFER_DST_BIT | RD::BUFFER_USAGE_STORAGE_BUFFER_BIT, RD::MEMORY_ALLOCATION_TYPE_GPU, "CubeInstanceBuffer");

    uint32_t stagingBufferSize = std::max(instancedDataSize, vertexDataSize);
    BufferID stagingBuffer = device->CreateBuffer(stagingBufferSize, RD::BUFFER_USAGE_TRANSFER_SRC_BIT, RD::MEMORY_ALLOCATION_TYPE_CPU, "StagingBuffer");
    uint8_t *stagingBufferPtr = device->MapBuffer(stagingBuffer);

    BufferUploadRequest uploadRequests[] = {
        {vertices.data(), vertexBuffer, vertexDataSize},
        {indices.data(), indexBuffer, static_cast<uint32_t>(numVertices * sizeof(uint32_t))},
        {voxels.data(), instanceDataBuffer, instancedDataSize},
    };

    //@TODO Move to transfer queue
    RD::ImmediateSubmitInfo submitInfo;
    submitInfo.queue = device->GetDeviceQueue(RD::QUEUE_TYPE_GRAPHICS);
    submitInfo.commandPool = device->CreateCommandPool(submitInfo.queue, "TempCommandPool");
    submitInfo.commandBuffer = device->CreateCommandBuffer(submitInfo.commandPool, "TempCommandBuffer");
    submitInfo.fence = device->CreateFence("TempFence");
    for (auto &request : uploadRequests) {
        std::memcpy(stagingBufferPtr, request.data, request.size);

        device->ImmediateSubmit([&](CommandBufferID commandBuffer) {
            RD::BufferCopyRegion copyRegion{
                0, 0, request.size};
            device->CopyBuffer(commandBuffer, stagingBuffer, request.bufferId, &copyRegion);
        },
                                &submitInfo);
        device->WaitForFence(&submitInfo.fence, 1, UINT64_MAX);
        device->ResetFences(&submitInfo.fence, 1);
        device->ResetCommandPool(submitInfo.commandPool);
    }
    device->Destroy(submitInfo.fence);
    device->Destroy(submitInfo.commandPool);
    device->Destroy(stagingBuffer);

    RD::BoundUniform boundedUniform[] = {
        {RD::BINDING_TYPE_STORAGE_BUFFER, 0, vertexBuffer},
        {RD::BINDING_TYPE_STORAGE_BUFFER, 1, instanceDataBuffer},
    };
    uniformSet = device->CreateUniformSet(pipeline, boundedUniform, static_cast<uint32_t>(std::size(boundedUniform)), 1, "InstancedVoxelDataSet");
}

void VoxelRenderer::Render(CommandBufferID commandBuffer, glm::mat4 VP) {
    device->BindPipeline(commandBuffer, pipeline);
    device->BindUniformSet(commandBuffer, pipeline, &uniformSet, 1);
    device->BindPushConstants(commandBuffer, pipeline, RD::SHADER_STAGE_VERTEX, &VP[0][0], 0, static_cast<uint32_t>(sizeof(glm::mat4)));

    device->BindIndexBuffer(commandBuffer, indexBuffer);
    device->DrawElementInstanced(commandBuffer, numVertices, numVoxels);
}

void VoxelRenderer::Shutdown() {
    device->Destroy(pipeline);
    device->Destroy(uniformSet);
    device->Destroy(instanceDataBuffer);
    device->Destroy(vertexBuffer);
    device->Destroy(indexBuffer);
}