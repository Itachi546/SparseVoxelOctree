#pragma once

#include <vector>
#include <memory>
#include <string>

#include "rendering/rendering-device.h"

class AsyncLoader;

struct RenderScene {

    virtual bool Initialize(const std::vector<std::string>& filenames, std::shared_ptr<AsyncLoader> asyncLoader, BufferID globalUB) = 0;

    virtual void PrepareDraws() = 0;

    virtual void Render(CommandBufferID commandBuffer) = 0;

    virtual void Shutdown() = 0;

    virtual ~RenderScene() {}
};