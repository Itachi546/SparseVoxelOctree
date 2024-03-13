#include "rendering-utils.h"

#include "utils.h"
#include <assert.h>
#include <iostream>

namespace RenderingUtils {

    RD::ShaderStage InferShaderTypeFromExtension(const std::string &extension) {
        if (extension == ".vert")
            return RD::ShaderStage::SHADER_STAGE_VERTEX;
        else if (extension == ".frag")
            return RD::ShaderStage::SHADER_STAGE_FRAGMENT;
        else if (extension == ".comp")
            return RD::ShaderStage::SHADER_STAGE_COMPUTE;
        else if (extension == ".geom")
            return RD::ShaderStage::SHADER_STAGE_GEOMETRY;
        assert("Undefined shader stage");
        return RD::ShaderStage::SHADER_STAGE_MAX;
    }

    ShaderID CreateShaderModuleFromFile(const std::string &filename, RD::ShaderBinding *bindings, uint32_t bindingCount, RD::PushConstant *pushConstants, uint32_t pushConstantCount) {
        auto shaderCode = utils::ReadFile(filename, std::ios::binary);
        if (!shaderCode.has_value()) {
            std::cerr << "Failed to open shader file: " << filename << std::endl;
            assert(0);
            return ShaderID{0ull};
        }

        std::string code = shaderCode.value();
        assert(code.size() % 4 == 0);
        uint32_t size = static_cast<uint32_t>(code.size());
        const uint32_t *pCode = reinterpret_cast<const uint32_t *>(code.c_str());

        std::string extension = utils::GetFileExtension(filename.substr(0, filename.find_last_of('.')));
        assert(extension.size() > 0);

        RD::ShaderDescription desc;
        desc.bindings = bindings;
        desc.bindingCount = bindingCount;
        desc.pushConstants = pushConstants;
        desc.pushConstantCount = pushConstantCount;
        desc.stage = InferShaderTypeFromExtension(extension);
        return RD::GetInstance()->CreateShader(pCode, size, &desc, filename);
    }
} // namespace RenderingUtils