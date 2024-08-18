#include "pch.h"
#include "debug.h"

#include "rendering/rendering-utils.h"

namespace Debug {

    BufferID gLineBuffer;
    PipelineID gLinePipeline;
    static uint32_t gLineBufferOffset = 0;
    static Line *gLineBufferPtr = nullptr;
    static const int MAX_LINE_COUNT = 1'000'000;
    UniformSetID gUniformSet;

    void Initialize() {
        uint32_t bufferSize = MAX_LINE_COUNT * sizeof(Line);
        RenderingDevice *device = RD::GetInstance();
        gLineBuffer = device->CreateBuffer(bufferSize, RD::BUFFER_USAGE_STORAGE_BUFFER_BIT, RD::MEMORY_ALLOCATION_TYPE_CPU, "Debug Draw SSBO");

        RD::UniformBinding bindings[2] = {
            {RD::BINDING_TYPE_STORAGE_BUFFER, 0, 0}};

        RD::PushConstant pushConstant = {0, static_cast<uint32_t>(sizeof(glm::mat4))};
        ShaderID shaders[2] = {};
        shaders[0] = RenderingUtils::CreateShaderModuleFromFile("Assets/SPIRV/debug.vert.spv", bindings, 1, &pushConstant, 1);
        shaders[1] = RenderingUtils::CreateShaderModuleFromFile("Assets/SPIRV/debug.frag.spv", nullptr, 0, nullptr, 0);

        RD::Format colorAttachmentFormats[] = {RD::FORMAT_B8G8R8A8_UNORM};
        RD::BlendState bs = RD::BlendState::Create();
        RD::RasterizationState rs = RD::RasterizationState::Create();
        rs.lineWidth = 2.0f;
        RD::DepthState ds = RD::DepthState::Create();
        ds.enableDepthTest = true;
        ds.enableDepthWrite = true;
        gLinePipeline = device->CreateGraphicsPipeline(shaders,
                                                       static_cast<uint32_t>(std::size(shaders)),
                                                       RD::TOPOLOGY_LINE_LIST,
                                                       &rs,
                                                       &ds,
                                                       colorAttachmentFormats,
                                                       &bs,
                                                       1,
                                                       RD::FORMAT_D24_UNORM_S8_UINT,
                                                       false,
                                                       "Debug Draw Pipeline");

        RD::BoundUniform boundUniform = {RD::BINDING_TYPE_STORAGE_BUFFER, 0, gLineBuffer};
        gUniformSet = device->CreateUniformSet(gLinePipeline, &boundUniform, 1, 0, "Debug Uniform Set");

        device->Destroy(shaders[0]);
        device->Destroy(shaders[1]);
    }

    void InitializeBufferPtr() {
        if (gLineBufferPtr == nullptr)
            gLineBufferPtr = reinterpret_cast<Line *>(RD::GetInstance()->MapBuffer(gLineBuffer));
    }

    void AddLine(const glm::vec3 &p0, const glm::vec3 &p1, uint32_t color) {
        InitializeBufferPtr();

        Line *line = (gLineBufferPtr + gLineBufferOffset);
        line->p0 = p0;
        line->p1 = p1;
        line->c0 = line->c1 = color;
        gLineBufferOffset += 1;
    }

    void AddRect(const glm::vec3 &min, const glm::vec3 &max, uint32_t color) {
        InitializeBufferPtr();

        Line *line = (gLineBufferPtr + gLineBufferOffset);

        glm::vec3 size = max - min;
        // Bottom
        line->p0 = min;
        line->p1 = {min.x + size.x, min.y, min.z};
        line->c0 = line->c1 = color;
        line++;

        line->p0 = {min.x + size.x, min.y, min.z};
        line->p1 = {min.x + size.x, min.y, min.z + size.z};
        line->c0 = line->c1 = color;
        line++;

        line->p0 = {min.x + size.x, min.y, min.z + size.z};
        line->p1 = {min.x, min.y, min.z + size.z};
        line->c0 = line->c1 = color;
        line++;

        line->p0 = {min.x, min.y, min.z + size.z};
        line->p1 = {min.x, min.y, min.z};
        line->c0 = line->c1 = color;
        line++;

        // Top
        line->p0 = {min.x, max.y, min.z};
        line->p1 = {min.x + size.x, max.y, min.z};
        line->c0 = line->c1 = color;
        line++;

        line->p0 = {min.x + size.x, max.y, min.z};
        line->p1 = {min.x + size.x, max.y, min.z + size.z};
        line->c0 = line->c1 = color;
        line++;

        line->p0 = {min.x + size.x, max.y, min.z + size.z};
        line->p1 = {min.x, max.y, min.z + size.z};
        line->c0 = line->c1 = color;
        line++;

        line->p0 = {min.x, max.y, min.z + size.z};
        line->p1 = {min.x, max.y, min.z};
        line->c0 = line->c1 = color;
        line++;

        line->p0 = {min.x, min.y, min.z};
        line->p1 = {min.x, min.y + size.y, min.z};
        line->c0 = line->c1 = color;
        line++;

        line->p0 = {min.x + size.x, min.y, min.z};
        line->p1 = {min.x + size.x, min.y + size.y, min.z};
        line->c0 = line->c1 = color;
        line++;

        line->p0 = {min.x + size.x, min.y, min.z + size.z};
        line->p1 = {min.x + size.x, min.y + size.y, min.z + size.z};
        line->c0 = line->c1 = color;
        line++;

        line->p0 = {min.x, min.y, min.z + size.z};
        line->p1 = {min.x, min.y + size.y, min.z + size.z};
        line->c0 = line->c1 = color;
        line++;

        gLineBufferOffset += 12;
    }

    void NewFrame() {
        gLineBufferOffset = 0;
    }

    void Render(CommandBufferID commandBuffer, glm::mat4 VP) {
        if (gLineBufferOffset == 0)
            return;

        uint32_t numLines = gLineBufferOffset;

        RenderingDevice *device = RD::GetInstance();
        device->BindPipeline(commandBuffer, gLinePipeline);
        device->BindUniformSet(commandBuffer, gLinePipeline, &gUniformSet, 1);
        device->BindPushConstants(commandBuffer, gLinePipeline, RD::SHADER_STAGE_VERTEX, &VP[0][0], 0, static_cast<uint32_t>(sizeof(glm::mat4)));
        device->Draw(commandBuffer, numLines * 2, 1, 0, 0);
    }

    void Shutdown() {
        RenderingDevice *device = RD::GetInstance();
        device->Destroy(gUniformSet);
        device->Destroy(gLineBuffer);
        device->Destroy(gLinePipeline);
    }

} // namespace Debug