#include "pch.h"

#include "terrain-voxelizer.h"
#include "rendering/rendering-utils.h"

void TerrainVoxelizer::Initialize(uint32_t voxelResolution) {
    this->voxelResolution = voxelResolution;
    this->device = RD::GetInstance();

    voxelCountBuffer = device->CreateBuffer(static_cast<uint32_t>(sizeof(uint32_t) * 2),
                                            RD::BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                            RD::MEMORY_ALLOCATION_TYPE_CPU,
                                            "Voxel Count Buffer");
    countBufferPtr = (uint32_t *)device->MapBuffer(voxelCountBuffer);
    std::memset(countBufferPtr, 0, sizeof(uint32_t) * 2);

    // Initialize Resources
    RD::PushConstant pushConstant = {0, sizeof(uint32_t)};
    {
        RD::UniformBinding bindings = {RD::BINDING_TYPE_STORAGE_BUFFER, 0, 0};
        ShaderID shader = RenderingUtils::CreateShaderModuleFromFile("assets/SPIRV/terrain-voxelizer-prepass.comp.spv", &bindings, 1, &pushConstant, 1);
        prepassPipeline = device->CreateComputePipeline(shader, false, "Terrain Voxelizer Prepass Pipeline");
        device->Destroy(shader);
    }

    {
        RD::UniformBinding bindings[] = {
            {RD::BINDING_TYPE_STORAGE_BUFFER, 0, 0},
            {RD::BINDING_TYPE_STORAGE_BUFFER, 0, 1},
        };
        ShaderID shader = RenderingUtils::CreateShaderModuleFromFile("assets/SPIRV/terrain-voxelizer.comp.spv", bindings, static_cast<uint32_t>(std::size(bindings)), &pushConstant, 1);
        mainPipeline = device->CreateComputePipeline(shader, false, "Terrain Voxelizer Main Pipeline");
        device->Destroy(shader);
    }
}

void TerrainVoxelizer::Voxelize(CommandPoolID cp, CommandBufferID cb) {
    FenceID waitFence = device->CreateFence("TempFence");
    RD::ImmediateSubmitInfo submitInfo = {
        .queue = device->GetDeviceQueue(RD::QUEUE_TYPE_GRAPHICS),
        .commandPool = cp,
        .commandBuffer = cb,
        .fence = waitFence,
    };

    {
        RD::BoundUniform prepassUniforms = {RD::BINDING_TYPE_STORAGE_BUFFER, 0, voxelCountBuffer};
        UniformSetID prepassUniformSet = device->CreateUniformSet(prepassPipeline, &prepassUniforms, 1, 0, "Temp Set");
        device->ImmediateSubmit([&](CommandBufferID commandBuffer) {
            device->BindPipeline(commandBuffer, prepassPipeline);
            device->BindUniformSet(commandBuffer, prepassPipeline, &prepassUniformSet, 1);
            device->BindPushConstants(commandBuffer, prepassPipeline, RD::SHADER_STAGE_COMPUTE, &voxelResolution, 0, sizeof(uint32_t));

            uint32_t workGroupSize = RenderingUtils::GetWorkGroupSize(voxelResolution, 8);
            device->DispatchCompute(commandBuffer, workGroupSize, workGroupSize, workGroupSize);
        },
                                &submitInfo);
        device->WaitForFence(&waitFence, 1, UINT64_MAX);
        device->Destroy(prepassUniformSet);

        this->voxelCount = countBufferPtr[0];
        LOG("Voxelization Prepass Voxel Count: " + std::to_string(this->voxelCount));
    }
    device->ResetFences(&waitFence, 1);

    // Allocate voxel fragment list buffer
    voxelFragmentBuffer = device->CreateBuffer(this->voxelCount * sizeof(uint64_t), RD::BUFFER_USAGE_STORAGE_BUFFER_BIT, RD::MEMORY_ALLOCATION_TYPE_GPU, "Voxel Fragment List Buffer");
    {
        RD::BoundUniform mainUniforms[] = {
            {RD::BINDING_TYPE_STORAGE_BUFFER, 0, voxelCountBuffer},
            {RD::BINDING_TYPE_STORAGE_BUFFER, 1, voxelFragmentBuffer},
        };

        UniformSetID mainUniformSet = device->CreateUniformSet(mainPipeline, mainUniforms, static_cast<uint32_t>(std::size(mainUniforms)), 0, "Temp Main Set");

        device->ImmediateSubmit([&](CommandBufferID commandBuffer) {
            device->BindPipeline(commandBuffer, mainPipeline);
            device->BindUniformSet(commandBuffer, mainPipeline, &mainUniformSet, 1);
            device->BindPushConstants(commandBuffer, mainPipeline, RD::SHADER_STAGE_COMPUTE, &voxelResolution, 0, sizeof(uint32_t));

            uint32_t workGroupSize = RenderingUtils::GetWorkGroupSize(voxelResolution, 8);
            device->DispatchCompute(commandBuffer, workGroupSize, workGroupSize, workGroupSize);
        },
                                &submitInfo);
        device->WaitForFence(&waitFence, 1, UINT64_MAX);
        device->Destroy(mainUniformSet);
        device->Destroy(waitFence);

        LOG("Voxelization Voxel Count: " + std::to_string(countBufferPtr[1]));
    }
}

void TerrainVoxelizer::Shutdown() {
    device->Destroy(voxelFragmentBuffer);
    device->Destroy(voxelCountBuffer);
    device->Destroy(prepassPipeline);
    device->Destroy(mainPipeline);
}
