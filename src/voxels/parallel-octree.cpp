#include "parallel-octree.h"

#include "TaskScheduler.h"
#include "core/thread-safe-vector.h"
#include "voxel-data.h"

#include <fstream>
#include <iostream>

inline Node CreateNode(NodeMask nodeMask, uint32_t childPtr = 0) {
    return Node{childPtr | (nodeMask << 30)};
}

ParallelOctree::ParallelOctree(const glm::vec3 &center, float size) : center(center), size(size) {
    nodePools.push_back(CreateNode(NodeMask::InternalLeafNode));
}

ParallelOctree::ParallelOctree(const char *filename) {
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

void ParallelOctree::CreateChildren(NodeData *parent, float size, ThreadSafeVector<NodeData> &childNodes) {
    std::lock_guard lock{nodePoolMutex};
    uint32_t firstChild = static_cast<uint32_t>(nodePools.size());
    float halfSize = size * 0.5f;

    for (int i = 0; i < 8; ++i) {
        nodePools.push_back(CreateNode(InternalLeafNode, 0));
        glm::vec3 childPos = parent->center + DIRECTIONS[i] * halfSize;
        childNodes.push(NodeData{childPos, firstChild + i});
    }
    nodePools[parent->nodeIndex] = CreateNode(NodeMask::InternalNode, firstChild);
}

bool ParallelOctree::IsRegionEmpty(VoxelData *voxels, const glm::vec3 &min, const glm::vec3 &max) {
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

bool ParallelOctree::CreateBrick(VoxelData *voxels, OctreeBrick *brick, const glm::vec3 &min, float size) {
    bool empty = true;
    for (int x = 0; x < BRICK_SIZE; ++x) {
        for (int y = 0; y < BRICK_SIZE; ++y) {
            for (int z = 0; z < BRICK_SIZE; ++z) {
                glm::vec3 t01 = glm::vec3(float(x), float(y), float(z)) / float(BRICK_SIZE - 1);
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

void ParallelOctree::Generate(VoxelData *generator) {
    std::cout << "Generating Octree..." << std::endl;
    std::cout << "Num Threads: " << enki::GetNumHardwareThreads() << std::endl;
    enki::TaskScheduler ts;
    ts.Initialize();

    const uint32_t depth = static_cast<uint32_t>(std::log2(size) / (std::log2(LEAF_NODE_SCALE) + 1));
    float currentSize = size;
    uint32_t dispatchCount = 1;
    std::cout << "Octree Depth: " << depth << std::endl;

    ThreadSafeVector<NodeData> nodeList[2];
    nodeList[0].push(NodeData{
        center,
    });

    for (uint32_t i = 0; i <= depth; ++i) {
        uint32_t cl = i % 2;
        uint32_t nl = 1 - cl;

        std::cout << "Processing Depth: " << i << " Total Nodes: " << nodeList[cl].size() << std::endl;
        enki::TaskSet task(dispatchCount, [&](enki::TaskSetPartition range, uint32_t threadNum) {
            for (uint32_t work = range.start; work < range.end; ++work) {
                // Be careful about reading reference from vector and using it as pointer
                NodeData &node = nodeList[cl].get(work);
                if (!this->IsRegionEmpty(generator, node.center - currentSize, node.center + currentSize)) {
                    CreateChildren(&node, currentSize, nodeList[nl]);
                }
            }
        });

        ts.AddTaskSetToPipe(&task);
        ts.WaitforTask(&task);

        nodeList[cl].reset();
        dispatchCount = nodeList[nl].size();
        currentSize = currentSize * 0.5f;
    }

    uint32_t cl = (depth + 1) % 2;
    std::cout << "Processing brick, Total bricks: " << nodeList[cl].size() << std::endl;

    enki::TaskSet task(dispatchCount, [&](enki::TaskSetPartition range, uint32_t threadNum) {
        for (uint32_t work = range.start; work < range.end; ++work) {
            NodeData &nodeData = nodeList[cl].get(work);
            Node node = nodePools[nodeData.nodeIndex];

            OctreeBrick brick;
            brick.position = nodeData.center;
            glm::vec3 min = nodeData.center - LEAF_NODE_SCALE * 0.5f;
            bool empty = CreateBrick(generator, &brick, min, (float)LEAF_NODE_SCALE);

            if (empty) {
                node = CreateNode(InternalLeafNode);
            } else {
                if (brick.IsConstantValue()) {
                    uint32_t color = (brick.data[0] >> 8);
                    node = CreateNode(NodeMask::LeafNode, color);
                } else {
                    std::lock_guard lock{brickPoolMutex};
                    assert(brickPools.size() % BRICK_ELEMENT_COUNT == 0);
                    uint32_t address = static_cast<uint32_t>((brickPools.size() / BRICK_ELEMENT_COUNT));
                    node = CreateNode(NodeMask::LeafNodeWithPtr, address);
                    brickPools.insert(brickPools.end(), brick.data.begin(), brick.data.end());
                }
            }
            nodePools[nodeData.nodeIndex] = node;
        }
    });

    ts.AddTaskSetToPipe(&task);
    ts.WaitforTask(&task);
}

void ParallelOctree::Serialize(const char *filename) {
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
