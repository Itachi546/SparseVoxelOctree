#include "voxel-app.h"

#include <glm/glm.hpp>
#include <thread>

using namespace std::chrono_literals;

static void GLAPIENTRY
MessageCallback(GLenum source,
                GLenum type,
                GLuint id,
                GLenum severity,
                GLsizei length,
                const GLchar *message,
                const void *userParam) {
    if (type != GL_DEBUG_TYPE_ERROR)
        return;
    fprintf(stderr, "GL CALLBACK: %s type = 0x%x, severity = 0x%x, message = %s\n",
            (type == GL_DEBUG_TYPE_ERROR ? "** GL ERROR **" : ""),
            type, severity, message);
}

VoxelApp::VoxelApp() : AppWindow("Voxel Application", glm::vec2{1360.0f, 769.0f}) {
    glEnable(GL_DEBUG_OUTPUT);
    glDebugMessageCallback(MessageCallback, 0);
    
    fullscreenShader = gfx::CreateGraphicsShader("../../assets/shaders/fullscreen.vert",
                                                 "../../assets/shaders/fullscreen.frag");
}

void VoxelApp::Run() {
    while (!glfwWindowShouldClose(AppWindow::glfwWindowPtr)) {
        glfwPollEvents();
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glViewport(0, 0, static_cast<int>(windowSize.x), static_cast<int>(windowSize.y));

        if (!AppWindow::minimized) {
            OnUpdate();
            OnRender();
        } else
            std::this_thread::sleep_for(1ms);

        glfwSwapBuffers(glfwWindowPtr);
    }
}

void VoxelApp::OnUpdate() {
}

void VoxelApp::OnRender() {
}

void VoxelApp::OnMouseMove(float x, float y) {
}

void VoxelApp::OnMouseScroll(float x, float y) {}
void VoxelApp::OnMouseButton(int key, int action) {}
void VoxelApp::OnKey(int key, int action) {}
void VoxelApp::OnResize(float width, float height) {
    minimized = width == 0 || height == 0;
    bool resized = width != windowSize.x || height != windowSize.y;
    if (!minimized && resized) {
        windowSize.x = width;
        windowSize.y = height;
    }
}

VoxelApp::~VoxelApp() {
}