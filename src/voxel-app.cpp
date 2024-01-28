#include "voxel-app.h"

#include <glm/glm.hpp>
#include <thread>

using namespace std::chrono_literals;

VoxelApp::VoxelApp() : AppWindow("Voxel Application", glm::vec2{1360.0f, 769.0f}) {
}

void VoxelApp::Run() {
    while (!glfwWindowShouldClose(AppWindow::glfwWindowPtr)) {
        glfwPollEvents();

        if (!AppWindow::minimized) {
            OnUpdate();
        } else
            std::this_thread::sleep_for(1ms);
    }
}

void VoxelApp::OnUpdate() {
}

void VoxelApp::OnMouseMove(float x, float y) {
}

void VoxelApp::OnMouseScroll(float x, float y) {}
void VoxelApp::OnMouseButton(int key, int action) {}
void VoxelApp::OnKey(int key, int action) {}
void VoxelApp::OnResize(float width, float height) {
    if (width == 0 || height == 0)
        minimized = true;
    bool resized = width != windowSize.x || height != windowSize.y;
    if (!minimized && resized) {
        windowSize.x = width;
        windowSize.y = height;
    }
}

VoxelApp::~VoxelApp() {
}