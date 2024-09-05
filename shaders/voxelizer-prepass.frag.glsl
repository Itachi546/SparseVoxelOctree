#version 460
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_GOOGLE_include_directive : enable

#include "voxelizer.glsl"

layout(location = 0) in vec3 gPos01;

layout(push_constant) uniform PushConstant {
    layout(offset = 8) uint uVoxelResolution;
};

layout(binding = 4, set = 0) writeonly buffer VoxelFragmentCountBuffer {
    uint voxelCount[];
};

void main() {
    atomicAdd(voxelCount[0], 1);
}