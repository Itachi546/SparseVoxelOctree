#include "pch.h"

#include "cpu-octree-utils.h"

static constexpr uint32_t LEAF_NODE_MASK = 0x40000000;
static constexpr uint32_t INTERNAL_NODE_MASK = 0x80000000;
static constexpr uint32_t CHILD_PTR_MASK = 0x3fffffff;
static constexpr uint32_t COLOR_MASK = 0xffffff;

namespace octree::utils {
    static void _ListVoxels(const std::vector<uint32_t> &octree, uint32_t nodeIndex, const glm::vec3 &center, float halfSize, std::vector<glm::vec4> &voxels) {
        uint32_t node = octree[nodeIndex];
        if ((node & LEAF_NODE_MASK) == LEAF_NODE_MASK) {
            uint32_t color = node & COLOR_MASK;
            voxels.push_back({center, color});
            return;
        }
        if ((node & 0x80000000)) {
            uint32_t childIndex = nodeIndex + (node & CHILD_PTR_MASK);
            float childHSize = halfSize * 0.5f;
            for (uint32_t i = 0; i < 8; ++i) {
                glm::vec3 region = {static_cast<float>(i & 1),
                                    static_cast<float>((i >> 1) & 1),
                                    static_cast<float>((i >> 2) & 1)};
                region = region * 2.0f - 1.0f;
                glm::vec3 childPos = center + region * childHSize;
                _ListVoxels(octree, childIndex + i, childPos, childHSize, voxels);
            }
        }
    }

    void ListVoxelsFromOctree(const std::vector<uint32_t> &octree, std::vector<glm::vec4> &outVoxels, float octreeDims) {
        _ListVoxels(octree, 0, glm::vec3(0.0f), octreeDims * 0.5f, outVoxels);
    }
} // namespace octree::utils
