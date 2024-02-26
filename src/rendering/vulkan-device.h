#pragma once

#include "rendering-device.h"

#include <volk.h>
#include <vector>

class VulkanDevice : public RenderingDevice {

    void Initialize() override;

    void Shutdown() override;

    inline void SetValidationMode(bool state) override {
        enableValidation = state;
    }

    bool enableValidation = false;

    VkInstance instance;
    uint32_t vulkanAPIVersion = VK_API_VERSION_1_3;

    std::vector<const char *> enabledInstanceExtensions;
    std::vector<const char *> enabledInstanceLayers;
    std::vector<const char *> enabledDeviceExtensions;

    std::vector<Device> gpus;
    std::vector<VkPhysicalDevice> physicalDevices;
    VkDevice device;
    VkPhysicalDevice physicalDevice;
    std::vector<VkQueueFamilyProperties> queueFamilyProperties;

  private:
    void FindValidationLayers(std::vector<const char *> &enabledLayers);
    void InitializeInstanceExtensions(std::vector<const char *> &enabledExtensions);
    void InitializeDevices();
};