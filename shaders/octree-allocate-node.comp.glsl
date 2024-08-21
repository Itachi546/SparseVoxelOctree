#version 460

layout(local_size_x = 32, local_size_y = 1, local_size_z = 1) in;

layout(binding = 0, set = 0) buffer SparseOctreeBuffer {
    uint octree[];
};

layout(binding = 0, set = 0) buffer OctreeBuildInfo {
    uint allocationBegin;
    uint allocationCount;

    uint totalAllocation;
    uint _padding;
};

void main() {
    uint id = gl_GlobalInvocationID.x;
    if (id > allocationCount)
        return;

    uint currentNode = allocationBegin + id;
    if ((octree[currentNode] & 0x80000000) != 0) {
        uint childPtr = totalAllocation - currentNode;
        octree[currentNode] |= childPtr;
        atomicAdd(totalAllocation, 8);
    }
    memoryBarrierShared();

    if (id == 0) {
        uint newAllocationBegin = allocationBegin + allocationCount;
        allocationBegin = newAllocationBegin;
        allocationCount = totalAllocation - newAllocationBegin;
    }
}