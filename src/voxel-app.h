#include "app-window.h"

#include <chrono>

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

    using Clock = std::chrono::high_resolution_clock;

    gfx::Shader fullscreenShader;
};