#pragma once

#include "rendering-device.h"
#define VK_NO_PROTOTYPES
#include <volk.h>

#include "core/resource-pool.h"

#define NOMINMAX
#include <vma/vk_mem_alloc.h>

#include <vector>
#include <memory>

class VulkanRenderingDevice : public RenderingDevice {

  public:
    void Initialize() override;

    void Shutdown() override;

    uint32_t GetDeviceCount() override {
        return static_cast<uint32_t>(gpus.size());
    }

    void CreateSurface(void *platformData) override;
    void CreateSwapchain(void *platformData, bool vsync = true) override;

    ShaderID CreateShader(const uint32_t *byteCode, uint32_t codeSizeInByte, ShaderDescription *desc, const std::string &name = "shader") override;
    PipelineID CreateGraphicsPipeline(const ShaderID *shaders,
                                      uint32_t shaderCount,
                                      Topology topology,
                                      const RasterizationState *rs,
                                      const DepthState *ds,
                                      const Format *colorAttachmentsFormat,
                                      const BlendState *attachmentBlendStates,
                                      uint32_t colorAttachmentCount,
                                      Format depthAttachmentFormat,
                                      const std::string &name) override;

    PipelineID CreateComputePipeline(const ShaderID shader, const std::string &name);

    CommandBufferID CreateCommandBuffer(CommandPoolID commandPool, const std::string &name = "commandBuffer") override;
    CommandPoolID CreateCommandPool(const std::string &name = "commandPool") override;

    TextureID CreateTexture(TextureDescription *description) override;

    void Destroy(PipelineID pipeline) override;
    void Destroy(CommandPoolID commandPool) override;
    void Destroy(ShaderID shaderModule) override;
    void Destroy(TextureID texture) override;

    Device *GetDevice(int index) override {
        return &gpus[index];
    }

    inline void SetValidationMode(bool state) override {
        enableValidation = state;
    }

    bool enableValidation = false;

    VkInstance instance = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT messanger;
    uint32_t vulkanAPIVersion = VK_API_VERSION_1_3;
    std::vector<VkPhysicalDevice> physicalDevices;
    VkPhysicalDeviceFeatures2 deviceFeatures2 = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
    VkPhysicalDeviceVulkan11Features deviceFeatures11 = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES};
    VkPhysicalDeviceVulkan12Features deviceFeatures12 = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
    VkPhysicalDeviceVulkan13Features deviceFeatures13 = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};

    VkDevice device = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    std::vector<VkQueueFamilyProperties> queueFamilyProperties;
    std::vector<uint32_t> selectedQueueFamilies;
    std::vector<VkQueue> queues;

    VmaAllocator vmaAllocator = VK_NULL_HANDLE;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkSemaphore timelineSemaphore = VK_NULL_HANDLE;

    struct VulkanShader {
        VkShaderModule shaderModule;
        std::vector<RD::ShaderBinding> layoutBindings;
        VkShaderStageFlagBits stage;
        std::vector<VkPushConstantRange> pushConstants;
    };

    struct VulkanPipeline {
        VkPipeline pipeline;
        VkPipelineLayout layout;
        VkDescriptorSetLayout setLayout;
    };

    struct VulkanSwapchain {
        VkSwapchainKHR swapchain;
        VkCompositeAlphaFlagBitsKHR surfaceComposite;
        VkSurfaceFormatKHR format;
        uint32_t minImageCount;
        uint32_t imageCount;
        uint32_t currentImageIndex;
        uint32_t width = UINT32_MAX;
        uint32_t height = UINT32_MAX;
        bool vsync;
        VkPresentModeKHR presentMode;
        VkSurfaceTransformFlagBitsKHR currentTransform;
        std::vector<VkImage> images;
        std::vector<VkImageView> imageViews;
    };

    struct VulkanTexture {
        uint32_t width, height, depth;
        uint32_t mipLevels, arrayLevels;

        VkImageAspectFlags imageAspect;
        VkFormat format;
        VkImageType imageType;

        VkImage image;
        VkImageView imageView;
        VmaAllocation allocation;
    };

    std::unique_ptr<VulkanSwapchain>
        swapchain;

    std::vector<const char *> enabledInstanceExtensions;
    std::vector<const char *> enabledInstanceLayers;
    std::vector<const char *> enabledDeviceExtensions;
    std::vector<Device> gpus;

    ResourcePool<VulkanShader> _shaders;
    ResourcePool<VkCommandPool> _commandPools;
    ResourcePool<VulkanPipeline> _pipeline;
    ResourcePool<VulkanTexture> _textures;

    std::vector<VkCommandBuffer> _commandBuffers;
    VkDescriptorPool _descriptorPool;

  private:
    void FindValidationLayers(std::vector<const char *> &enabledLayers);
    void InitializeInstanceExtensions(std::vector<const char *> &enabledExtensions);
    void InitializeDevice(uint32_t deviceIndex);
    void InitializeDevices();
    void InitializeAllocator();

    VkDevice CreateDevice(VkPhysicalDevice physicalDevice, std::vector<const char *> &enabledExtensions);
    VkSwapchainKHR CreateSwapchainInternal(std::unique_ptr<VulkanSwapchain> &swapchain);
    VkDescriptorPool CreateDescriptorPool();
    VkDescriptorSetLayout CreateDescriptorSetLayout(std::vector<VkDescriptorSetLayoutBinding> &bindings, uint32_t bindingCount);

    VkPipelineLayout CreatePipelineLayout(VkDescriptorSetLayout setLayout, std::vector<VkPushConstantRange> &pushConstantRanges);

    void SetDebugMarkerObjectName(VkObjectType objectType, uint64_t handle, const char *objectName);

    VkSemaphore CreateSemaphore();
};
