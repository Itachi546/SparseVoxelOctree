#pragma once

#include "app-window.h"

#include <chrono>

namespace gfx {
    struct Mesh;
    class Camera;
} // namespace gfx

struct Octree;
struct OctreeBrick;
struct DensityGenerator;

struct VoxelApp : AppWindow<VoxelApp> {
    VoxelApp();
    VoxelApp(const VoxelApp &) = delete;
    VoxelApp(const VoxelApp &&) = delete;
    VoxelApp &operator=(const VoxelApp &) = delete;
    VoxelApp &operator=(const VoxelApp &&) = delete;
    ~VoxelApp();

    void Run();

    void OnUpdate();

    void OnRender();

    void OnMouseMove(float x, float y);
    void OnMouseScroll(float x, float y);
    void OnMouseButton(int key, int action);
    void OnKey(int key, int action);
    void OnResize(float width, float height);

    void UpdateControls();

    void ProcessLoadList();

    using Clock = std::chrono::high_resolution_clock;

    gfx::Shader fullscreenShader;
    gfx::Mesh *cubeMesh;
    gfx::Buffer instanceBuffer;

    gfx::Camera *camera;
    Octree *octree;
    DensityGenerator *generator;

    bool mouseDown;
    glm::vec2 mouseDelta;
    glm::vec2 mousePos;

    float dt;
    float lastFrameTime;

    uint32_t voxelCount;
    std::vector<glm::vec3> loadList;
    OctreeBrick *tempBrick;
};
