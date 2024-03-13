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

    ShaderID shaders[2];

    RD::ShaderBinding uniforms[] = {
        {RD::UNIFORM_TYPE_UNIFORM_BUFFER, 0},
        {RD::UNIFORM_TYPE_STORAGE_BUFFER, 1},
        {RD::UNIFORM_TYPE_STORAGE_BUFFER, 2},
    };

    RD::PushConstant pushConstant = {
        .offset = 0,
        .size = sizeof(float) * 8,
    };

    shaders[0] = RenderingUtils::CreateShaderModuleFromFile("assets/SPIRV/raycast.vert.spv", nullptr, 0, nullptr, 0);
    shaders[1] = RenderingUtils::CreateShaderModuleFromFile("assets/SPIRV/raycast.frag.spv", uniforms, (uint32_t)std::size(uniforms), &pushConstant, 1);

    RD::RasterizationState rs = RD::RasterizationState::Create();
    RD::DepthState ds = RD::DepthState::Create();
    RD::BlendState bs = RD::BlendState::Create();
    RD::Format colorAttachments = RD::Format::FORMAT_R8G8B8A8_UNORM;

    PipelineID pipeline = device->CreateGraphicsPipeline(shaders,
                                                         static_cast<uint32_t>(std::size(shaders)),
                                                         RD::TOPOLOGY_TRIANGLE_LIST,
                                                         &rs,
                                                         &ds,
                                                         &colorAttachments,
                                                         &bs,
                                                         1,
                                                         RD::FORMAT_UNDEFINED,
                                                         "MainPipeline");

    for (auto &shader : shaders)
        device->Destroy(shader);

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        glfwSwapBuffers(window);
    }
    device->Destroy(pipeline);
    device->Destroy(commandPool);
    device->Shutdown();
}