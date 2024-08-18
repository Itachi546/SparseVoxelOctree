#pragma once

#include "rendering/rendering-device.h"
#include "math-utils.h"

struct Line {
    glm::vec3 p0;
    uint32_t c0;
    glm::vec3 p1;
    uint32_t c1;
};

namespace Debug {
    void Initialize();

    void NewFrame();

    void AddLine(const glm::vec3 &p0, const glm::vec3 &p1, uint32_t color = 0xffffffff);

    void AddRect(const glm::vec3 &min, const glm::vec3 &max, uint32_t color = 0xffffffff);

    void Render(CommandBufferID commandBuffer, glm::mat4 VP);

    void Shutdown();
} // namespace Debug