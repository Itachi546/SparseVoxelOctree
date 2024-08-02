#pragma once

#include "rendering/rendering-device.h"

#include <memory>

class Scene;

class Voxelizer {

    void Initialize(std::shared_ptr<Scene> scene);

    void Voxelize();

    void Shutdown();

  private:
    std::shared_ptr<Scene> scene;
    PipelineID pipeline;
    RD *device;
    BufferID voxelListBuffer;
};