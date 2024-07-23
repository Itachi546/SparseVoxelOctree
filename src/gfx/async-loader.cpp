#include "pch.h"
#include "async-loader.h"
#include "stb_image.h"

#include <thread>
using namespace std::chrono_literals;

void AsyncLoader::Initialize() {
    device = RD::GetInstance();
    execute = true;

    submitQueueInfo.queue = device->GetDeviceQueue(RD::QUEUE_TYPE_TRANSFER);
    submitQueueInfo.commandPool = device->CreateCommandPool(submitQueueInfo.queue, "Async Transfer Command Pool");
    submitQueueInfo.commandBuffer = device->CreateCommandBuffer(submitQueueInfo.commandPool, "Async Transfer Command Buffer");
    submitQueueInfo.fence = device->CreateFence("Async Transfer Fence");

    stagingBuffer = device->CreateBuffer(64 * 1024 * 1024, RD::BUFFER_USAGE_TRANSFER_SRC_BIT, RD::MEMORY_ALLOCATION_TYPE_CPU, "Texture Copy Staging Buffer");
    stagingBufferPtr = device->MapBuffer(stagingBuffer);
}

void AsyncLoader::Start() {
    _thread = std::thread([&]() {
        while (execute) {
            ProcessQueue(device);
        }
    });
}

void AsyncLoader::Shutdown() {
    execute = false;
    if (_thread.joinable()) {
        _thread.join();
    }
    device->Destroy(submitQueueInfo.commandPool);
    device->Destroy(stagingBuffer);
    device->Destroy(submitQueueInfo.fence);
}

void AsyncLoader::ProcessQueue(RD *device) {
    TextureLoadRequest request;
    if (textureLoadQueue.try_pop(&request)) {
        int width, height, _unused;
        uint8_t *data = stbi_load(request.path.c_str(), &width, &height, &_unused, STBI_rgb_alpha);

        RD::TextureDescription desc = RD::TextureDescription::Initialize(width, height);
        desc.format = RD::FORMAT_R8G8B8A8_UNORM;
        desc.usageFlags = RD::TEXTURE_USAGE_SAMPLED_BIT | RD::TEXTURE_USAGE_TRANSFER_DST_BIT;
        std::cout << "Loading texture: " << request.path << std::endl;
        std::memcpy(stagingBufferPtr, data, width * height * 4);

        RD::BufferImageCopyRegion copyRegion = {};
        copyRegion.bufferOffset = 0;

        std::vector<RD::TextureBarrier> transferBarrier = {
            RD::TextureBarrier{request.textureId, 0, RD::BARRIER_ACCESS_TRANSFER_WRITE_BIT, RD::TEXTURE_LAYOUT_TRANSFER_DST_OPTIMAL}};

        device->ImmediateSubmit([&](CommandBufferID cb) {
            device->PipelineBarrier(cb, RD::PIPELINE_STAGE_TOP_OF_PIPE_BIT, RD::PIPELINE_STAGE_TRANSFER_BIT, transferBarrier);

            device->CopyBufferToTexture(cb, stagingBuffer, request.textureId, &copyRegion);
        },
                                &submitQueueInfo);
        stbi_image_free(data);
        device->Destroy(request.textureId);
    } else {
        std::this_thread::sleep_for(200ms);
    }
}
