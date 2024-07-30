#include "pch.h"
#include "octree-renderer.h"

#include "ui/imgui-service.h"

OctreeRenderer::OctreeRenderer() {
    raycaster = new OctreeRaycaster();
}

void OctreeRenderer::Initialize(uint32_t width, uint32_t height, ParallelOctree *octree) {
    raycaster->Initialize(width, height, octree);
}

void OctreeRenderer::AddUI() {
}

void OctreeRenderer::Render(CommandBufferID commandBuffer, UniformSetID globalSet) {
    raycaster->Render(commandBuffer, globalSet);
}

void OctreeRenderer::Shutdown() {

    raycaster->Shutdown();
    delete raycaster;
}