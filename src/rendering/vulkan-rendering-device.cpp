#include "pch.h"
#include "vulkan-rendering-device.h"
#include "ui/win32-ui.h"

#define VMA_IMPLEMENTATION
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 1
#include <vma/vk_mem_alloc.h>

#define USE_INTEGRATED_GPU 0

constexpr const uint64_t MAIN_QUEUE = 0;
constexpr const uint64_t TRANSFER_QUEUE = 1;
constexpr const uint64_t COMPUTE_QUEUE = 2;

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
    VK_FORMAT_B8G8R8A8_UNORM,
    VK_FORMAT_R8G8B8A8_UNORM,
    VK_FORMAT_R8G8B8A8_SRGB,
    VK_FORMAT_R8G8B8_UNORM,
    VK_FORMAT_R8G8_UNORM,
    VK_FORMAT_R8_UNORM,
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

    std::vector<const char *> requestedLayers{"VK_LAYER_KHRONOS_validation", "VK_LAYER_KHRONOS_synchronization2"};
    for (auto &requested : requestedLayers) {
        bool available = false;
        for (auto &layer : availableLayers) {
            if (strcmp(layer.layerName, requested) == 0) {
                enabledLayers.push_back(requested);
                available = true;
                break;
            }
        }

        if (!available) {
            LOGE("Failed to find instance layer: " + std::string(requested));
        }
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
        if (!available) {
            LOGE("Failed to find instance extension: " + std::string(requestedExt));
        }
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
        if (!available) {
            LOGE("Failed to find device extension: " + std::string(requestedExt));
        }
    }

    uint32_t queueCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueCount, nullptr);
    ASSERT(queueCount > 0, "Queue count is zero");

    std::vector<VkQueueFamilyProperties> queueFamilyProperties(queueCount);
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueCount, queueFamilyProperties.data());

    _queueFamilyIndices[MAIN_QUEUE] = _queueFamilyIndices[TRANSFER_QUEUE] = _queueFamilyIndices[COMPUTE_QUEUE] = INVALID_QUEUE_ID;
    for (uint32_t i = 0; i < queueCount; ++i) {
        VkQueueFamilyProperties &queueFamily = queueFamilyProperties[i];
        if (queueFamilyProperties[i].queueCount == 0)
            continue;

        std::string log = "Family: " + std::to_string(i) + " flags: " + std::to_string(queueFamily.queueFlags) + " queueCount: " + std::to_string(queueFamily.queueCount);
        LOG(log);

        // Search for main queue that should be able to do all work (graphics, compute and transfer)
        if ((queueFamily.queueFlags & (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT)) == (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT)) {
            _queueFamilyIndices[MAIN_QUEUE] = i;
        }
        // Search for transfer queue
        if ((queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT) == 0 && (queueFamily.queueFlags & VK_QUEUE_TRANSFER_BIT)) {
            _queueFamilyIndices[TRANSFER_QUEUE] = i;
        }

        if ((queueFamily.queueFlags & (VK_QUEUE_TRANSFER_BIT | VK_QUEUE_GRAPHICS_BIT)) == 0 && (queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT)) {
            _queueFamilyIndices[COMPUTE_QUEUE] = i;
        }
    }

    ASSERT(_queueFamilyIndices[MAIN_QUEUE] != INVALID_QUEUE_ID, "Graphics Queue is not supported...");

#ifdef PLATFORM_WINDOWS
    static PFN_vkGetPhysicalDeviceWin32PresentationSupportKHR vkCheckPresentationSupport = (PFN_vkGetPhysicalDeviceWin32PresentationSupportKHR)VK_LOAD_FUNCTION(instance, "vkGetPhysicalDeviceWin32PresentationSupportKHR");
    if (!vkCheckPresentationSupport(physicalDevice, _queueFamilyIndices[MAIN_QUEUE])) {
        LOGE("Selected Device/Queue doesn't support presentation");
        HWND hwnd = (HWND) static_cast<WindowPlatformData *>(_platformData)->windowPtr;
        UI::ShowDialogBox(hwnd, "Presentation is not supported", "Error::Unsupported Feature");
        exit(-1);
    }
#endif

    float queuePriorities[] = {0.0f};
    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;

    uint32_t mainQueueIndex = _queueFamilyIndices[MAIN_QUEUE];
    queueCreateInfos.emplace_back(VkDeviceQueueCreateInfo{
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = mainQueueIndex,
        .queueCount = 1,
        .pQueuePriorities = queuePriorities,
    });

    uint32_t transferQueueIndex = _queueFamilyIndices[TRANSFER_QUEUE];
    if (transferQueueIndex < queueCount) {
        queueCreateInfos.emplace_back(VkDeviceQueueCreateInfo{
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .queueFamilyIndex = transferQueueIndex,
            .queueCount = 1,
            .pQueuePriorities = queuePriorities,
        });
    } else {
        ASSERT(0, "Transfer Queue is not supported");
#ifdef PLATFORM_WINDOWS
        HWND hwnd = (HWND) static_cast<WindowPlatformData *>(_platformData)->windowPtr;
        UI::ShowDialogBox(hwnd, "Transfer Queue is not supported", "Error::Unsupported Feature");
#endif
        exit(0);
    }

    uint32_t computeQueueIndex = _queueFamilyIndices[COMPUTE_QUEUE];
    if (computeQueueIndex < queueCount) {
        queueCreateInfos.emplace_back(VkDeviceQueueCreateInfo{
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .queueFamilyIndex = computeQueueIndex,
            .queueCount = 1,
            .pQueuePriorities = queuePriorities,
        });
    }

    assert(queueCreateInfos.size() > 0);
    LOG("Total queue selected: " + std::to_string(queueCreateInfos.size()));

    VkPhysicalDeviceDescriptorIndexingFeatures indexingFeatures{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES_EXT, nullptr};
    VkPhysicalDeviceFeatures2 supportedFeatures{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, &indexingFeatures};
    vkGetPhysicalDeviceFeatures2(physicalDevice, &supportedFeatures);
    bool bindlessSupported = indexingFeatures.descriptorBindingPartiallyBound && indexingFeatures.runtimeDescriptorArray;
    if (!bindlessSupported) {
        LOGE("Bindless resources is not supported ...");
#ifdef PLATFORM_WINDOWS
        HWND hwnd = (HWND) static_cast<WindowPlatformData *>(_platformData)->windowPtr;
        UI::ShowDialogBox(hwnd, "Bindless texture is not supported", "Error::Unsupported Feature");
#endif
        exit(0);
    }

    LOG("Bindless support found ...");

    deviceFeatures2.features.fragmentStoresAndAtomics = true;
    deviceFeatures2.features.multiDrawIndirect = true;
    deviceFeatures2.features.pipelineStatisticsQuery = true;
    deviceFeatures2.features.shaderInt16 = true;
    deviceFeatures2.features.samplerAnisotropy = true;
    deviceFeatures2.features.geometryShader = true;
    deviceFeatures2.features.wideLines = true;
    deviceFeatures2.features.shaderInt64 = true;

    deviceFeatures11.shaderDrawParameters = true;

    deviceFeatures12.drawIndirectCount = true;
    deviceFeatures12.shaderInt8 = true;
    deviceFeatures12.timelineSemaphore = true;
    deviceFeatures12.descriptorBindingPartiallyBound = true;
    deviceFeatures12.descriptorBindingSampledImageUpdateAfterBind = true;
    deviceFeatures12.descriptorBindingVariableDescriptorCount = true;
    deviceFeatures12.runtimeDescriptorArray = true;
    deviceFeatures12.shaderSampledImageArrayNonUniformIndexing = true;
    deviceFeatures12.shaderBufferInt64Atomics = true;

    deviceFeatures13.dynamicRendering = true;
    deviceFeatures13.synchronization2 = true;

    VkPhysicalDeviceDynamicRenderingUnusedAttachmentsFeaturesEXT dynamicRenderingUnusedAttachmentExt = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_UNUSED_ATTACHMENTS_FEATURES_EXT,
        .pNext = nullptr,
        .dynamicRenderingUnusedAttachments = true,
    };

    deviceFeatures2.pNext = &deviceFeatures11;
    deviceFeatures11.pNext = &deviceFeatures12;
    deviceFeatures12.pNext = &deviceFeatures13;
    deviceFeatures13.pNext = &dynamicRenderingUnusedAttachmentExt;

    VkDeviceCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = &deviceFeatures2,
        .queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size()),
        .pQueueCreateInfos = queueCreateInfos.data(),
        .enabledLayerCount = 0,
        .ppEnabledLayerNames = nullptr,
        .enabledExtensionCount = static_cast<uint32_t>(enabledExtensions.size()),
        .ppEnabledExtensionNames = enabledExtensions.data(),
        .pEnabledFeatures = nullptr,
    };

    VkDevice device = VK_NULL_HANDLE;
    VK_CHECK(vkCreateDevice(physicalDevice, &createInfo, nullptr, &device));

    vkGetDeviceQueue(device, mainQueueIndex, 0, &_queues[MAIN_QUEUE]);

    LOG("MainQueue: " + std::to_string(mainQueueIndex));
    LOG("TransferQueue: " + std::to_string(transferQueueIndex));
    LOG("ComputeQueue: " + std::to_string(computeQueueIndex));

    _queues[TRANSFER_QUEUE] = _queues[COMPUTE_QUEUE] = VK_NULL_HANDLE;
    if (transferQueueIndex < queueCount)
        vkGetDeviceQueue(device, transferQueueIndex, 0, &_queues[TRANSFER_QUEUE]);
    else {
        _queueFamilyIndices[TRANSFER_QUEUE] = mainQueueIndex;
        _queues[TRANSFER_QUEUE] = _queues[MAIN_QUEUE];
    }
    if (computeQueueIndex < queueCount)
        vkGetDeviceQueue(device, computeQueueIndex, 0, &_queues[COMPUTE_QUEUE]);
    else {
        _queueFamilyIndices[COMPUTE_QUEUE] = mainQueueIndex;
        _queues[COMPUTE_QUEUE] = _queues[MAIN_QUEUE];
    }
    return device;
}

FenceID VulkanRenderingDevice::CreateFence(const std::string &name, bool signalled) {
    uint64_t fenceId = _fences.Obtain();
    VkFence *fence = _fences.Access(fenceId);
    VkFenceCreateInfo createInfo{
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = signalled ? VK_FENCE_CREATE_SIGNALED_BIT : VkFenceCreateFlags(0),
    };
    VK_CHECK(vkCreateFence(device, &createInfo, nullptr, fence));

    SetDebugMarkerObjectName(VK_OBJECT_TYPE_FENCE, (uint64_t)*fence, name.c_str());
    return FenceID{fenceId};
}

void VulkanRenderingDevice::WaitForFence(FenceID *fence, uint32_t fenceCount, uint64_t timeout) {
    std::vector<VkFence> fences(fenceCount);
    for (uint32_t i = 0; i < fenceCount; ++i)
        fences[i] = *_fences.Access(fence[i].id);
    vkWaitForFences(device, fenceCount, fences.data(), true, timeout);
}

void VulkanRenderingDevice::ResetFences(FenceID *fences, uint32_t fenceCount) {
    std::vector<VkFence> vkFences(fenceCount);
    for (uint32_t i = 0; i < fenceCount; ++i)
        vkFences[i] = *_fences.Access(fences[i].id);
    vkResetFences(device, fenceCount, vkFences.data());
}

VkSemaphore VulkanRenderingDevice::CreateVulkanSemaphore(const std::string &name) {
    VkSemaphoreCreateInfo createInfo = {VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    VkSemaphore semaphore = VK_NULL_HANDLE;
    VK_CHECK(vkCreateSemaphore(device, &createInfo, nullptr, &semaphore));

    SetDebugMarkerObjectName(VK_OBJECT_TYPE_SEMAPHORE, (uint64_t)semaphore, name.c_str());
    return semaphore;
}

VkSampler VulkanRenderingDevice::CreateSampler(SamplerDescription *desc) {

    VkSamplerAddressMode addressMode = VkSamplerAddressMode(desc->addressMode);
    VkSamplerCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .pNext = nullptr,
        .magFilter = VkFilter(desc->magFilter),
        .minFilter = VkFilter(desc->minFilter),
        .mipmapMode = VkSamplerMipmapMode(desc->mipmapMode),
        .addressModeU = addressMode,
        .addressModeV = addressMode,
        .addressModeW = addressMode,
        .mipLodBias = desc->lodBias,
        .anisotropyEnable = desc->enableAnisotropy,
        .maxAnisotropy = desc->maxAnisotropy,
        .minLod = desc->minLod,
        .maxLod = desc->maxLod,
    };
    VkSampler sampler = VK_NULL_HANDLE;
    VK_CHECK(vkCreateSampler(device, &createInfo, nullptr, &sampler));
    return sampler;
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

VkDescriptorPool VulkanRenderingDevice::CreateDescriptorPool(VkDescriptorPoolSize *poolSizes, uint32_t poolCount, VkDescriptorPoolCreateFlags flags) {
    uint32_t maxSets = 0;
    for (uint32_t i = 0; i < poolCount; ++i) {
        maxSets += poolSizes[i].descriptorCount;
    }
    maxSets *= poolCount;
    VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .flags = flags, // VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
        .maxSets = maxSets,
        .poolSizeCount = poolCount,
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

    VK_CHECK(vmaCreateAllocator(&allocatorCreateInfo, &vmaAllocator));
}

void VulkanRenderingDevice::Initialize(void *platformData) {
    _platformData = platformData;

    VK_CHECK(volkInitialize());

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
            debugUtilsCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;
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

#if USE_INTEGRATED_GPU
    DeviceType deviceType = DeviceType::DEVICE_TYPE_INTEGRATED_GPU;
#else
    DeviceType deviceType = DeviceType::DEVICE_TYPE_DISCRETE_GPU;
#endif

    for (uint32_t deviceIndex = 0; deviceIndex < gpus.size(); ++deviceIndex) {
        if (gpus[deviceIndex].deviceType == deviceType) {
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
    _fences.Initialize(16, "Fences");
    _commandBuffers.reserve(32);

    // Global Descriptor Pool
    VkDescriptorPoolSize poolSizes[] = {
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 32},
        {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 32},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 32},
    };
    _descriptorPool = CreateDescriptorPool(poolSizes, (uint32_t)std::size(poolSizes));

    // Bindless Descriptor Pool
    VkDescriptorPoolSize bindlessPoolSize[] = {
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, MAX_BINDLESS_RESOURCES},
    };
    _bindlessDescriptorPool = CreateDescriptorPool(bindlessPoolSize, (uint32_t)std::size(bindlessPoolSize), VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT);

    // Bindless Descriptor Layout
    VkDescriptorSetLayoutBinding _bindlessSetLayouts[] = {
        {BINDLESS_TEXTURE_BINDING, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, MAX_BINDLESS_RESOURCES, VK_SHADER_STAGE_ALL, nullptr},
    };

    VkDescriptorSetLayoutCreateInfo bindlessLayoutInfo = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    bindlessLayoutInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT_EXT;
    bindlessLayoutInfo.bindingCount = (uint32_t)std::size(_bindlessSetLayouts);
    bindlessLayoutInfo.pBindings = _bindlessSetLayouts;

    VkDescriptorBindingFlags bindlessLayoutFlag = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT_EXT | VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT_EXT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT_EXT;
    VkDescriptorSetLayoutBindingFlagsCreateInfoEXT extendedInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO_EXT, nullptr};
    extendedInfo.bindingCount = 1;
    extendedInfo.pBindingFlags = &bindlessLayoutFlag;

    bindlessLayoutInfo.pNext = &extendedInfo;

    VK_CHECK(vkCreateDescriptorSetLayout(device, &bindlessLayoutInfo, nullptr, &_bindlessDescriptorSetLayout));

    uint32_t maxBinding = MAX_BINDLESS_RESOURCES - 1;
    VkDescriptorSetVariableDescriptorCountAllocateInfo countInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO,
        .pNext = nullptr,
        .descriptorSetCount = 1,
        .pDescriptorCounts = &maxBinding};

    VkDescriptorSetAllocateInfo bindlessDescriptorSetAllocateInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .pNext = &countInfo,
        .descriptorPool = _bindlessDescriptorPool,
        .descriptorSetCount = 1,
        .pSetLayouts = &_bindlessDescriptorSetLayout};
    VK_CHECK(vkAllocateDescriptorSets(device, &bindlessDescriptorSetAllocateInfo, &_bindlessDescriptorSet));
}

void VulkanRenderingDevice::CreateSurface() {
#ifdef PLATFORM_WINDOWS
    HWND hwnd = (HWND) static_cast<WindowPlatformData *>(_platformData)->windowPtr;
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
    VK_CHECK(vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, _queueFamilyIndices[MAIN_QUEUE], surface, &presentSupport));
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

VkDescriptorSetLayout VulkanRenderingDevice::CreateDescriptorSetLayout(VkDescriptorSetLayoutBinding *bindings, uint32_t bindingCount, VkDescriptorSetLayoutCreateFlags flags) {
    VkDescriptorSetLayoutCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .flags = flags,
        .bindingCount = bindingCount,
        .pBindings = bindings,
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
                                                         bool enableBindless,
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
        blendStates[i].blendEnable = attachmentBlendStates[i].enable;
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

    VkFormat stencilFormat = depthFormat == VK_FORMAT_D24_UNORM_S8_UINT ? depthFormat : VK_FORMAT_UNDEFINED;
    VkPipelineRenderingCreateInfo renderingCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .colorAttachmentCount = colorAttachmentCount,
        .pColorAttachmentFormats = colorFormats.data(),
        .depthAttachmentFormat = depthFormat,
        .stencilAttachmentFormat = stencilFormat,
    };
    createInfo.pNext = &renderingCreateInfo;

    uint64_t pipelineID = _pipeline.Obtain();
    VulkanPipeline *pipeline = _pipeline.Access(pipelineID);

    std::vector<VkDescriptorSetLayout> &setLayouts = pipeline->setLayout;
    setLayouts.reserve(setCount);
    for (auto &setBinding : setBindings) {
        if (setBinding.size() > 0)
            setLayouts.push_back(CreateDescriptorSetLayout(setBinding.data(), static_cast<uint32_t>(setBinding.size())));
    }

    if (enableBindless) {
        // Temporarily push back the bindlessSetLayout to create pipeline layout
        setLayouts.push_back(_bindlessDescriptorSetLayout);
    }
    VkPipelineLayout layout = CreatePipelineLayout(setLayouts, pushConstants);
    createInfo.layout = layout;

    // Remove bindlessDescriptorSetLayout as it is already destroyed
    if (enableBindless)
        setLayouts.pop_back();

    VK_CHECK(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &createInfo, nullptr, &pipeline->pipeline));

    pipeline->bindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    pipeline->layout = layout;
    pipeline->bindlessEnabled = enableBindless;

    SetDebugMarkerObjectName(VK_OBJECT_TYPE_PIPELINE, (uint64_t)pipeline->pipeline, name.c_str());
    return PipelineID{pipelineID};
}

PipelineID VulkanRenderingDevice::CreateComputePipeline(const ShaderID shader, bool enableBindless, const std::string &name) {
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
    for (auto &setBinding : setBindings) {
        if (setBinding.size() > 0)
            setLayouts.push_back(CreateDescriptorSetLayout(setBinding.data(), static_cast<uint32_t>(setBinding.size())));
    }

    VkPipelineLayout layout = CreatePipelineLayout(setLayouts, vkShader->pushConstants);
    VkComputePipelineCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .stage = shaderStage,
        .layout = layout,
    };

    VK_CHECK(vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &createInfo, nullptr, &pipeline->pipeline));
    pipeline->layout = layout;
    pipeline->bindPoint = VK_PIPELINE_BIND_POINT_COMPUTE;
    pipeline->bindlessEnabled = enableBindless;

    SetDebugMarkerObjectName(VK_OBJECT_TYPE_PIPELINE, (uint64_t)pipeline->pipeline, name.c_str());

    return PipelineID{pipelineID};
}

CommandPoolID VulkanRenderingDevice::CreateCommandPool(QueueID queue, const std::string &name) {
    VkCommandPoolCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = _queueFamilyIndices[queue.id],
    };

    uint64_t commandPoolID = _commandPools.Obtain();
    VkCommandPool *commandPool = _commandPools.Access(commandPoolID);
    VK_CHECK(vkCreateCommandPool(device, &createInfo, nullptr, commandPool));

    SetDebugMarkerObjectName(VK_OBJECT_TYPE_COMMAND_POOL, (uint64_t)*commandPool, name.c_str());

    return CommandPoolID(commandPoolID);
}

void VulkanRenderingDevice::ResetCommandPool(CommandPoolID commandPool) {
    VkCommandPool *cp = _commandPools.Access(commandPool.id);
    vkResetCommandPool(device, *cp, 0);
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

    // @TODO cache sampler
    if (description->samplerDescription)
        texture->sampler = CreateSampler(description->samplerDescription);
    else
        texture->sampler = VK_NULL_HANDLE;

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

    VkImageAspectFlags imageAspect = VK_IMAGE_ASPECT_COLOR_BIT;
    if (description->usageFlags & TEXTURE_USAGE_DEPTH_ATTACHMENT_BIT) {
        imageAspect = VK_IMAGE_ASPECT_DEPTH_BIT;

        if (description->usageFlags & TEXTURE_USAGE_STENCIL_ATTACHMENT_BIT)
            imageAspect |= VK_IMAGE_ASPECT_STENCIL_BIT;
    }
    texture->format = RD_FORMAT_TO_VK_FORMAT[description->format];
    texture->imageAspect = imageAspect;

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
    memoryUsage += texture->allocation->GetSize();

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

    memoryUsage += allocation->GetSize();
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

void VulkanRenderingDevice::CopyBufferToTexture(CommandBufferID commandBuffer, BufferID src, TextureID dst, BufferImageCopyRegion *region) {
    VulkanBuffer *buffer = _buffers.Access(src.id);
    VulkanTexture *texture = _textures.Access(dst.id);
    VkBufferImageCopy copyRegion = {};
    copyRegion.bufferOffset = region->bufferOffset;
    copyRegion.bufferRowLength = 0;
    copyRegion.bufferImageHeight = 0;

    copyRegion.imageSubresource.aspectMask = texture->imageAspect;
    copyRegion.imageSubresource.baseArrayLayer = 0;
    copyRegion.imageSubresource.layerCount = texture->arrayLevels;
    copyRegion.imageSubresource.mipLevel = 0;

    copyRegion.imageOffset = {0, 0, 0};
    copyRegion.imageExtent = {texture->width, texture->height, texture->depth};

    VkCommandBuffer cb = _commandBuffers[commandBuffer.id];

    ASSERT(texture->currentLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, "Error texture layout is not transfer dst optimal...");
    vkCmdCopyBufferToImage(cb, buffer->buffer, texture->image, texture->currentLayout, 1, &copyRegion);
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
            bufferInfo.range = uniform->range;
            writeSets[i].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            writeSets[i].pBufferInfo = &bufferInfo;
        } break;
        case BINDING_TYPE_STORAGE_BUFFER: {
            VulkanBuffer *buffer = _buffers.Access(uniform->resourceID.id);
            VkDescriptorBufferInfo &bufferInfo = bufferInfos.emplace_back(VkDescriptorBufferInfo{});
            bufferInfo.buffer = buffer->buffer;
            bufferInfo.offset = uniform->offset;
            bufferInfo.range = uniform->range;
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

    VK_CHECK(vkAcquireNextImageKHR(device, swapchain->swapchain, UINT64_MAX, imageAcquireSemaphore, VK_NULL_HANDLE, &swapchain->currentImageIndex));
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

QueueID VulkanRenderingDevice::GetDeviceQueue(QueueType queueType) {
    switch (queueType) {
    case RD::QUEUE_TYPE_GRAPHICS:
        return QueueID{MAIN_QUEUE};
    case RD::QUEUE_TYPE_TRANSFER:
        return QueueID{TRANSFER_QUEUE};
    case RD::QUEUE_TYPE_COMPUTE:
        return QueueID{COMPUTE_QUEUE};
    }
    return QueueID{MAIN_QUEUE};
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

        if (attachment.attachment.id == INVALID_TEXTURE_ID) {
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
        ASSERT(texture->currentLayout == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL || texture->currentLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, "Invalid image layout for depth image");
        depthAttachment.imageView = texture->imageView;
        depthAttachment.imageLayout = texture->currentLayout;
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

void VulkanRenderingDevice::DrawIndexedIndirect(CommandBufferID commandBuffer, BufferID indirectBuffer, uint32_t offset, uint32_t drawCount, uint32_t stride) {
    VkCommandBuffer cb = _commandBuffers[commandBuffer.id];
    VulkanBuffer *buffer = _buffers.Access(indirectBuffer.id);

    vkCmdDrawIndexedIndirect(cb, buffer->buffer, offset, drawCount, stride);
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
        srcStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dstStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        srcAccessFlag = 0;
        dstAccessFlag = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        break;
    case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
        srcStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dstStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
        srcLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        srcAccessFlag = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        dstAccessFlag = 0;
        break;
    default:
        LOGE("Undefined texture layout for swapchain");
        return;
    }

    VkCommandBuffer vkCommandBuffer = _commandBuffers[commandBuffer.id];
    VkImageMemoryBarrier presentBarrier = CreateImageBarrier(swapchain->images[currentImageIndex],
                                                             VK_IMAGE_ASPECT_COLOR_BIT,
                                                             srcAccessFlag,
                                                             dstAccessFlag,
                                                             srcLayout,
                                                             dstLayout, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED);

    vkCmdPipelineBarrier(vkCommandBuffer,
                         srcStage,
                         dstStage,
                         VK_DEPENDENCY_BY_REGION_BIT,
                         0, nullptr, 0, nullptr,
                         1, &presentBarrier);
}

void VulkanRenderingDevice::Submit(CommandBufferID commandBuffer, FenceID fence) {

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

    VkFence *vkFence;
    if (fence.id != INVALID_ID) {
        vkFence = _fences.Access(fence.id);
    } else
        vkFence = VK_NULL_HANDLE;

    VK_CHECK(vkQueueSubmit2(queue, 1, &submitInfo, *vkFence));
}

void VulkanRenderingDevice::SetViewport(CommandBufferID commandBuffer, float offsetX, float offsetY, float width, float height) {
    VkCommandBuffer cb = _commandBuffers[commandBuffer.id];
    VkViewport viewport{offsetX, offsetY, width, height, 0.0f, 1.0f};
    vkCmdSetViewport(cb, 0, 1, &viewport);
}

void VulkanRenderingDevice::SetScissor(CommandBufferID commandBuffer, int offsetX, int offsetY, uint32_t width, uint32_t height) {
    VkCommandBuffer cb = _commandBuffers[commandBuffer.id];
    VkRect2D scissor{offsetX, offsetY, width, height};
    vkCmdSetScissor(cb, 0, 1, &scissor);
}

void VulkanRenderingDevice::BindIndexBuffer(CommandBufferID commandBuffer, BufferID buffer) {
    VkCommandBuffer cb = _commandBuffers[commandBuffer.id];
    VulkanBuffer *vkBuffer = _buffers.Access(buffer.id);
    vkCmdBindIndexBuffer(cb, vkBuffer->buffer, 0, VK_INDEX_TYPE_UINT32);
}

void VulkanRenderingDevice::BindPipeline(CommandBufferID commandBuffer, PipelineID pipeline) {
    VulkanPipeline *vkPipeline = _pipeline.Access(pipeline.id);

    VkCommandBuffer cb = _commandBuffers[commandBuffer.id];
    vkCmdBindPipeline(cb, vkPipeline->bindPoint, vkPipeline->pipeline);

    if (vkPipeline->bindlessEnabled)
        vkCmdBindDescriptorSets(cb, vkPipeline->bindPoint, vkPipeline->layout, BINDLESS_TEXTURE_SET, 1, &_bindlessDescriptorSet, 0, nullptr);
}

void VulkanRenderingDevice::BindPushConstants(CommandBufferID commandBuffer, PipelineID pipeline, ShaderStage shaderStage, void *data, uint32_t offset, uint32_t size) {
    VulkanPipeline *vkPipeline = _pipeline.Access(pipeline.id);
    VkCommandBuffer cb = _commandBuffers[commandBuffer.id];
    vkCmdPushConstants(cb, vkPipeline->layout, RD_STAGE_TO_VK_SHADER_STAGE_BITS[shaderStage], offset, size, data);
}

void VulkanRenderingDevice::BindUniformSet(CommandBufferID commandBuffer, PipelineID pipeline, UniformSetID *uniformSets, uint32_t uniformSetCount) {
    VulkanPipeline *vkPipeline = _pipeline.Access(pipeline.id);
    VkCommandBuffer cb = _commandBuffers[commandBuffer.id];
    for (uint32_t i = 0; i < uniformSetCount; ++i) {
        VulkanUniformSet *vkUniformSet = _uniformSets.Access(uniformSets[i].id);
        vkCmdBindDescriptorSets(cb, vkPipeline->bindPoint, vkPipeline->layout, vkUniformSet->set, 1, &vkUniformSet->descriptorSet, 0, nullptr);
    }
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
                                                               uint32_t srcQueueFamily,
                                                               uint32_t dstQueueFamily,
                                                               uint32_t baseMipLevel,
                                                               uint32_t baseArrayLayer,
                                                               uint32_t levelCount,
                                                               uint32_t layerCount) {
    return {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = srcAccessMask,
        .dstAccessMask = dstAccessMask,
        .oldLayout = oldLayout,
        .newLayout = newLayout,
        .srcQueueFamilyIndex = srcQueueFamily,
        .dstQueueFamilyIndex = dstQueueFamily,
        .image = image,
        .subresourceRange = {
            .aspectMask = aspect,
            .baseMipLevel = baseMipLevel,
            .levelCount = levelCount,
            .baseArrayLayer = baseArrayLayer,
            .layerCount = layerCount,
        },
    };
}

void VulkanRenderingDevice::PipelineBarrier(CommandBufferID commandBuffer,
                                            PipelineStageBits srcStage,
                                            PipelineStageBits dstStage,
                                            TextureBarrier *textureBarriers,
                                            uint32_t barrierCount) {
    std::vector<VkImageMemoryBarrier> vkTextureBarriers;
    for (uint32_t i = 0; i < barrierCount; ++i) {
        TextureBarrier &textureBarrier = textureBarriers[i];
        VkImageLayout newLayout = VkImageLayout(textureBarrier.newLayout);
        VulkanTexture *vkTexture = _textures.Access(textureBarrier.texture.id);
        // if (newLayout == vkTexture->currentLayout)
        // continue;

        uint32_t srcQueueFamily = textureBarrier.srcQueueFamily.id == VK_QUEUE_FAMILY_IGNORED ? VK_QUEUE_FAMILY_IGNORED : _queueFamilyIndices[textureBarrier.srcQueueFamily.id];
        uint32_t dstQueueFamily = textureBarrier.dstQueueFamily.id == VK_QUEUE_FAMILY_IGNORED ? VK_QUEUE_FAMILY_IGNORED : _queueFamilyIndices[textureBarrier.dstQueueFamily.id];
        vkTextureBarriers.push_back(CreateImageBarrier(vkTexture->image, vkTexture->imageAspect,
                                                       VkAccessFlags(textureBarrier.srcAccess),
                                                       VkAccessFlags(textureBarrier.dstAccess),
                                                       vkTexture->currentLayout, newLayout,
                                                       srcQueueFamily,
                                                       dstQueueFamily,
                                                       textureBarrier.baseMipLevel,
                                                       textureBarrier.baseArrayLayer,
                                                       textureBarrier.levelCount, textureBarrier.layerCount));
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
                                              VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                              VK_QUEUE_FAMILY_IGNORED,
                                              VK_QUEUE_FAMILY_IGNORED));
        src->currentLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    }

    barriers.push_back(CreateImageBarrier(dst,
                                          VK_IMAGE_ASPECT_COLOR_BIT,
                                          VK_ACCESS_NONE,
                                          VK_ACCESS_TRANSFER_WRITE_BIT,
                                          VK_IMAGE_LAYOUT_UNDEFINED,
                                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                          VK_QUEUE_FAMILY_IGNORED,
                                          VK_QUEUE_FAMILY_IGNORED));

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
                                                               VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                                               VK_QUEUE_FAMILY_IGNORED,
                                                               VK_QUEUE_FAMILY_IGNORED);
    vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_DEPENDENCY_BY_REGION_BIT, 0, 0, 0, 0, 1, &swapchainBarrier);
}

void VulkanRenderingDevice::ImmediateSubmit(std::function<void(CommandBufferID)> &&function, ImmediateSubmitInfo *queueInfo) {
    ASSERT(queueInfo->fence.id != INVALID_ID, "Fence must be a valid fence for immediate submit");

    VkCommandBuffer cmd = _commandBuffers[queueInfo->commandBuffer.id];
    VkCommandBufferBeginInfo cmdBeginInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };

    VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

    function(queueInfo->commandBuffer);

    VK_CHECK(vkEndCommandBuffer(cmd));

    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_TRANSFER_BIT};

    VkSubmitInfo submitInfo{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount = 0,
        .pWaitSemaphores = nullptr,
        .pWaitDstStageMask = waitStages,
        .commandBufferCount = 1,
        .pCommandBuffers = &cmd,
        .signalSemaphoreCount = 0,
        .pSignalSemaphores = nullptr,
    };

    VkFence *vkFence = _fences.Access(queueInfo->fence.id);
    VK_CHECK(vkQueueSubmit(_queues[queueInfo->queue.id], 1, &submitInfo, *vkFence));
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

    uint32_t textureUpdateCount = static_cast<uint32_t>(bindlessTextureToUpdate.size());
    if (textureUpdateCount > 0) {
        std::vector<VkWriteDescriptorSet> writeSets(textureUpdateCount);
        std::vector<VkDescriptorImageInfo> imageInfos(textureUpdateCount);

        for (uint32_t i = 0; i < textureUpdateCount; ++i) {
            VulkanTexture *texture = _textures.Access(bindlessTextureToUpdate[i].id);
            VkWriteDescriptorSet &writeSet = writeSets[i];
            writeSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writeSet.pNext = nullptr;
            writeSet.dstSet = _bindlessDescriptorSet;
            writeSet.dstBinding = BINDLESS_TEXTURE_BINDING,
            writeSet.dstArrayElement = (uint32_t)bindlessTextureToUpdate[i].id;
            writeSet.descriptorCount = 1;
            writeSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;

            VkDescriptorImageInfo &imageInfo = imageInfos[i];
            imageInfo.sampler = texture->sampler;
            imageInfo.imageLayout = texture->currentLayout;
            imageInfo.imageView = texture->imageView;
            writeSet.pImageInfo = &imageInfo;
        }

        vkUpdateDescriptorSets(device, textureUpdateCount, writeSets.data(), 0, nullptr);
        bindlessTextureToUpdate.clear();
    }
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

void VulkanRenderingDevice::GenerateMipmap(CommandBufferID commandBuffer, TextureID texture) {
    VkCommandBuffer cb = _commandBuffers[commandBuffer.id];
    VulkanTexture *tex = _textures.Access(texture.id);

    // Mip 0 is already in transfer_src optimal, but rest of the mip is undefined
    // so we transition it to transfer dst so that we can copy
    VkImageMemoryBarrier transferBarriers[2];
    transferBarriers[0] = CreateImageBarrier(
        tex->image,
        tex->imageAspect,
        0,
        VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        UINT32_MAX, UINT32_MAX, 1, 0, tex->mipLevels - 1, 1);

    // After each copy we need to transition layout from transfer_dst to transfer src
    transferBarriers[1] = CreateImageBarrier(
        tex->image,
        tex->imageAspect,
        VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_ACCESS_TRANSFER_READ_BIT,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        UINT32_MAX, UINT32_MAX, 0, 0, 1, 1);

    vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_DEPENDENCY_BY_REGION_BIT, 0, 0, 0, 0, 2, transferBarriers);

    VkImageBlit imageBlit;
    imageBlit.srcSubresource.aspectMask = tex->imageAspect;
    imageBlit.srcSubresource.baseArrayLayer = 0;
    imageBlit.srcSubresource.layerCount = 1;

    imageBlit.dstSubresource.aspectMask = tex->imageAspect;
    imageBlit.dstSubresource.baseArrayLayer = 0;
    imageBlit.dstSubresource.layerCount = 1;

    imageBlit.srcOffsets[0] = {0, 0, 0};
    imageBlit.dstOffsets[0] = {0, 0, 0};

    int width = static_cast<int>(tex->width);
    int height = static_cast<int>(tex->height);

    for (uint32_t i = 1; i < tex->mipLevels; ++i) {
        imageBlit.srcSubresource.mipLevel = i - 1;
        imageBlit.dstSubresource.mipLevel = i;
        imageBlit.srcOffsets[1] = {width, height, 1};
        imageBlit.dstOffsets[1] = {width >> 1, height >> 1, 1};

        vkCmdBlitImage(cb, tex->image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, tex->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &imageBlit, VK_FILTER_LINEAR);

        width = width >> 1;
        height = height >> 1;

        transferBarriers[1].subresourceRange.baseMipLevel = i;
        vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_DEPENDENCY_BY_REGION_BIT, 0, 0, 0, 0, 1, &transferBarriers[1]);
    }

    tex->currentLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    TextureBarrier shaderReadBarrier{
        .texture = texture,
        .srcAccess = RD::BARRIER_ACCESS_TRANSFER_READ_BIT,
        .dstAccess = RD::BARRIER_ACCESS_SHADER_READ_BIT,
        .newLayout = RD::TEXTURE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        .srcQueueFamily = QUEUE_FAMILY_IGNORED,
        .dstQueueFamily = QUEUE_FAMILY_IGNORED,
        .baseMipLevel = 0,
        .baseArrayLayer = 0,
        .levelCount = tex->mipLevels,
        .layerCount = 1,
    };
    PipelineBarrier(commandBuffer, RD::PIPELINE_STAGE_TRANSFER_BIT, RD::PIPELINE_STAGE_FRAGMENT_SHADER_BIT, &shaderReadBarrier, 1);
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
    vkDestroySampler(device, vkTexture->sampler, nullptr);
    vkDestroyImageView(device, vkTexture->imageView, nullptr);
    vmaDestroyImage(vmaAllocator, vkTexture->image, vkTexture->allocation);
    _textures.Release(texture.id);
}

void VulkanRenderingDevice::Destroy(UniformSetID uniformSet) {
    VulkanUniformSet *vkUniformSet = _uniformSets.Access(uniformSet.id);
    vkFreeDescriptorSets(device, _descriptorPool, 1, &vkUniformSet->descriptorSet);
    _uniformSets.Release(uniformSet.id);
}

void VulkanRenderingDevice::Destroy(FenceID fence) {
    VkFence vkFence = *_fences.Access(fence.id);
    vkDestroyFence(device, vkFence, nullptr);
}

void VulkanRenderingDevice::Shutdown() {
    // Release resource pool
    _shaders.Shutdown();
    _commandPools.Shutdown();
    _pipeline.Shutdown();
    _textures.Shutdown();
    _uniformSets.Shutdown();
    _buffers.Shutdown();

    vkDestroyDescriptorSetLayout(device, _bindlessDescriptorSetLayout, nullptr);
    // vkFreeDescriptorSets(device, _bindlessDescriptorPool, 1, &_bindlessDescriptorSet);
    vkDestroyDescriptorPool(device, _bindlessDescriptorPool, nullptr);

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
    if (messanger != VK_NULL_HANDLE)
        vkDestroyDebugUtilsMessengerEXT(instance, messanger, nullptr);
    vkDestroyInstance(instance, nullptr);
}