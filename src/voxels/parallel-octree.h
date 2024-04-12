#include "octree-def.h"

#include "core/thread-safe-vector.h"
#include <vector>
#include <mutex>

struct VoxelData;

class ParallelOctree {

  public:
    ParallelOctree(const glm::vec3 &center, float size);
    ParallelOctree(const char *filename);

    void Generate(VoxelData *data, const glm::vec3 &cameraPosition);
    void Serialize(const char *filename);

    void ListVoxels(std::vector<glm::vec4> &voxels);

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
    Node InsertBrick(OctreeBrick *brick);

    void ListVoxelsFromBrick(const glm::vec3 &center, uint32_t brickPtr, float size, std::vector<glm::vec4> &voxels);
};