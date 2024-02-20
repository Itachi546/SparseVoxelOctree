#pragma once

#include <vector>
#include <array>
#include "math-utils.h"

struct VoxelData;

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

struct RayHit {
    float t;
    bool intersect;
    glm::vec3 normal;
    uint32_t color;
    int iteration;
};

struct Octree {

    Octree(const glm::vec3 &center, float size);

    Octree(const char *filename);

    void Insert(OctreeBrick *brick);

    void ListVoxels(std::vector<glm::vec4> &voxels);

    void Generate(VoxelData *generator);

    void Serialize(const char *filename);

    RayHit Raycast(glm::vec3 r0, glm::vec3 rd);

    std::vector<Node> nodePools;

    std::vector<uint32_t> brickPools;

    glm::vec3 center;
    float size;

  private:
    void CreateChildren(uint32_t parent);
    void Insert(const glm::vec3 &center, float size, uint32_t parent, OctreeBrick *brick);
    void InsertBrick(uint32_t parent, OctreeBrick *brick);
    uint32_t FindRegion(const glm::vec3 &center, float size, const glm::vec3 &p);

    void ListVoxels(const glm::vec3 &center, float size, uint32_t parent, std::vector<glm::vec4> &voxels);
    void ListVoxelsFromBrick(const glm::vec3 &center, uint32_t brickPtr, std::vector<glm::vec4> &voxels);

    void Generate(VoxelData *generator, const glm::vec3 &min, float size, uint32_t parent);

    bool IsRegionEmpty(VoxelData *generator, const glm::vec3 &min, const glm::vec3 &max);

    RayHit RaycastDDA(const glm::vec3 &r0, const glm::vec3 &rd, const glm::vec3 &dirMask, uint32_t brickStart, std::vector<AABB> &traversedNodes);
    bool CreateBrick(VoxelData *voxels, OctreeBrick *brick, const glm::vec3 &min, float size);
    OctreeBrick temp;
};
