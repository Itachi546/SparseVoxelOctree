#include "pch.h"
#include "vulkan-rendering-device.h"

#define VMA_IMPLEMENTATION
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 1
#include <vma/vk_mem_alloc.h>

#define VK_LOAD_FUNCTION(instance, pFuncName) (vkGetInstanceProcAddr(instance, pFuncName))
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
    } else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        LOGW(pCallbackData->pMessage);
    } else {
        LOG(pCallbackData->pMessage);
    }
    return VK_FALSE;
}

void VulkanRenderingDevice::SetDebugMarkerObjectName(VkObjectType objectType, uint64_t handle, const char *objectName) {
    if (!enableValidation)
        return;

    VkDebugUtilsObjectNameInfoEXT nameInfo = {
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
        .objectType = objectType,
        .objectHandle = handle,
        .pObjectName = objectName,
    };
    VK_CHECK(vkSetDebugUtilsObjectNameEXT(device, &nameInfo));
}

static VkShaderStageFlagBits RD_STAGE_TO_VK_SHADER_STAGE_BITS[RD::SHADER_STAGE_MAX] = {
    VK_SHADER_STAGE_VERTEX_BIT,
    VK_SHADER_STAGE_FRAGMENT_BIT,
    VK_SHADER_STAGE_COMPUTE_BIT,
    VK_SHADER_STAGE_GEOMETRY_BIT,
};

static VkFormat RD_FORMAT_TO_VK_FORMAT[RD::FORMAT_MAX] = {
    VK_FORMAT_A2B10G10R10_UNORM_PACK32,
    VK_FORMAT_B8G8R8A8_UNORM,
    VK_FORMAT_R8G8B8A8_UNORM,
    VK_FORMAT_R16_SFLOAT,
    VK_FORMAT_R16G16_SFLOAT,
    VK_FORMAT_R16G16B16_SFLOAT,
    VK_FORMAT_R16G16B16A16_SFLOAT,
    VK_FORMAT_R32G32B32A32_SFLOAT,
    VK_FORMAT_R32G32B32_SFLOAT,
    VK_FORMAT_R32G32_SFLOAT,
    VK_FORMAT_D16_UNORM,
    VK_FORMAT_D32_SFLOAT,
    VK_FORMAT_D32_SFLOAT_S8_UINT,
    VK_FORMAT_D24_UNORM_S8_UINT,
    VK_FORMAT_UNDEFINED,
};

static VkDescriptorType RD_BINDING_TYPE_TO_VK_DESCRIPTOR_TYPE[RD::BINDING_TYPE_MAX] = {
    VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
    VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
    VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
    VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
};

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

    std::vector<const char *> requestedExts{
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    };

    uint32_t availableExtCount = 0;
    VK_CHECK(vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &availableExtCount, nullptr));
    std::vector<VkExtensionProperties> availableExt(availableExtCount);
    VK_CHECK(vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &availableExtCount, availableExt.data()));

    for (auto &requestedExt : requestedExts) {
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
            selectedQueueFamilies.push_back(i);
            break;
        }
    }

#if _WIN32
    static PFN_vkGetPhysicalDeviceWin32PresentationSupportKHR vkCheckPresentationSupport = (PFN_vkGetPhysicalDeviceWin32PresentationSupportKHR)VK_LOAD_FUNCTION(instance, "vkGetPhysicalDeviceWin32PresentationSupportKHR");
    if (!vkCheckPresentationSupport(physicalDevice, selectedQueueFamilies[0])) {
        LOGE("Selected Device/Queue doesn't support presentation");
        exit(-1);
    }
#endif

    assert(queueCreateInfos.size() > 0);
    LOG("Total queue: " + std::to_string(queueCreateInfos.size()));

    deviceFeatures2.features.multiDrawIndirect = true;
    deviceFeatures2.features.pipelineStatisticsQuery = true;
    deviceFeatures2.features.shaderInt16 = true;
    deviceFeatures2.features.samplerAnisotropy = true;
    deviceFeatures2.features.geometryShader = true;
    deviceFeatures2.features.wideLines = true;

    deviceFeatures2.pNext = &deviceFeatures11;
    deviceFeatures11.shaderDrawParameters = true;
    deviceFeatures11.pNext = &deviceFeatures12;

    deviceFeatures12.drawIndirectCount = true;
    deviceFeatures12.shaderInt8 = true;
    deviceFeatures12.timelineSemaphore = true;
    deviceFeatures12.pNext = &deviceFeatures13;

    deviceFeatures13.dynamicRendering = true;
    deviceFeatures13.synchronization2 = true;

    VkDeviceCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = &deviceFeatures2,
        .queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size()),
        .pQueueCreateInfos = queueCreateInfos.data(),
        .enabledLayerCount = 0,
        .ppEnabledLayerNames = nullptr,
        .enabledExtensionCount = static_cast<uint32_t>(enabledExtensions.size()),
        .ppEnabledExtensionNames = enabledExtensions.data(),
        .pEnabledFeatures = nullptr};

    VkDevice device = VK_NULL_HANDLE;
    VK_CHECK(vkCreateDevice(physicalDevice, &createInfo, nullptr, &device));
    return device;
}

VkFence VulkanRenderingDevice::CreateFence(const std::string &name) {
    VkFence fence = 0;
    VkFenceCreateInfo createInfo{
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
    };
    VK_CHECK(vkCreateFence(device, &createInfo, nullptr, &fence));

    SetDebugMarkerObjectName(VK_OBJECT_TYPE_FENCE, (uint64_t)fence, name.c_str());
    return fence;
}

VkSemaphore VulkanRenderingDevice::CreateVulkanSemaphore(const std::string &name) {
    VkSemaphoreCreateInfo createInfo = {VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    VkSemaphore semaphore = VK_NULL_HANDLE;
    VK_CHECK(vkCreateSemaphore(device, &createInfo, nullptr, &semaphore));

    SetDebugMarkerObjectName(VK_OBJECT_TYPE_SEMAPHORE, (uint64_t)semaphore, name.c_str());
    return semaphore;
}

VkSwapchainKHR VulkanRenderingDevice::CreateSwapchainInternal(std::unique_ptr<VulkanSwapchain> &swapchain) {
    VkSwapchainCreateInfoKHR createInfo = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = surface,
        .minImageCount = swapchain->minImageCount,
        .imageFormat = swapchain->format.format,
        .imageColorSpace = swapchain->format.colorSpace,
        .imageExtent = VkExtent2D{swapchain->width, swapchain->height},
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices = nullptr,
        .preTransform = swapchain->currentTransform,
        .compositeAlpha = swapchain->surfaceComposite,
        .presentMode = swapchain->presentMode,
        .oldSwapchain = swapchain->swapchain,
    };
    VkSwapchainKHR swapchainKHR = VK_NULL_HANDLE;
    VkResult result = vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapchainKHR);
    return swapchainKHR;
}

void VulkanRenderingDevice::ResizeSwapchain() {
    VkSwapchainKHR oldSwapchain = swapchain->swapchain;
    for (auto &imageView : swapchain->imageViews)
        vkDestroyImageView(device, imageView, nullptr);

    swapchain->imageViews.clear();
    swapchain->images.clear();
    swapchain->imageCount = 0;
    CreateSwapchain(swapchain->vsync);

    vkDestroySwapchainKHR(device, oldSwapchain, nullptr);
}

void VulkanRenderingDevice::CreateSwapchain(bool vsync) {
    if (swapchain == nullptr) {
        swapchain = std::make_unique<VulkanSwapchain>();
        swapchain->vsync = vsync;
        swapchain->swapchain = VK_NULL_HANDLE;
    }

    VkSurfaceCapabilitiesKHR surfaceCapabilities = {};
    VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &surfaceCapabilities));
    if (surfaceCapabilities.currentExtent.width == 0 || surfaceCapabilities.currentExtent.height == 0)
        return;
    swapchain->width = surfaceCapabilities.currentExtent.width;
    swapchain->height = surfaceCapabilities.currentExtent.height;
    swapchain->minImageCount = std::min(std::max(2u, surfaceCapabilities.maxImageCount), 4u);

    uint32_t formatCount = 0;
    VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, nullptr));
    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, formats.data()));

    VkFormat requiredFormat = VK_FORMAT_B8G8R8A8_UNORM;
    VkColorSpaceKHR requiredColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    bool found = false;
    for (auto &supportedFormat : formats) {
        if (supportedFormat.format == requiredFormat && supportedFormat.colorSpace == requiredColorSpace) {
            swapchain->format = supportedFormat;
            found = true;
            break;
        }
    }

    if (!found) {
        LOGE("Couldn't find supported format and colorspace");
        return;
    }

    uint32_t presentModeCount = 0;
    VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, nullptr));
    std::vector<VkPresentModeKHR> presentModes(presentModeCount);
    VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, presentModes.data()));

    found = false;
    VkPresentModeKHR requiredPresentMode = vsync ? VK_PRESENT_MODE_MAILBOX_KHR : VK_PRESENT_MODE_IMMEDIATE_KHR;
    for (auto &presentMode : presentModes) {
        if (presentMode == requiredPresentMode) {
            swapchain->presentMode = presentMode;
            found = true;
            break;
        }
    }
    // Fallback to FIFO_KHR if not found
    if (!found)
        swapchain->presentMode = VK_PRESENT_MODE_FIFO_KHR;

    swapchain->surfaceComposite =
        (surfaceCapabilities.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR)
            ? VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR
        : (surfaceCapabilities.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR)
            ? VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR
        : (surfaceCapabilities.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR)
            ? VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR
            : VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;

    swapchain->currentTransform = surfaceCapabilities.currentTransform;
    swapchain->swapchain = CreateSwapchainInternal(swapchain);

    uint32_t imageCount = 0;
    VK_CHECK(vkGetSwapchainImagesKHR(device, swapchain->swapchain, &imageCount, nullptr));
    swapchain->images.resize(imageCount);
    swapchain->imageViews.resize(imageCount);

    VK_CHECK(vkGetSwapchainImagesKHR(device, swapchain->swapchain, &imageCount, swapchain->images.data()));
    swapchain->imageCount = imageCount;

    for (uint32_t i = 0; i < imageCount; ++i) {
        std::string name = "SwapchainImage" + std::to_string(i);
        SetDebugMarkerObjectName(VK_OBJECT_TYPE_IMAGE, (uint64_t)swapchain->images[i], name.c_str());
    }

    VkImageViewCreateInfo imageViewCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = swapchain->format.format,
        .components = {VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A},
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .levelCount = 1,
            .layerCount = 1,
        },
    };

    for (uint32_t i = 0; i < imageCount; ++i) {
        imageViewCreateInfo.image = swapchain->images[i];
        VK_CHECK(vkCreateImageView(device, &imageViewCreateInfo, nullptr, &swapchain->imageViews[i]));
    }
}

VkDescriptorPool VulkanRenderingDevice::CreateDescriptorPool() {
    VkDescriptorPoolSize poolSizes[3] = {
        {
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            32,
        },
        {
            VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
            32,
        },
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
         32},
    };
    VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
        .maxSets = 10,
        .poolSizeCount = static_cast<uint32_t>(std::size(poolSizes)),
        .pPoolSizes = poolSizes,
    };
    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
    VK_CHECK(vkCreateDescriptorPool(device, &descriptorPoolCreateInfo, nullptr, &descriptorPool));
    return descriptorPool;
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

    uint32_t queueCount = static_cast<uint32_t>(selectedQueueFamilies.size());
    assert(queueCount > 0);

    _queues.resize(queueCount);
    for (uint32_t i = 0; i < queueCount; ++i) {
        vkGetDeviceQueue(device, selectedQueueFamilies[i], 0, &_queues[i]);
    }
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

    VK_CHECK(vmaCreateAllocator(&allocatorCreateInfo, &vmaAllocator));
}

void VulkanRenderingDevice::Initialize() {
    VK_CHECK(volkInitialize());

    this->id = Resource::GetId();

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
        if (enableValidation) {
            debugUtilsCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;
            debugUtilsCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT;
            debugUtilsCreateInfo.pfnUserCallback = DebugUtilsMessengerCallback;
        }
        createInfo.pNext = &debugUtilsCreateInfo;
    }
    // Create Instance
    VK_CHECK(vkCreateInstance(&createInfo, nullptr, &instance));
    volkLoadInstance(instance);

    if (enableValidation) {
        VK_CHECK(vkCreateDebugUtilsMessengerEXT(instance, &debugUtilsCreateInfo, nullptr, &messanger));
    }

    InitializeDevices();

    for (uint32_t deviceIndex = 0; deviceIndex < gpus.size(); ++deviceIndex) {
        if (gpus[deviceIndex].deviceType == DeviceType::DEVICE_TYPE_DISCRETE_GPU) {
            InitializeDevice(deviceIndex);
            break;
        }
    }
    assert(device != VK_NULL_HANDLE);
    volkLoadDevice(device);

    InitializeAllocator();
    LOG("VMA Allocator Initialized ...");

    // Create Semaphore
    VkSemaphoreCreateInfo semaphoreCreateInfo = {VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    VkSemaphoreTypeCreateInfo semaphoreTypeCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
        .semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE,
        .initialValue = 0,
    };
    semaphoreCreateInfo.pNext = &semaphoreTypeCreateInfo;
    VK_CHECK(vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &timelineSemaphore));

    SetDebugMarkerObjectName(VK_OBJECT_TYPE_SEMAPHORE, (uint64_t)timelineSemaphore, "Timeline Semaphore");

    imageAcquireSemaphore = CreateVulkanSemaphore("Image Acquire Semaphore");
    renderEndSemaphore = CreateVulkanSemaphore("Render End Semaphore");

    // Initialize Resource Pools
    _shaders.Initialize(64, "ShaderPool");
    _pipeline.Initialize(64, "PipelinePool");
    _textures.Initialize(64, "TexturePool");
    _buffers.Initialize(64, "BufferPool");
    _uniformSets.Initialize(64, "UniformSetPool");
    _commandPools.Initialize(16, "CommandPool");
    _commandBuffers.reserve(16);

    _descriptorPool = CreateDescriptorPool();

    CommandPoolID commandPool = CreateCommandPool("Upload CommandPool");
    uploadCommandBuffer = CreateCommandBuffer(commandPool, "Upload Command Buffer");
    uploadFence = CreateFence("Upload Fence");
}

void VulkanRenderingDevice::CreateSurface(void *platformData) {
#ifdef _WIN32
    HWND hwnd = (HWND) static_cast<WindowPlatformData *>(platformData)->windowPtr;
    VkWin32SurfaceCreateInfoKHR createInfo = {
        .sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
        .hinstance = GetModuleHandle(0),
        .hwnd = hwnd,
    };
    PFN_vkCreateWin32SurfaceKHR createWin32Surface = (PFN_vkCreateWin32SurfaceKHR)VK_LOAD_FUNCTION(instance, "vkCreateWin32SurfaceKHR");
    VK_CHECK(createWin32Surface(instance, &createInfo, nullptr, &surface));
#else
#error "Unsupported Platform"
#endif
    VkBool32 presentSupport = false;
    VK_CHECK(vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, selectedQueueFamilies[0], surface, &presentSupport));
    if (!presentSupport) {
        LOGE("Presentation is supported by the device");
        exit(-1);
    }
}

ShaderID VulkanRenderingDevice::CreateShader(const uint32_t *byteCode, uint32_t codeSizeInByte, ShaderDescription *desc, const std::string &name) {
    VkShaderModuleCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = codeSizeInByte,
        .pCode = byteCode,
    };

    uint64_t shaderId = _shaders.Obtain();
    VulkanShader *shader = _shaders.Access(shaderId);
    shader->stage = RD_STAGE_TO_VK_SHADER_STAGE_BITS[desc->stage];
    VK_CHECK(vkCreateShaderModule(device, &createInfo, nullptr, &shader->shaderModule));

    RD::UniformBinding *bindings = desc->bindings;
    shader->layoutBindings.insert(shader->layoutBindings.end(), desc->bindings, desc->bindings + desc->bindingCount);

    std::sort(shader->layoutBindings.begin(), shader->layoutBindings.end(), [](const UniformBinding &lhs, const UniformBinding &rhs) {
        return lhs.set < rhs.set;
    });

    for (uint32_t i = 0; i < desc->pushConstantCount; ++i) {
        shader->pushConstants.push_back(VkPushConstantRange{
            .stageFlags = 0u | shader->stage,
            .offset = desc->pushConstants[i].offset,
            .size = desc->pushConstants[i].size,

        });
    };

    SetDebugMarkerObjectName(VK_OBJECT_TYPE_SHADER_MODULE, (uint64_t)shader->shaderModule, name.c_str());
    return ShaderID(shaderId);
}

VkPipelineLayout VulkanRenderingDevice::CreatePipelineLayout(std::vector<VkDescriptorSetLayout> &setLayouts, std::vector<VkPushConstantRange> &pushConstantRanges) {
    VkPipelineLayoutCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = static_cast<uint32_t>(setLayouts.size()),
        .pSetLayouts = setLayouts.data(),
        .pushConstantRangeCount = static_cast<uint32_t>(pushConstantRanges.size()),
        .pPushConstantRanges = pushConstantRanges.data(),
    };

    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    VK_CHECK(vkCreatePipelineLayout(device, &createInfo, nullptr, &pipelineLayout));
    return pipelineLayout;
}

VkDescriptorSetLayout VulkanRenderingDevice::CreateDescriptorSetLayout(std::vector<VkDescriptorSetLayoutBinding> &bindings, uint32_t bindingCount) {
    VkDescriptorSetLayoutCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = bindingCount,
        .pBindings = bindings.data(),
    };

    VkDescriptorSetLayout setLayout = VK_NULL_HANDLE;
    VK_CHECK(vkCreateDescriptorSetLayout(device, &createInfo, nullptr, &setLayout));
    return setLayout;
}

PipelineID VulkanRenderingDevice::CreateGraphicsPipeline(const ShaderID *shaders,
                                                         uint32_t shaderCount,
                                                         Topology topology,
                                                         const RasterizationState *rs,
                                                         const DepthState *ds,
                                                         const Format *colorAttachmentsFormat,
                                                         const BlendState *attachmentBlendStates,
                                                         uint32_t colorAttachmentCount,
                                                         Format depthAttachmentFormat,
                                                         const std::string &name) {
    VkGraphicsPipelineCreateInfo createInfo = {VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};

    std::vector<VkPipelineShaderStageCreateInfo> shaderStages(shaderCount);
    std::vector<VkPushConstantRange> pushConstants;
    uint32_t bindingCount = 0;
    uint32_t bindingFlag = 0;

    std::array<std::vector<VkDescriptorSetLayoutBinding>, MAX_SET_COUNT> setBindings;
    uint32_t setCount = 0;
    for (uint32_t i = 0; i < shaderCount; ++i) {
        VulkanShader *vkShader = _shaders.Access(shaders[i].id);
        shaderStages[i].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        shaderStages[i].module = vkShader->shaderModule;
        shaderStages[i].pName = "main";
        shaderStages[i].stage = vkShader->stage;

        // @TODO handle same bindings in vertex and fragment shader
        uint32_t uniformBindingCount = static_cast<uint32_t>(vkShader->layoutBindings.size());
        for (uint32_t i = 0; i < uniformBindingCount; ++i) {
            UniformBinding &layoutDef = vkShader->layoutBindings[i];
            VkDescriptorSetLayoutBinding &binding = setBindings[layoutDef.set].emplace_back(VkDescriptorSetLayoutBinding{});
            binding.binding = layoutDef.binding;
            binding.descriptorCount = 1;
            binding.descriptorType = RD_BINDING_TYPE_TO_VK_DESCRIPTOR_TYPE[layoutDef.bindingType];
            binding.pImmutableSamplers = nullptr;
            binding.stageFlags = vkShader->stage;
            setCount = std::max(layoutDef.set, setCount);
        }

        if (vkShader->pushConstants.size() > 0)
            pushConstants.insert(pushConstants.end(), vkShader->pushConstants.begin(), vkShader->pushConstants.end());
    }

    createInfo.stageCount = shaderCount;
    createInfo.pStages = shaderStages.data();

    VkPipelineVertexInputStateCreateInfo vertexInput = {VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    createInfo.pVertexInputState = &vertexInput;

    VkPipelineInputAssemblyStateCreateInfo inputAssembly = {VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    inputAssembly.topology = VkPrimitiveTopology(topology);
    createInfo.pInputAssemblyState = &inputAssembly;

    VkPipelineViewportStateCreateInfo viewportState = {VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;
    createInfo.pViewportState = &viewportState;

    VkPipelineRasterizationStateCreateInfo rasterizationState = {VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    rasterizationState.lineWidth = rs->lineWidth;
    rasterizationState.cullMode = (VkCullModeFlagBits)rs->cullMode;
    rasterizationState.frontFace = (VkFrontFace)rs->frontFace;
    rasterizationState.polygonMode = (VkPolygonMode)rs->polygonMode;
    createInfo.pRasterizationState = &rasterizationState;
    rasterizationState.depthClampEnable = ds->enableDepthClamp;

    VkPipelineMultisampleStateCreateInfo multisampleState = {VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    multisampleState.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    createInfo.pMultisampleState = &multisampleState;

    VkPipelineDepthStencilStateCreateInfo depthStencilState = {VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
    if (ds->enableDepthTest) {
        depthStencilState.depthTestEnable = ds->enableDepthTest;
        depthStencilState.depthWriteEnable = ds->enableDepthWrite;
        depthStencilState.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
        depthStencilState.maxDepthBounds = ds->maxDepthBounds;
        depthStencilState.minDepthBounds = ds->minDepthBounds;
    }
    createInfo.pDepthStencilState = &depthStencilState;

    std::vector<VkPipelineColorBlendAttachmentState> blendStates(colorAttachmentCount);
    for (uint32_t i = 0; i < colorAttachmentCount; ++i) {
        blendStates[i].blendEnable = false;
        blendStates[i].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    }
    VkPipelineColorBlendStateCreateInfo colorBlendState = {VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
    colorBlendState.attachmentCount = colorAttachmentCount;
    colorBlendState.pAttachments = blendStates.data();
    createInfo.pColorBlendState = &colorBlendState;

    VkDynamicState dynamicStates[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamicState = {VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
    dynamicState.dynamicStateCount = sizeof(dynamicStates) / sizeof(dynamicStates[0]);
    dynamicState.pDynamicStates = dynamicStates;
    createInfo.pDynamicState = &dynamicState;

    std::vector<VkFormat> colorFormats(colorAttachmentCount);
    for (uint32_t i = 0; i < colorAttachmentCount; ++i)
        colorFormats[i] = RD_FORMAT_TO_VK_FORMAT[colorAttachmentsFormat[i]];

    VkFormat depthFormat = RD_FORMAT_TO_VK_FORMAT[depthAttachmentFormat];

    VkPipelineRenderingCreateInfo renderingCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .colorAttachmentCount = colorAttachmentCount,
        .pColorAttachmentFormats = colorFormats.data(),
        .depthAttachmentFormat = depthFormat,
        .stencilAttachmentFormat = VK_FORMAT_UNDEFINED,
    };
    createInfo.pNext = &renderingCreateInfo;

    uint64_t pipelineID = _pipeline.Obtain();
    VulkanPipeline *pipeline = _pipeline.Access(pipelineID);

    std::vector<VkDescriptorSetLayout> &setLayouts = pipeline->setLayout;
    setLayouts.reserve(setCount);
    for (auto &setBinding : setBindings)
        setLayouts.push_back(CreateDescriptorSetLayout(setBinding, static_cast<uint32_t>(setBinding.size())));

    VkPipelineLayout layout = CreatePipelineLayout(setLayouts, pushConstants);
    createInfo.layout = layout;

    VK_CHECK(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &createInfo, nullptr, &pipeline->pipeline));

    pipeline->bindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    pipeline->layout = layout;
    SetDebugMarkerObjectName(VK_OBJECT_TYPE_PIPELINE, (uint64_t)pipeline->pipeline, name.c_str());
    return PipelineID{pipelineID};
}

PipelineID VulkanRenderingDevice::CreateComputePipeline(const ShaderID shader, const std::string &name) {
    VulkanShader *vkShader = _shaders.Access(shader.id);
    assert(vkShader->stage == VK_SHADER_STAGE_COMPUTE_BIT);

    VkPipelineShaderStageCreateInfo shaderStage = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = vkShader->stage,
        .module = vkShader->shaderModule,
        .pName = "main",
    };

    uint32_t uniformBindingCount = static_cast<uint32_t>(vkShader->layoutBindings.size());
    std::array<std::vector<VkDescriptorSetLayoutBinding>, MAX_SET_COUNT> setBindings;

    uint32_t setCount = 0;
    for (uint32_t i = 0; i < uniformBindingCount; ++i) {
        UniformBinding &layoutDef = vkShader->layoutBindings[i];
        VkDescriptorSetLayoutBinding &binding = setBindings[layoutDef.set].emplace_back(VkDescriptorSetLayoutBinding{});
        binding.binding = layoutDef.binding;
        binding.descriptorCount = 1;
        binding.descriptorType = RD_BINDING_TYPE_TO_VK_DESCRIPTOR_TYPE[layoutDef.bindingType];
        binding.pImmutableSamplers = nullptr;
        binding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        setCount = std::max(layoutDef.set, setCount);
    }

    uint64_t pipelineID = _pipeline.Obtain();
    VulkanPipeline *pipeline = _pipeline.Access(pipelineID);

    std::vector<VkDescriptorSetLayout> &setLayouts = pipeline->setLayout;
    setLayouts.reserve(setCount);
    for (auto &setBinding : setBindings)
        setLayouts.push_back(CreateDescriptorSetLayout(setBinding, static_cast<uint32_t>(setBinding.size())));

    VkPipelineLayout layout = CreatePipelineLayout(setLayouts, vkShader->pushConstants);
    VkComputePipelineCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .stage = shaderStage,
        .layout = layout,
    };

    VK_CHECK(vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &createInfo, nullptr, &pipeline->pipeline));
    pipeline->layout = layout;
    pipeline->bindPoint = VK_PIPELINE_BIND_POINT_COMPUTE;

    SetDebugMarkerObjectName(VK_OBJECT_TYPE_PIPELINE, (uint64_t)pipeline->pipeline, name.c_str());

    return PipelineID{pipelineID};
}

CommandPoolID VulkanRenderingDevice::CreateCommandPool(const std::string &name) {
    VkCommandPoolCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = selectedQueueFamilies[0],
    };

    uint64_t commandPoolID = _commandPools.Obtain();
    VkCommandPool *commandPool = _commandPools.Access(commandPoolID);
    VK_CHECK(vkCreateCommandPool(device, &createInfo, nullptr, commandPool));

    SetDebugMarkerObjectName(VK_OBJECT_TYPE_COMMAND_POOL, (uint64_t)*commandPool, name.c_str());

    return CommandPoolID(commandPoolID);
}

CommandBufferID VulkanRenderingDevice::CreateCommandBuffer(CommandPoolID commandPool, const std::string &name) {
    const VkCommandPool *vkcmdPool = _commandPools.Access(commandPool.id);
    VkCommandBufferAllocateInfo allocateInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = *vkcmdPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };

    VkCommandBuffer *commandBuffer = &_commandBuffers.emplace_back(VK_NULL_HANDLE);
    VK_CHECK(vkAllocateCommandBuffers(device, &allocateInfo, commandBuffer));

    SetDebugMarkerObjectName(VK_OBJECT_TYPE_COMMAND_BUFFER, (uint64_t)(*commandBuffer), name.c_str());

    return CommandBufferID(_commandBuffers.size() - 1);
}

TextureID VulkanRenderingDevice::CreateTexture(TextureDescription *description, const std::string &name) {
    uint64_t textureID = _textures.Obtain();
    VulkanTexture *texture = _textures.Access(textureID);
    texture->width = description->width;
    texture->height = description->height;
    texture->depth = description->depth;
    texture->mipLevels = description->mipMaps;
    texture->arrayLevels = description->arrayLayers;
    texture->imageType = VkImageType(description->textureType);
    texture->currentLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VkImageUsageFlags usage = 0;
    if ((description->usageFlags & TEXTURE_USAGE_TRANSFER_SRC_BIT))
        usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    if ((description->usageFlags & TEXTURE_USAGE_TRANSFER_DST_BIT))
        usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    if ((description->usageFlags & TEXTURE_USAGE_SAMPLED_BIT))
        usage |= VK_IMAGE_USAGE_SAMPLED_BIT;
    if ((description->usageFlags & TEXTURE_USAGE_COLOR_ATTACHMENT_BIT))
        usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    if ((description->usageFlags & TEXTURE_USAGE_DEPTH_ATTACHMENT_BIT))
        usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    if ((description->usageFlags & TEXTURE_USAGE_INPUT_ATTACHMENT_BIT))
        usage |= VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
    if ((description->usageFlags & TEXTURE_USAGE_STORAGE_BIT))
        usage |= VK_IMAGE_USAGE_STORAGE_BIT;

    texture->format = RD_FORMAT_TO_VK_FORMAT[description->format];
    texture->imageAspect = (description->usageFlags & TEXTURE_USAGE_DEPTH_ATTACHMENT_BIT) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;

    VkImageCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = texture->imageType,
        .format = texture->format,
        .extent = {texture->width, texture->height, texture->depth},
        .mipLevels = texture->mipLevels,
        .arrayLayers = texture->arrayLevels,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };

    VmaAllocationCreateInfo allocationCreateInfo = {};
    allocationCreateInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    allocationCreateInfo.flags = 0;

    // Create Image
    VmaAllocationInfo allocationInfo = {};
    VK_CHECK(vmaCreateImage(vmaAllocator, &createInfo, &allocationCreateInfo, &texture->image, &texture->allocation, &allocationInfo));
    SetDebugMarkerObjectName(VK_OBJECT_TYPE_IMAGE, (uint64_t)texture->image, name.c_str());

    VkImageViewCreateInfo imageViewCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = texture->image,
        .viewType = VkImageViewType(texture->imageType),
        .format = texture->format,
        .components = {VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A},
        .subresourceRange = {
            .aspectMask = texture->imageAspect,
            .levelCount = texture->mipLevels,
            .layerCount = texture->arrayLevels,
        },
    };
    VK_CHECK(vkCreateImageView(device, &imageViewCreateInfo, nullptr, &texture->imageView));
    SetDebugMarkerObjectName(VK_OBJECT_TYPE_IMAGE_VIEW, (uint64_t)texture->imageView, (name + "ImageView").c_str());
    return TextureID{textureID};
}

BufferID VulkanRenderingDevice::CreateBuffer(uint32_t size, uint32_t usageFlags, MemoryAllocationType allocationType, const std::string &name) {
    assert(size > 0);
    VkBufferCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = size,
        .usage = usageFlags,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };

    VmaAllocationCreateInfo allocationCreateInfo = {};

    switch (allocationType) {
    case MEMORY_ALLOCATION_TYPE_CPU: {
        bool isSrc = (usageFlags & BUFFER_USAGE_TRANSFER_SRC_BIT) > 0;
        bool isDst = (usageFlags & BUFFER_USAGE_TRANSFER_DST_BIT) > 0;

        // This is a staging buffer
        allocationCreateInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
        if (isDst && !isSrc)
            allocationCreateInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;
        allocationCreateInfo.flags |= VMA_ALLOCATION_CREATE_MAPPED_BIT;
        allocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
        break;
    }
    case MEMORY_ALLOCATION_TYPE_GPU: {
        allocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
        break;
    }
    }

    VkBuffer vkBuffer = VK_NULL_HANDLE;
    VmaAllocation allocation = nullptr;
    VmaAllocationInfo allocationInfo = {};
    VK_CHECK(vmaCreateBuffer(vmaAllocator, &createInfo, &allocationCreateInfo, &vkBuffer, &allocation, &allocationInfo));
    SetDebugMarkerObjectName(VK_OBJECT_TYPE_BUFFER, (uint64_t)vkBuffer, name.c_str());

    uint64_t bufferID = _buffers.Obtain();
    VulkanBuffer *buffer = _buffers.Access(bufferID);
    buffer->mapped = false;
    buffer->buffer = vkBuffer;
    buffer->allocation = allocation;
    buffer->size = size;
    return BufferID(bufferID);
}

uint8_t *VulkanRenderingDevice::MapBuffer(BufferID buffer) {
    VulkanBuffer *vkBuffer = _buffers.Access(buffer.id);
    void *ptr = nullptr;
    VK_CHECK(vmaMapMemory(vmaAllocator, vkBuffer->allocation, &ptr));
    vkBuffer->mapped = true;
    return (uint8_t *)ptr;
}

void VulkanRenderingDevice::CopyBuffer(CommandBufferID commandBuffer, BufferID src, BufferID dst, BufferCopyRegion *region) {
    VulkanBuffer *vkSrc = _buffers.Access(src.id);
    VulkanBuffer *vkDst = _buffers.Access(dst.id);
    vkCmdCopyBuffer(_commandBuffers[commandBuffer.id], vkSrc->buffer, vkDst->buffer, 1, (const VkBufferCopy *)region);
}

UniformSetID VulkanRenderingDevice::CreateUniformSet(PipelineID pipeline, BoundUniform *uniforms, uint32_t uniformCount, uint32_t set, const std::string &name) {
    std::vector<VkWriteDescriptorSet> writeSets(uniformCount);

    // @TODO replace with custom allocator
    std::vector<VkDescriptorImageInfo> imageInfos;
    std::vector<VkDescriptorBufferInfo> bufferInfos;
    imageInfos.reserve(16), bufferInfos.reserve(16);

    for (uint32_t i = 0; i < uniformCount; ++i) {
        BoundUniform *uniform = uniforms + i;
        writeSets[i] = {};
        writeSets[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writeSets[i].dstBinding = uniform->binding;
        writeSets[i].descriptorType = VK_DESCRIPTOR_TYPE_MAX_ENUM;
        writeSets[i].descriptorCount = 1;

        switch (uniform->bindingType) {
        case BINDING_TYPE_IMAGE: {
            VulkanTexture *texture = _textures.Access(uniform->resourceID.id);
            VkDescriptorImageInfo &imageInfo = imageInfos.emplace_back(VkDescriptorImageInfo{});
            imageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
            imageInfo.imageView = texture->imageView;

            writeSets[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            writeSets[i].pImageInfo = &imageInfo;
        } break;
        case BINDING_TYPE_UNIFORM_BUFFER: {
            VulkanBuffer *buffer = _buffers.Access(uniform->resourceID.id);
            VkDescriptorBufferInfo &bufferInfo = bufferInfos.emplace_back(VkDescriptorBufferInfo{});
            bufferInfo.buffer = buffer->buffer;
            bufferInfo.offset = uniform->offset;
            bufferInfo.range = uniform->size;
            writeSets[i].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            writeSets[i].pBufferInfo = &bufferInfo;
        } break;
        case BINDING_TYPE_STORAGE_BUFFER: {
            VulkanBuffer *buffer = _buffers.Access(uniform->resourceID.id);
            VkDescriptorBufferInfo &bufferInfo = bufferInfos.emplace_back(VkDescriptorBufferInfo{});
            bufferInfo.buffer = buffer->buffer;
            bufferInfo.offset = uniform->offset;
            bufferInfo.range = uniform->size;
            writeSets[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            writeSets[i].pBufferInfo = &bufferInfo;
        } break;
        default:
            assert(0 && "Undefined Binding Type");
            break;
        }
    }

    VulkanPipeline *vkPipeline = _pipeline.Access(pipeline.id);
    VkDescriptorSetAllocateInfo allocateInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = _descriptorPool,
        .descriptorSetCount = 1,
        .pSetLayouts = &vkPipeline->setLayout[set],
    };

    VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
    vkAllocateDescriptorSets(device, &allocateInfo, &descriptorSet);
    SetDebugMarkerObjectName(VK_OBJECT_TYPE_DESCRIPTOR_SET, (uint64_t)descriptorSet, name.c_str());

    for (auto &writeSet : writeSets)
        writeSet.dstSet = descriptorSet;

    vkUpdateDescriptorSets(device, uniformCount, writeSets.data(), 0, nullptr);

    uint64_t uniformSetID = _uniformSets.Obtain();
    VulkanUniformSet *uniformSet = _uniformSets.Access(uniformSetID);
    uniformSet->descriptorPool = _descriptorPool;
    uniformSet->descriptorSet = descriptorSet;
    uniformSet->set = set;

    return UniformSetID{uniformSetID};
}

void VulkanRenderingDevice::BeginFrame() {
    // Check swapchain resize
    VkSurfaceCapabilitiesKHR surfaceCaps = {};
    VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &surfaceCaps));
    uint32_t width = swapchain->width;
    uint32_t height = swapchain->height;

    bool resized = (width != surfaceCaps.currentExtent.width) || (height != surfaceCaps.currentExtent.height);
    if (resized)
        ResizeSwapchain();

    VK_CHECK(vkAcquireNextImageKHR(device, swapchain->swapchain, ~0ul, imageAcquireSemaphore, VK_NULL_HANDLE, &swapchain->currentImageIndex));
}

void VulkanRenderingDevice::BeginCommandBuffer(CommandBufferID commandBuffer) {
    VkCommandBuffer vkCommandBuffer = _commandBuffers[commandBuffer.id];

    VkCommandBufferBeginInfo beginInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };
    VK_CHECK(vkBeginCommandBuffer(vkCommandBuffer, &beginInfo));
}

void VulkanRenderingDevice::EndCommandBuffer(CommandBufferID commandBuffer) {
    VkCommandBuffer vkCommandBuffer = _commandBuffers[commandBuffer.id];
    VK_CHECK(vkEndCommandBuffer(vkCommandBuffer));
}

void VulkanRenderingDevice::BeginRenderPass(CommandBufferID commandBuffer, RenderingInfo *renderInfo) {
    VkRenderingInfo vkRenderingInfo = {
        .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
    };

    std::vector<VkRenderingAttachmentInfo> colorAttachments;
    for (uint32_t i = 0; i < renderInfo->colorAttachmentCount; ++i) {
        AttachmentInfo &attachment = renderInfo->pColorAttachments[i];
        VkRenderingAttachmentInfo &attachmentInfo = colorAttachments.emplace_back(VkRenderingAttachmentInfo{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO});
        attachmentInfo.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        attachmentInfo.loadOp = VkAttachmentLoadOp(attachment.loadOp);
        attachmentInfo.storeOp = VkAttachmentStoreOp(attachment.storeOp);
        attachmentInfo.clearValue.color = {attachment.clearColor[0], attachment.clearColor[1], attachment.clearColor[2], attachment.clearColor[3]};

        if (attachment.attachment.id == ~0u) {
            uint32_t currentImageIndex = swapchain->currentImageIndex;
            attachmentInfo.imageView = swapchain->imageViews[currentImageIndex];
        } else {
            VulkanTexture *texture = _textures.Access(attachment.attachment.id);
            attachmentInfo.imageView = texture->imageView;
        }
    }

    vkRenderingInfo.renderArea = {0, 0, renderInfo->width, renderInfo->height};
    vkRenderingInfo.layerCount = renderInfo->layerCount;
    vkRenderingInfo.colorAttachmentCount = renderInfo->colorAttachmentCount;
    vkRenderingInfo.pColorAttachments = colorAttachments.data();

    VkRenderingAttachmentInfo depthAttachment = {VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
    if (renderInfo->pDepthStencilAttachment != nullptr) {
        AttachmentInfo *attachment = renderInfo->pDepthStencilAttachment;
        VulkanTexture *texture = _textures.Access(attachment->attachment.id);
        depthAttachment.imageView = texture->imageView;
        depthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
        depthAttachment.loadOp = VkAttachmentLoadOp(attachment->loadOp);
        depthAttachment.storeOp = VkAttachmentStoreOp(attachment->storeOp);
        depthAttachment.clearValue.depthStencil = {attachment->clearDepth, attachment->clearStencil};
        vkRenderingInfo.pDepthAttachment = &depthAttachment;
    }

    VkCommandBuffer cb = _commandBuffers[commandBuffer.id];
    vkCmdBeginRendering(cb, &vkRenderingInfo);
}

void VulkanRenderingDevice::EndRenderPass(CommandBufferID commandBuffer) {
    VkCommandBuffer cb = _commandBuffers[commandBuffer.id];
    vkCmdEndRendering(cb);
}

void VulkanRenderingDevice::DrawElementInstanced(CommandBufferID commandBuffer, uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, uint32_t vertexOffset, uint32_t firstInstance) {
    VkCommandBuffer cb = _commandBuffers[commandBuffer.id];
    vkCmdDrawIndexed(cb, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
}

void VulkanRenderingDevice::Draw(CommandBufferID commandBuffer, uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance) {
    VkCommandBuffer cb = _commandBuffers[commandBuffer.id];
    vkCmdDraw(cb, vertexCount, instanceCount, firstVertex, firstInstance);
}

void VulkanRenderingDevice::PrepareSwapchain(CommandBufferID commandBuffer, TextureLayout layout) {
    // Check swapchain Image layout and transition if needed
    uint32_t currentImageIndex = swapchain->currentImageIndex;
    VkImageLayout dstLayout = VkImageLayout(layout);
    VkImageLayout srcLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VkPipelineStageFlagBits srcStage, dstStage;
    VkAccessFlags srcAccessFlag, dstAccessFlag;

    switch (dstLayout) {
    case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
        srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dstStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        srcAccessFlag = 0;
        dstAccessFlag = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        srcLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        break;
    case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
        srcStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dstStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
        srcAccessFlag = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        dstAccessFlag = 0;
        break;
    default:
        LOGE("Undefined texture layout for swapchain");
        return;
    }

    VkImageMemoryBarrier presentBarrier = CreateImageBarrier(swapchain->images[currentImageIndex],
                                                             VK_IMAGE_ASPECT_COLOR_BIT,
                                                             srcAccessFlag,
                                                             dstAccessFlag,
                                                             srcLayout,
                                                             dstLayout);

    VkCommandBuffer vkCommandBuffer = _commandBuffers[commandBuffer.id];
    vkCmdPipelineBarrier(vkCommandBuffer,
                         srcStage,
                         dstStage,
                         VK_DEPENDENCY_BY_REGION_BIT,
                         0, nullptr, 0, nullptr,
                         1, &presentBarrier);
}

void VulkanRenderingDevice::Submit(CommandBufferID commandBuffer) {

    VkQueue queue = _queues[0];

    VkSemaphoreSubmitInfoKHR waitSemaphores[] = {
        {VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO_KHR, nullptr, timelineSemaphore, lastSemaphoreValue_, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT_KHR, 0},
        {VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO_KHR, nullptr, imageAcquireSemaphore, 0, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0},
    };

    lastSemaphoreValue_++;

    VkSemaphoreSubmitInfoKHR signalSemaphores[]{
        {VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO_KHR, nullptr, timelineSemaphore, lastSemaphoreValue_, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT_KHR, 0},
        {VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO_KHR, nullptr, renderEndSemaphore, 0, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT_KHR, 0},
    };

    VkCommandBuffer vkCommandBuffer = _commandBuffers[commandBuffer.id];
    VkCommandBufferSubmitInfo commandBufferInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
        .commandBuffer = vkCommandBuffer,
    };

    VkSubmitInfo2 submitInfo = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
        .pNext = nullptr,
        .waitSemaphoreInfoCount = static_cast<uint32_t>(std::size(waitSemaphores)),
        .pWaitSemaphoreInfos = waitSemaphores,
        .commandBufferInfoCount = 1,
        .pCommandBufferInfos = &commandBufferInfo,
        .signalSemaphoreInfoCount = static_cast<uint32_t>(std::size(signalSemaphores)),
        .pSignalSemaphoreInfos = signalSemaphores,
    };

    VK_CHECK(vkQueueSubmit2(queue, 1, &submitInfo, VK_NULL_HANDLE));
}

void VulkanRenderingDevice::SetViewport(CommandBufferID commandBuffer, uint32_t offsetX, uint32_t offsetY, uint32_t width, uint32_t height) {
    VkCommandBuffer cb = _commandBuffers[commandBuffer.id];
    VkViewport viewport{(float)offsetX, (float)offsetY, (float)width, (float)height, 0.0f, 1.0f};
    vkCmdSetViewport(cb, 0, 1, &viewport);
}

void VulkanRenderingDevice::SetScissor(CommandBufferID commandBuffer, uint32_t offsetX, uint32_t offsetY, uint32_t width, uint32_t height) {
    VkCommandBuffer cb = _commandBuffers[commandBuffer.id];
    VkRect2D scissor{(int)offsetX, (int)offsetY, width, height};
    vkCmdSetScissor(cb, 0, 1, &scissor);
}

void VulkanRenderingDevice::BindIndexBuffer(CommandBufferID commandBuffer, BufferID buffer) {
    VkCommandBuffer cb = _commandBuffers[commandBuffer.id];
    VulkanBuffer *vkBuffer = _buffers.Access(buffer.id);
    vkCmdBindIndexBuffer(cb, vkBuffer->buffer, 0, VK_INDEX_TYPE_UINT32);
}

void VulkanRenderingDevice::BindPipeline(CommandBufferID commandBuffer, PipelineID pipeline) {
    VulkanPipeline *vkPipeline = _pipeline.Access(pipeline.id);
    vkCmdBindPipeline(_commandBuffers[commandBuffer.id], vkPipeline->bindPoint, vkPipeline->pipeline);
}

void VulkanRenderingDevice::BindPushConstants(CommandBufferID commandBuffer, PipelineID pipeline, ShaderStage shaderStage, void *data, uint32_t offset, uint32_t size) {
    VulkanPipeline *vkPipeline = _pipeline.Access(pipeline.id);
    VkCommandBuffer cb = _commandBuffers[commandBuffer.id];
    vkCmdPushConstants(cb, vkPipeline->layout, RD_STAGE_TO_VK_SHADER_STAGE_BITS[shaderStage], offset, size, data);
}

void VulkanRenderingDevice::BindUniformSet(CommandBufferID commandBuffer, PipelineID pipeline, UniformSetID uniformSet) {
    VulkanPipeline *vkPipeline = _pipeline.Access(pipeline.id);
    VulkanUniformSet *vkUniformSet = _uniformSets.Access(uniformSet.id);
    vkCmdBindDescriptorSets(_commandBuffers[commandBuffer.id], vkPipeline->bindPoint, vkPipeline->layout, vkUniformSet->set, 1, &vkUniformSet->descriptorSet, 0, nullptr);
}

void VulkanRenderingDevice::DispatchCompute(CommandBufferID commandBuffer, uint32_t workGroupX, uint32_t workGroupY, uint32_t workGroupZ) {
    vkCmdDispatch(_commandBuffers[commandBuffer.id], workGroupX, workGroupY, workGroupZ);
}

VkImageMemoryBarrier VulkanRenderingDevice::CreateImageBarrier(VkImage image,
                                                               VkImageAspectFlags aspect,
                                                               VkAccessFlags srcAccessMask,
                                                               VkAccessFlags dstAccessMask,
                                                               VkImageLayout oldLayout,
                                                               VkImageLayout newLayout,
                                                               uint32_t mipLevel,
                                                               uint32_t arrLayer,
                                                               uint32_t mipCount,
                                                               uint32_t layerCount) {
    return {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = srcAccessMask,
        .dstAccessMask = dstAccessMask,
        .oldLayout = oldLayout,
        .newLayout = newLayout,
        .image = image,
        .subresourceRange = {
            .aspectMask = aspect,
            .baseMipLevel = mipLevel,
            .levelCount = mipCount,
            .baseArrayLayer = arrLayer,
            .layerCount = layerCount,
        },
    };
}

void VulkanRenderingDevice::PipelineBarrier(CommandBufferID commandBuffer,
                                            PipelineStageBits srcStage,
                                            PipelineStageBits dstStage,
                                            std::vector<TextureBarrier> &textureBarriers) {

    std::vector<VkImageMemoryBarrier> vkTextureBarriers;
    for (uint32_t i = 0; i < textureBarriers.size(); ++i) {
        TextureBarrier &textureBarrier = textureBarriers[i];
        VkImageLayout newLayout = VkImageLayout(textureBarrier.newLayout);
        VulkanTexture *vkTexture = _textures.Access(textureBarrier.texture.id);
        if (newLayout == vkTexture->currentLayout)
            continue;

        vkTextureBarriers.push_back(CreateImageBarrier(vkTexture->image, vkTexture->imageAspect,
                                                       VkAccessFlags(textureBarrier.srcAccess),
                                                       VkAccessFlags(textureBarrier.dstAccess),
                                                       vkTexture->currentLayout, newLayout));
        vkTexture->currentLayout = newLayout;
    }

    if (vkTextureBarriers.size() > 0) {
        vkCmdPipelineBarrier(_commandBuffers[commandBuffer.id],
                             VkPipelineStageFlags(srcStage),
                             VkPipelineStageFlags(dstStage),
                             VK_DEPENDENCY_BY_REGION_BIT,
                             0, nullptr,
                             0, nullptr,
                             static_cast<uint32_t>(vkTextureBarriers.size()),
                             vkTextureBarriers.data());
    }
}

void VulkanRenderingDevice::CopyToSwapchain(CommandBufferID commandBuffer, TextureID texture) {
    uint32_t imageIndex = swapchain->currentImageIndex;
    VulkanTexture *src = _textures.Access(texture.id);
    VkImage dst = swapchain->images[imageIndex];

    std::vector<VkImageMemoryBarrier> barriers;
    if (src->currentLayout != VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL) {
        barriers.push_back(CreateImageBarrier(src->image,
                                              src->imageAspect,
                                              VK_ACCESS_SHADER_WRITE_BIT,
                                              VK_ACCESS_TRANSFER_READ_BIT,
                                              src->currentLayout,
                                              VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL));
        src->currentLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    }

    barriers.push_back(CreateImageBarrier(dst,
                                          VK_IMAGE_ASPECT_COLOR_BIT,
                                          VK_ACCESS_NONE,
                                          VK_ACCESS_TRANSFER_WRITE_BIT,
                                          VK_IMAGE_LAYOUT_UNDEFINED,
                                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL));

    VkCommandBuffer cb = _commandBuffers[commandBuffer.id];
    vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_DEPENDENCY_BY_REGION_BIT,
                         0, 0, 0, 0,
                         static_cast<uint32_t>(barriers.size()),
                         barriers.data());

    VkImageBlit blitRegion = {};
    blitRegion.srcSubresource.aspectMask = src->imageAspect;
    blitRegion.srcSubresource.mipLevel = 0;
    blitRegion.srcSubresource.layerCount = 1;

    blitRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    blitRegion.dstSubresource.layerCount = 1;
    blitRegion.srcOffsets[0] = {0, 0, 0};
    blitRegion.srcOffsets[1] = {(int)src->width, (int)src->height, 1};
    blitRegion.dstOffsets[0] = {0, (int)swapchain->height, 0};
    blitRegion.dstOffsets[1] = {(int)swapchain->width, 0, 1};

    vkCmdBlitImage(cb, src->image, src->currentLayout, dst, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blitRegion, VK_FILTER_LINEAR);

    // Transition swapchain layout back to present
    VkImageMemoryBarrier swapchainBarrier = CreateImageBarrier(dst,
                                                               VK_IMAGE_ASPECT_COLOR_BIT,
                                                               VK_ACCESS_TRANSFER_WRITE_BIT,
                                                               VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                                                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                                               VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_DEPENDENCY_BY_REGION_BIT, 0, 0, 0, 0, 1, &swapchainBarrier);
}

void VulkanRenderingDevice::ImmediateSubmit(std::function<void(CommandBufferID commandBuffer)> &&function) {
    VkCommandBuffer cmd = _commandBuffers[uploadCommandBuffer.id];
    VkCommandBufferBeginInfo cmdBeginInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };

    VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

    function(uploadCommandBuffer);

    VK_CHECK(vkEndCommandBuffer(cmd));

    VkSubmitInfo submitInfo{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount = 0,
        .pWaitSemaphores = nullptr,
        .commandBufferCount = 1,
        .pCommandBuffers = &cmd,
        .signalSemaphoreCount = 0,
        .pSignalSemaphores = nullptr,
    };

    VK_CHECK(vkQueueSubmit(_queues[0], 1, &submitInfo, uploadFence));

    vkWaitForFences(device, 1, &uploadFence, true, UINT64_MAX);
    vkResetFences(device, 1, &uploadFence);

    vkResetCommandPool(device, *_commandPools.Access(uploadCommandPool.id), 0);
}

void VulkanRenderingDevice::Present() {

    VkSemaphore waitSemaphores[] = {renderEndSemaphore};
    VkPresentInfoKHR presentInfo = {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = static_cast<uint32_t>(std::size(waitSemaphores)),
        .pWaitSemaphores = waitSemaphores,
        .swapchainCount = 1,
        .pSwapchains = &swapchain->swapchain,
        .pImageIndices = &swapchain->currentImageIndex,
    };

    VK_CHECK(vkQueuePresentKHR(_queues[0], &presentInfo));
    vkDeviceWaitIdle(device);
}

void VulkanRenderingDevice::Destroy(BufferID buffer) {
    VulkanBuffer *vkBuffer = _buffers.Access(buffer.id);
    if (vkBuffer->mapped) {
        vmaUnmapMemory(vmaAllocator, vkBuffer->allocation);
    }
    vmaDestroyBuffer(vmaAllocator, vkBuffer->buffer, vkBuffer->allocation);
    _buffers.Release(buffer.id);
}

void VulkanRenderingDevice::Destroy(CommandPoolID commandPool) {
    VkCommandPool *vkcmdPool = _commandPools.Access(commandPool.id);
    vkDestroyCommandPool(device, *vkcmdPool, nullptr);
    _commandPools.Release(commandPool.id);
}

void VulkanRenderingDevice::Destroy(PipelineID pipeline) {
    VulkanPipeline *vkPipeline = _pipeline.Access(pipeline.id);
    vkDestroyPipeline(device, vkPipeline->pipeline, nullptr);
    vkDestroyPipelineLayout(device, vkPipeline->layout, nullptr);
    for (auto &setLayout : vkPipeline->setLayout)
        vkDestroyDescriptorSetLayout(device, setLayout, nullptr);
    _pipeline.Release(pipeline.id);
}

void VulkanRenderingDevice::Destroy(ShaderID shaderModule) {
    VulkanShader *shader = _shaders.Access(shaderModule.id);
    vkDestroyShaderModule(device, shader->shaderModule, nullptr);
    shader->layoutBindings.clear();
    shader->pushConstants.clear();
    shader->shaderModule = VK_NULL_HANDLE;
    shader->stage = VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM;
    _shaders.Release(shaderModule.id);
}

void VulkanRenderingDevice::Destroy(TextureID texture) {
    VulkanTexture *vkTexture = _textures.Access(texture.id);
    vkDestroyImageView(device, vkTexture->imageView, nullptr);
    vmaDestroyImage(vmaAllocator, vkTexture->image, vkTexture->allocation);
    _textures.Release(texture.id);
}

void VulkanRenderingDevice::Destroy(UniformSetID uniformSet) {
    VulkanUniformSet *vkUniformSet = _uniformSets.Access(uniformSet.id);
    vkFreeDescriptorSets(device, _descriptorPool, 1, &vkUniformSet->descriptorSet);
    _uniformSets.Release(uniformSet.id);
}

void VulkanRenderingDevice::Shutdown() {
    Destroy(CommandPoolID{0ull});
    // Release resource pool
    _shaders.Shutdown();
    _commandPools.Shutdown();
    _pipeline.Shutdown();
    _textures.Shutdown();
    _uniformSets.Shutdown();
    _buffers.Shutdown();

    vkDestroyFence(device, uploadFence, nullptr);
    vkDestroyDescriptorPool(device, _descriptorPool, nullptr);
    vkDestroySemaphore(device, timelineSemaphore, nullptr);
    vkDestroySemaphore(device, imageAcquireSemaphore, nullptr);
    vkDestroySemaphore(device, renderEndSemaphore, nullptr);
    for (auto &view : swapchain->imageViews)
        vkDestroyImageView(device, view, nullptr);
    vmaDestroyAllocator(vmaAllocator);
    vkDestroySwapchainKHR(device, swapchain->swapchain, nullptr);
    vkDestroySurfaceKHR(instance, surface, nullptr);
    vkDestroyDevice(device, nullptr);
    vkDestroyDebugUtilsMessengerEXT(instance, messanger, nullptr);
    vkDestroyInstance(instance, nullptr);
}