#version 460
#extension GL_EXT_nonuniform_qualifier : enable

layout(location = 0) in vec3 vNormal;
layout(location = 1) in vec2 vUV;
layout(location = 2) in flat uint drawId;
layout(location = 3) in vec3 vWorldPos;

struct Material {
    vec4 albedo;
    vec4 emissive;

    float metallic;
    float roughness;
    float ao;
    float transparency;

    uint albedoMap;
    uint emissiveMap;
    uint metallicRoughnessMap;
    uint padding_;
};

layout(binding = 3, set = 1) readonly buffer Materials {
    Material materials[];
};

layout(binding = 4, set = 1) writeonly buffer VoxelFragmentBuffer {
    uint voxelFragmentList[];
};

layout(binding = 0, set = 2) uniform sampler2D uTextures[];

#define INVALID_TEXTURE ~0u

vec4 sampleTexture(uint textureId, vec2 uv) {
    return texture(uTextures[nonuniformEXT(textureId)], uv);
}

void main() {
    vec3 n = normalize(vNormal);
    Material material = materials[drawId];

    vec4 diffuseColor;
    if (material.albedoMap != INVALID_TEXTURE) {
        diffuseColor = sampleTexture(material.albedoMap, vUV);
    } else
        diffuseColor = vec4(1.0f);
    if (diffuseColor.a < 0.5)
        discard;
}