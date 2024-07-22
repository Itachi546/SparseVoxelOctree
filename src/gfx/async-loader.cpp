#include "pch.h"
#include "async-loader.h"
#include "stb_image.h"

#include <thread>
using namespace std::chrono_literals;

void AsyncLoader::Initialize() {
    device = RD::GetInstance();
    execute = true;

    QueueID transferQueue = device->GetDeviceQueue(RD::QUEUE_TYPE_TRANSFER);
    commandPool = device->CreateCommandPool(transferQueue, "Async Transfer Command Pool");
    commandBuffer = device->CreateCommandBuffer(commandPool, "Async Transfer Command Buffer");
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
    device->Destroy(commandPool);
}

void AsyncLoader::ProcessQueue(RD *device) {
    if (textureLoadQueue.size() == 0)
        std::this_thread::sleep_for(200ms);
    else {
        TextureLoadRequest request = textureLoadQueue.front();
        textureLoadQueue.pop();

        int width, height, comp;
        uint8_t *data = stbi_load(request.path.c_str(), &width, &height, &comp, 4);
        // ASSERT(comp == 4, "Invalid number of component");
        RD::TextureDescription desc = RD::TextureDescription::Initialize(width, height);
        if (comp == 3)
            desc.format = RD::FORMAT_R8G8B8_UNORM;
        else if (comp == 4)
            desc.format = RD::FORMAT_R8G8B8A8_UNORM;
        else
            ASSERT(0, "Unsupported no of image channel");
        desc.usageFlags = RD::TEXTURE_USAGE_SAMPLED_BIT | RD::TEXTURE_USAGE_TRANSFER_DST_BIT;

        std::string name = std::filesystem::path(request.path).filename().string();
        // @TODO need mutex
        std::cout << "Loading texture: " << name << ": path:" << request.path << std::endl;
        /* device->CreateTexture(&desc, request.path);
        device->ImmediateSubmit([](CommandBufferID cb) {

        });*/

        stbi_image_free(data);
    }
}
