#pragma once

#include "rendering-device.h"

namespace RenderingUtils {

    ShaderID CreateShaderModuleFromFile(const std::string &filename,
                                        RD::UniformBinding *bindings,
                                        uint32_t bindingCount,
                                        RD::PushConstant *pushConstants,
                                        uint32_t pushConstantCount);

    inline uint32_t GetMipLevel(uint32_t width, uint32_t height, uint32_t depth) {
        return (uint32_t)std::floor(std::log2(std::max({width, height, depth}))) + 1;
    }

    void GenerateMipmap(CommandBufferID commandBuffer, TextureID texture);
}; // namespace RenderingUtils