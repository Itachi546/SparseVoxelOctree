#include "voxel-app.h"
#include "utils.h"

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

    RenderingDevice::WindowPlatformData platformData;
    platformData.windowPtr = glfwGetWin32Window(window);
#if VULKAN_ENABLED
    RenderingDevice::GetInstance() = new VulkanRenderingDevice();
#endif
    RenderingDevice *device = RenderingDevice::GetInstance();
    device->SetValidationMode(true);
    device->Initialize();
    device->CreateSurface(&platformData);

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        glfwSwapBuffers(window);
    }

    device->Shutdown();
}