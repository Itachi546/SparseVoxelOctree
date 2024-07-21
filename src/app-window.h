#pragma once

#include "gfx/opengl.h"
#include <GLFW/glfw3.h>
#ifdef _WIN32
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#endif

#include "math-utils.h"
#include "rendering/rendering-device.h"
#ifdef VULKAN_ENABLED
#include "rendering/vulkan-rendering-device.h"
#endif

#include <iostream>
#include <string>

template <typename App>
struct AppWindow {

    AppWindow(const char *title, const glm::vec2 &windowSize) : windowSize(windowSize) {
        if (!glfwInit()) {
            LOGE("Failed to initialize GLFW");
            return;
        }

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowPtr = glfwCreateWindow(static_cast<int>(windowSize.x), static_cast<int>(windowSize.y), title, nullptr, nullptr);
        glfwSetWindowUserPointer(glfwWindowPtr, this);

        glfwSetCursorPosCallback(glfwWindowPtr, [](GLFWwindow *window, double x, double y) {
            auto &app = *reinterpret_cast<App *>(glfwGetWindowUserPointer(window));
            app.OnMouseMove(float(x), float(y));
        });
        glfwSetScrollCallback(
            glfwWindowPtr,
            [](GLFWwindow *window, double x, double y) {
                auto &app = *reinterpret_cast<App *>(glfwGetWindowUserPointer(window));
                app.OnMouseScroll(static_cast<float>(x), static_cast<float>(y));
            });
        glfwSetMouseButtonCallback(
            glfwWindowPtr,
            [](GLFWwindow *window, int key, int action, int mods) {
                auto &app = *reinterpret_cast<App *>(glfwGetWindowUserPointer(window));
                app.OnMouseButton(key, action);
            });
        glfwSetKeyCallback(
            glfwWindowPtr,
            [](GLFWwindow *window, int key, int, int action, int) {
                auto &app = *reinterpret_cast<App *>(glfwGetWindowUserPointer(window));
                app.OnKey(key, action);
            });
        glfwSetWindowSizeCallback(
            glfwWindowPtr,
            [](GLFWwindow *window, int sx, int sy) {
                auto &app = *reinterpret_cast<App *>(glfwGetWindowUserPointer(window));
                app.OnResize(static_cast<float>(sx), static_cast<float>(sy));
            });

        RD::WindowPlatformData platformData;
        platformData.windowPtr = glfwGetWin32Window(glfwWindowPtr);
#if VULKAN_ENABLED
        RD::GetInstance() = new VulkanRenderingDevice();
#endif
        RenderingDevice *device = RenderingDevice::GetInstance();
        device->SetValidationMode(true);
        device->Initialize();
        device->CreateSurface(&platformData);
        device->CreateSwapchain(true);
    }

    virtual ~AppWindow() {
        RenderingDevice::GetInstance()->Shutdown();
        glfwDestroyWindow(glfwWindowPtr);
        glfwTerminate();
    }

    GLFWwindow *glfwWindowPtr;
    glm::vec2 windowSize;
    bool minimized = false;
};
