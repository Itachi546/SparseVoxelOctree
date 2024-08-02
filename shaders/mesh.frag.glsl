#version 460
#extension GL_EXT_nonuniform_qualifier : enable

layout(location = 0) out vec4 fragColor;

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

layout(binding = 4, set = 0) readonly buffer Materials {
    Material materials[];
};

#define INVALID_TEXTURE ~0u

layout(binding = 10, set = 1) uniform sampler2D uTextures[];

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

    vec3 ld = normalize(vec3(0.1f, 1.0f, 0.5f));

    vec3 col = vec3(0.0f);
    col += max(dot(n, ld), 0.1f) * diffuseColor.rgb;
    col /= (1.0f + col);
    col = pow(col, vec3(0.4545));
    fragColor = vec4(col, diffuseColor.a);
}