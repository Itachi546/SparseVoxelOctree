#pragma once

#include "rendering-device.h"

namespace RenderingUtils {

    ShaderID CreateShaderModuleFromFile(const std::string &filename,
                                        RD::UniformBinding *bindings,
                                        uint32_t bindingCount,
                                        RD::PushConstant *pushConstants,
                                        uint32_t pushConstantCount);
}; // namespace RenderingUtils