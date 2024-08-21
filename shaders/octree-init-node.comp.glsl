#version 460

layout(local_size_x = 32, local_size_y = 1, local_size_z = 1) in;

layout(binding = 0, set = 0) buffer SparseOctreeBuffer {
    uint octree[];
};

// AllocationBegin and AllocationCount keep track of the previously
// allocated node and update it's child pointer when allocating new
// nodes
layout(binding = 0, set = 0) buffer OctreeBuildInfo {
    uint allocationBegin;
    uint allocationCount;
    uint totalAllocation;
    uint _padding;
};

void main() {
    uint id = gl_GlobalInvocationID.x;
    octree[allocationBegin + id] = 0;
}