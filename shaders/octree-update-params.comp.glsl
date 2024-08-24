#version 460

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;

layout(binding = 0, set = 0) buffer DispatchIndirectBuffer {
    uint workGroupX;
    uint workGroupY;
    uint workGroupZ;
};

layout(binding = 1, set = 0) buffer OctreeBuildInfo {
    uint allocationBegin;
    uint allocationCount;
    uint allocatedThisFrame;
};

void main() {
    uint id = gl_GlobalInvocationID.x;
    if (id > 0)
        return;

    allocationBegin = allocationBegin + allocationCount;
    allocationCount = allocatedThisFrame;
    allocatedThisFrame = 0;
    workGroupX = (allocationCount + 31) / 32;
}