#include "octree-raycaster.h"
#include "parallel-octree.h"
#include "gfx/camera.h"
#include "gfx/gpu-timer.h"

#include "rendering/rendering-utils.h"

void OctreeRaycaster::Initialize(uint32_t outputWidth, uint32_t outputHeight, ParallelOctree *octree) {

    width = outputWidth;
    height = outputHeight;
    minBound = octree->center - octree->size;
    maxBound = octree->center + octree->size;

    RenderingDevice *device = RD::GetInstance();

    RD::UniformBinding bindings[] = {
        RD::UniformBinding{RD::BINDING_TYPE_UNIFORM_BUFFER, 0, 0},
        RD::UniformBinding{RD::BINDING_TYPE_STORAGE_BUFFER, 1, 0},
        RD::UniformBinding{RD::BINDING_TYPE_STORAGE_BUFFER, 1, 1},
        RD::UniformBinding{RD::BINDING_TYPE_IMAGE, 1, 2},
        RD::UniformBinding{RD::BINDING_TYPE_IMAGE, 1, 3},
    };
    RD::PushConstant pushConstant = {
        .offset = 0,
        .size = sizeof(float) * 8,
    };

    ShaderID shaderID = RenderingUtils::CreateShaderModuleFromFile("assets/SPIRV/raycast.comp.spv", bindings, static_cast<uint32_t>(std::size(bindings)), &pushConstant, 1);
    pipeline = device->CreateComputePipeline(shaderID, "Raycast Compute Pipeline");
    device->Destroy(shaderID);

    uint32_t nodeDataSize = static_cast<uint32_t>(octree->nodePools.size() * sizeof(uint32_t));
    nodesBuffer = device->CreateBuffer(nodeDataSize, RD::BUFFER_USAGE_STORAGE_BUFFER_BIT | RD::BUFFER_USAGE_TRANSFER_DST_BIT, RD::MEMORY_ALLOCATION_TYPE_GPU, "NodeBuffer");

    uint32_t brickDataSize = static_cast<uint32_t>(octree->brickPools.size() * sizeof(OctreeBrick));
    brickBuffer = device->CreateBuffer(brickDataSize, RD::BUFFER_USAGE_STORAGE_BUFFER_BIT | RD::BUFFER_USAGE_TRANSFER_DST_BIT, RD::MEMORY_ALLOCATION_TYPE_GPU, "BrickBuffer");

    BufferID stagingBuffer = device->CreateBuffer(nodeDataSize + brickDataSize, RD::BUFFER_USAGE_TRANSFER_SRC_BIT, RD::MEMORY_ALLOCATION_TYPE_CPU, "StagingBuffer");
    void *stagingBufferPtr = device->MapBuffer(stagingBuffer);
    std::memcpy(stagingBufferPtr, octree->nodePools.data(), nodeDataSize);
    std::memcpy((uint8_t *)stagingBufferPtr + nodeDataSize, octree->brickPools.data(), brickDataSize);

    device->ImmediateSubmit([&](CommandBufferID commandBuffer) {
        RD::BufferCopyRegion copyRegion = {0, 0, nodeDataSize};
        device->CopyBuffer(commandBuffer, stagingBuffer, nodesBuffer, &copyRegion);
        copyRegion.srcOffset = nodeDataSize;
        copyRegion.size = brickDataSize;
        device->CopyBuffer(commandBuffer, stagingBuffer, brickBuffer, &copyRegion);
    });
    device->Destroy(stagingBuffer);

    RD::TextureDescription textureDescription = RD::TextureDescription::Initialize(1920, 1080);
    textureDescription.usageFlags = RD::TEXTURE_USAGE_STORAGE_BIT | RD::TEXTURE_USAGE_TRANSFER_SRC_BIT;
    outputTexture = device->CreateTexture(&textureDescription, "RaycastOutputTexture");

    textureDescription.usageFlags = RD::TEXTURE_USAGE_STORAGE_BIT;
    accumulationTexture = device->CreateTexture(&textureDescription, "RaycastAccumulationTexture");

    outputImageBarrier.push_back({outputTexture, RD::BARRIER_ACCESS_NONE, RD::BARRIER_ACCESS_SHADER_WRITE_BIT, RD::TEXTURE_LAYOUT_GENERAL});
    outputImageBarrier.push_back({accumulationTexture, RD::BARRIER_ACCESS_NONE, RD::BARRIER_ACCESS_SHADER_READ_BIT | RD::BARRIER_ACCESS_SHADER_WRITE_BIT, RD::TEXTURE_LAYOUT_GENERAL});

    RD::BoundUniform boundedUniform[4] = {
        {RD::BINDING_TYPE_STORAGE_BUFFER, 0, nodesBuffer},
        {RD::BINDING_TYPE_STORAGE_BUFFER, 1, brickBuffer},
        {RD::BINDING_TYPE_IMAGE, 2, outputTexture},
        {RD::BINDING_TYPE_IMAGE, 3, accumulationTexture},
    };
    resourceSet = device->CreateUniformSet(pipeline, boundedUniform, static_cast<uint32_t>(std::size(boundedUniform)), 1, "RaycastResourceSet");
}

void OctreeRaycaster::Render(CommandBufferID commandBuffer, UniformSetID globalSet) {
    glm::vec4 dims[2] = {glm::vec4{minBound, spp}, glm::vec4{maxBound, spp}};
    auto *device = RD::GetInstance();
    device->PipelineBarrier(commandBuffer, RD::PIPELINE_STAGE_TOP_OF_PIPE_BIT, RD::PIPELINE_STAGE_COMPUTE_SHADER_BIT, outputImageBarrier);
    device->BindPipeline(commandBuffer, pipeline);
    device->BindUniformSet(commandBuffer, pipeline, globalSet);
    device->BindUniformSet(commandBuffer, pipeline, resourceSet);
    device->BindPushConstants(commandBuffer, pipeline, RD::SHADER_STAGE_COMPUTE, &dims, 0, sizeof(float) * 8);
    device->DispatchCompute(commandBuffer, (width / 8) + 1, (height / 8) + 1, 1);
    spp += 1.0f;
}

void OctreeRaycaster::Shutdown() {
    auto *device = RD::GetInstance();
    device->Destroy(pipeline);
    device->Destroy(nodesBuffer);
    device->Destroy(outputTexture);
    device->Destroy(accumulationTexture);
    device->Destroy(brickBuffer);
    device->Destroy(resourceSet);
}
