#version 460
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_GOOGLE_include_directive : enable

#define ENABLE_BINDLESS_SET
#include "material.glsl"

layout(location = 0) in vec3 gWorldPos;
layout(location = 1) in vec2 gUV;
layout(location = 2) in flat uint gDrawId;
layout(location = 3) in vec3 gVoxelWorldPos;

layout(binding = 5, set = 0) writeonly buffer VoxelFragmentBuffer {
    uint voxelFragmentList;
};

layout(r8, binding = 6, set = 0) uniform writeonly image3D uVoxelTexture;

void main() {
    atomicAdd(voxelFragmentList, 1);

    // WorldPos range from the -VOXEL_SIZE to VOXEL_SIZE 
    ivec3 voxelCoord = ivec3(gVoxelWorldPos);
    imageStore(uVoxelTexture, voxelCoord, vec4(0.5f));
}