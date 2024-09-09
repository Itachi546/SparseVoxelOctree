#version 460

#extension GL_GOOGLE_include_directive : enable
#extension GL_ARB_gpu_shader_int64 : enable

#include "terrain.glsl"

layout(local_size_x = 8, local_size_y = 8, local_size_z = 8) in;

layout(set = 0, binding = 0) writeonly buffer VoxelCountBuffer {
    uint voxelCount[];
};

layout(set = 0, binding = 1) writeonly buffer VoxelFragmentList {
    uint64_t voxelFragmentList[];
};

layout(push_constant) uniform PushConstant {
    uint voxelResolution;
};

void main() {
    ivec3 p = ivec3(gl_GlobalInvocationID.xyz);
    float d = GetNoise(p - voxelResolution * 0.5f);
    if (d <= 0.0f) {
        uint index = atomicAdd(voxelCount[1], 1);
        uint color = 0xffffff;
        voxelFragmentList[index] = uint64_t(color) << 40 |
                                   uint64_t(p.z) << 24 |
                                   uint64_t(p.y) << 12 |
                                   uint64_t(p.x);
    }
}