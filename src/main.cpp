#include "voxel-app.h"
#include "utils.h"

#include "voxels/voxel-data.h"
#include "rendering/vulkan-device.h"

int main() {
    /*
    VoxelApp app;
    app.Run();
    return 0;
    */

    RenderingDevice *device = new VulkanDevice();
    device->SetValidationMode(true);
    device->Initialize();
    while (true) {
    }
}