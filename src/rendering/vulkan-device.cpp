#include "vulkan-device.h"

#include <iostream>
#include <string>
#include <assert.h>
#include <vector>

#define LOGE(err)                      \
    {                                  \
        std::cout << err << std::endl; \
        assert(0);                     \
    }

#define LOG(msg) \
    std::cout << msg << std::endl;

#define VK_CHECK(x)                                             \
    do {                                                        \
        VkResult err = x;                                       \
        if (err) {                                              \
            LOGE("Detected vulkan error" + std::to_string(err)) \
        }                                                       \
    } while (0)

VKAPI_ATTR VkBool32 VKAPI_CALL DebugUtilsMessengerCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                                                           VkDebugUtilsMessageTypeFlagsEXT messageTypes,
                                                           const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
                                                           void *pUserData) {
    if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
        LOGE(pCallbackData->pMessage);
    } else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
        std::cout << "Warning: " << pCallbackData->pMessage << std::endl;
    else
        LOG(pCallbackData->pMessage);
    return VK_FALSE;
}

void VulkanDevice::FindValidationLayers(std::vector<const char *> &enabledLayers) {
    uint32_t instanceLayerCount;
    VK_CHECK(vkEnumerateInstanceLayerProperties(&instanceLayerCount, nullptr));

    std::vector<VkLayerProperties> availableLayers(instanceLayerCount);
    VK_CHECK(vkEnumerateInstanceLayerProperties(&instanceLayerCount, availableLayers.data()));

    std::vector<const char *> requestedLayers{"VK_LAYER_KHRONOS_validation"};
    for (auto &requested : requestedLayers) {
        bool available = false;
        for (auto &layer : availableLayers) {
            if (strcmp(layer.layerName, requested) == 0) {
                enabledLayers.push_back(requested);
                available = true;
                break;
            }
        }

        if (!available)
            LOGE("Failed to find instance layer: " + std::string(requested));
    }
}

void VulkanDevice::InitializeInstanceExtensions(std::vector<const char *> &enabledExtensions) {
    std::vector<const char *> requestedInstanceExtensions{
        VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME,
        VK_KHR_SURFACE_EXTENSION_NAME,
        VK_EXT_SWAPCHAIN_COLOR_SPACE_EXTENSION_NAME,
        VK_KHR_WIN32_SURFACE_EXTENSION_NAME};

    if (enableValidation) {
        requestedInstanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        requestedInstanceExtensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
    }

    uint32_t availableExtensionCount = 0;
    VK_CHECK(vkEnumerateInstanceExtensionProperties(nullptr, &availableExtensionCount, nullptr));
    std::vector<VkExtensionProperties> availableExtensions(availableExtensionCount);
    VK_CHECK(vkEnumerateInstanceExtensionProperties(nullptr, &availableExtensionCount, availableExtensions.data()));

    for (auto &requestedExt : requestedInstanceExtensions) {
        bool available = false;
        for (auto &availableExt : availableExtensions) {
            if (std::strcmp(requestedExt, availableExt.extensionName) == 0) {
                enabledExtensions.push_back(requestedExt);
                available = true;
                break;
            }
        }
        if (!available)
            LOGE("Failed to find instance extension: " + std::string(requestedExt));
    }
}

void VulkanDevice::InitializeDevices() {
    uint32_t deviceCount = 0;
    VK_CHECK(vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr));
    if (deviceCount == 0) {
        LOGE("No Vulkan Supported GPU Found");
        exit(-1);
    }
    assert(deviceCount > 0);

    physicalDevices.resize(deviceCount);
    gpus.resize(deviceCount);
    VK_CHECK(vkEnumeratePhysicalDevices(instance, &deviceCount, physicalDevices.data()));

    // Select Physical devices
    std::vector<const char *> requiredExtensions{
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME,
        VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME};

    physicalDevice = physicalDevices[0];
    for (uint32_t i = 0; i < deviceCount; ++i) {
        VkPhysicalDeviceProperties2 properties{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};
        vkGetPhysicalDeviceProperties2(physicalDevices[i], &properties);
        gpus[i].deviceType = DeviceType{properties.properties.deviceType};
        gpus[i].vendor = properties.properties.vendorID;
        gpus[i].name = properties.properties.deviceName;

        if (properties.properties.deviceType = VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
            physicalDevice = physicalDevices[i];
            break;
        }
    }

    uint32_t queuePropertiesCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queuePropertiesCount, nullptr);
    assert(queuePropertiesCount > 0);
    queueFamilyProperties.resize(queuePropertiesCount);
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queuePropertiesCount, queueFamilyProperties.data());
}

void VulkanDevice::Initialize() {
    VK_CHECK(volkInitialize());

    if (enableValidation) {
        FindValidationLayers(enabledInstanceLayers);
    }
    InitializeInstanceExtensions(enabledInstanceExtensions);

    VkApplicationInfo applicationInfo = {VK_STRUCTURE_TYPE_APPLICATION_INFO};
    applicationInfo.pApplicationName = "Vox-Engine";
    applicationInfo.pEngineName = "Voxel Engine";
    applicationInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    applicationInfo.apiVersion = vulkanAPIVersion;

    VkInstanceCreateInfo createInfo = {VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    createInfo.pApplicationInfo = &applicationInfo;
    createInfo.ppEnabledExtensionNames = enabledInstanceExtensions.data();
    createInfo.enabledExtensionCount = static_cast<uint32_t>(enabledInstanceExtensions.size());
    createInfo.ppEnabledLayerNames = enabledInstanceLayers.data();
    createInfo.enabledLayerCount = static_cast<uint32_t>(enabledInstanceLayers.size());

    // Create debug messenger
    VkDebugUtilsMessengerCreateInfoEXT debugUtilsCreateInfo = {VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT};
    if (enableValidation) {
        debugUtilsCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;
        debugUtilsCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT;
        debugUtilsCreateInfo.pfnUserCallback = DebugUtilsMessengerCallback;
    }
    createInfo.pNext = &debugUtilsCreateInfo;

    // Create Instance
    VK_CHECK(vkCreateInstance(&createInfo, nullptr, &instance));
    volkLoadInstance(instance);

    InitializeDevices();
}

void VulkanDevice::Shutdown() {

    vkDestroyInstance(instance, nullptr);
}