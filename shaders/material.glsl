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

#ifdef ENABLE_BINDLESS_SET
layout(binding = 10, set = 1) uniform sampler2D uTextures[];
#endif

vec4 sampleTexture(uint textureId, vec2 uv) {
    return texture(uTextures[nonuniformEXT(textureId)], uv);
}

vec4 sampleTextureLOD(uint textureId, vec2 uv, float lod) {
    return textureLod(uTextures[nonuniformEXT(textureId)], uv, lod);
}
