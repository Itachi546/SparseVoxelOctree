#pragma once

#include "gfx/opengl.h"
#include <GLFW/glfw3.h>

#include "math-utils.h"

#include <iostream>
#include <string>

template <typename App>
struct AppWindow {

    AppWindow(const char *title, const glm::vec2 &windowSize) : windowSize(windowSize) {
        glfwInit();

        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

        glfwWindowPtr = glfwCreateWindow(static_cast<int>(windowSize.x), static_cast<int>(windowSize.y), title, nullptr, nullptr);
        glfwSetWindowUserPointer(glfwWindowPtr, this);
        glfwMakeContextCurrent(glfwWindowPtr);
        // glfwSwapInterval(1);

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

        gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);

        const char *rendererInfo = reinterpret_cast<const char *>(glGetString(GL_RENDERER));
        const char *vendorInfo = reinterpret_cast<const char *>(glGetString(GL_VENDOR));
        const char *openglVersion = reinterpret_cast<const char *>(glGetString(GL_VERSION));

        std::cout << "Renderer: " << std::string(rendererInfo) << std::endl;
        std::cout << "Vendor: " << std::string(vendorInfo) << std::endl;
        std::cout << "OpenGL Version: " << std::string(openglVersion) << std::endl;

        glEnable(GL_DEPTH_TEST);
        glClearColor(0.5f, 0.5f, 0.5f, 1.0f);
    }

    virtual ~AppWindow() {
        glfwDestroyWindow(glfwWindowPtr);
        glfwTerminate();
    }

    GLFWwindow *glfwWindowPtr;
    glm::vec2 windowSize;
    bool minimized = false;
};
