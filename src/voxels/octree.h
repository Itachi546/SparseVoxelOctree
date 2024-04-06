#pragma once

#include <vector>
#include <array>
#include "math-utils.h"
#include "octree-def.h"

struct VoxelData;

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
