#include "vulkan-rendering-device.h"

#define VMA_IMPLEMENTATION
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 1
#include <vma/vk_mem_alloc.h>

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

void VulkanRenderingDevice::FindValidationLayers(std::vector<const char *> &enabledLayers) {
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

void VulkanRenderingDevice::InitializeInstanceExtensions(std::vector<const char *> &enabledExtensions) {
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

void VulkanRenderingDevice::InitializeDevices() {
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

    for (uint32_t i = 0; i < deviceCount; ++i) {
        VkPhysicalDeviceProperties2 properties{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};
        vkGetPhysicalDeviceProperties2(physicalDevices[i], &properties);
        gpus[i].deviceType = DeviceType{properties.properties.deviceType};
        gpus[i].vendor = properties.properties.vendorID;
        gpus[i].name = properties.properties.deviceName;
    }
}

VkDevice VulkanRenderingDevice::CreateDevice(VkPhysicalDevice physicalDevice, std::vector<const char *> &enabledExtensions) {
    std::vector<const char *> requestedExt{
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME,
        VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME,
        VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME};

    uint32_t availableExtCount = 0;
    VK_CHECK(vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &availableExtCount, nullptr));
    std::vector<VkExtensionProperties> availableExt(availableExtCount);
    VK_CHECK(vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &availableExtCount, availableExt.data()));

    for (auto &requestedExt : requestedExt) {
        bool available = false;
        for (auto &availableExt : availableExt) {
            if (std::strcmp(requestedExt, availableExt.extensionName) == 0) {
                enabledExtensions.push_back(requestedExt);
                available = true;
                break;
            }
        }
        if (!available)
            LOGE("Failed to find device extension: " + std::string(requestedExt));
    }

    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    // VkQueueFlags queueFlags = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT;
    //  @TODO support multiple queue later
    float queuePriorities[] = {0.0f};
    for (uint32_t i = 0; i < queueFamilyProperties.size(); ++i) {
        if (queueFamilyProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            queueCreateInfos.push_back({
                .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                .queueFamilyIndex = i,
                .queueCount = std::min(queueFamilyProperties[i].queueCount, 1u),
                .pQueuePriorities = queuePriorities,
            });
            break;
        }
    }

    assert(queueCreateInfos.size() > 0);
    LOG("Total queue: " + std::to_string(queueCreateInfos.size()));

    VkDeviceCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size()),
        .pQueueCreateInfos = queueCreateInfos.data(),
        .enabledLayerCount = 0,
        .ppEnabledLayerNames = nullptr,
        .enabledExtensionCount = static_cast<uint32_t>(enabledExtensions.size()),
        .ppEnabledExtensionNames = enabledExtensions.data(),
    };

    VkDevice device = VK_NULL_HANDLE;
    VK_CHECK(vkCreateDevice(physicalDevice, &createInfo, nullptr, &device));
    return device;
}

void VulkanRenderingDevice::InitializeDevice(uint32_t deviceIndex) {

    physicalDevice = physicalDevices[deviceIndex];
    VkPhysicalDeviceProperties2 properties{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};
    vkGetPhysicalDeviceProperties2(physicalDevice, &properties);
    LOG("Selected Device: " + std::string(properties.properties.deviceName));

    uint32_t queuePropertiesCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queuePropertiesCount, nullptr);
    assert(queuePropertiesCount > 0);
    queueFamilyProperties.resize(queuePropertiesCount);
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queuePropertiesCount, queueFamilyProperties.data());

    device = CreateDevice(physicalDevice, enabledDeviceExtensions);
}

void VulkanRenderingDevice::InitializeAllocator() {

    VmaVulkanFunctions vulkanFunctions = {};
    vulkanFunctions.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
    vulkanFunctions.vkGetDeviceProcAddr = vkGetDeviceProcAddr;

    VmaAllocatorCreateInfo allocatorCreateInfo = {
        .physicalDevice = physicalDevice,
        .device = device,
        .pVulkanFunctions = &vulkanFunctions,
        .instance = instance,
        .vulkanApiVersion = vulkanAPIVersion,
    };

    VK_CHECK(vmaCreateAllocator(&allocatorCreateInfo, &allocator));
}

void VulkanRenderingDevice::Initialize() {
    VK_CHECK(volkInitialize());

    this->id = rand();

    if (enableValidation) {
        FindValidationLayers(enabledInstanceLayers);
    }
    InitializeInstanceExtensions(enabledInstanceExtensions);

    VkApplicationInfo applicationInfo = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "Vox-Engine",
        .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
        .pEngineName = "Voxel Engine",
        .apiVersion = vulkanAPIVersion,
    };

    VkInstanceCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &applicationInfo,
        .enabledLayerCount = static_cast<uint32_t>(enabledInstanceLayers.size()),
        .ppEnabledLayerNames = enabledInstanceLayers.data(),
        .enabledExtensionCount = static_cast<uint32_t>(enabledInstanceExtensions.size()),
        .ppEnabledExtensionNames = enabledInstanceExtensions.data(),
    };

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

    for (uint32_t deviceIndex = 0; deviceIndex < gpus.size(); ++deviceIndex) {
        if (gpus[deviceIndex].deviceType == DeviceType::DEVICE_TYPE_DISCRETE_GPU)
            InitializeDevice(deviceIndex);
    }
    assert(device != VK_NULL_HANDLE);

    InitializeAllocator();
    LOG("VMA Allocator Initialized ...");
}

void VulkanRenderingDevice::Shutdown() {

    vkDestroyInstance(instance, nullptr);
}