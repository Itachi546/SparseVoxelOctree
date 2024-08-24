#version 460

layout(local_size_x = 32, local_size_y = 1, local_size_z = 1) in;

layout(binding = 0, set = 0) buffer SparseOctreeBuffer {
    uint octree[];
};

layout(binding = 1, set = 0) readonly buffer VoxelFragmentBuffer {
    uint voxelFragments[];
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
    return position - uVoxelDims * 0.5f;
}

void main() {
    uint threadId = gl_GlobalInvocationID.x;
    if (threadId >= uVoxelCount)
        return;

    vec3 position = getPositionFromUint(voxelFragments[threadId]);

    uint childIndex = 0;
    uint node = octree[0];

    vec3 center = vec3(0.0f);
    bool bFlag = true;

    float halfDims = uVoxelDims * 0.5f;
    // Traverse upto given current level
    for (int i = 0; i < uLevel; ++i) {
        halfDims *= 0.5f;
        if ((node & 0x80000000) == 0) {
            bFlag = false;
            break;
        }

        childIndex += node & 0x7FFFFFFF;
        ivec3 region = ivec3(greaterThan(position, center));
        childIndex += region.x * 4 + region.y * 2 + region.z;
        center += (region * 2.0 - 1.0) * halfDims;
        node = octree[childIndex];
    }

    if (bFlag)
        octree[childIndex] |= 0x80000000;
}
