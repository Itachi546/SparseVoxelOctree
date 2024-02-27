#pragma once

#include "rendering-device.h"
#define VK_NO_PROTOTYPES
#include <volk.h>

#define NOMINMAX
#include <vma/vk_mem_alloc.h>

#include <vector>

class VulkanRenderingDevice : public RenderingDevice {

  public:
    void Initialize() override;

    void Shutdown() override;

    uint32_t GetDeviceCount() override {
        return static_cast<uint32_t>(gpus.size());
    }

    Device *GetDevice(int index) override {
        return &gpus[index];
    }

    inline void SetValidationMode(bool state) override {
        enableValidation = state;
    }

    bool enableValidation = false;

    VkInstance instance = VK_NULL_HANDLE;
    uint32_t vulkanAPIVersion = VK_API_VERSION_1_3;
    std::vector<VkPhysicalDevice> physicalDevices;
    VkDevice device = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    std::vector<VkQueueFamilyProperties> queueFamilyProperties;
    VmaAllocator allocator = VK_NULL_HANDLE;

    std::vector<const char *> enabledInstanceExtensions;
    std::vector<const char *> enabledInstanceLayers;
    std::vector<const char *> enabledDeviceExtensions;
    std::vector<Device> gpus;

  private:
    void FindValidationLayers(std::vector<const char *> &enabledLayers);
    void InitializeInstanceExtensions(std::vector<const char *> &enabledExtensions);
    void InitializeDevice(uint32_t deviceIndex);
    void InitializeDevices();
    void InitializeAllocator();
    VkDevice CreateDevice(VkPhysicalDevice physicalDevice, std::vector<const char *> &enabledExtensions);
};
