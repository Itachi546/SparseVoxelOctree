#include "octree-raycaster.h"
#include "octree.h"
#include "gfx/camera.h"
#include "gfx/gpu-timer.h"

void OctreeRaycaster::Initialize(Octree *octree) {
    shader.Create("assets/shaders/raycast.vert", "assets/shaders/raycast.frag");

    minBound = octree->center - octree->size;
    maxBound = octree->center + octree->size;

    uint32_t nodePoolSize = static_cast<uint32_t>(octree->nodePools.size() * sizeof(Node));
    nodesBuffer = gfx::CreateBuffer(octree->nodePools.data(), nodePoolSize, GL_DYNAMIC_STORAGE_BIT);

    uint32_t brickPoolSize = static_cast<uint32_t>(octree->brickPools.size() * sizeof(uint32_t));
    brickBuffer = gfx::CreateBuffer(octree->brickPools.data(), brickPoolSize, GL_DYNAMIC_STORAGE_BIT);
}

void OctreeRaycaster::Render(gfx::Camera *camera, glm::vec3 lightPosition, glm::vec2 resolution) {
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
}

void OctreeRaycaster::Shutdown() {
    shader.Destroy();
    gfx::DestroyBuffer(nodesBuffer);
    gfx::DestroyBuffer(brickBuffer);
}
