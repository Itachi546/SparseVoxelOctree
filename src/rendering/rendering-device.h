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
        UNIFORM_TYPE_TEXTURE,
        UNIFORM_TYPE_IMAGE,
        UNIFORM_TYPE_STORAGE_BUFFER,
        UNIFORM_TYPE_UNIFORM_BUFFER,
        UNIFORM_TYPE_MAX
    };

    struct ShaderBinding {
        BindingType bindingType;
        uint32_t binding;
        uint32_t size;
    };

    struct PushConstant {
        uint32_t offset;
        uint32_t size;
    };

    struct ShaderDescription {
        bool isCompute = false;
        uint32_t localSize[3] = {};
        ShaderBinding *bindings;
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

    struct WindowPlatformData {
        void *windowPtr;
    };

    virtual void Initialize() = 0;

    virtual void CreateSurface(void *platformData) = 0;

    virtual void CreateSwapchain(void *platformData, bool vsync = true) = 0;

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

    virtual void SetValidationMode(bool state) = 0;

    virtual ShaderID CreateShader(const uint32_t *byteCode, uint32_t codeSizeInBytes, ShaderDescription *desc, const std::string &name = "shader") = 0;
    // virtual gfx::PipelineID CreateGraphicsPipeline(PipelineDescription *desc) = 0;

    virtual CommandBufferID CreateCommandBuffer(CommandPoolID commandPool, const std::string &name = "commandBuffer") = 0;
    virtual CommandPoolID CreateCommandPool(const std::string &name = "commandPool") = 0;

    virtual void Destroy(PipelineID pipeline) = 0;
    virtual void Destroy(ShaderID shaderModule) = 0;
    virtual void Destroy(CommandPoolID commandPool) = 0;

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