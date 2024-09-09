#include "pch.h"
#include "async-loader.h"

#include "gltf-loader.h"
#include "tinygltf/stb_image.h"
#include "render-scene.h"

#include <thread>
using namespace std::chrono_literals;

void AsyncLoader::Initialize(std::shared_ptr<RenderScene> scene) {
    device = RD::GetInstance();
    execute = true;
    this->scene = scene;

    transferSubmitInfo.queue = device->GetDeviceQueue(RD::QUEUE_TYPE_TRANSFER);
    mainQueue = device->GetDeviceQueue(RD::QUEUE_TYPE_GRAPHICS);

    transferSubmitInfo.commandPool = device->CreateCommandPool(transferSubmitInfo.queue, "Async Transfer Command Pool");
    transferSubmitInfo.commandBuffer = device->CreateCommandBuffer(transferSubmitInfo.commandPool, "Async Transfer Command Buffer");
    transferSubmitInfo.fence = device->CreateFence("Async Transfer Fence");

    stagingBuffer = device->CreateBuffer(MB(64), RD::BUFFER_USAGE_TRANSFER_SRC_BIT, RD::MEMORY_ALLOCATION_TYPE_CPU, "Texture Copy Staging Buffer");
    stagingBufferPtr = device->MapBuffer(stagingBuffer);
}

void AsyncLoader::Start() {
    _thread = std::thread([&]() {
        while (execute) {
            ProcessQueue(device);
        }
    });
}

void AsyncLoader::LoadTextureSync(std::string filename, TextureID textureId, RD::ImmediateSubmitInfo *submitInfo) {
    int width, height, _unused;
    uint8_t *data = stbi_load(filename.c_str(), &width, &height, &_unused, STBI_rgb_alpha);

    RD::SamplerDescription samplerDesc = RD::SamplerDescription::Initialize();
    RD::TextureDescription desc = RD::TextureDescription::Initialize(width, height);
    desc.samplerDescription = &samplerDesc;
    desc.format = RD::FORMAT_R8G8B8A8_UNORM;
    desc.usageFlags = RD::TEXTURE_USAGE_SAMPLED_BIT | RD::TEXTURE_USAGE_TRANSFER_DST_BIT;
    LOG("Loading texture: " + filename);
    std::memcpy(stagingBufferPtr, data, width * height * 4);

    RD::BufferImageCopyRegion copyRegion = {};
    copyRegion.bufferOffset = 0;

    RD::TextureBarrier transferBarrier[] = {
        RD::TextureBarrier{
            .texture = textureId,
            .srcAccess = 0,
            .dstAccess = RD::BARRIER_ACCESS_TRANSFER_WRITE_BIT,
            .newLayout = RD::TEXTURE_LAYOUT_TRANSFER_DST_OPTIMAL,
            .srcQueueFamily = QUEUE_FAMILY_IGNORED,
            .dstQueueFamily = QUEUE_FAMILY_IGNORED,
            .baseMipLevel = 0,
            .baseArrayLayer = 0,
            .levelCount = 1,
            .layerCount = 1,
        },
        RD::TextureBarrier{
            .texture = textureId,
            .srcAccess = RD::BARRIER_ACCESS_TRANSFER_WRITE_BIT,
            .dstAccess = 0,
            .newLayout = RD::TEXTURE_LAYOUT_TRANSFER_DST_OPTIMAL,
            .srcQueueFamily = submitInfo->queue,
            .dstQueueFamily = mainQueue,
            .baseMipLevel = 0,
            .baseArrayLayer = 0,
            .levelCount = 1,
            .layerCount = 1,
        },
    };

    bool isTransferQueue = submitInfo->queue == this->transferSubmitInfo.queue;
    device->ImmediateSubmit([&](CommandBufferID cb) {
        device->PipelineBarrier(cb, RD::PIPELINE_STAGE_TOP_OF_PIPE_BIT, RD::PIPELINE_STAGE_TRANSFER_BIT, &transferBarrier[0], 1, nullptr, 0);

        device->CopyBufferToTexture(cb, stagingBuffer, textureId, &copyRegion);

        // If the current queue is main queue
        if (isTransferQueue) {
            device->PipelineBarrier(cb, RD::PIPELINE_STAGE_TRANSFER_BIT, RD::PIPELINE_STAGE_ALL_COMMANDS_BIT, &transferBarrier[1], 1, nullptr, 0);
        }
        else {
            device->GenerateMipmap(cb, textureId);
        } },
                            submitInfo);

    device->WaitForFence(&submitInfo->fence, 1, UINT64_MAX);
    device->ResetFences(&submitInfo->fence, 1);
    device->ResetCommandPool(submitInfo->commandPool);
    stbi_image_free(data);
}

void AsyncLoader::Shutdown() {
    execute = false;
    if (_thread.joinable()) {
        _thread.join();
    }

    device->Destroy(transferSubmitInfo.commandPool);
    device->Destroy(stagingBuffer);
    device->Destroy(transferSubmitInfo.fence);
}

void AsyncLoader::ProcessQueue(RD *device) {
    TextureLoadRequest request;
    if (textureLoadQueue.try_pop(&request)) {
        LoadTextureSync(request.path, request.textureId, &transferSubmitInfo);
        scene->AddTexturesToUpdate(request.textureId);
    } else {
        std::this_thread::sleep_for(200ms);
    }
}
