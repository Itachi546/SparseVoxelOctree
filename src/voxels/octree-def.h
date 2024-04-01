#pragma once

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
    glm::vec3(1.0f, 0.5f, 0.5f),
};

enum NodeMask {
    InternalLeafNode = 0,
    InternalNode = 1,
    LeafNode = 2,
    LeafNodeWithPtr = 3
};

struct Node {
    uint32_t value;

    inline uint32_t GetNodeMask() const {
        return (value >> 30);
    }

    inline uint32_t GetChildPtr() const {
        return (value & 0x3fffffff);
    }
};

constexpr const uint32_t BRICK_SIZE = 8;
constexpr const uint32_t BRICK_ELEMENT_COUNT = BRICK_SIZE * BRICK_SIZE * BRICK_SIZE;
constexpr const uint32_t LEAF_NODE_SCALE = 1;
constexpr const float BRICK_GRID_SIZE = float(LEAF_NODE_SCALE) / float(BRICK_SIZE);
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
