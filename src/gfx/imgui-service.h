#pragma once

#include "opengl.h"
#include <glm/glm.hpp>

#include <imgui.h>

#include <vector>
#include <sstream>
#include <chrono>
#include <ctime>

struct GLFWwindow;

namespace ImGuiService {
    void Initialize(GLFWwindow *window);

    void NewFrame();

    void Render();

    void Shutdown();

    inline bool IsFocused() {
        return ImGui::IsWindowFocused();
    }

}; // namespace ImGuiService
