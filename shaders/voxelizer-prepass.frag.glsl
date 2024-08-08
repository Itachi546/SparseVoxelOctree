#version 460
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_GOOGLE_include_directive : enable

// @TODO TEMP
layout(binding = 10, set = 1) uniform sampler2D uTextures[];

layout(location = 0) in vec3 gVoxelWorldPos;

layout(binding = 4, set = 0) writeonly buffer VoxelFragmentCountBuffer {
    uint voxelCount[];
};

layout(r8, binding = 5, set = 0) uniform writeonly image3D uVoxelTexture;

void main() {
    atomicAdd(voxelCount[0], 1);
    ivec3 voxelCoord = ivec3(gVoxelWorldPos);
    imageStore(uVoxelTexture, voxelCoord, vec4(0.5f));
}