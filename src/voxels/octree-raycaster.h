#pragma once

#include "gfx/opengl.h"
#include <glm/glm.hpp>

struct Octree;

namespace gfx {
    class Camera;
};

struct OctreeRaycaster {

    OctreeRaycaster() = default;

    void Initialize(Octree *octree);

    void Render(gfx::Camera *camera, glm::vec3 lightPosition, glm::vec2 resolution);

    void Shutdown();

    virtual ~OctreeRaycaster() = default;

    gfx::Shader shader;
    gfx::Buffer nodesBuffer;
    gfx::Buffer brickBuffer;

    glm::vec3 minBound;
    glm::vec3 maxBound;
};