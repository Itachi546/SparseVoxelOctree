#version 460

layout(local_size_x = 32, local_size_y = 1, local_size_z = 1) in;

layout(binding = 0, set = 0) readonly buffer VoxelFragmentBuffer {
    uint voxelFragments[];
};

layout(binding = 1, set = 1) buffer SparseOctreeBuffer {
    uint octree[];
};

layout(push_constant) uniform PushConstants {
    uint uVoxelCount;
    uint uLevel;
    uint uVoxelDims;
};

vec3 getPositionFromUint(uint p) {
    vec3 position;
    position.x = (p >> 20) & 0x3ff;
    position.y = (p >> 10) & 0x3ff;
    position.z = p & 0x3ff;
    return position;
}

void main() {
    uint threadId = gl_GlobalInvocationID.x;
    if (threadId >= uVoxelCount)
        return;

    uint voxelPosition = voxelFragments[threadId];

    vec3 halfDims = vec3(uVoxelDims * 0.5f);

    uint childPtr = 0;
    uint rootNode = octree[0];

    // Traverse upto given current level
    for (int i = 0; i < uLevel; ++i) {
    }
}
