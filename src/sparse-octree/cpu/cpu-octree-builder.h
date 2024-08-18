#pragma once

#include <vector>
#include <glm/glm.hpp>

class CpuOctreeBuilder {
  public:
    void Initialize();

    void ListVoxels(std::vector<glm::vec4> &voxels);

    void DebugOctree(int level);

    void Build();

    const uint32_t kDims = 64;
    const uint32_t kLevels = 7;

  private:
    std::vector<uint32_t> voxelFragmentList;
    std::vector<uint32_t> octree;

    void InitializeFragmentList();

    void InitNode();
    void FlagNode(uint32_t level);
    void AllocateNode();

    void _ListVoxel(uint32_t parent, const glm::vec3 &center, float halfSize, uint32_t level, std::vector<glm::vec4> &voxels);
    void _DebugOctree(uint32_t parent, const glm::vec3 &center, float halfSize, uint32_t level, uint32_t debugLevel);

    uint32_t allocationBegin = 0;
    uint32_t allocationCount = 1;

    inline glm::ivec3 convertToPositiveRange(glm::ivec3 p) {
        return p + int(kDims * 0.5f);
    }

    inline glm::ivec3 convertToFullRange(glm::ivec3 p) {
        return p - int(kDims * 0.5f);
    }
};