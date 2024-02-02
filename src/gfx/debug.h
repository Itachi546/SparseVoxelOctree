#pragma once

#include "opengl.h"
#include "math-utils.h"

struct Line {
    glm::vec3 p0;
    glm::vec3 c0;

    glm::vec3 p1;
    glm::vec3 c1;
};

namespace Debug {
    void Initialize();

    void AddLine(const glm::vec3 &p0, const glm::vec3 &p1, const glm::vec3 &color = {1.0f, 0.0f, 1.0f});

    void AddRect(const glm::vec3 &min, const glm::vec3 &max, const glm::vec3 &color = {1.0f, 0.0f, 1.0f});

    void Render(glm::mat4 VP);

    void Shutdown();
} // namespace DebugDraw