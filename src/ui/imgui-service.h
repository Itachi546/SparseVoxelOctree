#pragma once

#include "rendering/rendering-device.h"
#include "imgui/imgui.h"

#include <glm/glm.hpp>
#include <vector>
#include <sstream>
#include <chrono>
#include <ctime>

struct GLFWwindow;

namespace ImGuiService {
    void Initialize(GLFWwindow *window, CommandBufferID commandBuffer);

    void NewFrame();

    void Render(CommandBufferID commandBuffer);

    void Shutdown();

    inline bool IsFocused() {
        return ImGui::IsWindowFocused();
    }

}; // namespace ImGuiService
