#include "pch.h"

#include "octree-builder.h"

#include "voxelizer.h"
#include "gfx/render-scene.h"

void OctreeBuilder::Initialize(std::shared_ptr<RenderScene> scene) {

    this->scene = scene;
    voxelizer = std::make_shared<Voxelizer>();
    voxelizer->Initialize(scene, kDims);
}

void OctreeBuilder::Build(CommandPoolID commandPool, CommandBufferID commandBuffer) {

    voxelizer->Voxelize(commandPool, commandBuffer);
}

void OctreeBuilder::Debug(CommandBufferID commandBuffer, const gfx::Camera *camera) {
    voxelizer->RayMarch(commandBuffer, camera);
}

void OctreeBuilder::Shutdown() {
    voxelizer->Shutdown();
}
