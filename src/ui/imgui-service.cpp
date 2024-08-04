#include "pch.h"
#include "imgui-service.h"

#include "imgui/imgui_internal.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_vulkan.h"

#include "rendering/vulkan-rendering-device.h"
#include <GLFW/glfw3.h>

namespace ImGuiService {

    void Initialize(GLFWwindow *window, CommandBufferID commandBuffer) {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();

        ImGuiIO &io = ImGui::GetIO();
        (void)io;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

        VulkanRenderingDevice *device = static_cast<VulkanRenderingDevice *>(RD::GetInstance());
        ImGui::StyleColorsDark();

        ImGui_ImplVulkan_LoadFunctions([](const char *function_name, void *vulkan_instance) {
            return vkGetInstanceProcAddr(*(reinterpret_cast<VkInstance *>(vulkan_instance)), function_name);
        },
                                       &device->instance);

        ImGui_ImplGlfw_InitForVulkan(window, true);

        ImGui_ImplVulkan_InitInfo initInfo = {};
        initInfo.Instance = device->instance;
        initInfo.PhysicalDevice = device->physicalDevice;
        initInfo.Device = device->device;
        initInfo.Queue = device->_queues[0];
        initInfo.MinImageCount = device->swapchain->minImageCount;
        initInfo.ImageCount = device->swapchain->imageCount;
        initInfo.DescriptorPool = device->_descriptorPool;
        initInfo.Allocator = 0;
        initInfo.UseDynamicRendering = true;
        initInfo.ColorAttachmentFormat = device->swapchain->format.format;

        ImGui_ImplVulkan_Init(&initInfo, VK_NULL_HANDLE);

        RD::ImmediateSubmitInfo submitInfo;
        submitInfo.queue = device->GetDeviceQueue(RD::QUEUE_TYPE_GRAPHICS);
        submitInfo.commandPool = device->CreateCommandPool(submitInfo.queue, "TempCommandPool");
        submitInfo.commandBuffer = device->CreateCommandBuffer(submitInfo.commandPool, "TempCommandBuffer");
        submitInfo.fence = device->CreateFence("TempFence");

        device->ImmediateSubmit([&device](CommandBufferID commandBuffer) {
            VkCommandBuffer cb = device->_commandBuffers[commandBuffer.id];
            ImGui_ImplVulkan_CreateFontsTexture(cb);
        },
                                &submitInfo);

        device->WaitForFence(&submitInfo.fence, 1, UINT64_MAX);
        device->Destroy(submitInfo.fence);
        device->Destroy(submitInfo.commandPool);

        ImGui_ImplVulkan_DestroyFontUploadObjects();
    }

    void NewFrame() {
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
    }

    void Render(CommandBufferID commandBuffer) {
        ImGui::Render();
        return;
        VulkanRenderingDevice *device = static_cast<VulkanRenderingDevice *>(RD::GetInstance());
        VkCommandBuffer cb = device->_commandBuffers[commandBuffer.id];
        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cb);
    }

    void Shutdown() {
        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
    }
} // namespace ImGuiService