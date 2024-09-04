#include "pch.h"

#include "octree-builder.h"

#include "voxelizer.h"
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
    voxelizer = std::make_shared<Voxelizer>();
    voxelizer->Initialize(scene, kDims);

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

    voxelizer->Voxelize(commandPool, commandBuffer);

    uint32_t voxelCount = voxelizer->voxelCount;
    uint32_t octreeSize = (voxelCount * kLevels * 3 * VOXEL_DATA_SIZE) / 4;
    // @TEMP Remove CPU Allocation
    octreeBuffer = device->CreateBuffer(octreeSize, RD::BUFFER_USAGE_STORAGE_BUFFER_BIT, RD::MEMORY_ALLOCATION_TYPE_CPU, "OctreeBuffer");

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
            {RD::BINDING_TYPE_STORAGE_BUFFER, 1, voxelizer->voxelFragmentListBuffer},
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

            TagNode(commandBuffer, i);

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

    uint64_t octreeElmCount = buildInfoPtr[0] + buildInfoPtr[1];

    float octreeMemory = (octreeElmCount * sizeof(uint32_t)) / (1024.0f * 1024.0f);
    LOG("Octree Memory: " + std::to_string(octreeMemory) + "MB");

    // @TEMP CPU side
    std::vector<uint32_t> octree(octreeElmCount);
    void *gpuOctreeData = device->MapBuffer(octreeBuffer);
    std::memcpy(octree.data(), gpuOctreeData, octreeElmCount * sizeof(uint32_t));

    std::vector<glm::vec4> voxels;
    octree::utils::ListVoxelsFromOctree(octree, voxels, static_cast<float>(kDims));
    renderer = std::make_shared<VoxelRenderer>();
    renderer->Initialize(voxels);
}

void OctreeBuilder::InitializeNode(CommandBufferID commandBuffer) {
    device->BindPipeline(commandBuffer, pipelineInitNode);
    device->BindUniformSet(commandBuffer, pipelineInitNode, &initNodeSet, 1);
    device->DispatchComputeIndirect(commandBuffer, dispatchIndirectBuffer, 0);
}

void OctreeBuilder::TagNode(CommandBufferID commandBuffer, uint32_t level) {
    device->BindPipeline(commandBuffer, pipelineTagNode);
    device->BindUniformSet(commandBuffer, pipelineTagNode, &tagNodeSet, 1);

    uint32_t data[] = {voxelizer->voxelCount, level, kDims};
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

void OctreeBuilder::Debug(CommandBufferID commandBuffer, std::shared_ptr<gfx::Camera> camera) {
    static bool raycast = false;
    ImGui::Checkbox("Raycast", &raycast);
    if (raycast) {
        voxelizer->RayMarch(commandBuffer, camera);
    } else {
        glm::mat4 VP = camera->GetProjectionMatrix() * camera->GetViewMatrix();
        renderer->Render(commandBuffer, VP);
    }
}

void OctreeBuilder::Shutdown() {
    renderer->Shutdown();
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

    voxelizer->Shutdown();
}
