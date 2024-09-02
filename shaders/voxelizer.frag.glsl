#version 460
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_GOOGLE_include_directive : enable
#extension GL_ARB_gpu_shader_int64 : enable

#define ENABLE_BINDLESS_SET
#include "material.glsl"
#include "voxelizer.glsl"

layout(location = 0) in vec3 gWorldPos;
layout(location = 1) in vec2 gUV;
layout(location = 2) in flat uint gDrawID;

layout(binding = 5, set = 0) writeonly buffer VoxelFragmentCountBuffer {
    uint voxelCount[];
};

layout(binding = 6, set = 0) writeonly buffer VoxelFragmentBuffer {
    uint64_t voxelFragment[];
};

layout(rgba8, binding = 7, set = 0) uniform writeonly image3D voxelTexture;

void main() {
    uint index = atomicAdd(voxelCount[1], 1);

    Material material = materials[gDrawID];
    vec4 diffuseColor;
    if (material.albedoMap != INVALID_TEXTURE) {
        diffuseColor = sampleTextureLOD(material.albedoMap, gUV, 0.0f);
    } else
        diffuseColor = material.albedo;

    if (diffuseColor.a < 0.5)
        discard;

    uint color = packUnorm4x8(diffuseColor);

    ivec3 vp = ivec3(WorldToTextureSpace(gWorldPos));
    // voxelFragment[index] = (vp.x << 20) | (vp.y << 10) | vp.z;
    voxelFragment[index] = uint64_t(color) << 40 |
                           uint64_t(vp.z) << 24 |
                           uint64_t(vp.y) << 12 |
                           uint64_t(vp.x);

    imageStore(voxelTexture, vp, vec4(diffuseColor.rgb, 1.0f));
}