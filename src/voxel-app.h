#pragma once

#include "app-window.h"

#include <chrono>
#include <vector>

class AsyncLoader;
struct RenderScene;
class OctreeBuilder;
class OctreeTracer;

namespace gfx {
    class Camera;
} // namespace gfx

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
    TextureID CreateSwapchainDepthAttachment();

    using Clock = std::chrono::high_resolution_clock;

    std::shared_ptr<gfx::Camera> camera;

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
    TextureID depthAttachment;
    FenceID renderFence;

    std::shared_ptr<AsyncLoader> asyncLoader;
    std::shared_ptr<RenderScene> scene;
    std::shared_ptr<OctreeBuilder> octreeBuilder;
    std::shared_ptr<OctreeTracer> octreeTracer;

    BufferID globalUB;
    uint8_t *globalUBPtr;

    int sceneMode = 2;
};
