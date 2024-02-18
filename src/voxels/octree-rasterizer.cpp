#include "octree-rasterizer.h"

#include "octree.h"
#include "gfx/mesh.h"
#include "gfx/camera.h"
#include "gfx/gpu-timer.h"

#include <vector>

void OctreeRasterizer::Initialize(Octree *octree) {
    shader.Create("assets/shaders/instanced.vert",
                  "assets/shaders/instanced.frag");

    cubeMesh = new gfx::Mesh();
    gfx::Mesh::CreateCube(cubeMesh);

    std::vector<glm::vec4> voxels;
    octree->ListVoxels(voxels);
    voxelCount = static_cast<uint32_t>(voxels.size());

    instanceBuffer = gfx::CreateBuffer(nullptr, voxelCount * sizeof(glm::vec4), GL_DYNAMIC_STORAGE_BIT);
    if (voxels.size() > 0)
        glNamedBufferSubData(instanceBuffer, 0, sizeof(glm::vec4) * voxels.size(), voxels.data());
}

void OctreeRasterizer::Render(gfx::Camera *camera) {
    if (voxelCount > 0) {
        GpuTimer::Begin("Rasterizer");
        glm::mat4 VP = camera->GetViewProjectionMatrix();
        shader.Bind();
        shader.SetUniformMat4("uVP", &VP[0][0]);
        shader.SetBuffer(0, instanceBuffer);
        cubeMesh->DrawInstanced(voxelCount);
        shader.Unbind();
        GpuTimer::End();
    }
}

void OctreeRasterizer::Shutdown() {
    shader.Destroy();
    gfx::DestroyBuffer(instanceBuffer);
    delete cubeMesh;
}
