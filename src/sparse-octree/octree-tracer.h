#pragma once

#include "rendering/rendering-device.h"

#include <memory>
#include <glm/glm.hpp>

class OctreeBuilder;

namespace gfx {
    class Camera;
}

class OctreeTracer {
  public:
    void Initialize(std::shared_ptr<OctreeBuilder> builder);

    void Trace(CommandBufferID commandBuffer, std::shared_ptr<gfx::Camera> camera);

    void Shutdown();

  private:
    PipelineID pipeline;
    UniformSetID uniformSet;
    
    std::shared_ptr<OctreeBuilder> builder;

    struct PushConstants {
        glm::mat4 invP;
        glm::mat4 invV;
        glm::mat4 invM;
        glm::vec4 camPos;
    } pushConstants;
};