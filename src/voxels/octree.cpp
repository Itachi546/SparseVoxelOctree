#include "octree.h"

#include <iostream>

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

void Octree::GenerateVoxels(std::vector<glm::vec4> &voxels) {
    GenerateVoxels(center, size, 0, voxels);
}

void Octree::GenerateVoxels(const glm::vec3 &center, float size, uint32_t parent, std::vector<glm::vec4> &voxels) {
    Node node = nodePools[parent];
    uint32_t nodeMask = node.GetNodeMask();
    float halfSize = size * 0.5f;

    if (nodeMask == NodeMask::InternalNode) {
        uint32_t firstChild = node.GetChildPtr();
        for (int i = 0; i < 8; ++i) {
            glm::vec3 childPos = center + DIRECTIONS[i] * halfSize;
            GenerateVoxels(childPos, halfSize, firstChild + i, voxels);
        }
    } else if (nodeMask == NodeMask::LeafNode) {
        voxels.push_back(glm::vec4(center, size));
    } else if (nodeMask == NodeMask::LeafNodeWithPtr) {
        uint32_t brickPtr = node.GetChildPtr() * BRICK_ELEMENT_COUNT;
        GenerateVoxelsFromBrick(center, brickPtr, voxels);
    }
}

void Octree::GenerateVoxelsFromBrick(const glm::vec3 &center, uint32_t brickPtr, std::vector<glm::vec4> &voxels) {
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
