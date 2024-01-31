#pragma once

#include <vector>
#include <array>
#include "math-utils.h"

constexpr glm::vec3 DIRECTIONS[] = {
    glm::vec3(-1.0f, -1.0f, -1.0f), // 0 0 0
    glm::vec3(1.0f, -1.0f, -1.0f),  // 0 0 1
    glm::vec3(-1.0f, 1.0f, -1.0f),  // 0 1 0
    glm::vec3(1.0f, 1.0f, -1.0f),   // 0 1 1
    glm::vec3(-1.0f, -1.0f, 1.0f),  // 1 0 0
    glm::vec3(1.0f, -1.0f, 1.0f),   // 1 0 1
    glm::vec3(-1.0f, 1.0f, 1.0f),   // 1 1 0
    glm::vec3(1.0f, 1.0f, 1.0f)     // 1 1 1
};

const glm::vec3 COLORS[] = {
    glm::vec3(1.0f, 0.0f, 0.0f),
    glm::vec3(0.0f, 1.0f, 0.0f),
    glm::vec3(0.0f, 0.0f, 1.0f),
    glm::vec3(1.0f, 0.0f, 1.0f),
};

enum NodeMask {
    InternalLeafNode = 0,
    InternalNode = 1,
    LeafNode = 2,
    LeafNodeWithPtr = 3
};

struct Node {
    uint32_t value;

    uint32_t GetNodeMask() const {
        return value >> 30;
    }

    uint32_t GetChildPtr() const {
        return value & 0x3fffffff;
    }
};

constexpr const uint32_t BRICK_SIZE = 16;
constexpr const uint32_t BRICK_ELEMENT_COUNT = BRICK_SIZE * BRICK_SIZE * BRICK_SIZE;
constexpr const uint32_t LEAF_NODE_SCALE = 4;

struct OctreeBrick {
    std::array<uint32_t, BRICK_ELEMENT_COUNT> data;
    glm::vec3 position;

    bool IsConstantValue() const {
        uint32_t value = data[0];
        for (int i = 1; i < data.size(); ++i) {
            if (value != data[i])
                return false;
        }
        return true;
    }
};

struct Octree {

    Octree(const glm::vec3 &center, float size);

    void Insert(OctreeBrick *brick);

    void GenerateVoxels(std::vector<glm::vec4> &voxels);

    std::vector<Node> nodePools;

    std::vector<uint32_t> brickPools;

    glm::vec3 center;
    float size;

  private:
    void CreateChildren(uint32_t parent);
    void Insert(const glm::vec3 &center, float size, uint32_t parent, OctreeBrick *brick);
    void InsertBrick(uint32_t parent, OctreeBrick *brick);
    uint32_t FindRegion(const glm::vec3 &center, float size, const glm::vec3 &p);

    void GenerateVoxels(const glm::vec3 &center, float size, uint32_t parent, std::vector<glm::vec4> &voxels);
    void GenerateVoxelsFromBrick(const glm::vec3 &center, uint32_t brickPtr, std::vector<glm::vec4> &voxels);
};
