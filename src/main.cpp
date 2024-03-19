#include "voxel-app.h"
#include "utils.h"

#include "rendering/rendering-utils.h"
#include "voxels/voxel-data.h"

#ifdef VULKAN_ENABLED
#include "rendering/vulkan-rendering-device.h"
#endif

#include <GLFW/glfw3.h>

#ifdef _WIN32
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#endif

int main() {
    if (!glfwInit()) {
        std::cout << "Failed to initialize GLFW" << std::endl;
        return -1;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow *window = glfwCreateWindow(1920, 1080, "Hello World", 0, 0);
    if (window == nullptr) {
        std::cout << "Failed to create GLFW window" << std::endl;
    }

    RD::WindowPlatformData platformData;
    platformData.windowPtr = glfwGetWin32Window(window);
#if VULKAN_ENABLED
    RD::GetInstance() = new VulkanRenderingDevice();
#endif
    RenderingDevice *device = RenderingDevice::GetInstance();
    device->SetValidationMode(true);
    device->Initialize();
    device->CreateSurface(&platformData);
    device->CreateSwapchain(&platformData, true);

    CommandPoolID commandPool = device->CreateCommandPool();
    CommandBufferID commandBuffer = device->CreateCommandBuffer(commandPool);

    RD::ShaderBinding bindings[] = {
        {RD::BINDING_TYPE_IMAGE, 0},
    };

    ShaderID compShader = RenderingUtils::CreateShaderModuleFromFile("assets/SPIRV/raycast.comp.spv",
                                                                     bindings, static_cast<uint32_t>(std::size(bindings)),
                                                                     nullptr, 0);

    PipelineID pipeline = device->CreateComputePipeline(compShader, "RayCast Compute");
    device->Destroy(compShader);

    RD::TextureDescription textureDescription = RD::TextureDescription::Initialize(1920, 1080);
    textureDescription.usageFlags = RD::TEXTURE_USAGE_INPUT_ATTACHMENT_BIT | RD::TEXTURE_USAGE_TRANSFER_SRC_BIT;
    TextureID texture = device->CreateTexture(&textureDescription);

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        glfwSwapBuffers(window);
    }
    device->Destroy(texture);
    device->Destroy(pipeline);
    device->Destroy(commandPool);
    device->Shutdown();
}