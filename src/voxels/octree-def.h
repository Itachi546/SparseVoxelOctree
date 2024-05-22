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

constexpr const uint32_t NUM_BRICK = 8;
constexpr const uint32_t NUM_BRICK2 = NUM_BRICK * NUM_BRICK;
constexpr const uint32_t LEAF_NODE_SCALE = 1;
constexpr const float BRICK_GRID_SIZE = float(LEAF_NODE_SCALE) / float(NUM_BRICK);

struct OctreeBrick {
    uint64_t occupancy[8] = {0ull, 0ull, 0ull, 0ull, 0ull, 0ull, 0ull, 0ull};

    bool IsConstantValue() const {
        for (int i = 0; i < 8; ++i) {
            if (occupancy[i] > 0)
                return false;
        }
        return true;
    }

    void SetNonEmpty(uint32_t index) {
        uint32_t layer = index / NUM_BRICK2;
        uint32_t offset = index % NUM_BRICK2;

        assert(layer < 8 && offset < 64);
        occupancy[layer] |= (1ull << (NUM_BRICK2 - 1ull - offset));
    }

    bool IsEmpty(uint32_t index) {
        uint32_t layer = index / NUM_BRICK2;
        uint32_t offset = index % NUM_BRICK2;
        uint64_t mask = (1ull << (NUM_BRICK2 - 1ull - offset));

        assert(layer < 8 && offset < 64);
        return (occupancy[layer] & mask) == mask;
    }
};
