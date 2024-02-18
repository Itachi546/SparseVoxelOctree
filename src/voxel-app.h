#pragma once

#include "app-window.h"

#include <chrono>

namespace gfx {
    class Camera;
} // namespace gfx

struct Octree;
struct OctreeBrick;
struct OctreeRaycaster;
struct OctreeRasterizer;

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

    void LoadFromFile(const char *filename, float scale);

    using Clock = std::chrono::high_resolution_clock;

    gfx::Camera *camera;
    Octree *octree;
    OctreeRaycaster *raycaster;
    OctreeRasterizer *rasterizer;

    float dt;
    float lastFrameTime;

    // Debug Variables
    bool enableRasterizer = false;
};
