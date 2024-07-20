#pragma once

#include "rendering/rendering-device.h"

#include <memory>

struct Scene;

class Voxelizer {

    void Initialize(std::shared_ptr<Scene> scene);

    void Shutdown();

  private:
};