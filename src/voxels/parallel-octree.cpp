#include "parallel-octree.h"

#include "TaskScheduler.h"
#include "core/thread-safe-vector.h"
#include "voxel-data.h"
#include "gfx/camera.h"
#include "math-utils.h"
#include "gfx/imgui-service.h"
#include <fstream>
#include <iostream>
#include <glm/gtx/component_wise.hpp>

inline Node CreateNode(NodeMask nodeMask, uint32_t childPtr = 0) {
    return Node{childPtr | (nodeMask << 30)};
}

enki::TaskScheduler ts;
ParallelOctree::ParallelOctree(const glm::vec3 &center, float size) : center(center), size(size) {
    nodePools.push_back(CreateNode(NodeMask::InternalLeafNode));
    ts.Initialize();
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
    float halfSize = size * 0.25f;

    for (int i = 0; i < 8; ++i) {
        nodePools.push_back(CreateNode(InternalLeafNode, 0));
        glm::vec3 childPos = parent->center + DIRECTIONS[i] * halfSize;
        childNodes.push(NodeData{childPos, firstChild + i, true});
    }
    nodePools[parent->nodeIndex] = CreateNode(NodeMask::InternalNode, firstChild);
}

Node ParallelOctree::InsertBrick(OctreeBrick *brick) {
    Node node = CreateNode(NodeMask::InternalLeafNode);
    if (brick->IsConstantValue()) {
        uint32_t color = (brick->data[0] >> 8);
        node = CreateNode(NodeMask::LeafNode, color);
    } else {
        std::lock_guard lock{brickPoolMutex};
        assert(brickPools.size() % BRICK_ELEMENT_COUNT == 0);
        uint32_t address = static_cast<uint32_t>((brickPools.size() / BRICK_ELEMENT_COUNT));
        node = CreateNode(NodeMask::LeafNodeWithPtr, address);
        brickPools.insert(brickPools.end(), brick->data.begin(), brick->data.end());
    }
    return node;
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
// Create octree brick from AABB
bool ParallelOctree::CreateBrick(VoxelData *voxels, OctreeBrick *brick, const glm::vec3 &min, float size) {
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

float ParallelOctree::CalculateLOD(float camDist) {
    if (camDist < 16.0f)
        return 1.0f;
    else if (camDist < 32.0f)
        return 2.0f;
    else if (camDist < 64.0f)
        return 4.0f;
    else if (camDist < 128.0f)
        return 8.0f;
    else if (camDist < 256.0f)
        return 16.0f;
    else
        return 32.0f;
}

void ParallelOctree::Generate(VoxelData *generator, const glm::vec3 &cameraPosition) {
    this->generator = generator;

    std::cout << "Generating Octree..." << std::endl;
    std::cout << "Num Threads: " << enki::GetNumHardwareThreads() << std::endl;

    const uint32_t depth = static_cast<uint32_t>(std::log2(size * 2.0f) / (std::log2(LEAF_NODE_SCALE) + 1));
    float currentSize = size * 2.0f;
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
                NodeData &nodeData = nodeList[cl].get(work);
                float halfSize = currentSize * 0.5f;
                if (!this->IsRegionEmpty(generator, nodeData.center - halfSize, nodeData.center + halfSize)) {
                    float camDist = glm::length(cameraPosition - nodeData.center);
                    float lod = CalculateLOD(camDist);

                    if (lod >= currentSize) {
                        OctreeBrick brick{};
                        brick.position = nodeData.center;
                        glm::vec3 min = nodeData.center - halfSize;
                        bool empty = CreateBrick(generator, &brick, min, currentSize);
                        Node node = nodePools[nodeData.nodeIndex];
                        if (empty)
                            node = CreateNode(InternalLeafNode);
                        else
                            node = InsertBrick(&brick);
                        nodePools[nodeData.nodeIndex] = node;
                    } else
                        CreateChildren(&nodeData, currentSize, nodeList[nl]);
                }
            }
        });

        ts.AddTaskSetToPipe(&task);
        ts.WaitforTask(&task);

        nodeList[cl].reset();
        dispatchCount = nodeList[nl].size();
        if (dispatchCount == 0)
            break;
        currentSize = currentSize * 0.5f;
    }
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

void ParallelOctree::ListVoxels(std::vector<glm::vec4> &voxels, gfx::Camera *camera) {
    uint32_t dispatchCount = 1;
    float currentSize = size;
    const uint32_t depth = static_cast<uint32_t>(std::log2(size * 2.0f) / (std::log2(LEAF_NODE_SCALE) + 1));

    ThreadSafeVector<NodeData> nodeList[2];
    nodeList[0].push(NodeData{center, 0});

    std::mutex insertionMutex;
    for (uint32_t i = 0; i <= depth; ++i) {
        uint32_t cl = i % 2;
        uint32_t nl = 1 - cl;

        enki::TaskSet task(dispatchCount, [&](enki::TaskSetPartition range, uint32_t threadNum) {
            for (uint32_t work = range.start; work < range.end; ++work) {
                // Be careful about reading reference from vector and using it as pointer
                NodeData &nodeData = nodeList[cl].get(work);
                Node node = nodePools[nodeData.nodeIndex];

                glm::vec3 center = nodeData.center;
                if (!AABBFrustumIntersection(center - currentSize, center + currentSize, camera->frustumPlanes))
                    continue;

                if (node.GetNodeMask() == NodeMask::InternalNode) {
                    for (int i = 0; i < 8; ++i) {
                        glm::vec3 childPos = center + DIRECTIONS[i] * currentSize * 0.5f;
                        nodeList[nl].push(NodeData{childPos, node.GetChildPtr() + i});
                    }
                } else if (node.GetNodeMask() == NodeMask::LeafNode) {
                    std::lock_guard insertionGuard{insertionMutex};
                    voxels.push_back(glm::vec4{center, currentSize});
                } else if (node.GetNodeMask() == NodeMask::LeafNodeWithPtr) {
                    std::lock_guard insertionGuard{insertionMutex};
                    ListVoxelsFromBrick(center, node.GetChildPtr() * BRICK_ELEMENT_COUNT, currentSize * 2.0f, voxels);
                }
            }
        });

        ts.AddTaskSetToPipe(&task);
        ts.WaitforTask(&task);

        nodeList[cl].reset();
        dispatchCount = nodeList[nl].size();
        currentSize = currentSize * 0.5f;
    }
}

void ParallelOctree::ListVoxelsFromBrick(const glm::vec3 &center, uint32_t brickPtr, float gridSize, std::vector<glm::vec4> &voxels) {
    float unitVoxelHalfSize = (gridSize * 0.5f) / float(NUM_BRICK);
    glm::vec3 min = center - gridSize * 0.5f;
    for (int x = 0; x < NUM_BRICK; ++x) {
        for (int y = 0; y < NUM_BRICK; ++y) {
            for (int z = 0; z < NUM_BRICK; ++z) {
                uint32_t offset = x * NUM_BRICK * NUM_BRICK + y * NUM_BRICK + z;
                uint32_t val = brickPools[brickPtr + offset];
                if (val > 0) {
                    glm::vec3 t01 = glm::vec3(float(x), float(y), float(z)) / float(NUM_BRICK);
                    glm::vec3 position = min + t01 * gridSize;
                    // Calculate center and size
                    voxels.push_back(glm::vec4(position + unitVoxelHalfSize, unitVoxelHalfSize));
                }
            }
        }
    }
}

void ParallelOctree::Update(gfx::Camera *camera) {
    // Traverse octree on each level
    // Calculate new LOD
    // Check if it matches with existing LOD
    // If it doesn't match we process it

    float currentSize = size * 2.0f;
    const uint32_t depth = static_cast<uint32_t>(std::log2(size * 2.0f) / (std::log2(LEAF_NODE_SCALE) + 1));

    std::vector<NodeData> nodeLists[2];
    nodeLists[0].push_back(NodeData{center, 0});

    struct NodeUpdateJob {
        uint32_t nodeIndex;
        glm::vec3 center;
        float size;
    };
    const glm::vec3 cameraPosition = camera->GetPosition();
    std::vector<NodeUpdateJob> updateJobs;

    auto begin = std::chrono::high_resolution_clock::now();
    for (uint32_t cd = 0; cd <= depth; ++cd) {
        uint32_t cl = cd % 2;
        uint32_t nl = 1 - cl;
        float halfSize = currentSize * 0.5f;
        for (uint32_t n = 0; n < nodeLists[cl].size(); ++n) {
            // Be careful about reading reference from vector and using it as pointer
            NodeData &nodeData = nodeLists[cl][n];
            Node node = nodePools[nodeData.nodeIndex];

            glm::vec3 center = nodeData.center;
            uint32_t mask = node.GetNodeMask();
            // Check if it is leaf node
            float camDist = glm::compMax(glm::abs(cameraPosition - center));
            float expectedSize = CalculateLOD(camDist);

            if (mask == NodeMask::LeafNodeWithPtr || mask == NodeMask::LeafNode) {
                // Case I, we need to remove some child nodes
                if (expectedSize < currentSize) {
                    // Return brick to brick pool
                    if (mask == NodeMask::LeafNodeWithPtr)
                        freeBrickPools.push(node.GetChildPtr());
                    // CreateChildren(&nodeData, currentSize, nodeLists[nl]);
                    //  Check if node pool has any free pointer
                    uint32_t childPtr = 0;
                    if (freeNodePools.size() > 0) {
                        childPtr = freeNodePools.pop();
                    } else {
                        childPtr = static_cast<uint32_t>(nodePools.size());
                        for (uint32_t child = 0; child < 8; ++child)
                            nodePools.push_back(CreateNode(NodeMask::InternalNode));
                    }
                    float childSize = halfSize * 0.5f;
                    for (int child = 0; child < 8; ++child) {
                        nodePools[childPtr + child] = CreateNode(InternalLeafNode, 0);
                        glm::vec3 childPos = center + DIRECTIONS[child] * childSize;
                        nodeLists[nl].push_back(NodeData{
                            childPos,
                            childPtr + child,
                            true});
                    }
                    nodePools[nodeData.nodeIndex] = CreateNode(NodeMask::InternalNode, childPtr);
                }
            } else if (mask == NodeMask::InternalNode) {
                if (expectedSize >= currentSize) {
                    // Remove child nodes
                    uint32_t childPtr = node.GetChildPtr();
                    freeNodePools.push(childPtr);

                    updateJobs.push_back(NodeUpdateJob{nodeData.nodeIndex, nodeData.center, currentSize});
                } else {
                    for (int i = 0; i < 8; ++i) {
                        glm::vec3 childPos = center + DIRECTIONS[i] * halfSize * 0.5f;
                        nodeLists[nl].push_back(NodeData{childPos, node.GetChildPtr() + i});
                    }
                }
            } else if (mask == NodeMask::InternalLeafNode && nodeData.newNode) {

                if (expectedSize >= currentSize) {
                    uint32_t childPtr = 0;

                    updateJobs.push_back(NodeUpdateJob{nodeData.nodeIndex, nodeData.center, currentSize});
                    nodePools[nodeData.nodeIndex] = CreateNode(NodeMask::LeafNodeWithPtr, childPtr);
                }
            }
        }

        nodeLists[cl].clear();
        currentSize = currentSize * 0.5f;
    }

    auto end = std::chrono::high_resolution_clock::now();
    uint64_t duration = std::chrono::duration_cast<std::chrono::microseconds>(end - begin).count();
    ImGui::Text("Octree traversal: %.3f", float(duration) / 1000.0f);

    uint32_t freedBlock = freeBrickPools.size();
    ImGui::Text("Total block freed: %d", freedBlock);
    ImGui::Text("Current Size: %.2f", currentSize);
    uint32_t dispatchCount = static_cast<uint32_t>(updateJobs.size());
    ImGui::Text("Total new nodes: %d", dispatchCount);

    begin = std::chrono::high_resolution_clock::now();
    if (dispatchCount > 0) {
        enki::TaskSet brickUpdateTask(dispatchCount, [&](enki::TaskSetPartition range, uint32_t threadNum) {
            for (uint32_t work = range.start; work < range.end; ++work) {
                NodeUpdateJob &nodeData = updateJobs[work];

                OctreeBrick brick;
                float halfSize = nodeData.size * 0.5f;
                bool empty = CreateBrick(generator, &brick, nodeData.center - halfSize, nodeData.size);
                if (empty) {
                    nodePools[nodeData.nodeIndex] = CreateNode(NodeMask::InternalLeafNode);
                } else {

                    if (brick.IsConstantValue()) {
                        uint32_t color = (brick.data[0] >> 8);
                        nodePools[nodeData.nodeIndex] = CreateNode(NodeMask::LeafNode, color);
                    } else {
                        uint32_t childPtr = 0;
                        if (freeBrickPools.try_pop(&childPtr)) {
                            std::lock_guard<std::mutex> brickPoolLock{brickPoolMutex};
                            std::memcpy(brickPools.data() + childPtr * BRICK_ELEMENT_COUNT, brick.data.data(), BRICK_ELEMENT_COUNT * sizeof(uint32_t));
                            // std::memcpy(brickPools.data() + childPtr * BRICK_ELEMENT_COUNT, brick.data.data(), BRICK_ELEMENT_COUNT * sizeof(uint32_t));
                        } else {
                            std::lock_guard<std::mutex> brickPoolLock{brickPoolMutex};
                            childPtr = static_cast<uint32_t>(brickPools.size() / BRICK_ELEMENT_COUNT);
                            brickPools.insert(brickPools.end(), brick.data.begin(), brick.data.end());
                        }
                        nodePools[nodeData.nodeIndex] = CreateNode(NodeMask::LeafNodeWithPtr, childPtr);
                    }
                }
            }
        });
        ts.AddTaskSetToPipe(&brickUpdateTask);
        ts.WaitforTask(&brickUpdateTask);
    }
    end = std::chrono::high_resolution_clock::now();
    duration = std::chrono::duration_cast<std::chrono::microseconds>(end - begin).count();
    ImGui::Text("Brick Generation: %.2f", float(duration) / 1000.0f);
    // std::cout << "Total free bricks: " << freeBrickPools.size() << std::endl;
}
