#include "octree.h"

#include "voxel-data.h"
#include <glm/gtx/component_wise.hpp>

#include <execution>
#include <iostream>
#include <fstream>
#include <stack>

static Node CreateNode(NodeMask nodeMask, uint32_t childPtr = 0) {
    return Node{childPtr | (nodeMask << 30)};
}

Octree::Octree(const glm::vec3 &center, float size) : center(center), size(size) {
    nodePools.push_back(CreateNode(NodeMask::InternalLeafNode));
}

void Octree::CreateChildren(uint32_t parent) {
    uint32_t firstChild = static_cast<uint32_t>(nodePools.size());
    nodePools[parent] = CreateNode(NodeMask::InternalNode, firstChild);
    for (int i = 0; i < 8; ++i)
        nodePools.push_back(CreateNode(InternalLeafNode, 0));
}

void Octree::InsertBrick(uint32_t parent, OctreeBrick *brick) {
    Node node = nodePools[parent];
    uint32_t mask = node.GetNodeMask();
    if (brick->IsConstantValue()) {
        if (mask == NodeMask::LeafNodeWithPtr) {
            // @TODO reuse the free brick
        }
        node = CreateNode(NodeMask::LeafNode, brick->data[0]);
    } else {
        if (mask == NodeMask::LeafNode || NodeMask::InternalNode) {
            uint32_t address = static_cast<uint32_t>((brickPools.size() / BRICK_ELEMENT_COUNT));
            node = CreateNode(NodeMask::LeafNodeWithPtr, address);
        }
        uint32_t address = nodePools[parent].GetChildPtr();
        brickPools.insert(brickPools.end(), brick->data.begin(), brick->data.end());
    }
    nodePools[parent] = node;
}

void Octree::Insert(const glm::vec3 &center, float size, uint32_t parent, OctreeBrick *brick) {
    uint32_t nodeMask = nodePools[parent].GetNodeMask();
    float halfSize = size * 0.5f;

    switch (nodeMask) {
    case InternalLeafNode: {
        if (size >= LEAF_NODE_SCALE) {
            CreateChildren(parent);
            uint32_t region = FindRegion(center, size, brick->position);
            uint32_t childIndex = nodePools[parent].GetChildPtr() + region;
            glm::vec3 childPos = center + DIRECTIONS[region] * halfSize;
            Insert(childPos, halfSize, childIndex, brick);
        } else
            InsertBrick(parent, brick);
        break;
    }
    case InternalNode: {
        uint32_t region = FindRegion(center, size, brick->position);
        uint32_t childIndex = nodePools[parent].GetChildPtr() + region;
        glm::vec3 childPos = center + DIRECTIONS[region] * halfSize;
        Insert(childPos, halfSize, childIndex, brick);
        break;
    }
    case LeafNode:
    case LeafNodeWithPtr:
        InsertBrick(parent, brick);
        break;
    }
}

void Octree::Insert(OctreeBrick *brick) {
    Insert(center, size, 0, brick);
}

uint32_t Octree::FindRegion(const glm::vec3 &center, float size, const glm::vec3 &p) {
    float halfSize = size * 0.5f;
    for (int i = 0; i < 8; ++i) {
        glm::vec3 childPos = center + DIRECTIONS[i] * halfSize;
        AABB aabb{childPos - halfSize, childPos + halfSize};
        if (aabb.ContainPoint(p))
            return i;
    }

    assert(0);
    return ~0u;
}

void Octree::ListVoxels(std::vector<glm::vec4> &voxels) {
    ListVoxels(center, size, 0, voxels);
}

void Octree::ListVoxels(const glm::vec3 &center, float size, uint32_t parent, std::vector<glm::vec4> &voxels) {
    Node node = nodePools[parent];
    uint32_t nodeMask = node.GetNodeMask();
    float halfSize = size * 0.5f;

    if (nodeMask == NodeMask::InternalNode) {
        uint32_t firstChild = node.GetChildPtr();
        for (int i = 0; i < 8; ++i) {
            glm::vec3 childPos = center + DIRECTIONS[i] * halfSize;
            ListVoxels(childPos, halfSize, firstChild + i, voxels);
        }
    } else if (nodeMask == NodeMask::LeafNode) {
        voxels.push_back(glm::vec4(center, size));
    } else if (nodeMask == NodeMask::LeafNodeWithPtr) {
        uint32_t brickPtr = node.GetChildPtr() * BRICK_ELEMENT_COUNT;
        ListVoxelsFromBrick(center, brickPtr, voxels);
    }
}

void Octree::ListVoxelsFromBrick(const glm::vec3 &center, uint32_t brickPtr, std::vector<glm::vec4> &voxels) {
    float voxelHalfSize = (LEAF_NODE_SCALE / float(BRICK_SIZE)) * 0.5f;
    glm::vec3 min = center - LEAF_NODE_SCALE * 0.5f;
    float size = float(LEAF_NODE_SCALE);
    for (int x = 0; x < BRICK_SIZE; ++x) {
        for (int y = 0; y < BRICK_SIZE; ++y) {
            for (int z = 0; z < BRICK_SIZE; ++z) {
                uint32_t offset = x * BRICK_SIZE * BRICK_SIZE + y * BRICK_SIZE + z;
                uint32_t val = brickPools[brickPtr + offset];
                if (val > 0) {
                    glm::vec3 t01 = glm::vec3(float(x), float(y), float(z)) / 16.0f;
                    glm::vec3 position = min + t01 * size;
                    // Calculate center and size
                    voxels.push_back(glm::vec4(position + voxelHalfSize, voxelHalfSize));
                }
            }
        }
    }
}

bool Octree::IsRegionEmpty(VoxelData *voxels, const glm::vec3 &min, const glm::vec3 &max) {
    glm::vec3 size = max - min;
    for (int x = 0; x < BRICK_SIZE; ++x) {
        for (int y = 0; y < BRICK_SIZE; ++y) {
            for (int z = 0; z < BRICK_SIZE; ++z) {
                glm::vec3 t01 = glm::vec3(float(x), float(y), float(z)) / 16.0f;
                glm::vec3 p = min + t01 * size;
                uint32_t val = voxels->Sample(p);
                if (val <= 0.0f) {
                    return false;
                }
            }
        }
    }
    return true;
}

void Octree::Generate(VoxelData *voxels) {
    Generate(voxels, center, size, 0);
}

bool Octree::CreateBrick(VoxelData *voxels, OctreeBrick *brick, const glm::vec3 &min, float size) {
    bool empty = true;
    for (int x = 0; x < BRICK_SIZE; ++x) {
        for (int y = 0; y < BRICK_SIZE; ++y) {
            for (int z = 0; z < BRICK_SIZE; ++z) {
                glm::vec3 t01 = glm::vec3(float(x), float(y), float(z)) / 16.0f;
                glm::vec3 p = min + t01 * size;
                uint32_t val = voxels->Sample(p);
                uint32_t index = x * BRICK_SIZE * BRICK_SIZE + y * BRICK_SIZE + z;
                if (val > 0) {
                    brick->data[index] = val;
                    empty = false;
                } else
                    brick->data[index] = 0;
            }
        }
    }
    return empty;
}

void Octree::Generate(VoxelData *voxels, const glm::vec3 &center, float size, uint32_t parent) {
    /*
    if (brickPools.size() > 0 || nodePools.size() > 0) {
        std::cerr << "Octree is already initialized" << std::endl;
        return;
    }
    */
    if (size < LEAF_NODE_SCALE) {
        glm::vec3 min = center - LEAF_NODE_SCALE * 0.5f;
        float size = float(LEAF_NODE_SCALE);
        bool empty = CreateBrick(voxels, &temp, min, size);
        temp.position = center;
        if (empty)
            nodePools[parent] = CreateNode(NodeMask::InternalLeafNode);
        else
            InsertBrick(parent, &temp);
        return;
    }

    glm::vec3 min = center - size;
    if (!IsRegionEmpty(voxels, center - size, center + size)) {
        CreateChildren(parent);
        float halfSize = size * 0.5f;

        uint32_t firstChild = nodePools[parent].GetChildPtr();
        for (int i = 0; i < 8; ++i) {
            glm::vec3 childPos = center + DIRECTIONS[i] * halfSize;
            Generate(voxels, childPos, halfSize, firstChild + i);
        }
    } else
        nodePools[parent] = CreateNode(NodeMask::InternalLeafNode);
}

/*
 * p = r0 + t * rd
 * t = (p - r0) / rd
 */
inline uint32_t dir2Index(glm::vec3 dir) {
    dir = dir * 0.5f + 0.5f;
    return int(dir.z) << 2 | int(dir.y) << 1 | int(dir.x);
}

bool Octree::Raycast(const Ray &ray) {
    // @TODO handle divide by zero
    glm::vec3 invRd = 1.0f / (ray.direction + EPSILON);
    glm::vec3 r0_rd = ray.origin / ray.direction;

    glm::vec3 min = center - size;
    glm::vec3 max = center + size;
    glm::vec3 tMin = min * invRd - r0_rd;
    glm::vec3 tMax = max * invRd - r0_rd;
    glm::vec2 t = {glm::compMax(tMin), glm::compMin(tMax)};

    if (t.x > t.y || t.y < 0.0f)
        return false;

    float h = t.y;
    glm::vec3 tMid = (tMin + tMax) * 0.5f;

    // Find entry node
    glm::vec3 idx = glm::mix(-glm::sign(ray.direction), glm::sign(ray.direction), glm::lessThanEqual(tMid, glm::vec3(t.x)));
    size *= 0.5f;
    glm::vec3 p = min + size * idx * 0.5f;
    int nodeId = dir2Index(idx);

    const int MAX_ITERATION = 1000;
    float currentSize = size;
    for (int i = 0; i < MAX_ITERATION; ++i) {
        float tCorner = glm::compMin(p * invRd - r0_rd);
        Node node = nodePools[nodeId];
        uint32_t nodeMask = node.GetNodeMask();

        if (nodeMask == NodeMask::LeafNode || nodeMask == NodeMask::LeafNodeWithPtr) {
            // @TODO calculate normals or brick march
            break;
        } else if (node.GetNodeMask() == NodeMask::InternalNode) {
            // @TODO we need to skip the node behind the camera
            // Push

            continue;
        }

        // Advance

        // Pop
    }
}

Octree::Octree(const char *filename) {
    std::ifstream inFile(filename, std::ios::binary);
    if (!inFile) {
        std::cerr << "Failed to load file: " << filename << std::endl;
        return;
    }
    inFile.read(reinterpret_cast<char *>(&center), sizeof(glm::vec3));
    inFile.read(reinterpret_cast<char *>(&size), sizeof(float));

    uint32_t nodeCount = 0;
    inFile.read(reinterpret_cast<char *>(&nodeCount), sizeof(uint32_t));
    if (nodeCount > 0) {
        nodePools.resize(nodeCount);
        inFile.read(reinterpret_cast<char *>(nodePools.data()), sizeof(uint32_t) * nodeCount);
    }

    uint32_t brickCount = 0;
    inFile.read(reinterpret_cast<char *>(&brickCount), sizeof(uint32_t));
    if (brickCount > 0) {
        uint32_t brickElementCount = brickCount * BRICK_ELEMENT_COUNT;
        brickPools.resize(brickElementCount);
        inFile.read(reinterpret_cast<char *>(brickPools.data()), sizeof(uint32_t) * brickElementCount);
    }
}

void Octree::Serialize(const char *filename) {
    std::ofstream outFile(filename, std::ios::binary);
    outFile.write(reinterpret_cast<const char *>(&center), sizeof(glm::vec3));
    outFile.write(reinterpret_cast<const char *>(&size), sizeof(float));

    uint32_t nodeCount = static_cast<uint32_t>(nodePools.size());
    outFile.write(reinterpret_cast<const char *>(&nodeCount), sizeof(uint32_t));
    outFile.write(reinterpret_cast<const char *>(nodePools.data()), sizeof(uint32_t) * nodeCount);

    uint32_t brickCount = static_cast<uint32_t>(brickPools.size() / BRICK_ELEMENT_COUNT);
    outFile.write(reinterpret_cast<const char *>(&brickCount), sizeof(uint32_t));
    outFile.write(reinterpret_cast<const char *>(brickPools.data()), sizeof(uint32_t) * brickCount * BRICK_ELEMENT_COUNT);
}
