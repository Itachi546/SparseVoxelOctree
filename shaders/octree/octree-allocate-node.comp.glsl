#version 460

layout(local_size_x = 32, local_size_y = 1, local_size_z = 1) in;

layout(binding = 0, set = 0) buffer SparseOctreeBuffer {
    uint octree[];
};

layout(binding = 1, set = 0) buffer OctreeBuildInfo {
    uint allocationBegin;
    uint allocationCount;
    uint allocationThisFrame;
};

void main() {
    uint id = gl_GlobalInvocationID.x;
    if (id > allocationCount)
        return;

    uint currentNode = allocationBegin + id;
    if ((octree[currentNode] & 0x80000000) != 0) {
        uint offset = atomicAdd(allocationThisFrame, 8);
        uint childPtr = allocationCount + offset - id;
        octree[currentNode] |= childPtr;
    }
}