#include "octree.h"

#include "voxel-data.h"
#include "gfx/debug.h"

#include <glm/gtx/component_wise.hpp>
#include <execution>
#include <iostream>
#include <fstream>
#include <stack>

#include "gfx/debug.h"

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
        uint32_t color = (brick->data[0] >> 8);
        node = CreateNode(NodeMask::LeafNode, color);
    } else {
        if (mask == NodeMask::LeafNode || NodeMask::InternalNode) {
            uint32_t address = static_cast<uint32_t>((brickPools.size() / BRICK_ELEMENT_COUNT));
            node = CreateNode(NodeMask::LeafNodeWithPtr, address);
        }
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

using namespace glm;
int FindEntryNode(vec3 &p, float scale, float t, vec3 tM) {
    ivec3 mask = vec3(step(vec3(t), tM));
    p -= glm::vec3(mask) * scale;
    return mask.x | (mask.y << 1) | (mask.z << 2);
}

#define REFLECT(p, c) 2.0f * c - p
struct StackData {
    vec3 position;
    uint firstSibling;
    int idx;
    float tMax;
    float size;
};

glm::vec3 Reflect(glm::vec3 p, const glm::vec3 &c, const glm::vec3 &dir) {
    if (dir.x < 0.0) {
        p.x = REFLECT(p.x, c.x);
    }
    if (dir.y < 0.0) {
        p.y = REFLECT(p.y, c.y);
    }
    if (dir.z < 0.0) {
        p.z = REFLECT(p.z, c.z);
    }
    return p;
}

RayHit Octree::RaycastDDA(const glm::vec3 &r0, const glm::vec3 &rd, int octaneMask, uint32_t brickStart, std::vector<AABB> &aabbs) {
    glm::vec3 stepDir = glm::sign(rd);
    glm::vec3 tStep = 1.0f / rd;
    glm::vec3 p = glm::floor(r0);

    glm::vec3 ip = p;
    if (!IsBitSet<int>(octaneMask, 0))
        ip.x = 15.0f - p.x;
    if (!IsBitSet<int>(octaneMask, 1))
        ip.y = 15.0f - p.y;
    if (!IsBitSet<int>(octaneMask, 2))
        ip.z = 15.0f - p.z;

    uint32_t voxelIndex = int(ip.x) * BRICK_SIZE * BRICK_SIZE + int(ip.y) * BRICK_SIZE + int(ip.z);
    RayHit rayHit = {false, 0.0f};
    if (brickPools[brickStart + voxelIndex] > 0) {
        rayHit.intersect = true;
        vec3 t = (p - r0) * tStep;
        rayHit.t = max(max(t.x, t.y), t.z);
        return rayHit;
    }

    vec3 t = (1.0f - fract(r0)) * tStep;

    const int iteration = 40;
    for (int i = 0; i < iteration; ++i) {
        glm::vec3 nearestAxis = glm::step(t, glm::vec3(t.y, t.z, t.x)) * glm::step(t, glm::vec3(t.z, t.x, t.y));
        p += nearestAxis * stepDir;
        t += nearestAxis * tStep;
        if (p.x < 0.0f || p.x > 15.0f || p.y < 0.0f || p.y > 15.0f || p.z < 0.0f || p.z > 15.0f)
            break;
        aabbs.push_back(AABB{p, p + 1.0f});
        glm::vec3 ip = p;
        if (!IsBitSet<int>(octaneMask, 0))
            ip.x = 15.0f - p.x;
        if (!IsBitSet<int>(octaneMask, 1))
            ip.y = 15.0f - p.y;
        if (!IsBitSet<int>(octaneMask, 2))
            ip.z = 15.0f - p.z;

        uint32_t voxelIndex = int(ip.x) * BRICK_SIZE * BRICK_SIZE + int(ip.y) * BRICK_SIZE + int(ip.z);
        if (brickPools[brickStart + voxelIndex] > 0) {
            rayHit.intersect = true;
            vec3 t = (p - r0) * tStep;
            rayHit.t = max(max(t.x, t.y), t.z);
            break;
        }
    }
    return rayHit;
}

bool Octree::Raycast(vec3 r0, vec3 rd, vec3 &intersection, vec3 &normal, std::vector<AABB> &aabb) {
    vec3 r0_orig = r0;
    int octaneMask = 7;
    if (rd.x < 0.0) {
        octaneMask ^= 1;
        r0.x = REFLECT(r0.x, center.x);
    }
    if (rd.y < 0.0) {
        octaneMask ^= 2;
        r0.y = REFLECT(r0.y, center.y);
    }
    if (rd.z < 0.0) {
        octaneMask ^= 4;
        r0.z = REFLECT(r0.z, center.z);
    }

    vec3 d = abs(rd);
    vec3 invRayDir = 1.0f / (d + EPSILON);
    vec3 r0_rd = r0 * invRayDir;

    glm::vec3 aabbMin = center - size;
    glm::vec3 aabbMax = center + size;
    vec3 tMin = aabbMin * invRayDir - r0_rd;
    vec3 tMax = aabbMax * invRayDir - r0_rd;

    vec2 t = vec2(max(tMin.x, max(tMin.y, tMin.z)), min(tMax.x, min(tMax.y, tMax.z)));
    if (t.x > t.y || t.y < 0.0)
        return false;

    float currentSize = size;

    // Determine Entry Node
    vec3 p = aabbMax;
    vec3 tM = (tMin + tMax) * 0.5f;

    bool processChild = true;
    int idx = FindEntryNode(p, currentSize, t.x, tM);

    uint32_t firstSibling = nodePools[0].GetChildPtr();
    vec3 col = vec3(0.5, 0.7, 1.0);
    vec3 n = vec3(0.0);
    int i = 0;
    std::stack<StackData> stacks;

    // @TEMP
    bool hasIntersect = false;
    for (; i < 1000; ++i) {
        {
            glm::vec3 dMin = Reflect(p - currentSize, center, rd);
            glm::vec3 dMax = Reflect(p, center, rd);
            // aabb.emplace_back(AABB{dMin, dMax});
        }

        uint32_t nodeIndex = firstSibling + (idx ^ octaneMask);
        Node nodeDescriptor = nodePools[nodeIndex];
        vec3 tCorner = p * invRayDir - r0_rd;
        float tcMax = min(min(tCorner.x, tCorner.y), tCorner.z);

        if (processChild) {
            uint32_t mask = nodeDescriptor.GetNodeMask();
            if (mask == LeafNode) {
                intersection = r0_orig + t.x * rd;
                hasIntersect = true;
                break;
            } else if (mask == LeafNodeWithPtr) {
                glm::vec3 dMin = Reflect(p - currentSize, center, rd);
                glm::vec3 dMax = Reflect(p, center, rd);
                aabb.emplace_back(AABB{dMin, dMax});
                glm::vec3 intersection = r0 + t.x * d;
                glm::vec3 brickMax = glm::vec3(BRICK_SIZE);
                glm::vec3 brickPos = Remap(intersection, p - currentSize, p, glm::vec3(0.0f), brickMax);
                brickPos = glm::max(brickPos, 0.0f);
                std::vector<AABB> intersectedNodes;
                RayHit rayHit = RaycastDDA(brickPos, d, octaneMask, nodeDescriptor.GetChildPtr() * BRICK_ELEMENT_COUNT, intersectedNodes);
                // Check intersection
                for (auto &grid : intersectedNodes) {
                    aabb.push_back(AABB{
                        Reflect(Remap(grid.min, glm::vec3(0.0f), brickMax, p - currentSize, p), center, rd),
                        Reflect(Remap(grid.max, glm::vec3(0.0f), brickMax, p - currentSize, p), center, rd),
                    });
                }

                if (rayHit.intersect) {
                    // Debug draw nodes
                    glm::vec3 bP = r0_orig + (rayHit.t * 0.25f + t.x) * rd;
                    // bP = Remap(bP, glm::vec3(0.0f), brickMax, p - currentSize, p);
                    // bP = Reflect(bP, center, rd);
                    aabb.push_back(AABB{bP - 0.05f, bP + 0.05f});
                    hasIntersect = true;
                    break;
                }
            } else if (mask == InternalNode) {

                // Intersect with t-span of cube
                float tvMax = min(t.y, tcMax);
                float halfSize = currentSize * 0.5f;
                vec3 tCenter = (p - halfSize) * invRayDir - r0_rd;

                if (t.x <= tvMax) {
                    StackData stackData;
                    stackData.position = p;
                    stackData.firstSibling = firstSibling;
                    stackData.idx = idx;
                    stackData.tMax = t.y;
                    stackData.size = currentSize;
                    stacks.push(stackData);
                }

                // Find Entry Node
                currentSize = halfSize;
                idx = FindEntryNode(p, currentSize, t.x, tCenter);
                firstSibling = nodeDescriptor.GetChildPtr();
                t.y = tvMax;
                continue;
            }
        }
        processChild = true;

        // Advance
        int lastSibling = idx ^ octaneMask;
        ivec3 stepDir = ivec3(step(tCorner, vec3(tcMax)));
        int stepMask = stepDir.x | (stepDir.y << 1) | (stepDir.z << 2);
        p += glm::vec3(stepDir) * currentSize;
        t.x = tcMax;
        idx ^= stepMask;
        // If the idx is set for corresponding direction and stepMask is also
        // in same direction then we are exiting parent
        // E.g.
        /*
         * +-----------+
         * |  10 |  11 |
         * +-----------+
         * |  00 |  01 |
         * +-----------+
         * if we are currently at 10 and stepMask is 01
         *  Conclusion: Valid because X is currently 0 and can take the step to 11
         * if we are currently at 10 and stepMask is 10
         *  Conclusion: Invalid because we have already taken a step in Y-Axis
         */

        if ((idx & stepMask) != 0) {
            // Pop operation
            if (stacks.size() > 0) {
                StackData stackData = stacks.top();
                stacks.pop();
                p = stackData.position;
                firstSibling = stackData.firstSibling;
                t.y = stackData.tMax;
                idx = stackData.idx;
                currentSize = stackData.size;
            } else
                currentSize = currentSize * 2.0f;
            processChild = false;
        }

        if (currentSize > size)
            break;
    }
    return hasIntersect;
}
