#include "pch.h"

#include "octree-builder.h"

#include "voxelizer/voxelizer.h"
#include "voxelizer/scene-voxelizer.h"
#include "voxelizer/terrain-voxelizer.h"

#include "gfx/render-scene.h"
#include "rendering/rendering-utils.h"
#include "voxel-renderer.h"
#include "gfx/camera.h"
#include "cpu-octree-utils.h"
#include <imgui.h>

void OctreeBuilder::Initialize(std::shared_ptr<RenderScene> scene) {

    // @TODO we may have to decide what to re-initialize and what to
    // destroy, so that same object can be recycled or instanced can
    // be created without duplicating the pipeline.
    this->scene = scene;

    device = RD::GetInstance();

    RD::UniformBinding bindings[] = {
        {RD::BINDING_TYPE_STORAGE_BUFFER, 0, 0},
        {RD::BINDING_TYPE_STORAGE_BUFFER, 0, 1},
    };
    uint32_t bindingCount = static_cast<uint32_t>(std::size(bindings));

    {
        ShaderID shader = RenderingUtils::CreateShaderModuleFromFile("assets/SPIRV/octree-init-node.comp.spv", bindings, bindingCount, nullptr, 0);
        pipelineInitNode = device->CreateComputePipeline(shader, false, "InitOctreeNodePipeline");
        device->Destroy(shader);
    }

    {
        RD::PushConstant pushConstant = {0, sizeof(uint32_t) * 3};
        ShaderID shader = RenderingUtils::CreateShaderModuleFromFile("assets/SPIRV/octree-tag-node.comp.spv", bindings, bindingCount, &pushConstant, 1);
        pipelineTagNode = device->CreateComputePipeline(shader, false, "TagOctreeNodePipeline");
        device->Destroy(shader);
    }
    {
        ShaderID shader = RenderingUtils::CreateShaderModuleFromFile("assets/SPIRV/octree-allocate-node.comp.spv", bindings, bindingCount, nullptr, 0);
        pipelineAllocateNode = device->CreateComputePipeline(shader, false, "AllocateOctreeNodePipeline");
        device->Destroy(shader);
    }
    {
        ShaderID shader = RenderingUtils::CreateShaderModuleFromFile("assets/SPIRV/octree-update-params.comp.spv", bindings, bindingCount, nullptr, 0);
        pipelineUpdateParams = device->CreateComputePipeline(shader, false, "UpdateDispatchParams");
        device->Destroy(shader);
    }
}

void OctreeBuilder::Build(CommandPoolID commandPool, CommandBufferID commandBuffer) {
    //std::shared_ptr<Voxelizer> voxelizer = std::make_shared<TerrainVoxelizer>();
    std::shared_ptr<Voxelizer> voxelizer = std::make_shared<SceneVoxelizer>(scene);
    voxelizer->Initialize(kResolution);
    voxelizer->Voxelize(commandPool, commandBuffer);

    uint32_t voxelCount = voxelizer->voxelCount;
    uint32_t octreeSize = (voxelCount * kLevels * VOXEL_DATA_SIZE * 4) / 3;

    octreeBuffer = device->CreateBuffer(octreeSize, RD::BUFFER_USAGE_STORAGE_BUFFER_BIT, RD::MEMORY_ALLOCATION_TYPE_GPU, "OctreeBuffer");
    LOG("Allocated Octree Memory: " + std::to_string(InMB(octreeSize)) + "MB");

    buildInfoBuffer = device->CreateBuffer(sizeof(uint32_t) * 3, RD::BUFFER_USAGE_STORAGE_BUFFER_BIT, RD::MEMORY_ALLOCATION_TYPE_CPU, "OctreeBuildInfoBuffer");
    uint32_t *buildInfoPtr = (uint32_t *)device->MapBuffer(buildInfoBuffer);
    buildInfoPtr[0] = 0, buildInfoPtr[1] = 1, buildInfoPtr[2] = 0;

    dispatchIndirectBuffer = device->CreateBuffer(sizeof(uint32_t) * 3, RD::BUFFER_USAGE_STORAGE_BUFFER_BIT | RD::BUFFER_USAGE_INDIRECT_BUFFER_BIT, RD::MEMORY_ALLOCATION_TYPE_CPU, "OctreeDispatchIndirectBuffer");
    uint32_t *ptr = (uint32_t *)device->MapBuffer(dispatchIndirectBuffer);
    ptr[0] = 1, ptr[1] = 1, ptr[2] = 1;

    {
        RD::BoundUniform boundUniforms[] = {
            {RD::BINDING_TYPE_STORAGE_BUFFER, 0, octreeBuffer},
            {RD::BINDING_TYPE_STORAGE_BUFFER, 1, buildInfoBuffer},
        };
        initNodeSet = device->CreateUniformSet(pipelineInitNode, boundUniforms, static_cast<uint32_t>(std::size(boundUniforms)), 0, "InitNodeSet");
    }

    {
        RD::BoundUniform boundUniforms[] = {
            {RD::BINDING_TYPE_STORAGE_BUFFER, 0, octreeBuffer},
            {RD::BINDING_TYPE_STORAGE_BUFFER, 1, voxelizer->voxelFragmentBuffer},
        };
        tagNodeSet = device->CreateUniformSet(pipelineTagNode, boundUniforms, static_cast<uint32_t>(std::size(boundUniforms)), 0, "TagNodeSet");
    }
    {
        RD::BoundUniform boundUniforms[] = {
            {RD::BINDING_TYPE_STORAGE_BUFFER, 0, octreeBuffer},
            {RD::BINDING_TYPE_STORAGE_BUFFER, 1, buildInfoBuffer},
        };
        allocateNodeSet = device->CreateUniformSet(pipelineAllocateNode, boundUniforms, static_cast<uint32_t>(std::size(boundUniforms)), 0, "AllocateNodeSet");
    }

    {
        RD::BoundUniform boundUniforms[] = {
            {RD::BINDING_TYPE_STORAGE_BUFFER, 0, dispatchIndirectBuffer},
            {RD::BINDING_TYPE_STORAGE_BUFFER, 1, buildInfoBuffer},
        };
        updateParamsSet = device->CreateUniformSet(pipelineUpdateParams, boundUniforms, static_cast<uint32_t>(std::size(boundUniforms)), 0, "UpdateParamsSet");
    }

    RD::ImmediateSubmitInfo submitInfo;
    submitInfo.queue = device->GetDeviceQueue(RD::QUEUE_TYPE_GRAPHICS);
    submitInfo.commandPool = device->CreateCommandPool(submitInfo.queue, "TempCommandPool");
    submitInfo.commandBuffer = device->CreateCommandBuffer(submitInfo.commandPool, "TempCommandBuffer");
    submitInfo.fence = device->CreateFence("TempFence");

    RD::BufferBarrier tagNodeBarrier[] = {
        {octreeBuffer, RD::BARRIER_ACCESS_SHADER_WRITE_BIT, RD::BARRIER_ACCESS_SHADER_WRITE_BIT | RD::BARRIER_ACCESS_SHADER_READ_BIT, QUEUE_FAMILY_IGNORED, QUEUE_FAMILY_IGNORED, 0, UINT64_MAX},
    };

    RD::BufferBarrier allocateNodeBarrier[] = {
        {octreeBuffer, RD::BARRIER_ACCESS_SHADER_WRITE_BIT, RD::BARRIER_ACCESS_SHADER_WRITE_BIT | RD::BARRIER_ACCESS_SHADER_READ_BIT, QUEUE_FAMILY_IGNORED, QUEUE_FAMILY_IGNORED, 0, UINT64_MAX},
        {buildInfoBuffer, RD::BARRIER_ACCESS_SHADER_WRITE_BIT, RD::BARRIER_ACCESS_SHADER_READ_BIT, QUEUE_FAMILY_IGNORED, QUEUE_FAMILY_IGNORED, 0, UINT64_MAX},
    };

    RD::BufferBarrier updateParamsBarrier[] = {
        {dispatchIndirectBuffer, RD::BARRIER_ACCESS_INDIRECT_COMMAND_READ_BIT, RD::BARRIER_ACCESS_SHADER_WRITE_BIT, QUEUE_FAMILY_IGNORED, QUEUE_FAMILY_IGNORED, 0, UINT64_MAX},
        {buildInfoBuffer, RD::BARRIER_ACCESS_SHADER_READ_BIT, RD::BARRIER_ACCESS_SHADER_WRITE_BIT | RD::BARRIER_ACCESS_SHADER_READ_BIT, QUEUE_FAMILY_IGNORED, QUEUE_FAMILY_IGNORED, 0, UINT64_MAX},
    };

    for (uint32_t i = 0; i < kLevels; ++i) {
        device->ImmediateSubmit([&](CommandBufferID commandBuffer) {
            InitializeNode(commandBuffer);

            device->PipelineBarrier(commandBuffer,
                                    RD::PIPELINE_STAGE_COMPUTE_SHADER_BIT, RD::PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                    nullptr, 0,
                                    tagNodeBarrier, static_cast<uint32_t>(std::size(tagNodeBarrier)));

            TagNode(commandBuffer, i, voxelCount);

            device->PipelineBarrier(commandBuffer,
                                    RD::PIPELINE_STAGE_COMPUTE_SHADER_BIT, RD::PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                    nullptr, 0,
                                    allocateNodeBarrier, static_cast<uint32_t>(std::size(allocateNodeBarrier)));

            // Skip this for leaf node
            if (i != kLevels - 1) {
                AllocateNode(commandBuffer);

                device->PipelineBarrier(commandBuffer,
                                        RD::PIPELINE_STAGE_COMPUTE_SHADER_BIT | RD::PIPELINE_STAGE_DRAW_INDIRECT_BIT, RD::PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                        nullptr, 0,
                                        updateParamsBarrier, static_cast<uint32_t>(std::size(updateParamsBarrier)));
                UpdateParams(commandBuffer);
            }
        },
                                &submitInfo);
        device->WaitForFence(&submitInfo.fence, 1, UINT64_MAX);
        device->ResetFences(&submitInfo.fence, 1);
        device->ResetCommandPool(submitInfo.commandPool);
    }
    device->Destroy(submitInfo.commandPool);
    device->Destroy(submitInfo.fence);
    voxelizer->Shutdown();

    octreeElmCount = buildInfoPtr[0] + buildInfoPtr[1];

    float octreeMemory = InMB(octreeElmCount * sizeof(uint32_t));
    LOG("Actual Octree Memory: " + std::to_string(octreeMemory) + "MB");
}

void OctreeBuilder::InitializeNode(CommandBufferID commandBuffer) {
    device->BindPipeline(commandBuffer, pipelineInitNode);
    device->BindUniformSet(commandBuffer, pipelineInitNode, &initNodeSet, 1);
    device->DispatchComputeIndirect(commandBuffer, dispatchIndirectBuffer, 0);
}

void OctreeBuilder::TagNode(CommandBufferID commandBuffer, uint32_t level, uint32_t voxelCount) {
    device->BindPipeline(commandBuffer, pipelineTagNode);
    device->BindUniformSet(commandBuffer, pipelineTagNode, &tagNodeSet, 1);

    uint32_t data[] = {voxelCount, level, kResolution};
    device->BindPushConstants(commandBuffer, pipelineTagNode, RD::SHADER_STAGE_COMPUTE, &data, 0, sizeof(uint32_t) * 3);

    uint32_t workGroupSize = RenderingUtils::GetWorkGroupSize(data[0], 32);
    device->DispatchCompute(commandBuffer, data[0], 1, 1);
}

void OctreeBuilder::AllocateNode(CommandBufferID commandBuffer) {
    device->BindPipeline(commandBuffer, pipelineAllocateNode);
    device->BindUniformSet(commandBuffer, pipelineAllocateNode, &allocateNodeSet, 1);
    device->DispatchComputeIndirect(commandBuffer, dispatchIndirectBuffer, 0);
}

void OctreeBuilder::UpdateParams(CommandBufferID commandBuffer) {
    device->BindPipeline(commandBuffer, pipelineUpdateParams);
    device->BindUniformSet(commandBuffer, pipelineUpdateParams, &updateParamsSet, 1);
    device->DispatchCompute(commandBuffer, 1, 1, 1);
}

void OctreeBuilder::Shutdown() {
    device->Destroy(dispatchIndirectBuffer);
    device->Destroy(octreeBuffer);
    device->Destroy(buildInfoBuffer);

    device->Destroy(pipelineInitNode);
    device->Destroy(pipelineTagNode);
    device->Destroy(pipelineAllocateNode);
    device->Destroy(pipelineUpdateParams);

    device->Destroy(initNodeSet);
    device->Destroy(tagNodeSet);
    device->Destroy(allocateNodeSet);
    device->Destroy(updateParamsSet);
}
