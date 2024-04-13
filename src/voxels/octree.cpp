#include "octree.h"

#include "voxel-data.h"
#include "gfx/debug.h"

#include <glm/gtx/component_wise.hpp>
#include <execution>
#include <iostream>
#include <fstream>
#include <stack>

#include "gfx/debug.h"

#define DEBUG_OCTREE_TRAVERSAL 0

inline Node CreateNode(NodeMask nodeMask, uint32_t childPtr = 0) {
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
    float voxelHalfSize = (LEAF_NODE_SCALE / float(NUM_BRICK)) * 0.5f;
    glm::vec3 min = center - LEAF_NODE_SCALE * 0.5f;
    float size = float(LEAF_NODE_SCALE);
    for (int x = 0; x < NUM_BRICK; ++x) {
        for (int y = 0; y < NUM_BRICK; ++y) {
            for (int z = 0; z < NUM_BRICK; ++z) {
                uint32_t offset = x * NUM_BRICK * NUM_BRICK + y * NUM_BRICK + z;
                uint32_t val = brickPools[brickPtr + offset];
                if (val > 0) {
                    glm::vec3 t01 = glm::vec3(float(x), float(y), float(z)) / float(NUM_BRICK);
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
    for (int x = 0; x < 32; ++x) {
        for (int y = 0; y < 32; ++y) {
            for (int z = 0; z < 32; ++z) {
                glm::vec3 t01 = glm::vec3(float(x), float(y), float(z)) / 31.0f;
                glm::vec3 p = min + t01 * size;
                uint32_t val = voxels->Sample(p);
                if (val > 0) {
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
    for (int x = 0; x < NUM_BRICK; ++x) {
        for (int y = 0; y < NUM_BRICK; ++y) {
            for (int z = 0; z < NUM_BRICK; ++z) {
                glm::vec3 t01 = glm::vec3(float(x), float(y), float(z)) / float(NUM_BRICK - 1);
                glm::vec3 p = min + t01 * size;
                uint32_t val = voxels->Sample(p);
                uint32_t index = x * NUM_BRICK * NUM_BRICK + y * NUM_BRICK + z;
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

int FindEntryNode(glm::vec3 &p, float scale, float t, glm::vec3 tM) {
    glm::ivec3 mask = glm::vec3(glm::step(glm::vec3(t), tM));
    p -= glm::vec3(mask) * scale;
    return mask.x | (mask.y << 1) | (mask.z << 2);
}

#define REFLECT(p, c) 2.0f * c - p
struct StackData {
    glm::vec3 position;
    uint32_t firstSibling;
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

RayHit Octree::RaycastDDA(const glm::vec3 &r0, const glm::vec3 &invRd, const glm::vec3 &dirMask, uint32_t brickStart, std::vector<AABB> &traversedNodes) {
    const int gridEndMargin = NUM_BRICK - 1;
    glm::vec3 stepDir = mix(glm::vec3(-1.0f), glm::vec3(1.0f), dirMask);
    glm::vec3 tStep = invRd;

    glm::vec3 p = floor(r0);
    p = glm::mix(glm::vec3(gridEndMargin) - p, p, dirMask);

    RayHit rayHit;
    rayHit.intersect = false;
    rayHit.t = 0.0f;
    rayHit.iteration = 0;
    glm::vec3 t = (1.0f - fract(r0)) * tStep;

    glm::vec3 nearestAxis = glm::step(t, glm::vec3(t.y, t.z, t.x)) *
                            glm::step(t, glm::vec3(t.z, t.x, t.y));

    for (int i = 0; i < 100; ++i) {
#if DEBUG_OCTREE_TRAVERSAL
        glm::vec3 dp = glm::mix(glm::vec3(gridEndMargin) - p, p, dirMask);
        traversedNodes.push_back(AABB{dp, dp + 1.0f});
#endif
        uint32_t voxelIndex = brickStart + uint32_t(p.x * NUM_BRICK * NUM_BRICK + p.y * NUM_BRICK + p.z);
        uint32_t color = brickPools[voxelIndex];
        if (color > 0) {
            // Undo the reflection
            p = glm::mix(glm::vec3(gridEndMargin) - p, p, dirMask);
            glm::vec3 t = (p - r0) * tStep;
            float tMax = glm::max(glm::max(t.x, t.y), t.z);
            rayHit.normal = -nearestAxis;
            rayHit.t = tMax * BRICK_GRID_SIZE;
            rayHit.intersect = true;
            rayHit.color = color;
            rayHit.iteration = i;
            return rayHit;
        }

        nearestAxis = glm::step(t, glm::vec3(t.y, t.z, t.x)) *
                      glm::step(t, glm::vec3(t.z, t.x, t.y));
        p += nearestAxis * stepDir;
        t += nearestAxis * tStep;
        if (p.x < 0.0f || p.x > gridEndMargin || p.y < 0.0f || p.y > gridEndMargin || p.z < 0.0f || p.z > gridEndMargin)
            break;
    }
    return rayHit;
}

RayHit Octree::Raycast(glm::vec3 r0, glm::vec3 rd) {
    glm::vec3 aabbMin = center - size;
    glm::vec3 aabbMax = center + size;
    glm::vec3 center = (aabbMin + aabbMax) * 0.5f;

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

    glm::vec3 d = glm::abs(rd);
    glm::vec3 invRayDir = 1.0f / (d + EPSILON);
    glm::vec3 r0_rd = r0 * invRayDir;

    glm::vec3 tMin = aabbMin * invRayDir - r0_rd;
    glm::vec3 tMax = aabbMax * invRayDir - r0_rd;

    RayHit rayHit;
    rayHit.intersect = false;
    rayHit.iteration = 0;

    glm::vec2 t = glm::vec2(glm::max(tMin.x, glm::max(tMin.y, tMin.z)),
                            glm::min(tMax.x, glm::min(tMax.y, tMax.z)));
    if (t.x > t.y || t.y < 0.0)
        return rayHit;

    float currentSize = size;
    float octreeSize = currentSize;

    // Determine Entry Node
    glm::vec3 p = aabbMax;
    glm::vec3 tM = (tMin + tMax) * 0.5f;

    bool processChild = true;
    int idx = FindEntryNode(p, currentSize, t.x, tM);

    uint32_t firstSibling = nodePools[0].GetChildPtr();
    int i = 0;
    std::stack<StackData> stacks;
    for (; i < 1000; ++i) {
#if DEBUG_OCTREE_TRAVERSAL
        glm::vec3 dMin = Reflect(p - currentSize, center, rd);
        glm::vec3 dMax = Reflect(p, center, rd);
        uint32_t depth = static_cast<uint32_t>(stacks.size() % std::size(COLORS));
        Debug::AddRect(dMin, dMax, COLORS[depth]);
#endif
        uint32_t nodeIndex = firstSibling + (idx ^ octaneMask);
        Node nodeDescriptor = nodePools[nodeIndex];
        glm::vec3 tCorner = p * invRayDir - r0_rd;
        float tcMax = glm::min(glm::min(tCorner.x, tCorner.y), tCorner.z);

        if (processChild && tcMax >= 0.0f) {
            uint32_t mask = nodeDescriptor.GetNodeMask();
            if (mask == LeafNode) {
                glm::vec3 dir = p - (r0 + t.x * d);
                rayHit.normal = -glm::step(glm::vec3(dir.y, dir.z, dir.x), dir) *
                                glm::step(glm::vec3(dir.z, dir.x, dir.y), dir) * sign(rd);
                rayHit.intersect = true;
                rayHit.t = t.x;
                rayHit.color = nodeDescriptor.GetChildPtr();
                break;
            } else if (mask == LeafNodeWithPtr) {
                uint32_t brickPointer = nodeDescriptor.GetChildPtr() * BRICK_ELEMENT_COUNT;
                glm::vec3 intersectPos = r0 + glm::max(t.x, 0.0f) * d;
                glm::vec3 brickMax = glm::vec3(NUM_BRICK);
                glm::vec3 brickPos = Remap<glm::vec3>(intersectPos, p - currentSize, p, glm::vec3(0.0f), brickMax);
                brickPos = glm::clamp(brickPos, glm::vec3(0.0f), brickMax);

                std::vector<AABB> traversedNodes;
                RayHit brickHit = RaycastDDA(brickPos, invRayDir, glm::ivec3(glm::greaterThan(rd, glm::vec3(0.0f))), brickPointer, traversedNodes);
#if DEBUG_OCTREE_TRAVERSAL
                for (auto &node : traversedNodes) {
                    glm::vec3 nodeMin = Reflect(Remap<glm::vec3>(node.min, glm::vec3(0.0f), brickMax, p - currentSize, p), center, rd);
                    glm::vec3 nodeMax = Reflect(Remap<glm::vec3>(node.max, glm::vec3(0.0f), brickMax, p - currentSize, p), center, rd);
                    Debug::AddRect(nodeMin, nodeMax);
                }
#endif
                if (brickHit.intersect) {
                    // Debug draw nodes
                    rayHit.normal = brickHit.normal * sign(rd);
                    rayHit.intersect = true;
                    rayHit.t = glm::max(t.x, 0.0f) + brickHit.t;
                    rayHit.color = brickHit.color;
                    rayHit.iteration = brickHit.iteration;
                    break;
                }
            } else if (mask == InternalNode) {

                // Intersect with t-span of cube
                float tvMax = glm::min(t.y, tcMax);
                float halfSize = currentSize * 0.5f;
                glm::vec3 tCenter = (p - halfSize) * invRayDir - r0_rd;

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
        glm::ivec3 stepDir = glm::ivec3(step(tCorner, glm::vec3(tcMax)));
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

        if (currentSize > octreeSize)
            break;
    }

    rayHit.iteration += i;
    return rayHit;
}
