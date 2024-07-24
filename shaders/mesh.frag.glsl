#version 460

layout(location = 0) out vec4 fragColor;

layout(location = 0) in vec3 vNormal;
layout(location = 1) in vec2 vUV;
layout(location = 2) in flat uint drawId;

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

void main() {
    vec3 n = normalize(vNormal);
    fragColor = vec4(n, 1.0f);
}