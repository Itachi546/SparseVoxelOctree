#version 460

#extension GL_EXT_shader_explicit_arithmetic_types_int64 : enable
#extension GL_EXT_shader_atomic_int64 : enable

layout(binding = 3, set = 0) writeonly buffer VoxelFragmentBuffer {
    uint64_t voxelCount;
};

void main() {
    atomicAdd(voxelCount, 1);
}