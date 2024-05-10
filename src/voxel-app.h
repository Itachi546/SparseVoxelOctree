#pragma once

#include "app-window.h"

#include <chrono>

namespace gfx {
    class Camera;
} // namespace gfx

class ParallelOctree;
struct OctreeBrick;
struct OctreeRenderer;
struct VoxelData;

struct VoxelApp : AppWindow<VoxelApp> {
    VoxelApp();
    VoxelApp(const VoxelApp &) = delete;
    VoxelApp(const VoxelApp &&) = delete;
    VoxelApp &operator=(const VoxelApp &) = delete;
    VoxelApp &operator=(const VoxelApp &&) = delete;
    ~VoxelApp();

    void Run();

    void OnUpdate();

    void OnRenderUI();

    void OnRender();

    void OnMouseMove(float x, float y);
    void OnMouseScroll(float x, float y);
    void OnMouseButton(int key, int action);
    void OnKey(int key, int action);
    void OnResize(float width, float height);

    void UpdateControls();

    void LoadFromFile(const char *filename, float scale, uint32_t kOctreeDims);

    using Clock = std::chrono::high_resolution_clock;

    gfx::Camera *camera;
    ParallelOctree *octree;
    OctreeRenderer *octreeRenderer;

    float dt;
    float lastFrameTime;

    // Debug Variables
    bool enableRasterizer = false;
    bool show = true;

    struct FrameData {
        glm::mat4 uInvP;
        glm::mat4 uInvV;

        glm::mat4 P;
        glm::mat4 V;
        glm::mat4 VP;

        glm::vec3 uLightPosition;
        float uScreenWidth;

        glm::vec3 uCameraPosition;
        float uScreenHeight;
        float time;
    } frameData;

    glm::vec3 origin;
    glm::vec3 target;
    glm::vec3 lightPosition;

    RenderingDevice *device;
    CommandPoolID commandPool;
    CommandBufferID commandBuffer;

    BufferID globalUB;
    uint8_t *globalUBPtr;

    UniformSetID gUniSetRasterizer, gUniSetRaycast;
};
