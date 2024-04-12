#include "octree-renderer.h"
#include "rendering/rendering-utils.h"
#include "parallel-octree.h"
#include "gfx/mesh.h"

OctreeRenderer::OctreeRenderer(uint32_t width, uint32_t height) : width(width), height(height) {
    device = RD::GetInstance();
}

void OctreeRenderer::SetupRasterizer(ParallelOctree *octree) {
    RD::UniformBinding bindings[] = {
        RD::UniformBinding{RD::BINDING_TYPE_UNIFORM_BUFFER, 0, 0},
        RD::UniformBinding{RD::BINDING_TYPE_STORAGE_BUFFER, 1, 0},
        RD::UniformBinding{RD::BINDING_TYPE_STORAGE_BUFFER, 1, 1},
    };

    ShaderID shaders[2] = {
        RenderingUtils::CreateShaderModuleFromFile("assets/SPIRV/instanced-voxel.vert.spv",
                                                   bindings,
                                                   static_cast<uint32_t>(std::size(bindings)),
                                                   nullptr, 0),
        RenderingUtils::CreateShaderModuleFromFile("assets/SPIRV/instanced-voxel.frag.spv",
                                                   nullptr, 0,
                                                   nullptr, 0),
    };

    RD::Format colorAttachmentFormats[] = {RD::FORMAT_R8G8B8A8_UNORM};
    RD::BlendState blendState = RD::BlendState::Create();

    RD::RasterizationState rs = RD::RasterizationState::Create();
    RD::DepthState ds = RD::DepthState::Create();
    ds.enableDepthTest = true;
    ds.enableDepthWrite = true;
    voxelRasterPipeline = device->CreateGraphicsPipeline(shaders, static_cast<uint32_t>(std::size(shaders)),
                                                         RD::Topology::TOPOLOGY_TRIANGLE_LIST,
                                                         &rs,
                                                         &ds,
                                                         colorAttachmentFormats,
                                                         &blendState,
                                                         static_cast<uint32_t>(std::size(colorAttachmentFormats)),
                                                         RD::FORMAT_D32_SFLOAT,
                                                         "Voxel Rasterization Pipeline");

    device->Destroy(shaders[0]);
    device->Destroy(shaders[1]);

    std::vector<glm::vec4> voxels;
    octree->ListVoxels(voxels);
    numVoxels = static_cast<uint32_t>(voxels.size());

    uint32_t bufferSize = static_cast<uint32_t>(voxels.size() * sizeof(glm::vec4));
    instanceDataBuffer = device->CreateBuffer(bufferSize, RD::BUFFER_USAGE_STORAGE_BUFFER_BIT, RD::MEMORY_ALLOCATION_TYPE_CPU, "InstancedDataBuffer");
    void *bufferPtr = device->MapBuffer(instanceDataBuffer);
    std::memcpy(bufferPtr, voxels.data(), bufferSize);

    gfx::Mesh::CreateCube(&voxelMesh);

    RD::BoundUniform boundedUniform[] = {
        {RD::BINDING_TYPE_STORAGE_BUFFER, 0, voxelMesh.vertexBuffer},
        {RD::BINDING_TYPE_STORAGE_BUFFER, 1, instanceDataBuffer},
    };
    instancedUniformSet = device->CreateUniformSet(voxelRasterPipeline, boundedUniform, static_cast<uint32_t>(std::size(boundedUniform)), 1, "InstancedVoxelDataSet");
}

void OctreeRenderer::Initialize(ParallelOctree *octree) {
    RD::TextureDescription desc = RD::TextureDescription::Initialize(width, height);
    desc.usageFlags = RD::TEXTURE_USAGE_COLOR_ATTACHMENT_BIT | RD::TEXTURE_USAGE_TRANSFER_SRC_BIT;
    colorAttachment = device->CreateTexture(&desc, "MainColorAttachment");

    desc.usageFlags = RD::TEXTURE_USAGE_DEPTH_ATTACHMENT_BIT;
    desc.format = RD::FORMAT_D32_SFLOAT;
    depthAttachment = device->CreateTexture(&desc, "Voxel Raster Depth Attachment");

    SetupRasterizer(octree);
}

void OctreeRenderer::RasterizeVoxel(CommandBufferID commandBuffer, UniformSetID globalSet) {
    /*
    std::vector<RD::TextureBarrier> imageBarrier;
    imageBarrier.push_back(RD::TextureBarrier{
        colorAttachment,
        RD::BARRIER_ACCESS_TRANSFER_READ_BIT,
        RD::BARRIER_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        RD::TEXTURE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    });
    device->PipelineBarrier(commandBuffer, RD::PIPELINE_STAGE_TRANSFER_BIT, RD::PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, imageBarrier);
    */
    RD::AttachmentInfo colorAttachmentInfos = {
        .loadOp = RD::LOAD_OP_CLEAR,
        .storeOp = RD::STORE_OP_STORE,
        .clearColor = {0.5f, 0.5f, 0.5f, 1.0f},
        .clearDepth = 0,
        .clearStencil = 0,
        .attachment = colorAttachment,
    };

    RD::AttachmentInfo depthAttachmentInfo = {
        .loadOp = RD::LOAD_OP_CLEAR,
        .storeOp = RD::STORE_OP_STORE,
        .clearDepth = 1.0f,
        .clearStencil = 0,
        .attachment = depthAttachment,
    };

    RD::RenderingInfo renderingInfo = {
        .width = width,
        .height = height,
        .layerCount = 1,
        .colorAttachmentCount = 1,
        .pColorAttachments = &colorAttachmentInfos,
        .pDepthStencilAttachment = &depthAttachmentInfo,
    };

    device->BeginRenderPass(commandBuffer, &renderingInfo);
    device->SetViewport(commandBuffer, 0, 0, width, height);
    device->SetScissor(commandBuffer, 0, 0, width, height);

    device->BindPipeline(commandBuffer, voxelRasterPipeline);
    device->BindUniformSet(commandBuffer, voxelRasterPipeline, globalSet);
    device->BindUniformSet(commandBuffer, voxelRasterPipeline, instancedUniformSet);

    device->BindIndexBuffer(commandBuffer, voxelMesh.indexBuffer);
    device->DrawElementInstanced(commandBuffer, voxelMesh.numVertices, numVoxels);

    device->EndRenderPass(commandBuffer);
}

void OctreeRenderer::Render(CommandBufferID commandBuffer, UniformSetID globalSet) {
    RasterizeVoxel(commandBuffer, globalSet);
}

void OctreeRenderer::Shutdown() {
    device->Destroy(voxelRasterPipeline);
    device->Destroy(colorAttachment);
}