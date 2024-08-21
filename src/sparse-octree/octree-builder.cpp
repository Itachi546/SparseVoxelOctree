#include "pch.h"

#include "octree-builder.h"

#include "voxelizer.h"
#include "gfx/render-scene.h"
#include "rendering/rendering-utils.h"

void OctreeBuilder::Initialize(std::shared_ptr<RenderScene> scene) {

    // @TODO we may have to decide what to re-initialize and what to
    // destroy, so that same object can be recycled or instanced can
    // be created without duplicating the pipeline.
    this->scene = scene;
    voxelizer = std::make_shared<Voxelizer>();
    voxelizer->Initialize(scene, kDims);

    device = RD::GetInstance();
    {
        RD::UniformBinding binding = {RD::BINDING_TYPE_STORAGE_BUFFER, 0, 0};
        ShaderID shader = RenderingUtils::CreateShaderModuleFromFile("assets/SPIRV/octree-init-node.comp.spv", &binding, 1, nullptr, 0);
        pipelineInitNode = device->CreateComputePipeline(shader, false, "InitOctreeNodePipeline");
        device->Destroy(shader);
    }

    RD::UniformBinding binding[] = {
        {RD::BINDING_TYPE_STORAGE_BUFFER, 0, 0},
        {RD::BINDING_TYPE_STORAGE_BUFFER, 0, 1},
    };
    {
        RD::PushConstant pushConstant = {0, sizeof(uint32_t) * 3};
        ShaderID shader = RenderingUtils::CreateShaderModuleFromFile("assets/SPIRV/octree-flag-node.comp.spv", binding, static_cast<uint32_t>(std::size(binding)), &pushConstant, 1);
        pipelineTagNode = device->CreateComputePipeline(shader, false, "FlagOctreeNodePipeline");
        device->Destroy(shader);
    }
    {
        ShaderID shader = RenderingUtils::CreateShaderModuleFromFile("assets/SPIRV/octree-allocate-node.comp.spv", binding, static_cast<uint32_t>(std::size(binding)), nullptr, 0);
        pipelineAllocateNode = device->CreateComputePipeline(shader, false, "AllocateOctreeNodePipeline");
        device->Destroy(shader);
    }
}

void OctreeBuilder::Build(CommandPoolID commandPool, CommandBufferID commandBuffer) {

    voxelizer->Voxelize(commandPool, commandBuffer);

    uint32_t voxelCount = voxelizer->voxelCount;
    uint32_t octreeSize = (voxelCount * kLevels * 3 * sizeof(uint32_t)) / 4;
    octreeBuffer = device->CreateBuffer(octreeSize, RD::BUFFER_USAGE_STORAGE_BUFFER_BIT, RD::MEMORY_ALLOCATION_TYPE_GPU, "OctreeBuffer");

    buildInfoBuffer = device->CreateBuffer(sizeof(uint32_t) * 2, RD::BUFFER_USAGE_STORAGE_BUFFER_BIT, RD::MEMORY_ALLOCATION_TYPE_CPU, "OctreeBuildInfoBuffer");
    uint32_t *ptr = (uint32_t *)device->MapBuffer(buildInfoBuffer);
    ptr[0] = 0, ptr[1] = 1;

    {
        RD::BoundUniform boundUniforms = {RD::BINDING_TYPE_STORAGE_BUFFER, 0, octreeBuffer};
        initNodeSet = device->CreateUniformSet(pipelineInitNode, &boundUniforms, 1, 0, "InitNodeSet");
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

    InitializeNode(commandBuffer);
}

void OctreeBuilder::InitializeNode(CommandBufferID commandBuffer) {
}

void OctreeBuilder::TagNode(CommandBufferID commandBuffer) {
}

void OctreeBuilder::AllocateNode(CommandBufferID commandBuffer) {
}

void OctreeBuilder::Debug(CommandBufferID commandBuffer, const gfx::Camera *camera) {
    voxelizer->RayMarch(commandBuffer, camera);
}

void OctreeBuilder::Shutdown() {
    device->Destroy(octreeBuffer);
    device->Destroy(buildInfoBuffer);
    device->Destroy(pipelineInitNode);
    device->Destroy(pipelineTagNode);
    device->Destroy(pipelineAllocateNode);
    device->Destroy(initNodeSet);
    device->Destroy(tagNodeSet);
    device->Destroy(allocateNodeSet);
    voxelizer->Shutdown();
}
