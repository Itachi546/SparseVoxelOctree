#include "voxel-app.h"

#include "voxels/parallel-octree.h"
#include "voxels/voxel-data.h"

// @TODO Temp
ParallelOctree *LoadFromFile(const char *filename, float scale, uint32_t kOctreeDims) {
    ParallelOctree *octree = new ParallelOctree(glm::vec3(0.0f), float(kOctreeDims));
    VoxModelData model;
    model.Load(filename, scale);
    {
        auto start = std::chrono::high_resolution_clock::now();
        octree->Generate(&model);
        auto end = std::chrono::high_resolution_clock::now();
        float duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count() / 1000.0f;
        std::cout << "Total time to generate chunk: " << duration << "ms" << std::endl;
    }
    model.Destroy();
    return octree;
}

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