#pragma once

#include "rendering/rendering-device.h"

#include <thread>
#include <queue>

struct TextureLoadRequest {
    std::string path;
    TextureID textureId;
};

struct BufferUploadRequest {
    void *data;
    union {
        TextureID textureId;
        BufferID bufferId;
    } resource;
};

class AsyncLoader {
  public:
    void Initialize();

    void Start();

    void RequestTextureLoad(std::string filename, TextureID textureId) {
        textureLoadQueue.emplace(TextureLoadRequest{filename, textureId});
    }

    void Shutdown();

  private:
    bool execute;
    std::thread _thread;

    std::queue<TextureLoadRequest> textureLoadQueue;

    BufferID stagingBuffer;
    CommandBufferID commandBuffer;
    CommandPoolID commandPool;
    RD *device;
    void ProcessQueue(RD *device);
};