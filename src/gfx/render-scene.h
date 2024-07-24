#pragma once

#include <vector>
#include <memory>
#include <string>

class AsyncLoader;

struct RenderScene {

    virtual bool Initialize(const std::vector<std::string>& filenames, std::shared_ptr<AsyncLoader> asyncLoader) = 0;

    virtual void PrepareDraws() = 0;

    virtual void Render() = 0;

    virtual void Shutdown() = 0;

    virtual ~RenderScene() {}
};