#include "octree-renderer.h"

#include "octree-rasterizer.h"
#include "gfx/imgui-service.h"

OctreeRenderer::OctreeRenderer() {
    rasterizer = new OctreeRasterizer();
    raycaster = new OctreeRaycaster();
}

void OctreeRenderer::Initialize(uint32_t width, uint32_t height) {
    rasterizer->Initialize(width, height);
    raycaster->Initialize(width, height);
}

void OctreeRenderer::Update(ParallelOctree *octree, gfx::Camera *camera) {
    if (renderMode == RenderMode_Rasterizer)
        rasterizer->Update(octree, camera);
    else
        raycaster->Update(octree);
}

void OctreeRenderer::AddUI() {
    if (renderMode == RenderMode_Rasterizer)
        ImGui::Text("Total Voxel: %d", rasterizer->numVoxels);

    static int currentItem = 0;
    if (ImGui::Combo("RenderMode", &currentItem, "Rasterizer\0Raycaster")) {
        renderMode = OctreeRenderMode(currentItem);
    }
}

void OctreeRenderer::Render(CommandBufferID commandBuffer, UniformSetID globalSet) {
    if (renderMode == RenderMode_Rasterizer)
        rasterizer->Render(commandBuffer, globalSet);
    else if (renderMode == RenderMode_Raycaster)
        raycaster->Render(commandBuffer, globalSet);
}

void OctreeRenderer::Shutdown() {
    rasterizer->Shutdown();
    delete rasterizer;

    raycaster->Shutdown();
    delete raycaster;
}