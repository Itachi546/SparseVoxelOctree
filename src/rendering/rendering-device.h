#pragma once

#include <string>
#include "core/resource.h"

struct ID {
    size_t id = 0;
    inline ID() = default;
    ID(size_t _id) : id(_id) {}
};
#define DEFINE_ID(m_name)                                                                    \
    struct m_name##ID : public ID {                                                          \
        inline operator bool() const { return id != 0; }                                     \
        inline m_name##ID &operator=(m_name##ID p_other) {                                   \
            id = p_other.id;                                                                 \
            return *this;                                                                    \
        }                                                                                    \
        inline bool operator<(const m_name##ID &p_other) const { return id < p_other.id; }   \
        inline bool operator==(const m_name##ID &p_other) const { return id == p_other.id; } \
        inline bool operator!=(const m_name##ID &p_other) const { return id != p_other.id; } \
        inline m_name##ID(const m_name##ID &p_other) : ID(p_other.id) {}                     \
        inline explicit m_name##ID(uint64_t p_int) : ID(p_int) {}                            \
        inline explicit m_name##ID(void *p_ptr) : ID((size_t)p_ptr) {}                       \
        inline m_name##ID() = default;                                                       \
    };                                                                                       \
    /* Ensure type-punnable to pointer. Makes some things easier.*/                          \
    static_assert(sizeof(m_name##ID) == sizeof(void *));

DEFINE_ID(Shader)
DEFINE_ID(CommandBuffer)
DEFINE_ID(CommandPool)
DEFINE_ID(Pipeline)
DEFINE_ID(Texture)
DEFINE_ID(UniformSet)
DEFINE_ID(Buffer)

class RenderingDevice : public Resource {
  public:
    enum class DeviceType {
        DEVICE_TYPE_OTHER = 0x0,
        DEVICE_TYPE_INTEGRATED_GPU = 0x1,
        DEVICE_TYPE_DISCRETE_GPU = 0x2,
        DEVICE_TYPE_VIRTUAL_GPU = 0x3,
        DEVICE_TYPE_CPU = 0x4,
        DEVICE_TYPE_MAX = 0x5
    };

    struct Device {
        std::string name;
        uint32_t vendor;
        DeviceType deviceType;
    };

    enum ShaderStage {
        SHADER_STAGE_VERTEX = 0,
        SHADER_STAGE_FRAGMENT,
        SHADER_STAGE_COMPUTE,
        SHADER_STAGE_GEOMETRY,
        SHADER_STAGE_MAX,
    };

    enum BindingType {
        BINDING_TYPE_TEXTURE,
        BINDING_TYPE_IMAGE,
        BINDING_TYPE_STORAGE_BUFFER,
        BINDING_TYPE_UNIFORM_BUFFER,
        BINDING_TYPE_MAX
    };

    struct UniformBinding {
        BindingType bindingType;
        uint32_t set;
        uint32_t binding;
    };

    struct BoundUniform {
        BindingType bindingType;
        uint32_t binding;
        ID resourceID;
        uint64_t offset = 0;
        uint64_t size = ~0ull;
    };

    struct PushConstant {
        uint32_t offset;
        uint32_t size;
    };

    struct ShaderDescription {
        UniformBinding *bindings;
        uint32_t bindingCount;

        PushConstant *pushConstants;
        uint32_t pushConstantCount;

        ShaderStage stage;
    };

    enum CullMode {
        CULL_MODE_NONE = 0,
        CULL_MODE_FRONT,
        CULL_MODE_BACK,
        CULL_MODE_FRONT_AND_BACK,
        CULL_MODE_MAX
    };

    enum FrontFace {
        FRONT_FACE_COUNTER_CLOCKWISE = 0,
        FRONT_FACE_CLOCKWISE = 1,
        FRONT_FACE_MAX
    };

    enum PolygonMode {
        POLYGON_MODE_FILL = 0,
        POLYGON_MODE_LINE,
        POLYGON_MODE_POINT,
        POLYGON_MODE_MAX
    };

    enum Topology {
        TOPOLOGY_POINT_LIST = 0,
        TOPOLOGY_LINE_LIST = 1,
        TOPOLOGY_LINE_STRIP = 2,
        TOPOLOGY_TRIANGLE_LIST = 3,
        TOPOLOGY_TRIANGLE_STRIP = 4,
        TOPOLOGY_TRIANGLE_FAN = 5,
        TOPOLOGY_LINE_LIST_WITH_ADJACENCY = 6,
        TOPOLOGY_LINE_STRIP_WITH_ADJACENCY = 7,
        TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY = 8,
        TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY = 9,
        TOPOLOGY_PATCH_LIST = 10,
        TOPOLOGY_MAX = 11
    };

    enum Format {
        FORMAT_R10G10B10A2_UNORM = 0,
        FORMAT_B8G8R8A8_UNORM,
        FORMAT_R8G8B8A8_UNORM,
        FORMAT_R16_SFLOAT,
        FORMAT_R16G16_SFLOAT,
        FORMAT_R16G16B16_SFLOAT,
        FORMAT_R16G16B16A16_SFLOAT,
        FORMAT_R32G32B32A32_SFLOAT,
        FORMAT_R32G32B32_SFLOAT,
        FORMAT_R32G32_SFLOAT,
        FORMAT_D16_UNORM,
        FORMAT_D32_SFLOAT,
        FORMAT_D32_SFLOAT_S8_UINT,
        FORMAT_D24_UNORM_S8_UINT,
        FORMAT_UNDEFINED,
        FORMAT_MAX
    };

    struct RasterizationState {
        float lineWidth;
        CullMode cullMode;
        PolygonMode polygonMode;
        FrontFace frontFace;

        static RasterizationState Create() {
            return RasterizationState{
                .lineWidth = 1.0f,
                .cullMode = CULL_MODE_BACK,
                .polygonMode = POLYGON_MODE_FILL,
                .frontFace = FRONT_FACE_CLOCKWISE,
            };
        }
    };

    struct DepthState {
        bool enableDepthClamp;
        bool enableDepthTest;
        bool enableDepthWrite;
        float maxDepthBounds, minDepthBounds;

        static DepthState Create() {
            return DepthState{
                .enableDepthClamp = false,
                .enableDepthTest = false,
                .enableDepthWrite = false,
                .maxDepthBounds = 1.0f,
                .minDepthBounds = 0.0f,
            };
        }
    };

    struct BlendState {
        bool enable;

        static BlendState Create() {
            return BlendState{
                .enable = false,
            };
        }
    };

    enum TextureType {
        TEXTURE_TYPE_1D = 0,
        TEXTURE_TYPE_2D = 1,
        TEXTURE_TYPE_3D = 2,
        TEXTURE_TYPE_MAX = 3
    };

    enum TextureUsageBits {
        TEXTURE_USAGE_TRANSFER_SRC_BIT = (1 << 0),
        TEXTURE_USAGE_TRANSFER_DST_BIT = (1 << 1),
        TEXTURE_USAGE_SAMPLED_BIT = (1 << 2),
        TEXTURE_USAGE_COLOR_ATTACHMENT_BIT = (1 << 3),
        TEXTURE_USAGE_DEPTH_ATTACHMENT_BIT = (1 << 4),
        TEXTURE_USAGE_INPUT_ATTACHMENT_BIT = (1 << 5),
        TEXTURE_USAGE_STORAGE_BIT = (1 << 6),
    };

    enum BufferUsageBits {
        BUFFER_USAGE_TRANSFER_SRC_BIT = (1 << 0),
        BUFFER_USAGE_TRANSFER_DST_BIT = (1 << 1),
        BUFFER_USAGE_UNIFORM_BUFFER_BIT = (1 << 4),
        BUFFER_USAGE_STORAGE_BUFFER_BIT = (1 << 5),
        BUFFER_USAGE_INDEX_BUFFER_BIT = (1 << 6),
        BUFFER_USAGE_VERTEX_BUFFER_BIT = (1 << 7),
        BUFFER_USAGE_INDIRECT_BUFFER_BIT = (1 << 8)
    };

    enum PipelineStageBits {
        PIPELINE_STAGE_TOP_OF_PIPE_BIT = (1 << 0),
        PIPELINE_STAGE_DRAW_INDIRECT_BIT = (1 << 1),
        PIPELINE_STAGE_VERTEX_INPUT_BIT = (1 << 2),
        PIPELINE_STAGE_VERTEX_SHADER_BIT = (1 << 3),
        PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT = (1 << 4),
        PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT = (1 << 5),
        PIPELINE_STAGE_GEOMETRY_SHADER_BIT = (1 << 6),
        PIPELINE_STAGE_FRAGMENT_SHADER_BIT = (1 << 7),
        PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT = (1 << 8),
        PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT = (1 << 9),
        PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT = (1 << 10),
        PIPELINE_STAGE_COMPUTE_SHADER_BIT = (1 << 11),
        PIPELINE_STAGE_TRANSFER_BIT = (1 << 12),
        PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT = (1 << 13),
        PIPELINE_STAGE_ALL_GRAPHICS_BIT = (1 << 15),
        PIPELINE_STAGE_ALL_COMMANDS_BIT = (1 << 16),
    };

    enum BarrierAccessBits {
        BARRIER_ACCESS_NONE = 0,
        BARRIER_ACCESS_INDIRECT_COMMAND_READ_BIT = (1 << 0),
        BARRIER_ACCESS_INDEX_READ_BIT = (1 << 1),
        BARRIER_ACCESS_VERTEX_ATTRIBUTE_READ_BIT = (1 << 2),
        BARRIER_ACCESS_UNIFORM_READ_BIT = (1 << 3),
        BARRIER_ACCESS_INPUT_ATTACHMENT_READ_BIT = (1 << 4),
        BARRIER_ACCESS_SHADER_READ_BIT = (1 << 5),
        BARRIER_ACCESS_SHADER_WRITE_BIT = (1 << 6),
        BARRIER_ACCESS_COLOR_ATTACHMENT_READ_BIT = (1 << 7),
        BARRIER_ACCESS_COLOR_ATTACHMENT_WRITE_BIT = (1 << 8),
        BARRIER_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT = (1 << 9),
        BARRIER_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT = (1 << 10),
        BARRIER_ACCESS_TRANSFER_READ_BIT = (1 << 11),
        BARRIER_ACCESS_TRANSFER_WRITE_BIT = (1 << 12),
        BARRIER_ACCESS_HOST_READ_BIT = (1 << 13),
        BARRIER_ACCESS_HOST_WRITE_BIT = (1 << 14),
        BARRIER_ACCESS_MEMORY_READ_BIT = (1 << 15),
        BARRIER_ACCESS_MEMORY_WRITE_BIT = (1 << 16),
        BARRIER_ACCESS_FRAGMENT_SHADING_RATE_ATTACHMENT_READ_BIT = (1 << 23),
    };

    enum TextureLayout {
        TEXTURE_LAYOUT_UNDEFINED,
        TEXTURE_LAYOUT_GENERAL,
        TEXTURE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        TEXTURE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        TEXTURE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
        TEXTURE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        TEXTURE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        TEXTURE_LAYOUT_TRANSFER_DST_OPTIMAL,
        TEXTURE_LAYOUT_PREINITIALIZED,
        TEXTURE_LAYOUT_VRS_ATTACHMENT_OPTIMAL = 1000164003,
    };

    struct TextureBarrier {
        TextureID texture;
        BarrierAccessBits srcAccess;
        BarrierAccessBits dstAccess;
        TextureLayout newLayout;
    };

    enum MemoryAllocationType {
        MEMORY_ALLOCATION_TYPE_CPU,
        MEMORY_ALLOCATION_TYPE_GPU
    };

    struct TextureDescription {
        Format format;
        uint32_t width;
        uint32_t height;
        uint32_t depth;
        uint32_t arrayLayers;
        uint32_t mipMaps;

        TextureType textureType;
        uint32_t usageFlags;

        static TextureDescription Initialize(uint32_t width, uint32_t height, uint32_t depth = 1) {
            return TextureDescription{
                .format = FORMAT_R8G8B8A8_UNORM,
                .width = width,
                .height = height,
                .depth = 1,
                .arrayLayers = 1,
                .mipMaps = 1,
                .textureType = TEXTURE_TYPE_2D,
                .usageFlags = 0,
            };
        }
    };

    struct WindowPlatformData {
        void *windowPtr;
    };

    virtual void Initialize() = 0;

    virtual void SetValidationMode(bool state) = 0;

    virtual void CreateSurface(void *platformData) = 0;
    virtual void CreateSwapchain(bool vsync = true) = 0;
    virtual PipelineID CreateGraphicsPipeline(const ShaderID *shaders,
                                              uint32_t shaderCount,
                                              Topology topology,
                                              const RasterizationState *rs,
                                              const DepthState *ds,
                                              const Format *colorAttachmentsFormat,
                                              const BlendState *attachmentBlendStates,
                                              uint32_t colorAttachmentCount,
                                              Format depthAttachmentFormat,
                                              const std::string &name) = 0;
    virtual PipelineID CreateComputePipeline(const ShaderID shader, const std::string &name) = 0;
    virtual TextureID CreateTexture(TextureDescription *description, const std::string &name) = 0;
    virtual ShaderID CreateShader(const uint32_t *byteCode, uint32_t codeSizeInBytes, ShaderDescription *desc, const std::string &name = "shader") = 0;
    virtual CommandBufferID CreateCommandBuffer(CommandPoolID commandPool, const std::string &name = "commandBuffer") = 0;
    virtual CommandPoolID CreateCommandPool(const std::string &name = "commandPool") = 0;
    virtual UniformSetID CreateUniformSet(PipelineID pipeline, BoundUniform *uniforms, uint32_t uniformCount, uint32_t set, const std::string &name) = 0;
    virtual BufferID CreateBuffer(uint32_t size, uint32_t usageFlags, MemoryAllocationType allocationType, const std::string &name) = 0;
    virtual uint8_t *MapBuffer(BufferID buffer) = 0;

    virtual void BindPipeline(CommandBufferID commandBuffer, PipelineID pipeline) = 0;
    virtual void BindPushConstants(CommandBufferID commandBuffer, PipelineID pipeline, ShaderStage shaderStage, void *data, uint32_t offset, uint32_t size) = 0;
    virtual void BindUniformSet(CommandBufferID commandBuffer, PipelineID pipeline, UniformSetID uniformSet) = 0;
    virtual void DispatchCompute(CommandBufferID commandBuffer, uint32_t workGroupX, uint32_t workGroupY, uint32_t workGroupZ) = 0;
    virtual void Submit(CommandBufferID commandBuffer) = 0;

    // @TODO Hardcoded just for testing
    virtual void PipelineBarrier(CommandBufferID commandBuffer,
                                 PipelineStageBits srcStage,
                                 PipelineStageBits dstStage,
                                 std::vector<TextureBarrier> &textureBarriers) = 0;

    // @TODO seperate it into copy and present
    virtual void CopyToSwapchain(CommandBufferID commandBuffer, TextureID texture) = 0;

    virtual void BeginFrame() = 0;
    virtual void BeginCommandBuffer(CommandBufferID commandBuffer) = 0;
    virtual void EndCommandBuffer(CommandBufferID commandBuffer) = 0;
    virtual void Present() = 0;

    virtual void Destroy(PipelineID pipeline) = 0;
    virtual void Destroy(ShaderID shaderModule) = 0;
    virtual void Destroy(CommandPoolID commandPool) = 0;
    virtual void Destroy(TextureID texture) = 0;
    virtual void Destroy(UniformSetID uniformSet) = 0;
    virtual void Destroy(BufferID buffer) = 0;

    virtual void Shutdown() = 0;

    virtual uint32_t GetDeviceCount() = 0;
    virtual Device *GetDevice(int index) = 0;

    static RenderingDevice *&GetInstance() {
#ifdef VULKAN_ENABLED
        static RenderingDevice *device = nullptr;
        return device;
#else
#error No Supported Device Selected
#endif
    }
}; // namespace gfx

using RD = RenderingDevice;