#include "voxel-app.h"

#include "voxels/parallel-octree.h"
#include "voxels/voxel-data.h"

int main() {
    /*
    constexpr uint32_t kOctreeDims = 64;
    ParallelOctree *octree = LoadFromFile("assets/models/cathedral.vox", 0.8f, kOctreeDims);
    // octree->Serialize("cathedral.octree");
    delete octree;
    */
    VoxelApp app;
    app.Run();
    return 0;
}