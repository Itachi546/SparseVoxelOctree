#pragma once

#include "rendering/rendering-device.h"
#include "core/thread-safe-queue.h"

#include <thread>

struct TextureLoadRequest {
    std::string path;
    TextureID textureId;
};

struct RenderScene;

struct BufferUploadRequest {
    void *data;
    BufferID bufferId;
    uint32_t size;
};

class AsyncLoader {
  public:
    void Initialize(std::shared_ptr<RenderScene> scene);

    void Start();

    void RequestTextureLoad(std::string filename, TextureID textureId) {
        textureLoadQueue.push(TextureLoadRequest{filename, textureId});
    }

    void Shutdown();

  private:
    bool execute;
    std::thread _thread;

    QueueID mainQueue;
    ThreadSafeQueue<TextureLoadRequest> textureLoadQueue;

    BufferID stagingBuffer;
    uint8_t *stagingBufferPtr;
    RD::ImmediateSubmitInfo submitQueueInfo;
    RD *device;
    void ProcessQueue(RD *device);

    std::shared_ptr<RenderScene> scene;
};