#version 460
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_GOOGLE_include_directive : enable

#include "voxelizer.glsl"

layout(location = 0) in vec3 gWorldPos;

layout(binding = 4, set = 0) writeonly buffer VoxelFragmentCountBuffer {
    uint voxelCount[];
};

layout(r8, binding = 5, set = 0) uniform writeonly image3D uVoxelTexture;

void main() {
    ivec3 voxelCoord = ivec3(WorldToTextureSpace(gWorldPos));
    atomicAdd(voxelCount[0], 1);
    imageStore(uVoxelTexture, voxelCoord, vec4(1.0f));
}