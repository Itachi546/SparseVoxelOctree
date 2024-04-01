#include "octree-def.h"

#include "core/thread-safe-vector.h"
#include <vector>
#include <mutex>

struct VoxelData;

class ParallelOctree {

  public:
    ParallelOctree(const glm::vec3 &center, float size);

    void Generate(VoxelData *data);

    std::vector<Node> nodePools;
    std::vector<uint32_t> brickPools;

    glm::vec3 center;
    float size;

    std::mutex nodePoolMutex;
    std::mutex brickPoolMutex;

  private:
    struct NodeData {
        glm::vec3 center;
        uint32_t nodeIndex;
    };

    bool IsRegionEmpty(VoxelData *voxels, const glm::vec3 &min, const glm::vec3 &max);
    void CreateChildren(NodeData *parent, float size, ThreadSafeVector<NodeData> &childNodes);
    bool CreateBrick(VoxelData *voxels, OctreeBrick *brick, const glm::vec3 &min, float size);
};