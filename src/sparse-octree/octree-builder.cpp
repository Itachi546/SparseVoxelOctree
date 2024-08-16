#include "pch.h"

#include "octree-builder.h"

void OctreeBuilder::Initialize() {
}

void OctreeBuilder::Build() {
    InitializeFragmentList();
    octree.resize(voxelFragmentList.size() * 10 * sizeof(uint32_t));
    for (uint32_t i = 0; i < kLevels; ++i) {
        InitNode();
        FlagNode(i);
        AllocateNode();
    }
}

void OctreeBuilder::InitializeFragmentList() {

    for (uint32_t x = 0; x < kDims; ++x) {
        for (uint32_t y = 0; y < kDims; ++y) {
            for (uint32_t z = 0; z < kDims; ++z) {
                float d = glm::length(glm::vec3(x, y, z) - glm::vec3(kDims * 0.5f)) - 15.0f;
                if (d <= 0.0f) {
                    uint32_t position = (x << 20) | (y << 10) | z;
                    voxelFragmentList.push_back(position);
                }
            }
        }
    }
}

void OctreeBuilder::InitNode() {
    for (uint32_t i = allocationBegin; i < allocationCount; ++i) {
        octree[i] = 0;
    }
}

void OctreeBuilder::FlagNode(uint32_t level) {
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
            childIndex = node & 0x7FFFFFFF;

            glm::bvec3 region = glm::greaterThan(position, center);
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

void OctreeBuilder::AllocateNode() {
    uint32_t totalAllocated = 0;

    for (uint32_t i = 0; i < allocationCount; ++i) {
        uint32_t currentNode = allocationBegin + i;
        if ((octree[currentNode] & 0x80000000)) {
            uint32_t allocationIndex = totalAllocated + allocationCount;
            // Store the offset from current node to child
            octree[currentNode] |= allocationIndex - currentNode;
            totalAllocated += 8;
        }
    }

    allocationBegin += allocationCount;
    allocationCount = totalAllocated;
}
