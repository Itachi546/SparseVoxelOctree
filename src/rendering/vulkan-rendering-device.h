#pragma once

#include "rendering-device.h"
#define VK_NO_PROTOTYPES
#define VULKAN_1_3
#include <volk.h>

#include "core/resource-pool.h"

#include <vma/vk_mem_alloc.h>

#include <vector>
#include <memory>
#include <array>

class VulkanRenderingDevice : public RenderingDevice {

  public:
    void Initialize(void *platformData) override;

    void Shutdown() override;

    uint32_t GetDeviceCount() override {
        return static_cast<uint32_t>(gpus.size());
    }

    void CreateSurface() override;
    void CreateSwapchain(bool vsync = true) override;
    ShaderID CreateShader(const uint32_t *byteCode, uint32_t codeSizeInByte, ShaderDescription *desc, const std::string &name) override;
    PipelineID CreateGraphicsPipeline(const ShaderID *shaders,
                                      uint32_t shaderCount,
                                      Topology topology,
                                      const RasterizationState *rs,
                                      const DepthState *ds,
                                      const Format *colorAttachmentsFormat,
                                      const BlendState *attachmentBlendStates,
                                      uint32_t colorAttachmentCount,
                                      Format depthAttachmentFormat,
                                      bool enableBindless,
                                      const std::string &name) override;

    PipelineID CreateComputePipeline(const ShaderID shader, bool enableBindless, const std::string &name) override;
    CommandBufferID CreateCommandBuffer(CommandPoolID commandPool, const std::string &name) override;

    CommandPoolID CreateCommandPool(QueueID queue, const std::string &name = "CommandPool") override;
    void ResetCommandPool(CommandPoolID commandPool) override;

    TextureID CreateTexture(TextureDescription *description, const std::string &name) override;
    UniformSetID CreateUniformSet(PipelineID pipeline, BoundUniform *uniforms, uint32_t uniformCount, uint32_t set, const std::string &name) override;

    FenceID CreateFence(const std::string &name = "fence", bool signalled = false) override;
    void WaitForFence(FenceID *fence, uint32_t fenceCount, uint64_t timeout) override;
    void ResetFences(FenceID *fences, uint32_t fenceCount) override;

    BufferID CreateBuffer(uint32_t size, uint32_t usageFlags, MemoryAllocationType allocationType, const std::string &name) override;
    uint8_t *MapBuffer(BufferID buffer) override;

    void CopyBuffer(CommandBufferID commandBuffer, BufferID src, BufferID dst, BufferCopyRegion *region) override;
    void CopyBufferToTexture(CommandBufferID commandBuffer, BufferID src, TextureID dst, BufferImageCopyRegion *region) override;

    uint64_t GetMemoryUsage() override {
        return memoryUsage;
    }

    void BeginFrame() override;
    void BeginCommandBuffer(CommandBufferID commandBuffer) override;
    void EndCommandBuffer(CommandBufferID commandBuffer) override;

    QueueID GetDeviceQueue(QueueType queueType) override;

    void BeginRenderPass(CommandBufferID commandBuffer, RenderingInfo *renderingInfo) override;
    void EndRenderPass(CommandBufferID commandBuffer) override;

    void SetViewport(CommandBufferID commandBuffer, float offsetX, float offsetY, float width, float height) override;
    void SetScissor(CommandBufferID commandBuffer, int offsetX, int offsetY, uint32_t width, uint32_t height) override;

    void DrawElementInstanced(CommandBufferID commandBuffer, uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex = 0, uint32_t vertexOffset = 0, uint32_t firstInstance = 0) override;
    void Draw(CommandBufferID commandBuffer, uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance) override;
    void DrawIndexedIndirect(CommandBufferID commandBuffer, BufferID indirectBuffer, uint32_t offset, uint32_t drawCount, uint32_t stride) override;

    void Submit(CommandBufferID commandBuffer, FenceID fence) override;
    void ImmediateSubmit(std::function<void(CommandBufferID commandBufferfence)> &&function, ImmediateSubmitInfo *queueInfo) override;

    void Present() override;

    void PipelineBarrier(CommandBufferID commandBuffer,
                         PipelineStageBits srcStage,
                         PipelineStageBits dstStage,
                         TextureBarrier *barrier,
                         uint32_t barrierCount) override;

    void BindIndexBuffer(CommandBufferID commandBuffer, BufferID buffer) override;
    void BindPipeline(CommandBufferID commandBuffer, PipelineID pipeline) override;
    void BindUniformSet(CommandBufferID commandBuffer, PipelineID pipeline, UniformSetID *uniformSet, uint32_t uniformSetCount) override;
    void BindPushConstants(CommandBufferID commandBuffer, PipelineID pipeline, ShaderStage shaderStage, void *data, uint32_t offset, uint32_t size) override;
    void DispatchCompute(CommandBufferID commandBuffer, uint32_t workGroupX, uint32_t workGroupY, uint32_t workGroupZ = 1) override;

    void PrepareSwapchain(CommandBufferID commandBuffer, TextureLayout layout) override;

    void CopyToSwapchain(CommandBufferID commandBuffer, TextureID texture) override;

    // Single Threaded, can only be called from main thread after ownership transfer
    void UpdateBindlessTexture(TextureID texture) override {
        bindlessTextureToUpdate.push_back(texture);
    }

    void GenerateMipmap(CommandBufferID commandBuffer, TextureID texture) override;

    void Destroy(PipelineID pipeline) override;
    void Destroy(CommandPoolID commandPool) override;
    void Destroy(ShaderID shaderModule) override;
    void Destroy(TextureID texture) override;
    void Destroy(UniformSetID uniformSet) override;
    void Destroy(BufferID buffer) override;
    void Destroy(FenceID fence) override;

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

    VkPhysicalDeviceConservativeRasterizationPropertiesEXT conservativeRasterProps{};
    VkPhysicalDeviceFeatures2 deviceFeatures2 = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
    VkPhysicalDeviceVulkan11Features deviceFeatures11 = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES};
    VkPhysicalDeviceVulkan12Features deviceFeatures12 = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
    VkPhysicalDeviceVulkan13Features deviceFeatures13 = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};

    VkDevice device = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;

    // 0 - mainQueue
    // 1 - transferQueue
    // 2 - computeQueue
    uint32_t _queueFamilyIndices[3];
    VkQueue _queues[3];

    VmaAllocator vmaAllocator = VK_NULL_HANDLE;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkSemaphore timelineSemaphore = VK_NULL_HANDLE;
    VkSemaphore renderEndSemaphore = VK_NULL_HANDLE;
    VkSemaphore imageAcquireSemaphore = VK_NULL_HANDLE;
    uint64_t lastSemaphoreValue_ = 0;

    struct VulkanShader {
        VkShaderModule shaderModule;
        std::vector<RD::UniformBinding> layoutBindings;
        VkShaderStageFlagBits stage;
        std::vector<VkPushConstantRange> pushConstants;
    };

    struct VulkanPipeline {
        VkPipeline pipeline;
        VkPipelineLayout layout;
        std::vector<VkDescriptorSetLayout> setLayout;
        VkPipelineBindPoint bindPoint;
        bool bindlessEnabled;
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

        VkImageLayout currentLayout;
        VkSampler sampler;
    };

    struct VulkanBuffer {
        VkBuffer buffer;
        VmaAllocation allocation;
        uint32_t size;
        uint32_t samplerId;
        bool mapped;
    };

    struct VulkanUniformSet {
        VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
        VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
        uint32_t set = 0;
    };

    std::unique_ptr<VulkanSwapchain> swapchain;

    std::vector<const char *> enabledInstanceExtensions;
    std::vector<const char *> enabledInstanceLayers;
    std::vector<const char *> enabledDeviceExtensions;
    std::vector<Device> gpus;

    ResourcePool<VulkanShader> _shaders;
    ResourcePool<VkCommandPool> _commandPools;
    ResourcePool<VulkanPipeline> _pipeline;
    ResourcePool<VulkanTexture> _textures;
    ResourcePool<VulkanUniformSet> _uniformSets;
    ResourcePool<VulkanBuffer> _buffers;
    ResourcePool<VkFence> _fences;
    std::vector<VkCommandBuffer> _commandBuffers;

    VkDescriptorPool _descriptorPool;
    VkDescriptorPool _bindlessDescriptorPool;
    VkDescriptorSetLayout _bindlessDescriptorSetLayout;
    VkDescriptorSet _bindlessDescriptorSet;

    static const uint32_t MAX_SET_COUNT = 4;
    static const uint32_t MAX_BINDLESS_RESOURCES = 16536;
    static const uint32_t BINDLESS_TEXTURE_BINDING = 10;
    static const uint32_t BINDLESS_TEXTURE_SET = 1;

    uint64_t memoryUsage = 0;
    void *_platformData;

    std::vector<TextureID> bindlessTextureToUpdate;

  private:
    void FindValidationLayers(std::vector<const char *> &enabledLayers);
    void InitializeInstanceExtensions(std::vector<const char *> &enabledExtensions);
    void InitializeDevice(uint32_t deviceIndex);
    void InitializeDevices();
    void InitializeAllocator();

    VkDevice CreateDevice(VkPhysicalDevice physicalDevice, std::vector<const char *> &enabledExtensions);
    VkSwapchainKHR CreateSwapchainInternal(std::unique_ptr<VulkanSwapchain> &swapchain);
    VkDescriptorPool CreateDescriptorPool(VkDescriptorPoolSize *poolSizes, uint32_t poolCount, VkDescriptorPoolCreateFlags flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT);
    VkDescriptorSetLayout CreateDescriptorSetLayout(VkDescriptorSetLayoutBinding *binding, uint32_t bindingCount, VkDescriptorSetLayoutCreateFlags flags = 0);
    VkPipelineLayout CreatePipelineLayout(std::vector<VkDescriptorSetLayout> &setLayouts, std::vector<VkPushConstantRange> &pushConstantRanges);
    VkImageMemoryBarrier CreateImageBarrier(VkImage image,
                                            VkImageAspectFlags aspect,
                                            VkAccessFlags srcAccessMask,
                                            VkAccessFlags dstAccessMask,
                                            VkImageLayout oldLayout,
                                            VkImageLayout newLayout,
                                            uint32_t srcQueueFamilyIndex,
                                            uint32_t dstQueueFamilyIndex,
                                            uint32_t baseMipLevel = 0,
                                            uint32_t baseArrayLayer = 0,
                                            uint32_t levelCount = UINT32_MAX,
                                            uint32_t layerCount = UINT32_MAX);

    void SetDebugMarkerObjectName(VkObjectType objectType, uint64_t handle, const char *objectName);
    void ResizeSwapchain();

    VkSemaphore CreateVulkanSemaphore(const std::string &name = "semaphore");

    VkSampler CreateSampler(SamplerDescription *desc);
};
