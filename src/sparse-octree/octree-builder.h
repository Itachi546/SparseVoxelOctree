#pragma once

#include <vector>
#include <glm/glm.hpp>

class OctreeBuilder {
  public:
    void Initialize();

    void Build();

  private:
    std::vector<uint32_t> voxelFragmentList;
    std::vector<uint32_t> octree;
    const uint32_t kDims = 64;
    const uint32_t kLevels = 7;

    void InitializeFragmentList();

    void InitNode();
    void FlagNode(uint32_t level);
    void AllocateNode();

    uint32_t allocationBegin = 0;
    uint32_t allocationCount = 1;

    inline glm::ivec3 convertToPositiveRange(glm::ivec3 p) {
        return p + int(kDims * 0.5f);
    }

    inline glm::ivec3 convertToFullRange(glm::ivec3 p) {
        return p - int(kDims * 0.5f);
    }
};