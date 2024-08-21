#version 460
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_GOOGLE_include_directive : enable

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
    uint voxelFragment[];
};

void main() {
    uint index = atomicAdd(voxelCount[1], 1);
    ivec3 vp = ivec3(WorldToTextureSpace(gWorldPos));
    voxelFragment[index] = (vp.x << 20) | (vp.y << 10) | vp.z;
}