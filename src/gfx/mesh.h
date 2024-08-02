#pragma once

#include <glm/glm.hpp>

#include "rendering/rendering-device.h"

struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 uv;
};

struct MaterialInfo {
    glm::vec4 albedo;
    glm::vec4 emissive;

    float metallic;
    float roughness;
    float ao;
    float transparency;

    uint32_t albedoMap;
    uint32_t emissiveMap;
    uint32_t metallicRoughnessMap;
    uint32_t padding_;

    void Initialize() {
        albedo = {1.0f, 1.0f, 1.0f, 1.0f};
        emissive = {0.0f, 0.0f, 0.0f, 0.0f};

        metallic = 0.1f;
        roughness = 0.5f;
        ao = 1.0f;
        transparency = 1.0f;

        albedoMap = INVALID_TEXTURE_ID;
        emissiveMap = INVALID_TEXTURE_ID;
        metallicRoughnessMap = INVALID_TEXTURE_ID;
        padding_ = INVALID_TEXTURE_ID;
    }
};

struct MeshGroup {
    std::vector<glm::mat4> transforms;
    std::vector<MaterialInfo> materials;
    std::vector<std::string> names;
    std::vector<RD::DrawElementsIndirectCommand> drawCommands;

    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
};