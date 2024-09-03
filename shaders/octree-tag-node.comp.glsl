#version 460

#extension GL_ARB_gpu_shader_int64 : enable

layout(local_size_x = 32, local_size_y = 1, local_size_z = 1) in;

layout(binding = 0, set = 0) buffer SparseOctreeBuffer {
    uint octree[];
};

layout(binding = 1, set = 0) readonly buffer VoxelFragmentBuffer {
    uint64_t voxelFragments[];
};

layout(push_constant) uniform PushConstants {
    uint uVoxelCount;
    uint uLevel;
    uint uVoxelDims;
};

vec3 getPositionFromUint(uint64_t voxel) {
    vec3 position;
    /*
    position.x = float((p >> 20) & 0x3ff);
    position.y = float((p >> 10) & 0x3ff);
    position.z = float(p & 0x3ff);
    */

    position.x = float(voxel & 0xfff);
    position.y = float((voxel >> 12) & 0xfff);
    position.z = float((voxel >> 24) & 0xfff);
    return position - uVoxelDims * 0.5f;
}

uint getColorFromUint(uint64_t voxel) {
    return uint((voxel >> 40) & 0xffffff);
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

        childIndex += node & 0x3FFFFFFF;
        ivec3 region = ivec3(greaterThanEqual(position, center));
        childIndex += region.x * 4 + region.y * 2 + region.z * 1;
        center += (region * 2.0 - 1.0) * halfDims;
        node = octree[childIndex];
    }

    if (bFlag) {
        const float leafNodeLevel = log2(uVoxelDims);
        if (uLevel == uint(leafNodeLevel)) {
            uint col = getColorFromUint(voxelFragments[threadId]);
            col |= 0x40000000;
            atomicExchange(octree[childIndex], col);
        } else
            octree[childIndex] |= 0x80000000;
    }
}
