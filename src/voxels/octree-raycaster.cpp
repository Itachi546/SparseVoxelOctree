#include "octree-raycaster.h"
#include "parallel-octree.h"
#include "gfx/camera.h"
#include "gfx/gpu-timer.h"

#include "rendering/rendering-utils.h"

void OctreeRaycaster::Initialize(uint32_t outputWidth, uint32_t outputHeight) {

    width = outputWidth;
    height = outputHeight;

    RenderingDevice *device = RD::GetInstance();

    RD::UniformBinding bindings[] = {
        RD::UniformBinding{RD::BINDING_TYPE_UNIFORM_BUFFER, 0, 0},
        RD::UniformBinding{RD::BINDING_TYPE_STORAGE_BUFFER, 1, 0},
        RD::UniformBinding{RD::BINDING_TYPE_STORAGE_BUFFER, 1, 1},
        RD::UniformBinding{RD::BINDING_TYPE_IMAGE, 1, 2},
    };
    RD::PushConstant pushConstant = {
        .offset = 0,
        .size = sizeof(float) * 8,
    };

    ShaderID shaderID = RenderingUtils::CreateShaderModuleFromFile("assets/SPIRV/raycast.comp.spv", bindings, static_cast<uint32_t>(std::size(bindings)), &pushConstant, 1);
    pipeline = device->CreateComputePipeline(shaderID, "Raycast Compute Pipeline");
    device->Destroy(shaderID);
    // shader.Create("assets/shaders/raycast.vert", "assets/shaders/raycast.frag");

    uint32_t nodePoolSize = static_cast<uint32_t>(TOTAL_MAX_NODES * sizeof(Node));
    nodesBuffer = device->CreateBuffer(nodePoolSize, RD::BUFFER_USAGE_STORAGE_BUFFER_BIT, RD::MEMORY_ALLOCATION_TYPE_CPU, "NodeBuffer");
    nodeBufferPtr = device->MapBuffer(nodesBuffer);

    uint32_t brickPoolSize = static_cast<uint32_t>(TOTAL_MAX_BRICK * sizeof(uint32_t));
    brickBuffer = device->CreateBuffer(brickPoolSize, RD::BUFFER_USAGE_STORAGE_BUFFER_BIT, RD::MEMORY_ALLOCATION_TYPE_CPU, "BrickBuffer");
    brickBufferPtr = device->MapBuffer(brickBuffer);

    RD::TextureDescription textureDescription = RD::TextureDescription::Initialize(1920, 1080);
    textureDescription.usageFlags = RD::TEXTURE_USAGE_STORAGE_BIT | RD::TEXTURE_USAGE_TRANSFER_SRC_BIT;
    outputTexture = device->CreateTexture(&textureDescription, "RaycastOutputTexture");

    outputImageBarrier.push_back({outputTexture, RD::BARRIER_ACCESS_NONE, RD::BARRIER_ACCESS_SHADER_WRITE_BIT, RD::TEXTURE_LAYOUT_GENERAL});

    RD::BoundUniform boundedUniform[3] = {
        {RD::BINDING_TYPE_STORAGE_BUFFER, 0, nodesBuffer},
        {RD::BINDING_TYPE_STORAGE_BUFFER, 1, brickBuffer},
        {RD::BINDING_TYPE_IMAGE, 2, outputTexture},
    };
    resourceSet = device->CreateUniformSet(pipeline, boundedUniform, static_cast<uint32_t>(std::size(boundedUniform)), 1, "RaycastResourceSet");
}

void OctreeRaycaster::Update(ParallelOctree *octree) {
    minBound = octree->center - octree->size;
    maxBound = octree->center + octree->size;
    assert(octree->nodePools.size() <= TOTAL_MAX_NODES);
    assert((octree->brickPools.size() / BRICK_ELEMENT_COUNT) <= TOTAL_MAX_BRICK);
    std::memcpy(nodeBufferPtr, octree->nodePools.data(), octree->nodePools.size() * sizeof(uint32_t));
    std::memcpy(brickBufferPtr, octree->brickPools.data(), octree->brickPools.size() * sizeof(uint32_t));
}

void OctreeRaycaster::Render(CommandBufferID commandBuffer, UniformSetID globalSet) {
    glm::vec4 dims[2] = {glm::vec4(minBound, 0.0f), glm::vec4(maxBound, 0.0f)};
    auto *device = RD::GetInstance();
    device->PipelineBarrier(commandBuffer, RD::PIPELINE_STAGE_TOP_OF_PIPE_BIT, RD::PIPELINE_STAGE_COMPUTE_SHADER_BIT, outputImageBarrier);
    device->BindPipeline(commandBuffer, pipeline);
    device->BindUniformSet(commandBuffer, pipeline, globalSet);
    device->BindUniformSet(commandBuffer, pipeline, resourceSet);
    device->BindPushConstants(commandBuffer, pipeline, RD::SHADER_STAGE_COMPUTE, &dims, 0, sizeof(float) * 8);
    device->DispatchCompute(commandBuffer, (width / 8) + 1, (height / 8) + 1, 1);

    /*
    // GpuProfiler::Begin("Raycast");
    glm::mat4 invP = camera->GetInvProjectionMatrix();
    glm::mat4 invV = camera->GetInvViewMatrix();
    glm::vec3 cameraPosition = camera->GetPosition();

    GpuTimer::Begin("Raycast");
    shader.Bind();
    shader.SetBuffer(0, nodesBuffer);
    shader.SetBuffer(1, brickBuffer);
    shader.SetUniformMat4("uInvP", &invP[0][0]);
    shader.SetUniformMat4("uInvV", &invV[0][0]);
    shader.SetUniformFloat3("uCamPos", &cameraPosition[0]);
    shader.SetUniformFloat3("uAABBMin", &minBound[0]);
    shader.SetUniformFloat3("uAABBMax", &maxBound[0]);
    shader.SetUniformFloat3("uLightPos", &lightPosition[0]);
    shader.SetUniformFloat2("uResolution", &resolution[0]);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    shader.Unbind();
    GpuTimer::End();
    */
}

void OctreeRaycaster::Shutdown() {
    auto *device = RD::GetInstance();
    device->Destroy(pipeline);
    device->Destroy(nodesBuffer);
    device->Destroy(outputTexture);
    device->Destroy(brickBuffer);
    device->Destroy(resourceSet);
}
