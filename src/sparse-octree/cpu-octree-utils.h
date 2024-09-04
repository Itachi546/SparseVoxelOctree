#pragma once

#include <glm/glm.hpp>
#include <vector>

namespace octree::utils {

    void ListVoxelsFromOctree(const std::vector<uint32_t> &octree, std::vector<glm::vec4> &outVoxels, float octreeDims);
};