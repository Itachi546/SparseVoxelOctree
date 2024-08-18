#include "pch.h"

#include "cpu-octree-builder.h"
#include "gfx/debug.h"

void CpuOctreeBuilder::Initialize() {
}

void CpuOctreeBuilder::Build() {
    InitializeFragmentList();
    octree.resize(voxelFragmentList.size() * 10 * sizeof(uint32_t));
    for (uint32_t i = 0; i < kLevels; ++i) {
        InitNode();
        FlagNode(i);
        AllocateNode();
    }
}

void CpuOctreeBuilder::InitializeFragmentList() {
    for (uint32_t x = 0; x < kDims; ++x) {
        for (uint32_t y = 0; y < kDims; ++y) {
            for (uint32_t z = 0; z < kDims; ++z) {
                glm::vec3 p = convertToFullRange(glm::vec3(x, y, z));
                float d = glm::length(p) - 20.0f;
                if (d < 0.0f) {
                    glm::ivec3 ip = convertToPositiveRange(p);
                    uint32_t up = (ip.x << 20) | (ip.y << 10) | ip.z;
                    voxelFragmentList.push_back(up);
                }
            }
        }
    }
}

void CpuOctreeBuilder::InitNode() {
    for (uint32_t i = allocationBegin; i < allocationCount; ++i) {
        octree[i] = 0;
    }
}

void CpuOctreeBuilder::FlagNode(uint32_t level) {
    for (int l = 0; l < voxelFragmentList.size(); ++l) {
        uint32_t p = voxelFragmentList[l];

        glm::vec3 position;
        position.x = static_cast<float>((p >> 20) & 0x3ff);
        position.y = static_cast<float>((p >> 10) & 0x3ff);
        position.z = static_cast<float>(p & 0x3ff);
        position = convertToFullRange(glm::ivec3(position));

        uint32_t childIndex = 0;
        uint32_t node = octree[childIndex];
        bool bFlag = true;

        glm::vec3 center = glm::vec3(0.0f);
        float nodeSize = static_cast<float>(kDims * 0.5f);
        for (uint32_t i = 0; i < level; ++i) {
            nodeSize *= 0.5f;
            if ((node & 0x80000000) == 0) {
                bFlag = false;
                break;
            }
            childIndex += node & 0x7FFFFFFF;

            glm::ivec3 region = glm::greaterThan(position, center);
            childIndex += region.z * 4 + region.y * 2 + region.x;

            center += (glm::vec3(region) * 2.0f - 1.0f) * nodeSize;
            node = octree[childIndex];
            // Select the region and find child index
            // if the child index is not marked, mark it
        }

        if (bFlag) {
            octree[childIndex] |= 0x80000000;
        }
    }
}

void CpuOctreeBuilder::AllocateNode() {
    uint32_t endPtr = allocationBegin + allocationCount;
    for (uint32_t i = 0; i < allocationCount; ++i) {
        uint32_t currentNode = allocationBegin + i;
        if ((octree[currentNode] & 0x80000000)) {
            uint32_t allocationIndex = endPtr;
            // Store the offset from current node to child
            octree[currentNode] |= allocationIndex - currentNode;
            endPtr += 8;
        }
    }

    allocationBegin += allocationCount;
    allocationCount = endPtr - allocationBegin;
}

glm::vec3 REGIONS[8] = {
    {-1.0f, -1.0f, -1.0f}, // 0, 0, 0  [0]
    {-1.0f, -1.0f, 1.0f},  // 0, 0, 1  [1]
    {-1.0f, 1.0f, -1.0f},  // 0, 1, 0  [2]
    {-1.0f, 1.0f, 1.0f},   // 0, 1, 1  [3]
    {1.0f, -1.0f, -1.0f},  // 1, 0, 0  [4]
    {1.0f, -1.0f, 1.0f},   // 1, 0, 1  [5]
    {1.0f, 1.0f, -1.0f},   // 1, 1, 0  [6]
    {1.0f, 1.0f, 1.0f},    // 1, 1, 1  [7]

};

void CpuOctreeBuilder::_ListVoxel(uint32_t nodeIndex, const glm::vec3 &position, float halfSize, uint32_t level, std::vector<glm::vec4> &voxels) {
    uint32_t node = octree[nodeIndex];
    if (level == kLevels - 1) {
        voxels.push_back({position, halfSize});
        return;
    }
    if ((node & 0x80000000)) {
        uint32_t childIndex = nodeIndex + (node & 0x7fffffff);
        float childHSize = halfSize * 0.5f;
        for (uint32_t i = 0; i < 8; ++i) {
            glm::vec3 childPos = position + REGIONS[i] * childHSize;
            _ListVoxel(childIndex + i, childPos, childHSize, level + 1, voxels);
        }
    }
}

static const uint32_t colors[4] = {
    0xff000000,
    0x00ff0000,
    0x0000ff00,
    0xff00ff00,
};

void CpuOctreeBuilder::_DebugOctree(uint32_t nodeIndex, const glm::vec3 &center, float halfSize, uint32_t level, uint32_t debugLevel) {
    uint32_t node = octree[nodeIndex];
    if (level == debugLevel) {
        Debug::AddRect(center - halfSize, center + halfSize, colors[debugLevel]);
    }
    if (level == kLevels - 1)
        return;

    if ((node & 0x80000000)) {
        uint32_t childIndex = nodeIndex + (node & 0x7fffffff);
        float childHSize = halfSize * 0.5f;
        for (uint32_t i = 0; i < 8; ++i) {
            glm::vec3 childPos = center + REGIONS[i] * childHSize;
            _DebugOctree(childIndex + i, childPos, childHSize, level + 1, debugLevel);
        }
    }
}

void CpuOctreeBuilder::ListVoxels(std::vector<glm::vec4> &voxels) {
    _ListVoxel(0, glm::vec3(0.0f), kDims * 0.5f, 0, voxels);
}

void CpuOctreeBuilder::DebugOctree(int level) {
    Debug::AddRect(glm::vec3(-20.0f), glm::vec3(20.0f));
    _DebugOctree(0, glm::vec3(0.0f), kDims * 0.5f, 0, level);
}
