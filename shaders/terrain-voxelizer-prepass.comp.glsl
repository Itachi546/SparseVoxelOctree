#version 460

#extension GL_GOOGLE_include_directive : enable

#include "terrain.glsl"

layout(local_size_x = 8, local_size_y = 8, local_size_z = 8) in;

layout(set = 0, binding = 0) writeonly buffer VoxelCountBuffer {
    uint voxelCount[];
};

layout(push_constant) uniform PushConstant {
    uint voxelResolution;
};

void main() {
    ivec3 p = ivec3(gl_GlobalInvocationID.xyz);
    float d = GetNoise(p - voxelResolution * 0.5f);
    if (d <= 0.0f)
        atomicAdd(voxelCount[0], 1);
}